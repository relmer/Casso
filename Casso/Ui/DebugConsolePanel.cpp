#include "Pch.h"

#include "DebugConsolePanel.h"

#include "Chrome/ChromeTheme.h"
#include "Chrome/TitleBar.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName     = L"Casso.DebugConsole.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle   = L"Debug Console";
    constexpr LPCWSTR  s_kpszMonoFamily    = L"Consolas";

    constexpr int      s_kPreferredWidthDip  = 720;
    constexpr int      s_kPreferredHeightDip = 480;


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
//  DebugConsolePanel
//
////////////////////////////////////////////////////////////////////////////////

DebugConsolePanel::DebugConsolePanel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DebugConsolePanel
//
////////////////////////////////////////////////////////////////////////////////

DebugConsolePanel::~DebugConsolePanel()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Registers the chrome-window class for the debug console and brings
//  the host HWND into existence. Idempotent -- a second call while the
//  window is already open returns S_OK without re-creating anything.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsolePanel::Create (
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
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::Show()
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

void DebugConsolePanel::Hide()
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

void DebugConsolePanel::Destroy()
{
    m_window.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
//  Public render entry point invoked by the shell's frame loop. The
//  chrome shell composites our content underneath its title bar.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsolePanel::RenderFrame()
{
    return m_window.Render();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::SetTheme (const ChromeTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Log
//
//  Appends one line to the buffer. Safe to call from any thread (the
//  same DEBUGMSG forwarder may run off the audio or render thread).
//  Keeps the buffer capped at s_kMaxLines by dropping the oldest line
//  once the cap is hit -- the head is the entry the user is least
//  likely to still be scrolled to.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::Log (const std::wstring & message)
{
    std::lock_guard<std::mutex>  lock (m_bufferMutex);


    if (m_lines.size() >= s_kMaxLines)
    {
        m_lines.erase (m_lines.begin());
        if (m_scrollLine > 0)
        {
            m_scrollLine--;
        }
    }
    m_lines.push_back (message);

    // Auto-scroll to bottom when the user is already pinned to the end
    // (scroll line within one page of the tail). Otherwise leave the
    // viewport where the user left it.
    int  visible = LinesVisible();
    int  tailTop = std::max (0, (int) m_lines.size() - visible);
    if (m_scrollLine >= tailTop - 1)
    {
        m_scrollLine = tailTop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogConfig
//
//  Narrow-string overload that widens via UTF-8 -> UTF-16 conversion
//  and forwards to Log. Mirrors the legacy DebugConsole contract used
//  by config-summary call sites that already carry std::string.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::LogConfig (const std::string & summary)
{
    int            needed = 0;
    std::wstring   wide;


    if (summary.empty())
    {
        return;
    }

    needed = MultiByteToWideChar (CP_UTF8, 0, summary.c_str(), (int) summary.size(), nullptr, 0);
    if (needed <= 0)
    {
        return;
    }

    wide.resize ((size_t) needed);
    MultiByteToWideChar (CP_UTF8, 0, summary.c_str(), (int) summary.size(), &wide[0], needed);

    Log (wide);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowClassName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DebugConsolePanel::GetWindowClassName() const
{
    return s_kpszClassName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DebugConsolePanel::GetWindowTitle() const
{
    return s_kpszWindowTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostCreated
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsolePanel::OnHostCreated (
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

    hr = m_painter.Initialize (device, context);
    CHRA (hr);

    hr = m_text.Initialize (device);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostDestroyed
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnHostDestroyed()
{
    m_text.UnbindBackBuffer();
    m_text.Shutdown();
    m_painter.Shutdown();
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

HRESULT DebugConsolePanel::OnHostResize (int widthPx, int heightPx, UINT dpi)
{
    HRESULT  hr = S_OK;


    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_dpi      = dpi;

    BAIL_OUT_IF (m_swapChain == nullptr, S_OK);

    m_text.UnbindBackBuffer();
    ReleaseRenderTargets();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     (UINT) m_widthPx,
                                     (UINT) m_heightPx,
                                     DXGI_FORMAT_B8G8R8A8_UNORM,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

    ClampScroll();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Paints the title bar plus the log buffer body. Body geometry is
//  derived from the host client area; the title bar already lives in
//  the top strip so we offset the text region below it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsolePanel::Render()
{
    HRESULT                       hr            = S_OK;
    float                         clearColor[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
    ComPtr<ID3D11Texture2D>       backBuffer;
    ComPtr<IDXGISurface>          surface;
    ChromeVisualState             visual        = {};
    D3D11_VIEWPORT                vp            = {};
    uint32_t                      textArgb      = 0xFFE6E2D8;
    float                         bodyTopPx     = 0.0f;
    float                         padPx         = (float) MulDiv (s_kPadDp, (int) m_dpi, 96);
    float                         lineHeightPx  = (float) LineHeightPx();
    int                           startLine     = 0;
    int                           visibleLines  = 0;
    int                           total         = 0;
    int                           i             = 0;


    BAIL_OUT_IF (m_swapChain == nullptr || m_rtv == nullptr || m_context == nullptr, S_OK);

    if (!m_text.IsTargetBound())
    {
        hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
        CHRA (hr);

        hr = backBuffer.As (&surface);
        CHRA (hr);

        hr = m_text.BindBackBuffer (surface.Get(), m_dpi, m_dpi);
        CHRA (hr);
    }

    if (m_theme != nullptr)
    {
        ArgbToFloat4 (m_theme->titleBarBottomArgb, clearColor);
        clearColor[3] = 1.0f;
        textArgb      = m_theme->dropdownItemTextArgb;
    }

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float) m_widthPx;
    vp.Height   = (float) m_heightPx;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &vp);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDraw();
    CHRA (hr);

    visual.dpi = m_dpi;
    if (m_titleBar != nullptr && m_theme != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, visual, *m_theme);
    }

    bodyTopPx = (float) (m_titleBar != nullptr ? m_titleBar->GetTitleHeight() : 0);

    {
        std::lock_guard<std::mutex>  lock (m_bufferMutex);

        total        = (int) m_lines.size();
        visibleLines = LinesVisible();
        ClampScroll();
        startLine    = m_scrollLine;

        for (i = 0; i < visibleLines && (startLine + i) < total; i++)
        {
            const std::wstring & line = m_lines[(size_t) (startLine + i)];
            float                yPx  = bodyTopPx + padPx + (float) i * lineHeightPx;

            hr = m_text.DrawString (line.c_str(),
                                    padPx,
                                    yPx,
                                    (float) m_widthPx - 2.0f * padPx,
                                    lineHeightPx,
                                    textArgb,
                                    (float) s_kFontDip * (float) m_dpi / 96.0f,
                                    s_kpszMonoFamily);
            CHRA (hr);
        }
    }

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetChromeTheme
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::SetChromeTheme (TitleBar * titleBar, const ChromeTheme * theme)
{
    m_titleBar = titleBar;
    m_theme    = theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredClientSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE DebugConsolePanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};


    size.cx = MulDiv (s_kPreferredWidthDip,  (int) dpi, 96);
    size.cy = MulDiv (s_kPreferredHeightDip, (int) dpi, 96);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnLButtonDown (int x, int y)
{
    (void) x;
    (void) y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnLButtonUp (int x, int y)
{
    (void) x;
    (void) y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnMouseMove (int x, int y)
{
    (void) x;
    (void) y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
//  WHEEL_DELTA per notch -> 3 lines per notch, matching the default
//  Win32 EDIT control behaviour the legacy console exposed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnMouseWheel (int x, int y, int delta)
{
    int  notches    = 0;
    int  lineDelta  = 0;


    (void) x;
    (void) y;

    notches   = delta / WHEEL_DELTA;
    lineDelta = -notches * 3;

    std::lock_guard<std::mutex>  lock (m_bufferMutex);
    m_scrollLine += lineDelta;
    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  PgUp / PgDn / Home / End / arrow scroll. Ctrl+C copies the entire
//  buffer to the clipboard (granular text selection is intentionally
//  deferred -- see plan T056-T058). Ctrl+A is a no-op for now since
//  "select all" carries no visible state without selection rendering.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsolePanel::OnKey (WPARAM vk)
{
    bool  handled  = false;
    bool  ctrlDown = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    int   page     = std::max (1, LinesVisible() - 1);


    if (ctrlDown && (vk == 'C' || vk == 'c'))
    {
        CopyAllToClipboard();
        return true;
    }

    {
        std::lock_guard<std::mutex>  lock (m_bufferMutex);

        switch (vk)
        {
            case VK_UP:
                m_scrollLine -= 1;
                handled = true;
                break;

            case VK_DOWN:
                m_scrollLine += 1;
                handled = true;
                break;

            case VK_PRIOR:
                m_scrollLine -= page;
                handled = true;
                break;

            case VK_NEXT:
                m_scrollLine += page;
                handled = true;
                break;

            case VK_HOME:
                m_scrollLine = 0;
                handled = true;
                break;

            case VK_END:
                m_scrollLine = (int) m_lines.size();
                handled = true;
                break;

            default:
                break;
        }

        if (handled)
        {
            ClampScroll();
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
//  Enter is a no-op for the console -- there is nothing to commit.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::Accept()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Escape / WM_CLOSE just hide the panel (preserves the buffer for the
//  next Ctrl+D press), matching the legacy console's behaviour.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::Cancel()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSwapChain
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsolePanel::EnsureSwapChain()
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

HRESULT DebugConsolePanel::CreateBackBufferRtv()
{
    HRESULT                  hr         = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;


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

void DebugConsolePanel::ReleaseRenderTargets()
{
    if (m_context != nullptr)
    {
        ID3D11RenderTargetView *  nullRtv = nullptr;
        m_context->OMSetRenderTargets (1, &nullRtv, nullptr);
    }
    m_rtv.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampScroll
//
//  Pins m_scrollLine to [0, max(0, total - 1)]. Caller must hold
//  m_bufferMutex.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::ClampScroll()
{
    int  total   = (int) m_lines.size();
    int  visible = LinesVisible();
    int  maxTop  = std::max (0, total - visible);


    if (m_scrollLine < 0)
    {
        m_scrollLine = 0;
    }
    if (m_scrollLine > maxTop)
    {
        m_scrollLine = maxTop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinesVisible
//
////////////////////////////////////////////////////////////////////////////////

int DebugConsolePanel::LinesVisible() const
{
    int  titleBarHeightPx = MulDiv (32, (int) m_dpi, 96);
    int  bodyHeightPx     = std::max (0, m_heightPx - titleBarHeightPx - 2 * MulDiv (s_kPadDp, (int) m_dpi, 96));
    int  lineHeight       = LineHeightPx();


    if (lineHeight <= 0)
    {
        return 1;
    }
    return std::max (1, bodyHeightPx / lineHeight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DebugConsolePanel::LineHeightPx() const
{
    return MulDiv (s_kFontDip + 4, (int) m_dpi, 96);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyAllToClipboard
//
//  Joins every line with CRLF and shoves it into the Win32 clipboard
//  as CF_UNICODETEXT. Falls through silently on any clipboard failure
//  -- the user just hits Ctrl+C again if it doesn't take.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::CopyAllToClipboard()
{
    std::wstring  joined;
    size_t        totalChars = 0;
    HGLOBAL       hMem       = nullptr;
    wchar_t     * dst        = nullptr;


    {
        std::lock_guard<std::mutex>  lock (m_bufferMutex);

        for (const auto & line : m_lines)
        {
            totalChars += line.size() + 2;
        }
        joined.reserve (totalChars);
        for (const auto & line : m_lines)
        {
            joined.append (line);
            joined.append (L"\r\n");
        }
    }

    if (joined.empty())
    {
        return;
    }

    if (!OpenClipboard (m_hwnd))
    {
        return;
    }

    EmptyClipboard();

    hMem = GlobalAlloc (GMEM_MOVEABLE, (joined.size() + 1) * sizeof (wchar_t));
    if (hMem == nullptr)
    {
        CloseClipboard();
        return;
    }

    dst = (wchar_t *) GlobalLock (hMem);
    if (dst == nullptr)
    {
        GlobalFree (hMem);
        CloseClipboard();
        return;
    }

    memcpy (dst, joined.c_str(), (joined.size() + 1) * sizeof (wchar_t));
    GlobalUnlock (hMem);

    if (SetClipboardData (CF_UNICODETEXT, hMem) == nullptr)
    {
        GlobalFree (hMem);
    }

    CloseClipboard();
}
