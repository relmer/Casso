#include "Pch.h"

#include "DiskIIDebugPanel.h"

#include "Chrome/ChromeTheme.h"
#include "Chrome/TitleBar.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName  = L"Casso.DiskIIDebug.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle = L"Disk II Debug";

    constexpr int      s_kPreferredWidthDip  = 960;
    constexpr int      s_kPreferredHeightDip = 600;
    constexpr UINT     s_kSwapBufferCount     = 2;



    void ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
    {
        outRgba[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
        outRgba[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
        outRgba[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
        outRgba[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugPanel::DiskIIDebugPanel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DiskIIDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugPanel::~DiskIIDebugPanel()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Registers the window class for this panel content type, then asks
//  the chrome shell to stand up the host HWND bound to this content.
//  Idempotent -- a second call while already open is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const ChromeTheme    * theme)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_window.IsOpen(), S_OK);

    CBRAEx (hInstance, E_INVALIDARG);
    CBRAEx (device,    E_INVALIDARG);
    CBRAEx (context,   E_INVALIDARG);

    m_device  = device;
    m_context = context;
    m_theme   = theme;

    hr = m_window.RegisterClass (hInstance, s_kpszClassName);
    CHRA (hr);

    hr = m_window.Create (hwndOwner, this, device, context, theme);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Brings the host window to the front. Lifecycle assumes Create has
//  already succeeded; ShowWindow on a null HWND is a no-op anyway.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Show()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_SHOW);
        SetForegroundWindow (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Hide()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_HIDE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Destroy()
{
    m_window.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
//  Public render entry point invoked by the host frame loop. Delegates
//  to the chrome shell which composites our content (currently just a
//  themed background clear) under its title bar.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::RenderFrame()
{
    return m_window.Render();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  IChromedPanelContent override -- invoked by the chrome shell during
//  its own Render. T044 lands the panel body as a flat themed clear;
//  T046+ layers in widget rendering atop this.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::Render()
{
    HRESULT  hr        = S_OK;
    float    clear[4]  = { 0.08f, 0.08f, 0.08f, 1.0f };



    BAIL_OUT_IF (m_swapChain == nullptr || m_rtv == nullptr || m_context == nullptr, S_OK);

    if (m_theme != nullptr)
    {
        ArgbToFloat4 (m_theme->titleBarBottomArgb, clear);
        clear[3] = 1.0f;
    }

    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clear);

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::SetTheme (const ChromeTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowClassName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskIIDebugPanel::GetWindowClassName() const
{
    return s_kpszClassName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskIIDebugPanel::GetWindowTitle() const
{
    return s_kpszWindowTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostCreated
//
//  Stands up the swap chain bound to the host HWND. Title-bar pointer
//  and theme are remembered so SetChromeTheme calls land on the same
//  TitleBar instance the shell painted into.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::OnHostCreated (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi,
    TitleBar             * titleBar,
    const ChromeTheme    * theme)
{
    HRESULT  hr = S_OK;


    CBRAEx (hwnd,    E_INVALIDARG);
    CBRAEx (device,  E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    m_hwnd     = hwnd;
    m_device   = device;
    m_context  = context;
    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_dpi      = dpi;
    m_titleBar = titleBar;
    m_theme    = theme;

    hr = EnsureSwapChain();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostDestroyed
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnHostDestroyed()
{
    ReleaseRenderTargets();
    m_swapChain.Reset();
    m_hwnd     = nullptr;
    m_titleBar = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostResize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::OnHostResize (int widthPx, int heightPx, UINT dpi)
{
    HRESULT  hr = S_OK;


    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_dpi      = dpi;

    BAIL_OUT_IF (m_swapChain == nullptr, S_OK);

    ReleaseRenderTargets();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     (UINT) m_widthPx,
                                     (UINT) m_heightPx,
                                     DXGI_FORMAT_B8G8R8A8_UNORM,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetChromeTheme
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::SetChromeTheme (TitleBar * titleBar, const ChromeTheme * theme)
{
    m_titleBar = titleBar;
    m_theme    = theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredClientSize
//
//  T044 lands a fixed default size; T046 will replace this with a
//  layout-driven calculation once control families exist.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DiskIIDebugPanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};


    size.cx = MulDiv (s_kPreferredWidthDip,  (int) dpi, 96);
    size.cy = MulDiv (s_kPreferredHeightDip, (int) dpi, 96);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown / OnLButtonUp / OnMouseMove
//
//  No-ops in T044; control families add real input routing later.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnLButtonDown (int x, int y) { (void) x; (void) y; }
void DiskIIDebugPanel::OnLButtonUp   (int x, int y) { (void) x; (void) y; }
void DiskIIDebugPanel::OnMouseMove   (int x, int y) { (void) x; (void) y; }





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Returns false to let the chrome shell handle the key (e.g. Esc =
//  Cancel). T046+ will intercept tabs / arrows once focusable controls
//  exist.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugPanel::OnKey (WPARAM vk)
{
    (void) vk;
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
//  The panel is non-modal and has no commit semantics, so Enter is a
//  no-op (matches legacy DiskIIDebugDialog behaviour).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Accept()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Esc / WM_CLOSE / titlebar close all hide the panel rather than
//  destroying it, matching the legacy dialog: re-opening keeps the
//  filter state and the event ring populated.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Cancel()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsContentActive
//
//  Always true while the host is up -- Cancel hides the window without
//  asking the shell to destroy it, so the chrome shell must not tear
//  down the HWND on Cancel.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugPanel::IsContentActive() const
{
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSwapChain
//
//  Creates a flip-sequential swap chain on the host HWND if one is
//  not already attached. Uses straight-HWND swap chain (no DComp) --
//  the panel has no transparency / overlap requirements that would
//  need composition.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::EnsureSwapChain()
{
    HRESULT                hr           = S_OK;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  desc         = {};



    BAIL_OUT_IF (m_swapChain != nullptr, S_OK);

    CBRA (m_device);
    CBRA (m_hwnd);

    hr = m_device->QueryInterface (IID_PPV_ARGS (&dxgiDevice));
    CHRA (hr);

    hr = dxgiDevice->GetAdapter (&dxgiAdapter);
    CHRA (hr);

    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory));
    CHRA (hr);

    desc.Width            = (UINT) m_widthPx;
    desc.Height           = (UINT) m_heightPx;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo           = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = s_kSwapBufferCount;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags            = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device,
                                              m_hwnd,
                                              &desc,
                                              nullptr,
                                              nullptr,
                                              &m_swapChain);
    CHRA (hr);

    hr = dxgiFactory->MakeWindowAssociation (m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferRtv
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugPanel::CreateBackBufferRtv()
{
    HRESULT                       hr         = S_OK;
    ComPtr<ID3D11Texture2D>       backBuffer;



    CBRA (m_swapChain);
    CBRA (m_device);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseRenderTargets
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::ReleaseRenderTargets()
{
    if (m_context != nullptr)
    {
        ID3D11RenderTargetView *  nullRtv = nullptr;
        m_context->OMSetRenderTargets (1, &nullRtv, nullptr);
    }
    m_rtv.Reset();
}
