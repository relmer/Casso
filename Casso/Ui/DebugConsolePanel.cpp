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


    m_widthPx          = std::max (1, widthPx);
    m_heightPx         = std::max (1, heightPx);
    m_dpi              = dpi;
    m_charMetricsReady = false;

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

        EnsureCharMetrics();

        // Selection highlight is painted underneath the text so the
        // glyphs stay readable inside the highlighted span. The rect
        // colour comes from the theme's nav-hover so it tracks
        // Skeuomorphic / DarkModern / RetroTerminal automatically.
        if (HasSelection() && m_theme != nullptr)
        {
            Pos       lo         = {};
            Pos       hi         = {};
            uint32_t  selArgb    = m_theme->navHoverArgb;
            float     cellW      = (m_charWidthPx > 0.0f) ? m_charWidthPx : 1.0f;
            float     bodyRight  = (float) m_widthPx - padPx;

            OrderedSelection (lo, hi);

            for (i = 0; i < visibleLines && (startLine + i) < total; i++)
            {
                int  lineIdx = startLine + i;

                if (lineIdx < lo.line || lineIdx > hi.line)
                {
                    continue;
                }

                int    lineLen = (int) m_lines[(size_t) lineIdx].size();
                int    startCol = (lineIdx == lo.line) ? std::min (lo.column, lineLen) : 0;
                int    endCol   = (lineIdx == hi.line) ? std::min (hi.column, lineLen) : lineLen;
                float  yPx      = bodyTopPx + padPx + (float) i * lineHeightPx;
                float  xPx      = padPx + (float) startCol * cellW;
                float  wPx      = (lineIdx == hi.line)
                                  ? ((float) endCol * cellW + padPx - xPx)
                                  : (bodyRight - xPx);

                if (wPx < 1.0f)
                {
                    wPx = (lineIdx == hi.line && lineIdx == lo.line) ? 0.0f : cellW;
                }
                if (wPx > 0.0f)
                {
                    m_painter.FillRect (xPx, yPx, wPx, lineHeightPx, selArgb);
                }
            }
        }

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
//  Starts a click-drag text selection. Sets capture so we keep
//  receiving WM_MOUSEMOVE / WM_LBUTTONUP even when the cursor leaves
//  the client area, then anchors both ends of the selection at the
//  hit-test column under the cursor.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnLButtonDown (int x, int y)
{
    Pos  p = HitTestChar (x, y);


    if (m_hwnd != nullptr)
    {
        SetCapture (m_hwnd);
    }

    std::lock_guard<std::mutex>  lock (m_bufferMutex);

    m_selAnchor = p;
    m_selCaret  = p;
    m_selecting = true;
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


    if (GetCapture() == m_hwnd && m_hwnd != nullptr)
    {
        ReleaseCapture();
    }

    std::lock_guard<std::mutex>  lock (m_bufferMutex);
    m_selecting = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
//  Extends the live selection while the left button is held. Cursor
//  positions outside the body region auto-scroll the viewport by one
//  line per event so the user can drag-select past the visible window.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OnMouseMove (int x, int y)
{
    Pos  p             = {};
    int  bodyTop       = 0;
    int  bodyBottom    = 0;


    if (!m_selecting)
    {
        return;
    }

    p          = HitTestChar (x, y);
    bodyTop    = BodyTopPx() + MulDiv (s_kPadDp, (int) m_dpi, 96);
    bodyBottom = m_heightPx  - MulDiv (s_kPadDp, (int) m_dpi, 96);

    std::lock_guard<std::mutex>  lock (m_bufferMutex);

    if (y < bodyTop)
    {
        m_scrollLine -= 1;
        ClampScroll();
    }
    else if (y > bodyBottom)
    {
        m_scrollLine += 1;
        ClampScroll();
    }

    m_selCaret = p;
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
//  PgUp / PgDn scroll the viewport without moving the caret. Arrows /
//  Home / End move the caret (Shift extends the active selection,
//  unmodified collapses it; Ctrl jumps to buffer extremes for
//  Home / End). Ctrl+A selects the whole buffer; Ctrl+C copies the
//  current selection to the clipboard as CF_UNICODETEXT, or is a
//  no-op when the selection is empty.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsolePanel::OnKey (WPARAM vk)
{
    bool  handled  = false;
    bool  ctrlDown = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    bool  shiftDn  = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    int   page     = std::max (1, LinesVisible() - 1);


    if (ctrlDown && (vk == 'C' || vk == 'c'))
    {
        CopySelectionToClipboard();
        return true;
    }

    if (ctrlDown && (vk == 'A' || vk == 'a'))
    {
        SelectAll();
        return true;
    }

    {
        std::lock_guard<std::mutex>  lock (m_bufferMutex);
        int                          totalLines = (int) m_lines.size();

        switch (vk)
        {
            case VK_PRIOR:
                m_scrollLine -= page;
                handled = true;
                break;

            case VK_NEXT:
                m_scrollLine += page;
                handled = true;
                break;

            case VK_UP:
                if (m_selCaret.line > 0)
                {
                    m_selCaret.line  -= 1;
                    m_selCaret.column = ClampColumn (m_selCaret.line, m_selCaret.column);
                }
                handled = true;
                break;

            case VK_DOWN:
                if (m_selCaret.line + 1 < totalLines)
                {
                    m_selCaret.line  += 1;
                    m_selCaret.column = ClampColumn (m_selCaret.line, m_selCaret.column);
                }
                handled = true;
                break;

            case VK_LEFT:
                if (m_selCaret.column > 0)
                {
                    m_selCaret.column -= 1;
                }
                else if (m_selCaret.line > 0)
                {
                    m_selCaret.line  -= 1;
                    m_selCaret.column = ClampColumn (m_selCaret.line, INT_MAX);
                }
                handled = true;
                break;

            case VK_RIGHT:
                if (m_selCaret.column < ClampColumn (m_selCaret.line, INT_MAX))
                {
                    m_selCaret.column += 1;
                }
                else if (m_selCaret.line + 1 < totalLines)
                {
                    m_selCaret.line  += 1;
                    m_selCaret.column = 0;
                }
                handled = true;
                break;

            case VK_HOME:
                if (ctrlDown)
                {
                    m_selCaret.line   = 0;
                    m_selCaret.column = 0;
                    m_scrollLine      = 0;
                }
                else
                {
                    m_selCaret.column = 0;
                }
                handled = true;
                break;

            case VK_END:
                if (ctrlDown)
                {
                    m_selCaret.line   = std::max (0, totalLines - 1);
                    m_selCaret.column = ClampColumn (m_selCaret.line, INT_MAX);
                    m_scrollLine      = totalLines;
                }
                else
                {
                    m_selCaret.column = ClampColumn (m_selCaret.line, INT_MAX);
                }
                handled = true;
                break;

            default:
                break;
        }

        if (handled)
        {
            bool  isCaretMove = (vk == VK_UP   || vk == VK_DOWN ||
                                 vk == VK_LEFT || vk == VK_RIGHT ||
                                 vk == VK_HOME || vk == VK_END);

            if (isCaretMove && !shiftDn)
            {
                m_selAnchor = m_selCaret;
            }

            ClampScroll();

            // Keep caret visible on caret-move keys.
            if (isCaretMove)
            {
                int  visible = LinesVisible();
                if (m_selCaret.line < m_scrollLine)
                {
                    m_scrollLine = m_selCaret.line;
                }
                else if (m_selCaret.line >= m_scrollLine + visible)
                {
                    m_scrollLine = m_selCaret.line - visible + 1;
                }
                ClampScroll();
            }
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
//  CopySelectionToClipboard
//
//  Joins the currently selected text (CR/LF between lines) and shoves
//  it into the Win32 clipboard as CF_UNICODETEXT. Empty selection is
//  a no-op per the spec edge case; failures fall through silently so
//  the user can simply retry.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::CopySelectionToClipboard()
{
    std::wstring  joined;
    HGLOBAL       hMem       = nullptr;
    wchar_t     * dst        = nullptr;
    Pos           lo         = {};
    Pos           hi         = {};


    {
        std::lock_guard<std::mutex>  lock (m_bufferMutex);

        if (!HasSelection())
        {
            return;
        }

        OrderedSelection (lo, hi);

        if (lo.line == hi.line)
        {
            if (lo.line >= 0 && lo.line < (int) m_lines.size())
            {
                const std::wstring & line = m_lines[(size_t) lo.line];
                int  startCol = std::max (0, std::min (lo.column, (int) line.size()));
                int  endCol   = std::max (0, std::min (hi.column, (int) line.size()));
                joined.assign (line, (size_t) startCol, (size_t) (endCol - startCol));
            }
        }
        else
        {
            int  lastLine = std::min (hi.line, (int) m_lines.size() - 1);

            for (int li = lo.line; li <= lastLine; li++)
            {
                if (li < 0 || li >= (int) m_lines.size())
                {
                    continue;
                }
                const std::wstring & line = m_lines[(size_t) li];

                if (li == lo.line)
                {
                    int  startCol = std::max (0, std::min (lo.column, (int) line.size()));
                    joined.append (line, (size_t) startCol, std::wstring::npos);
                }
                else if (li == hi.line)
                {
                    int  endCol = std::max (0, std::min (hi.column, (int) line.size()));
                    joined.append (line, 0, (size_t) endCol);
                }
                else
                {
                    joined.append (line);
                }

                if (li != hi.line)
                {
                    joined.append (L"\r\n");
                }
            }
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





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureCharMetrics
//
//  Lazily measures the monospace cell width by querying DirectWrite
//  for a single 'M' at the current font size + DPI. Cached until
//  Reset (e.g. via DPI change in OnHostResize). Cheap to call every
//  frame because the bool gate short-circuits after the first hit.
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::EnsureCharMetrics()
{
    HRESULT  hr      = S_OK;
    float    width   = 0.0f;
    float    height  = 0.0f;
    float    fontPx  = (float) s_kFontDip * (float) m_dpi / 96.0f;


    if (m_charMetricsReady)
    {
        return;
    }
    if (!m_text.IsTargetBound())
    {
        return;
    }

    hr = m_text.MeasureString (L"M", fontPx, s_kpszMonoFamily, width, height);
    if (FAILED (hr) || width <= 0.0f)
    {
        m_charWidthPx = fontPx * 0.6f;
    }
    else
    {
        m_charWidthPx = width;
    }
    m_charMetricsReady = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestChar
//
//  Maps a body-relative pixel point to a {line, column} buffer
//  position. Returns a clamped position even for points above /
//  below / outside the body so click-drag selection past the edges
//  still makes sense.
//
////////////////////////////////////////////////////////////////////////////////

DebugConsolePanel::Pos DebugConsolePanel::HitTestChar (int xPx, int yPx) const
{
    Pos    p          = {};
    int    padPx      = MulDiv (s_kPadDp, (int) m_dpi, 96);
    int    bodyTop    = BodyTopPx() + padPx;
    int    lineH      = LineHeightPx();
    float  cellW      = (m_charWidthPx > 0.0f) ? m_charWidthPx : 1.0f;
    int    rowFromTop = 0;


    if (lineH <= 0)
    {
        return p;
    }

    rowFromTop = (yPx - bodyTop) / lineH;
    if (yPx < bodyTop) { rowFromTop = -1; }

    p.line = m_scrollLine + rowFromTop;
    if (p.line < 0) { p.line = 0; }
    if (p.line >= (int) m_lines.size())
    {
        p.line = std::max (0, (int) m_lines.size() - 1);
    }

    if (xPx <= padPx)
    {
        p.column = 0;
    }
    else
    {
        // Round to nearest character cell so the caret snaps to the
        // gap closest to the cursor rather than always the left edge.
        p.column = (int) (((float) (xPx - padPx) + cellW * 0.5f) / cellW);
    }
    p.column = ClampColumn (p.line, p.column);
    return p;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampColumn
//
////////////////////////////////////////////////////////////////////////////////

int DebugConsolePanel::ClampColumn (int line, int col) const
{
    int  maxCol = 0;


    if (line < 0 || line >= (int) m_lines.size())
    {
        return 0;
    }

    maxCol = (int) m_lines[(size_t) line].size();
    if (col < 0)      { col = 0; }
    if (col > maxCol) { col = maxCol; }
    return col;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollapseSelectionToCaret
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::CollapseSelectionToCaret()
{
    m_selAnchor = m_selCaret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::SelectAll()
{
    std::lock_guard<std::mutex>  lock (m_bufferMutex);


    m_selAnchor.line   = 0;
    m_selAnchor.column = 0;

    if (m_lines.empty())
    {
        m_selCaret = m_selAnchor;
    }
    else
    {
        m_selCaret.line   = (int) m_lines.size() - 1;
        m_selCaret.column = (int) m_lines.back().size();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSelection
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsolePanel::HasSelection() const
{
    return (m_selAnchor.line != m_selCaret.line) ||
           (m_selAnchor.column != m_selCaret.column);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OrderedSelection
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::OrderedSelection (Pos & lo, Pos & hi) const
{
    bool  anchorFirst = (m_selAnchor.line < m_selCaret.line) ||
                        (m_selAnchor.line == m_selCaret.line &&
                         m_selAnchor.column <= m_selCaret.column);


    if (anchorFirst)
    {
        lo = m_selAnchor;
        hi = m_selCaret;
    }
    else
    {
        lo = m_selCaret;
        hi = m_selAnchor;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveCaretLineEnd
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsolePanel::MoveCaretLineEnd (Pos & p, bool toEnd) const
{
    p.column = toEnd ? ClampColumn (p.line, INT_MAX) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BodyTopPx
//
////////////////////////////////////////////////////////////////////////////////

int DebugConsolePanel::BodyTopPx() const
{
    return (m_titleBar != nullptr) ? m_titleBar->GetTitleHeight() : 0;
}
