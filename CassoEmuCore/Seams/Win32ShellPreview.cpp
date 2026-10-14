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
//  posted to it is in that queue. A helper at low integrity can post the
//  owner only the messages the owner lets through.
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

    for (UINT helperMessage : { Win32PreviewHelper::kReadyMessage, Win32PreviewHelper::kResultMessage })
    {
        ok = ChangeWindowMessageFilterEx (owner, helperMessage, MSGFLT_ALLOW, nullptr);
        CWR (ok);
    }

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
//  Post WM_CLOSE to each helper and terminate any that has not exited a
//  second later, before destroying the host, since their windows are inside
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::Stop()
{
    DWORD  waited = WAIT_OBJECT_0;



    if (m_thread.joinable())
    {
        PostThreadMessageW (m_threadId, WM_QUIT, 0, 0);
        m_thread.join();
    }

    for (Helper & helper : m_helpers)
    {
        if (helper.window != nullptr)
        {
            PostMessageW (helper.window, WM_CLOSE, 0, 0);
        }

        if (helper.process != nullptr)
        {
            waited = WaitForSingleObject (helper.process, s_kStopWaitMs);

            if (waited != WAIT_OBJECT_0)
            {
                TerminateProcess (helper.process, 0);
            }

            CloseHandle (helper.process);
        }

        helper = Helper();
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
    HRESULT  hr = Win32PreviewHelper::GetHandlerId (path, id);



    return SUCCEEDED (hr) || IsPictureOrVideo (path);
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
//  A file with a preview handler goes to the helper for the handler's
//  integrity level, started if it is not running; anything else, or a load
//  that cannot be sent to a helper, goes to the thumbnail thread. The host moves to
//  the rectangle now and becomes visible only when a handler draws in it, so
//  a picture or a failure has no empty window over the pane.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Win32ShellPreview::Load (const std::wstring & path, const RECT & rectPx, COLORREF background, COLORREF text)
{
    HRESULT  hr   = S_OK;
    CLSID    id   = {};
    int      slot = Win32PreviewHelper::kLowSlot;
    Job      job;



    job.request    = ++m_request;
    job.path       = path;
    job.rect       = rectPx;
    job.background = background;
    job.text       = text;

    m_current    = job;
    m_activeSlot = -1;
    m_awaiting   = false;

    if (m_host != nullptr)
    {
        SetWindowLongPtrW (m_host, GWLP_USERDATA, (LONG_PTR) background);
        ShowWindow (m_host, SW_HIDE);
        SetWindowPos (m_host, HWND_TOP, rectPx.left, rectPx.top, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top, SWP_NOACTIVATE);
    }

    HideHelpers();

    hr = Win32PreviewHelper::GetHandlerId (path, id);

    if (SUCCEEDED (hr))
    {
        slot = Win32PreviewHelper::GetSlot (id);
        hr   = EnsureHelper (slot);
    }

    if (SUCCEEDED (hr))
    {
        m_activeSlot = slot;
        m_awaiting   = m_helpers[(size_t) slot].window == nullptr;
        hr           = m_awaiting ? S_OK : SendLoad();
    }

    if (FAILED (hr))
    {
        m_activeSlot = -1;
        m_awaiting   = false;
        PostPictureJob (job);
    }

    return job.request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::EnsureHelper
//
//  A helper that has exited, from a handler's crash or otherwise, is
//  replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::EnsureHelper (int slot)
{
    HRESULT   hr      = S_OK;
    Helper  & helper  = m_helpers[(size_t) slot];
    DWORD     waited  = WAIT_TIMEOUT;



    if (helper.process != nullptr)
    {
        waited = WaitForSingleObject (helper.process, 0);
        BAIL_OUT_IF (waited == WAIT_TIMEOUT, S_OK);

        CloseHandle (helper.process);
        helper = Helper();
    }

    hr = Win32PreviewHelper::Launch (m_owner, slot, &helper.process);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::SendLoad
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellPreview::SendLoad()
{
    HRESULT                         hr     = S_OK;
    Win32PreviewHelper::LoadHeader  header;
    bool                            active = m_activeSlot >= 0 && m_helpers[(size_t) m_activeSlot].window != nullptr;



    CBREx (active, E_UNEXPECTED);

    header.request    = m_current.request;
    header.background = m_current.background;
    header.text       = m_current.text;

    hr = Win32PreviewHelper::SendLoad (m_helpers[(size_t) m_activeSlot].window, m_owner, header, m_current.path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::OnHelperMessage
//
//  A helper's window arrives once, and is taken into the host; a load that
//  waited for it is sent then. A success for the latest load makes the
//  helper's window visible; a failure falls back to a thumbnail, then to the
//  bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellPreview::OnHelperMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    HRESULT   hr     = S_OK;
    int       slot   = (int) wParam;
    Result    result;



    if (msg == Win32PreviewHelper::kReadyMessage)
    {
        if (slot >= 0 && slot < Win32PreviewHelper::kSlotCount && m_helpers[(size_t) slot].process != nullptr)
        {
            m_helpers[(size_t) slot].window = (HWND) lParam;
            AdoptWindow (slot);

            if (m_awaiting && m_activeSlot == slot)
            {
                m_awaiting = false;
                hr         = SendLoad();

                if (FAILED (hr))
                {
                    m_activeSlot = -1;
                    PostPictureJob (m_current);
                }
            }
        }

        return true;
    }

    if (msg != Win32PreviewHelper::kResultMessage)
    {
        return false;
    }

    if ((uint64_t) wParam != m_request || m_activeSlot < 0)
    {
        return true;
    }

    hr = (HRESULT) lParam;

    if (FAILED (hr))
    {
        m_activeSlot = -1;
        PostPictureJob (m_current);
        return true;
    }

    ShowWindow (m_helpers[(size_t) m_activeSlot].window, SW_SHOWNA);

    result.request = m_current.request;
    result.outcome = Outcome::Shown;
    Publish (std::move (result));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::AdoptWindow
//
//  The helper's popup becomes a child of the host, filling it, hidden until
//  a handler draws in it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::AdoptWindow (int slot)
{
    HWND      window = m_helpers[(size_t) slot].window;
    LONG_PTR  style  = 0;
    RECT      client = {};



    if (window == nullptr || m_host == nullptr)
    {
        return;
    }

    style = GetWindowLongPtrW (window, GWL_STYLE);
    style = (style & ~(LONG_PTR) (WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_VISIBLE)) | WS_CHILD | WS_CLIPCHILDREN;

    SetParent (window, m_host);
    SetWindowLongPtrW (window, GWL_STYLE, style);
    GetClientRect (m_host, &client);
    SetWindowPos (window, HWND_TOP, 0, 0, client.right, client.bottom, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::HideHelpers
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::HideHelpers()
{
    for (Helper & helper : m_helpers)
    {
        if (helper.window != nullptr)
        {
            ShowWindow (helper.window, SW_HIDE);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::PostPictureJob
//
//  A job the thread drops is finished here as a failure, so every load ends
//  in a result.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::PostPictureJob (const Job & job)
{
    Job     * copy   = new Job (job);
    Result    result;



    if (m_threadId != 0 && PostThreadMessageW (m_threadId, s_kLoadMessage, 0, (LPARAM) copy))
    {
        return;
    }

    delete copy;

    result.request = job.request;
    Publish (std::move (result));
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
//  The helper's window follows the host, and the helper passes its new size
//  to the handler.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::SetRect (const RECT & rectPx)
{
    int  width  = rectPx.right - rectPx.left;
    int  height = rectPx.bottom - rectPx.top;



    if (m_host != nullptr)
    {
        SetWindowPos (m_host, HWND_TOP, rectPx.left, rectPx.top, width, height, SWP_NOACTIVATE);
    }

    for (Helper & helper : m_helpers)
    {
        if (helper.window != nullptr)
        {
            SetWindowPos (helper.window, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
        }
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
    HideHelpers();

    if (m_activeSlot >= 0 && m_helpers[(size_t) m_activeSlot].window != nullptr)
    {
        Win32PreviewHelper::SendUnload (m_helpers[(size_t) m_activeSlot].window, m_owner);
    }

    m_activeSlot = -1;
    m_awaiting   = false;
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
//  The thumbnail thread's loop, in a single-threaded apartment.
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
        else
        {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview::RunLoad
//
//  A thumbnail for a picture or a video; otherwise a failure, after which the
//  window draws the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellPreview::RunLoad (const Job & job)
{
    HRESULT  hr        = S_OK;
    HRESULT  hrPicture = S_OK;
    Result   result;



    result.request = job.request;

    BAIL_OUT_IF (job.request != m_request, S_OK);

    if (IsPictureOrVideo (job.path))
    {
        hrPicture      = MakePicture (job, result);
        result.outcome = SUCCEEDED (hrPicture) ? Outcome::Picture : Outcome::Failed;
    }

    Publish (std::move (result));

Error:
    return;
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
