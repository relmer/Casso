#include "Pch.h"

#include "ModernPrintDialog.h"

#include "../Resource.h"
#include "../../CassoEmuCore/Devices/Printer/PaperRenderer.h"
#include "../../CassoEmuCore/Devices/Printer/PrintPagination.h"
#include "../../CassoEmuCore/Devices/Printer/PrintRaster.h"
#include "../../CassoEmuCore/Devices/Printer/RgbaImage.h"

#include <windows.foundation.h>
#include <windows.graphics.printing.h>
#include <printmanagerinterop.h>
#include <documentsource.h>
#include <documenttarget.h>
#include <printpreview.h>
#include <roapi.h>
#include <DispatcherQueue.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <wincodec.h>

#pragma comment (lib, "runtimeobject.lib")
#pragma comment (lib, "CoreMessaging.lib")
#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "d3d11.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::Make;
using Microsoft::WRL::Wrappers::HStringReference;

namespace awgp = ABI::Windows::Graphics::Printing;




// Preview pages render at a modest fixed density -- the pane is small and
// resamples anyway; the FINAL print uses the user's configured 288 / 576 dpi.
static constexpr int     s_kPreviewDpi     = 150;

// US-Letter page box in DIPs (1/96"), the fallback when the print system's
// page description is unavailable.
static constexpr float   s_kLetterWDips    = 816.0f;
static constexpr float   s_kLetterHDips    = 1056.0f;

////////////////////////////////////////////////////////////////////////////////
//
//  PrintPageSource
//
//  The per-session document source handed to the OS print task. Owns a COPY
//  of the strip plus the render prefs, its own D3D/D2D stack (print callbacks
//  arrive on print-system threads -- never touch the UI renderer), and the
//  paginated page list. Implements:
//    - IPrintDocumentSource (WinRT marker) so PrintTaskSourceRequestedArgs
//      accepts it,
//    - IPrintPreviewPageCollection: Paginate + MakePage fill the preview pane,
//    - IPrintDocumentPageSource: MakeDocument spools the real job through
//      ID2D1PrintControl.
//  A mutex serializes the D2D context: preview and spool calls can overlap.
//
////////////////////////////////////////////////////////////////////////////////

class PrintPageSource
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          awgp::IPrintDocumentSource,
          IPrintDocumentPageSource,
          IPrintPreviewPageCollection>
{
    InspectableClass (L"Casso.PrintPageSource", BaseTrust);

public:
    HRESULT RuntimeClassInitialize (const PrintRaster & raster, int outputDpi, DotStyle style)
    {
        m_raster    = raster;   // session-owned copy: the live worker resumes immediately
        m_outputDpi = outputDpi;
        m_style     = style;
        m_pages     = PrintPagination::Paginate (m_raster);

        return m_pages.empty () ? E_FAIL : S_OK;
    }

    UINT32 PageCount () const { return (UINT32) m_pages.size (); }

    //
    // IPrintDocumentPageSource
    //

    IFACEMETHODIMP GetPreviewPageCollection (IPrintDocumentPackageTarget   * docPackageTarget,
                                             IPrintPreviewPageCollection  ** docPageCollection) override
    {
        HRESULT  hr = S_OK;

        if (docPackageTarget == nullptr || docPageCollection == nullptr)
        {
            return E_INVALIDARG;
        }

        hr = docPackageTarget->GetPackageTarget (ID_PREVIEWPACKAGETARGET_DXGI,
                                                 IID_PPV_ARGS (&m_previewTarget));
        if (FAILED (hr))
        {
            return hr;
        }

        return QueryInterface (IID_PPV_ARGS (docPageCollection));
    }

    IFACEMETHODIMP MakeDocument (IInspectable                * docSettings,
                                 IPrintDocumentPackageTarget * docPackageTarget) override
    {
        HRESULT                      hr       = S_OK;
        D2D_SIZE_F                   pageSize = D2D1::SizeF (s_kLetterWDips, s_kLetterHDips);
        D2D1_RECT_F                  box      = D2D1::RectF (0.0f, 0.0f, s_kLetterWDips, s_kLetterHDips);
        ComPtr<ID2D1PrintControl>    control;
        ComPtr<IWICImagingFactory2>  wic;

        if (docPackageTarget == nullptr)
        {
            return E_INVALIDARG;
        }

        // Real page geometry from the task options when available; content is
        // laid into the printer's IMAGEABLE rect so a physical device never
        // clips the fanfold edge (Print-to-PDF images the whole sheet anyway).
        {
            ComPtr<awgp::IPrintTaskOptionsCore>  options;

            if (docSettings != nullptr &&
                SUCCEEDED (docSettings->QueryInterface (IID_PPV_ARGS (&options))))
            {
                awgp::PrintPageDescription  desc = {};

                if (SUCCEEDED (options->GetPageDescription (1, &desc)) &&
                    desc.PageSize.Width > 0.0f && desc.PageSize.Height > 0.0f)
                {
                    pageSize = D2D1::SizeF (desc.PageSize.Width, desc.PageSize.Height);
                    box      = D2D1::RectF (0.0f, 0.0f, desc.PageSize.Width, desc.PageSize.Height);

                    if (desc.ImageableRect.Width > 0.0f && desc.ImageableRect.Height > 0.0f)
                    {
                        box = D2D1::RectF (desc.ImageableRect.X,
                                           desc.ImageableRect.Y,
                                           desc.ImageableRect.X + desc.ImageableRect.Width,
                                           desc.ImageableRect.Y + desc.ImageableRect.Height);
                    }
                }
            }
        }

        std::lock_guard<std::mutex>  lock (m_renderLock);

        hr = EnsureDeviceLocked ();
        if (FAILED (hr)) { return hr; }

        hr = CoCreateInstance (CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS (&wic));
        if (FAILED (hr)) { return hr; }

        hr = m_d2dDevice->CreatePrintControl (wic.Get (), docPackageTarget, nullptr, &control);
        if (FAILED (hr))
        {
            return hr;
        }

        for (size_t pageIx = 0; pageIx < m_pages.size (); pageIx++)
        {
            ComPtr<ID2D1CommandList>  list;
            ComPtr<ID2D1Bitmap1>      bitmap;

            hr = RenderPageBitmapLocked ((UINT32) pageIx, m_outputDpi, &bitmap);
            if (FAILED (hr)) { break; }

            hr = m_d2dContext->CreateCommandList (&list);
            if (FAILED (hr)) { break; }

            m_d2dContext->SetTarget (list.Get ());
            m_d2dContext->BeginDraw ();
            m_d2dContext->Clear (D2D1::ColorF (D2D1::ColorF::White));
            DrawPageWidthFitInBox (bitmap.Get (), box);
            hr = m_d2dContext->EndDraw ();
            m_d2dContext->SetTarget (nullptr);
            if (FAILED (hr)) { break; }

            hr = list->Close ();
            if (FAILED (hr)) { break; }

            hr = control->AddPage (list.Get (), pageSize, nullptr, nullptr, nullptr);
            if (FAILED (hr))
            {
                break;
            }
        }

        {
            HRESULT  hrClose = control->Close ();

            if (SUCCEEDED (hr)) { hr = hrClose; }
        }

        return hr;
    }

    //
    // IPrintPreviewPageCollection
    //

    IFACEMETHODIMP Paginate (UINT32 currentJobPage, IInspectable * docSettings) override
    {
        UNREFERENCED_PARAMETER (currentJobPage);
        UNREFERENCED_PARAMETER (docSettings);

        if (m_previewTarget != nullptr)
        {
            m_previewTarget->SetJobPageCount (PageCountType::FinalPageCount, PageCount ());
        }
        return S_OK;
    }

    IFACEMETHODIMP MakePage (UINT32 desiredJobPage, FLOAT width, FLOAT height) override
    {
        HRESULT                  hr     = S_OK;
        UINT32                   jobPage = desiredJobPage;
        UINT                     pxW    = 0;
        UINT                     pxH    = 0;
        ComPtr<ID3D11Texture2D>  texture;
        ComPtr<IDXGISurface>     surface;
        ComPtr<ID2D1Bitmap1>     target;
        ComPtr<ID2D1Bitmap1>     pageBitmap;

        if (m_previewTarget == nullptr || width <= 1.0f || height <= 1.0f)
        {
            return E_FAIL;
        }

        if (jobPage == JOB_PAGE_APPLICATION_DEFINED)
        {
            jobPage = 1;
        }
        if (jobPage < 1 || jobPage > PageCount ())
        {
            return E_INVALIDARG;
        }

        std::lock_guard<std::mutex>  lock (m_renderLock);

        hr = EnsureDeviceLocked ();
        if (FAILED (hr)) { return hr; }

        // The preview target composites at 96 DPI: surface pixels == DIPs.
        pxW = (UINT) (width  + 0.5f);
        pxH = (UINT) (height + 0.5f);

        {
            D3D11_TEXTURE2D_DESC  td = {};

            td.Width            = pxW;
            td.Height           = pxH;
            td.MipLevels        = 1;
            td.ArraySize        = 1;
            td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage            = D3D11_USAGE_DEFAULT;
            td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            hr = m_d3dDevice->CreateTexture2D (&td, nullptr, &texture);
            if (FAILED (hr)) { return hr; }
        }

        hr = texture.As (&surface);
        if (FAILED (hr)) { return hr; }

        {
            D2D1_BITMAP_PROPERTIES1  bp = D2D1::BitmapProperties1 (
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat (DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

            hr = m_d2dContext->CreateBitmapFromDxgiSurface (surface.Get (), &bp, &target);
            if (FAILED (hr)) { return hr; }
        }

        hr = RenderPageBitmapLocked (jobPage - 1, s_kPreviewDpi, &pageBitmap);
        if (FAILED (hr)) { return hr; }

        m_d2dContext->SetTarget (target.Get ());
        m_d2dContext->BeginDraw ();
        m_d2dContext->Clear (D2D1::ColorF (D2D1::ColorF::White));
        DrawPageWidthFit (pageBitmap.Get (), width, height);
        hr = m_d2dContext->EndDraw ();
        m_d2dContext->SetTarget (nullptr);
        if (FAILED (hr)) { return hr; }

        return m_previewTarget->DrawPage (jobPage, surface.Get (), 96.0f, 96.0f);
    }

private:
    // Create the session's private D3D device + D2D device context (lazy;
    // under m_renderLock). WARP fallback keeps preview working without GPU.
    HRESULT EnsureDeviceLocked ()
    {
        HRESULT               hr       = S_OK;
        ComPtr<IDXGIDevice>   dxgi;
        ComPtr<ID2D1Factory1> factory;
        ComPtr<ID2D1Device>   device;

        if (m_d2dContext != nullptr)
        {
            return S_OK;
        }

        hr = D3D11CreateDevice (nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, nullptr, nullptr);
        if (FAILED (hr))
        {
            hr = D3D11CreateDevice (nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                    nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, nullptr, nullptr);
        }
        if (FAILED (hr)) { return hr; }

        hr = m_d3dDevice.As (&dxgi);
        if (FAILED (hr)) { return hr; }

        hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
                                __uuidof (ID2D1Factory1), nullptr, (void **) factory.GetAddressOf ());
        if (FAILED (hr)) { return hr; }

        hr = factory->CreateDevice (dxgi.Get (), &device);
        if (FAILED (hr)) { return hr; }

        m_d2dDevice = device;

        return device->CreateDeviceContext (D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    }

    // Render one paginated page span through PaperRenderer at `dpi` and wrap
    // it as a D2D bitmap (RGBA -> premultiplied BGRA; pages are opaque).
    HRESULT RenderPageBitmapLocked (UINT32 pageIx, int dpi, ID2D1Bitmap1 ** out)
    {
        HRESULT                  hr = S_OK;
        PaperRenderer            renderer;
        PaperRenderer::Options   opt;
        RgbaImage                img;
        vector<Byte>             bgra;

        opt.outputDpi = dpi;
        opt.style     = m_style;

        hr = renderer.Render (m_raster, m_pages[pageIx].firstRow, m_pages[pageIx].lastRow, opt, img);
        if (FAILED (hr)) { return hr; }
        if (img.width <= 0 || img.height <= 0) { return E_FAIL; }

        bgra.resize ((size_t) img.width * img.height * 4);
        for (size_t i = 0; i < (size_t) img.width * img.height; i++)
        {
            bgra[i * 4 + 0] = img.rgba[i * 4 + 2];
            bgra[i * 4 + 1] = img.rgba[i * 4 + 1];
            bgra[i * 4 + 2] = img.rgba[i * 4 + 0];
            bgra[i * 4 + 3] = 0xFF;
        }

        {
            D2D1_BITMAP_PROPERTIES1  bp = D2D1::BitmapProperties1 (
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat (DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                (FLOAT) dpi, (FLOAT) dpi);

            hr = m_d2dContext->CreateBitmap (D2D1::SizeU ((UINT32) img.width, (UINT32) img.height),
                                             bgra.data (), (UINT32) img.width * 4, &bp, out);
        }

        return hr;
    }

    // Width-fit + top-align the page bitmap into a destination box (DIPs) --
    // identical composition rules to the classic GDI path's BlitRgbaToDc, so
    // preview, modern print, and classic print all agree.
    void DrawPageWidthFit (ID2D1Bitmap1 * bitmap, float boxW, float boxH)
    {
        DrawPageWidthFitInBox (bitmap, D2D1::RectF (0.0f, 0.0f, boxW, boxH));
    }

    void DrawPageWidthFitInBox (ID2D1Bitmap1 * bitmap, const D2D1_RECT_F & box)
    {
        D2D1_SIZE_F  sz    = bitmap->GetSize ();   // in DIPs (bitmap carries its dpi)
        float        boxW  = box.right - box.left;
        float        scale = (sz.width > 0.0f) ? (boxW / sz.width) : 1.0f;
        float        destW = sz.width  * scale;
        float        destH = sz.height * scale;
        float        x     = box.left + (boxW - destW) * 0.5f;

        // Top-aligned within the box: the fanfold continues across page breaks.
        m_d2dContext->DrawBitmap (bitmap,
                                  D2D1::RectF (x, box.top, x + destW, box.top + destH),
                                  1.0f,
                                  D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    }

    PrintRaster                              m_raster;
    int                                      m_outputDpi = 576;
    DotStyle                                 m_style     = DotStyle::Ink;
    vector<PrintPagination::PageRange>       m_pages;

    std::mutex                               m_renderLock;
    ComPtr<ID3D11Device>                     m_d3dDevice;
    ComPtr<ID2D1Device>                      m_d2dDevice;
    ComPtr<ID2D1DeviceContext>               m_d2dContext;
    ComPtr<IPrintPreviewDxgiPackageTarget>   m_previewTarget;
};




////////////////////////////////////////////////////////////////////////////////
//
//  ModernPrintDialog::~ModernPrintDialog
//
////////////////////////////////////////////////////////////////////////////////

ModernPrintDialog::~ModernPrintDialog ()
{
    if (m_registered && m_manager != nullptr)
    {
        ComPtr<awgp::IPrintManager>  manager;

        if (SUCCEEDED (m_manager.As (&manager)))
        {
            manager->remove_PrintTaskRequested (m_taskToken);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Modern print task handlers
//
//  Free functions so ShowAsync's registration stays flat. The document source
//  is passed and captured BY VALUE, so it is owned by this show and cannot be
//  nulled by another print -- the "connecting forever" hang came from a shared
//  session member that a prior task's completion reset out from under the event.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ModernSourceRequested (ComPtr<IUnknown> session,
                                      awgp::IPrintTaskSourceRequestedArgs * srcArgs)
{
    HRESULT                             hr = S_OK;
    ComPtr<awgp::IPrintDocumentSource>  docSource;

    hr = session.CopyTo (docSource.GetAddressOf());
    CHR (hr);

    hr = srcArgs->SetSource (docSource.Get());
    CHR (hr);

Error:
    return hr;
}




static HRESULT ModernTaskCompleted (HWND postHwnd, awgp::IPrintTaskCompletedEventArgs * done)
{
    awgp::PrintTaskCompletion  completion = awgp::PrintTaskCompletion_Abandoned;

    done->get_Completion (&completion);

    if (completion == awgp::PrintTaskCompletion_Submitted)
    {
        PostMessageW (postHwnd, WM_COMMAND, MAKEWPARAM (IDM_PRINTER_MODERN_SENT, 0), 0);
    }
    else if (completion == awgp::PrintTaskCompletion_Failed)
    {
        PostMessageW (postHwnd, WM_COMMAND, MAKEWPARAM (IDM_PRINTER_MODERN_FAILED, 0), 0);
    }

    return S_OK;
}




static HRESULT ModernTaskRequested (HWND postHwnd, ComPtr<IUnknown> session,
                                    awgp::IPrintTaskRequestedEventArgs * args)
{
    HRESULT                          hr             = S_OK;
    ComPtr<awgp::IPrintTaskRequest>  request;
    ComPtr<awgp::IPrintTask>         task;
    EventRegistrationToken           completedToken = {};

    hr = args->get_Request (&request);
    CHR (hr);

    hr = request->CreatePrintTask (
             HStringReference (L"Casso Printout").Get(),
             Callback<awgp::IPrintTaskSourceRequestedHandler> (
                 [session] (awgp::IPrintTaskSourceRequestedArgs * srcArgs) -> HRESULT
                 {
                     return ModernSourceRequested (session, srcArgs);
                 }).Get(),
             &task);
    CHR (hr);
    CBRA (task != nullptr);

    hr = task->add_Completed (
             Callback<ABI::Windows::Foundation::ITypedEventHandler<
                 awgp::PrintTask *, awgp::PrintTaskCompletedEventArgs *>> (
                 [postHwnd] (awgp::IPrintTask *, awgp::IPrintTaskCompletedEventArgs * done) -> HRESULT
                 {
                     return ModernTaskCompleted (postHwnd, done);
                 }).Get(),
             &completedToken);
    CHR (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ModernPrintDialog::ShowAsync
//
//  Builds the session document source (strip copy + pagination), lazily wires
//  the per-window PrintManager, registers a per-show PrintTaskRequested handler
//  that owns this show's source, and shows the OS print UI. Completion posts
//  IDM_PRINTER_MODERN_SENT / _FAILED back to the window; cancel posts nothing.
//  Every failure returns FAILED so the caller can fall back to the classic dialog.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ModernPrintDialog::ShowAsync (HWND hwnd, const PrintRaster & raster, int outputDpi, DotStyle style)
{
    HRESULT                       hr       = S_OK;
    ComPtr<IPrintManagerInterop>  interop;
    ComPtr<awgp::IPrintManager>   manager;
    ComPtr<PrintPageSource>       source;
    ComPtr<IInspectable>          showOp;
    ComPtr<IUnknown>              session;
    HWND                          postHwnd = hwnd;

    // Newer Windows builds route the modern print callbacks through a
    // DispatcherQueue; without one on the calling thread the dialog can sit at
    // "connecting" forever. Create a thread-bound queue controller once.
    if (m_dispatcherQueue == nullptr)
    {
        DispatcherQueueOptions  options = {};

        options.dwSize        = sizeof (options);
        options.threadType    = DQTYPE_THREAD_CURRENT;
        options.apartmentType = DQTAT_COM_STA;

        // Best-effort: without a queue the modern dialog can hang, but there is
        // no fallback if creation fails, so the result is not acted on.
        (void) CreateDispatcherQueueController (options, (PDISPATCHERQUEUECONTROLLER *) m_dispatcherQueue.GetAddressOf());
    }

    hr = Microsoft::WRL::MakeAndInitialize<PrintPageSource> (&source, raster, outputDpi, style);
    CHR (hr);

    if (m_interop == nullptr)
    {
        hr = RoGetActivationFactory (
                 HStringReference (RuntimeClass_Windows_Graphics_Printing_PrintManager).Get(),
                 IID_PPV_ARGS (&interop));
        CHR (hr);
        m_interop = interop;
    }
    else
    {
        hr = m_interop.As (&interop);
        CHR (hr);
    }

    if (m_manager == nullptr || m_hwnd != hwnd)
    {
        if (m_registered && m_manager != nullptr)
        {
            ComPtr<awgp::IPrintManager>  old;

            if (SUCCEEDED (m_manager.As (&old)))
            {
                old->remove_PrintTaskRequested (m_taskToken);
            }
            m_registered = false;
        }

        hr = interop->GetForWindow (hwnd, IID_PPV_ARGS (&manager));
        CHR (hr);
        m_manager = manager;
        m_hwnd    = hwnd;
    }
    else
    {
        hr = m_manager.As (&manager);
        CHR (hr);
    }

    // This print's document source, captured BY VALUE in the handler below --
    // never stored in a shared member the event reads later. A shared slot let
    // a prior print's task completion null it, so PrintTaskRequested read null,
    // dropped the request, and the dialog spun at "connecting" forever.
    hr = source.CopyTo (session.GetAddressOf());
    CHR (hr);

    // Re-register per show so handlers never accumulate on the per-window
    // PrintManager; the new handler owns this show's source.
    if (m_registered)
    {
        ComPtr<awgp::IPrintManager>  mgr;

        if (SUCCEEDED (manager.As (&mgr)))
        {
            mgr->remove_PrintTaskRequested (m_taskToken);
        }
        m_registered = false;
    }

    hr = manager->add_PrintTaskRequested (
             Callback<ABI::Windows::Foundation::ITypedEventHandler<
                 awgp::PrintManager *, awgp::PrintTaskRequestedEventArgs *>> (
                 [postHwnd, session] (awgp::IPrintManager *, awgp::IPrintTaskRequestedEventArgs * args) -> HRESULT
                 {
                     return ModernTaskRequested (postHwnd, session, args);
                 }).Get(),
             &m_taskToken);
    CHR (hr);
    m_registered = true;

    hr = interop->ShowPrintUIForWindowAsync (hwnd, IID_PPV_ARGS (&showOp));
    CHR (hr);

    // Hold the async operation for the session: releasing it immediately can
    // tear down the print experience's connection back into the app (the dialog
    // then spins at "connecting" forever).
    m_showOp = showOp;

Error:
    return hr;
}
