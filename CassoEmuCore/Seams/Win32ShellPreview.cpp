#include "Pch.h"

#include "Seams/Win32ShellPreview.h"
#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::~Win32ShellPreview
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellPreview::~Win32ShellPreview()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Start
//
//  The thread's message queue exists before Start returns, so the first load
//  posted to it is in that queue.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::Start (HWND owner, UINT message)
{
    HRESULT                 hr      = S_OK;
    WNDCLASSEXW             wc      = { sizeof (wc) };
    HINSTANCE               module  = GetModuleHandleW (nullptr);
    std::promise<DWORD>     started;
    std::future<DWORD>      ready   = started.get_future();
    ATOM                    atom    = 0;
    BOOL                    ok      = FALSE;



    CBRAEx (owner != nullptr, E_INVALIDARG);
    BAIL_OUT_IF (m_host != nullptr, S_OK);

    m_owner   = owner;
    m_message = message;

    if (!GetClassInfoExW (module, s_kpszHostClass, &wc))
    {
        wc               = { sizeof (wc) };
        wc.lpfnWndProc   = HostProc;
        wc.hInstance     = module;
        wc.lpszClassName = s_kpszHostClass;
        wc.hCursor       = LoadCursorW (nullptr, IDC_ARROW);

        atom = RegisterClassExW (&wc);
        CWR (atom != 0);
    }

    //  Layered at full opacity: the owner window is composited with per-pixel
    //  alpha, where a handler's GDI drawing has alpha 0 and is transparent to
    //  the desktop. A layered window is drawn at its own alpha.
    m_host = CreateWindowExW (WS_EX_LAYERED, s_kpszHostClass, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, 0, 0, owner, nullptr, module, nullptr);
    CWR (m_host != nullptr);

    ok = SetLayeredWindowAttributes (m_host, 0, 255, LWA_ALPHA);
    CWR (ok);

    m_thread = std::thread ([this, &started]()
    {
        MSG  peek = {};

        //  A thread's message queue is made by its first message call.
        PeekMessageW (&peek, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        started.set_value (GetCurrentThreadId());
        Run();
    });

    m_threadId = ready.get();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Stop
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::Stop()
{
    if (m_thread.joinable())
    {
        PostThreadMessageW (m_threadId, WM_QUIT, 0, 0);
        m_thread.join();
    }

    if (m_host != nullptr)
    {
        DestroyWindow (m_host);
        m_host = nullptr;
    }

    m_threadId = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::HasPreview
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellPreview::HasPreview (const std::wstring & path)
{
    CLSID    id = {};
    HRESULT  hr = GetHandlerId (path, id);



    return SUCCEEDED (hr) || IsPictureOrVideo (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::GetHandlerId
//
//  The class registered as the file type's preview handler, as Explorer
//  finds it: under the type's shellex key, by the handler interface's id.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::GetHandlerId (const std::wstring & path, CLSID & outId)
{
    HRESULT       hr        = S_OK;
    wchar_t       text[64]  = {};
    DWORD         length    = (DWORD) std::size (text);
    std::wstring  extension = fs::path (path).extension().wstring();
    bool          known     = !extension.empty();



    outId = CLSID_NULL;

    CBREx (known, E_INVALIDARG);

    hr = AssocQueryStringW (ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, extension.c_str(), s_kpszPreviewHandlerId, text, &length);
    CHR (hr);

    hr = CLSIDFromString (text, &outId);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::IsPictureOrVideo
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellPreview::IsPictureOrVideo (const std::wstring & path)
{
    PERCEIVED     type      = PERCEIVED_TYPE_UNSPECIFIED;
    PERCEIVEDFLAG flags     = 0;
    std::wstring  extension = fs::path (path).extension().wstring();
    HRESULT       hr        = extension.empty() ? E_INVALIDARG : AssocGetPerceivedType (extension.c_str(), &type, &flags, nullptr);



    return SUCCEEDED (hr) && (type == PERCEIVED_TYPE_IMAGE || type == PERCEIVED_TYPE_VIDEO);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Load
//
//  The host moves to the rectangle now and becomes visible only when a
//  handler draws in it, so a picture or a failure has no empty window over
//  the pane.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Win32ShellPreview::Load (const std::wstring & path, const RECT & rectPx, COLORREF background, COLORREF text)
{
    Job       * job     = new Job;
    uint64_t    request = ++m_request;



    job->request    = request;
    job->path       = path;
    job->rect       = rectPx;
    job->background = background;
    job->text       = text;

    if (m_host != nullptr)
    {
        SetWindowLongPtrW (m_host, GWLP_USERDATA, (LONG_PTR) background);
        ShowWindow (m_host, SW_HIDE);
        SetWindowPos (m_host, HWND_TOP, rectPx.left, rectPx.top, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top, SWP_NOACTIVATE);
    }

    if (m_threadId == 0 || !PostThreadMessageW (m_threadId, s_kLoadMessage, 0, (LPARAM) job))
    {
        delete job;
    }

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::ShowHost
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::ShowHost (bool show)
{
    if (m_host != nullptr)
    {
        ShowWindow (m_host, show ? SW_SHOWNA : SW_HIDE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::SetRect
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::SetRect (const RECT & rectPx)
{
    RECT  * rect = new RECT { 0, 0, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top };



    if (m_host != nullptr)
    {
        SetWindowPos (m_host, HWND_TOP, rectPx.left, rectPx.top, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top, SWP_NOACTIVATE);
    }

    if (m_threadId == 0 || !PostThreadMessageW (m_threadId, s_kRectMessage, 0, (LPARAM) rect))
    {
        delete rect;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Unload
//
//  A new number makes any load on its way stale.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::Unload()
{
    ++m_request;

    ShowHost (false);

    if (m_threadId != 0)
    {
        PostThreadMessageW (m_threadId, s_kUnloadMessage, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::TakeResult
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellPreview::TakeResult (Result & outResult)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    bool                         latest = m_result.has_value() && m_result->request == m_request;



    if (latest)
    {
        outResult = std::move (*m_result);
    }

    m_result.reset();

    return latest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::HostProc
//
//  The host's background is the pane's, under a handler that draws less than
//  all of it.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ShellPreview::HostProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT    client = {};
    HBRUSH  brush  = nullptr;



    if (msg == WM_ERASEBKGND)
    {
        brush = CreateSolidBrush ((COLORREF) GetWindowLongPtrW (hwnd, GWLP_USERDATA));
        GetClientRect (hwnd, &client);
        FillRect ((HDC) wParam, &client, brush);
        DeleteObject (brush);
        return 1;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Run
//
//  A single-threaded apartment with a message loop, for a handler's calls
//  back into it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::Run()
{
    HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
    HRESULT  hrName = ThreadName::SetForCurrentThread (L"Shell preview");
    MSG      msg    = {};



    IGNORE_RETURN_VALUE (hrName, S_OK);

    while (GetMessageW (&msg, nullptr, 0, 0) > 0)
    {
        if (msg.hwnd == nullptr && msg.message == s_kLoadMessage)
        {
            std::unique_ptr<Job>  job ((Job *) msg.lParam);

            RunLoad (*job);
        }
        else if (msg.hwnd == nullptr && msg.message == s_kRectMessage)
        {
            std::unique_ptr<RECT>  rect ((RECT *) msg.lParam);

            if (m_handler)
            {
                m_handler->SetRect (rect.get());
            }
        }
        else if (msg.hwnd == nullptr && msg.message == s_kUnloadMessage)
        {
            RunUnload();
        }
        else
        {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }
    }

    RunUnload();

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::RunLoad
//
//  A handler first, as Explorer prefers one; then a thumbnail for a picture
//  or a video; otherwise a failure, after which the window draws the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::RunLoad (const Job & job)
{
    HRESULT  hr        = S_OK;
    HRESULT  hrPicture = S_OK;
    Result   result;



    RunUnload();

    result.request = job.request;

    BAIL_OUT_IF (job.request != m_request, S_OK);

    hr = ShowHandler (job);

    if (SUCCEEDED (hr))
    {
        result.outcome = Outcome::Shown;
    }
    else if (IsPictureOrVideo (job.path))
    {
        hrPicture      = MakePicture (job, result);
        result.outcome = SUCCEEDED (hrPicture) ? Outcome::Picture : Outcome::Failed;
    }

    //  The selection moved on while this loaded.
    if (job.request != m_request)
    {
        RunUnload();
    }

    Publish (std::move (result));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::RunUnload
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::RunUnload()
{
    if (m_handler)
    {
        m_handler->Unload();
        m_handler.Reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::ShowHandler
//
//  Out of process first, in the shell's preview host as Explorer runs a
//  handler registered for it, then in this process for one that is not.
//  The handler is given the file by stream when it takes one, else as an
//  item, else by path, in Explorer's order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::ShowHandler (const Job & job)
{
    HRESULT                          hr       = S_OK;
    HRESULT                          hrColor  = S_OK;
    HRESULT                          hrStream = S_OK;
    HRESULT                          hrItem   = S_OK;
    HRESULT                          hrVisual = S_OK;
    CLSID                            id       = {};
    ComPtr<IPreviewHandler>          handler;
    ComPtr<IInitializeWithStream>    byStream;
    ComPtr<IInitializeWithItem>      byItem;
    ComPtr<IInitializeWithFile>      byFile;
    ComPtr<IPreviewHandlerVisuals>   visuals;
    ComPtr<IStream>                  stream;
    ComPtr<IShellItem>               item;
    RECT                             client   = { 0, 0, job.rect.right - job.rect.left, job.rect.bottom - job.rect.top };



    hr = GetHandlerId (job.path, id);
    CHR (hr);

    hr = CoCreateInstance (id, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&handler));

    if (FAILED (hr))
    {
        hr = CoCreateInstance (id, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&handler));
    }

    CHR (hr);

    hrStream = handler.As (&byStream);
    hrItem   = handler.As (&byItem);

    if (SUCCEEDED (hrStream))
    {
        hr = SHCreateStreamOnFileEx (job.path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
        CHR (hr);

        hr = byStream->Initialize (stream.Get(), STGM_READ);
    }
    else if (SUCCEEDED (hrItem))
    {
        hr = SHCreateItemFromParsingName (job.path.c_str(), nullptr, IID_PPV_ARGS (&item));
        CHR (hr);

        hr = byItem->Initialize (item.Get(), STGM_READ);
    }
    else
    {
        hr = handler.As (&byFile);
        CHR (hr);

        hr = byFile->Initialize (job.path.c_str(), STGM_READ);
    }

    CHR (hr);

    //  A handler that takes colors draws on the pane's.
    hrVisual = handler.As (&visuals);

    if (SUCCEEDED (hrVisual))
    {
        hrColor = visuals->SetBackgroundColor (job.background);
        IGNORE_RETURN_VALUE (hrColor, S_OK);

        hrColor = visuals->SetTextColor (job.text);
        IGNORE_RETURN_VALUE (hrColor, S_OK);
    }

    hr = handler->SetWindow (m_host, &client);
    CHR (hr);

    hr = handler->DoPreview();
    CHR (hr);

    m_handler = handler;

Error:
    if (FAILED (hr) && handler)
    {
        handler->Unload();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::MakePicture
//
//  The shell's thumbnail at the pane's size, fitted, as BGRA with no
//  transparency: a picture with an alpha channel is laid over the pane's
//  color, and one with none at all is opaque.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::MakePicture (const Job & job, Result & outResult)
{
    HRESULT                         hr       = S_OK;
    ComPtr<IShellItemImageFactory>  factory;
    HBITMAP                         bitmap   = nullptr;
    BITMAP                          info     = {};
    BITMAPINFO                      header   = {};
    HDC                             screen   = nullptr;
    SIZE                            size     = { (std::max) (1L, job.rect.right - job.rect.left), (std::max) (1L, job.rect.bottom - job.rect.top) };
    bool                            hasAlpha = false;
    uint32_t                        back     = 0;
    int                             written  = 0;



    hr = SHCreateItemFromParsingName (job.path.c_str(), nullptr, IID_PPV_ARGS (&factory));
    CHR (hr);

    hr = factory->GetImage (size, SIIGBF_THUMBNAILONLY, &bitmap);
    CHR (hr);

    written = GetObjectW (bitmap, sizeof (info), &info);
    CBR (written != 0);
    CBR (info.bmWidth > 0 && info.bmHeight > 0);

    header.bmiHeader.biSize        = sizeof (header.bmiHeader);
    header.bmiHeader.biWidth       = info.bmWidth;
    header.bmiHeader.biHeight      = -info.bmHeight;
    header.bmiHeader.biPlanes      = 1;
    header.bmiHeader.biBitCount    = 32;
    header.bmiHeader.biCompression = BI_RGB;

    outResult.width  = info.bmWidth;
    outResult.height = info.bmHeight;
    outResult.bgra.assign ((size_t) info.bmWidth * (size_t) info.bmHeight, 0);

    screen = GetDC (nullptr);
    written = GetDIBits (screen, bitmap, 0, (UINT) info.bmHeight, outResult.bgra.data(), &header, DIB_RGB_COLORS);
    CBR (written != 0);

    hasAlpha = std::any_of (outResult.bgra.begin(), outResult.bgra.end(), [] (uint32_t pixel) { return (pixel >> 24) != 0; });
    back     = 0xFF000000 | ((uint32_t) GetRValue (job.background) << 16) | ((uint32_t) GetGValue (job.background) << 8) | GetBValue (job.background);

    for (uint32_t & pixel : outResult.bgra)
    {
        uint32_t  alpha = hasAlpha ? (pixel >> 24) : 0xFF;
        uint32_t  mixed = 0xFF000000;

        for (int shift : { 0, 8, 16 })
        {
            uint32_t  channel = ((pixel >> shift) & 0xFF) + ((((back >> shift) & 0xFF) * (0xFF - alpha)) / 0xFF);

            mixed |= ((std::min) (channel, 0xFFu)) << shift;
        }

        pixel = mixed;
    }

Error:
    if (screen != nullptr)
    {
        ReleaseDC (nullptr, screen);
    }

    if (bitmap != nullptr)
    {
        DeleteObject (bitmap);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::Publish
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::Publish (Result result)
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_result = std::move (result);
    }

    PostMessageW (m_owner, m_message, 0, 0);
}
