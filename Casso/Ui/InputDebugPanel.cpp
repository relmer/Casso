#include "Pch.h"

#include "InputDebugPanel.h"

#include "Chrome/ChromeTheme.h"
#include "Chrome/TitleBar.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName   = L"Casso.InputDebug.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle = L"Input Debug";

    constexpr int      s_kPreferredWidthDip  = 960;
    constexpr int      s_kPreferredHeightDip = 600;
    constexpr UINT     s_kSwapBufferCount    = 2;
    constexpr float    s_kLabelFontDip       = 13.0f;

    constexpr uint32_t s_kDisplayDequeCap    = 100000;
    constexpr uint32_t s_kDrainBatchSize     = 256;
    constexpr uint32_t s_kClearDrainBatchSize = 64;

    constexpr Word     s_kOpenAppleAddress   = 0xC061;
    constexpr Word     s_kClosedAppleAddress = 0xC062;
    constexpr Word     s_kShiftButtonAddress = 0xC063;
    constexpr Byte     s_kButtonPressedBit   = 0x80;
    constexpr Byte     s_kAnyKeyDownBit      = 0x80;
    constexpr Byte     s_kAsciiMask          = 0x7F;

    constexpr LPCWSTR  s_kpszShowLabel  = L"Show:";
    constexpr LPCWSTR  s_kpszPauseLabel = L"Pause";
    constexpr LPCWSTR  s_kpszResumeLabel = L"Resume";
    constexpr LPCWSTR  s_kpszClearLabel = L"Clear";

    constexpr LPCWSTR  s_kpszCategoryLabels[kInputCategoryCheckCount] =
    {
        L"Host",
        L"Guest",
        L"System",
    };

    constexpr LPCWSTR  s_kpszCategoryTips[kInputCategoryCheckCount] =
    {
        L"Show host keyboard events from Windows and auto-repeat",
        L"Show guest soft-switch reads observed by emulated code",
        L"Show synthetic input-debug diagnostics",
    };


    void ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
    {
        outRgba[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
        outRgba[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
        outRgba[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
        outRgba[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;
    }


    void FormatCycleWithSeparators (uint64_t value, wchar_t * out, size_t cap)
    {
        wchar_t  digits[24] = {};
        int      n          = 0;
        int      outIdx     = 0;
        int      i          = 0;

        if (out == nullptr || cap == 0)
        {
            return;
        }

        if (value == 0)
        {
            digits[n++] = L'0';
        }
        else
        {
            while (value > 0 && n < (int) (sizeof (digits) / sizeof (digits[0])))
            {
                digits[n++] = static_cast<wchar_t> (L'0' + (value % 10));
                value      /= 10;
            }
        }

        for (i = n - 1; i >= 0; i--)
        {
            if (outIdx > 0 && (((i + 1) % 3) == 0))
            {
                if (outIdx + 1 >= (int) cap)
                {
                    break;
                }
                out[outIdx++] = L',';
            }

            if (outIdx + 1 >= (int) cap)
            {
                break;
            }
            out[outIdx++] = digits[i];
        }

        out[outIdx] = L'\0';
    }


    void FormatWallNow (wchar_t * out, size_t cap)
    {
        using namespace std::chrono;

        auto       now   = system_clock::now();
        auto       wall  = system_clock::to_time_t (now);
        auto       ms    = duration_cast<milliseconds> (now.time_since_epoch()) % 1000;
        std::tm    local = {};
        errno_t    err   = 0;

        if (out == nullptr || cap < 16)
        {
            if (out != nullptr && cap > 0)
            {
                out[0] = L'\0';
            }
            return;
        }

        err = localtime_s (&local, &wall);
        if (err != 0)
        {
            out[0] = L'\0';
            return;
        }

        swprintf_s (out, cap,
                    L"%02d:%02d:%02d.%03lld",
                    local.tm_hour,
                    local.tm_min,
                    local.tm_sec,
                    (long long) ms.count());
    }


    void FormatUptime (
        std::chrono::steady_clock::time_point  anchor,
        wchar_t                              * out,
        size_t                                 cap)
    {
        using namespace std::chrono;

        auto       now     = steady_clock::now();
        long long  totalMs = 0;
        long long  minutes = 0;
        long long  seconds = 0;
        long long  millis  = 0;

        if (out == nullptr || cap < 12)
        {
            if (out != nullptr && cap > 0)
            {
                out[0] = L'\0';
            }
            return;
        }

        if (now < anchor)
        {
            out[0] = L'\0';
            return;
        }

        totalMs = duration_cast<milliseconds> (now - anchor).count();
        minutes = totalMs / 60000;
        seconds = (totalMs / 1000) % 60;
        millis  = totalMs % 1000;

        swprintf_s (out, cap, L"%02lld:%02lld.%03lld", minutes, seconds, millis);
    }


    wchar_t PrintableChar (Byte value) noexcept
    {
        if (value >= 0x20 && value <= 0x7E)
        {
            return (wchar_t) value;
        }

        return L'.';
    }


    std::wstring FormatByteChar (Byte value)
    {
        return std::format (L"${:02X} '{}'", value, PrintableChar (value));
    }


    std::wstring SourceLabel (InputEventCategory category)
    {
        switch (category)
        {
            case InputEventCategory::Host:   return L"Host";
            case InputEventCategory::Guest:  return L"Guest";
            case InputEventCategory::System: return L"System";
        }

        return L"?";
    }


    LPCWSTR ButtonAnnotation (Word address) noexcept
    {
        switch (address)
        {
            case s_kOpenAppleAddress:   return L"Open-Apple/Btn0";
            case s_kClosedAppleAddress: return L"Closed-Apple/Btn1 (bow)";
            case s_kShiftButtonAddress: return L"Shift/Btn2";
            default:                    return L"";
        }
    }


    void FormatInputEvent (
        const InputEvent &                         src,
        std::chrono::steady_clock::time_point      uptimeAnchor,
        InputEventDisplay &                        out)
    {
        Word     address = 0;
        Byte     value   = 0;
        Byte     key     = 0;
        bool     strobe  = false;
        bool     akd     = false;
        bool     pressed = false;
        LPCWSTR  button  = nullptr;

        out.category = src.category;
        out.type     = src.type;
        out.cycle    = src.cycle;
        out.source   = SourceLabel (src.category);
        out.address.clear();
        out.value.clear();
        out.meaning.clear();

        FormatWallNow (out.wallStr.data(), out.wallStr.size());
        FormatUptime  (uptimeAnchor, out.uptimeStr.data(), out.uptimeStr.size());
        out.cycleStr[0] = L'\0';
        if (src.type != InputEventType::HostKeyDown && src.type != InputEventType::HostKeyUp)
        {
            FormatCycleWithSeparators (src.cycle, out.cycleStr.data(), out.cycleStr.size());
        }

        switch (src.type)
        {
            case InputEventType::HostKeyDown:
                value = src.payload.key.ascii;
                out.value   = FormatByteChar (value);
                out.meaning = std::format (L"Key down: {}", out.value);
                break;

            case InputEventType::HostKeyUp:
                value = src.payload.key.ascii;
                out.value   = FormatByteChar (value);
                out.meaning = std::format (L"Key up: {}", out.value);
                break;

            case InputEventType::HostAutoRepeat:
                value = src.payload.key.ascii;
                out.value   = FormatByteChar (value);
                out.meaning = std::format (L"Auto-repeat: {}", out.value);
                break;

            case InputEventType::KbdDataRead:
                address = src.payload.io.address;
                value   = src.payload.io.value;
                key     = value & s_kAsciiMask;
                strobe  = (src.payload.io.flags & InputEvent::kFlagStrobe) != 0;
                out.address = std::format (L"${:04X}", address);
                out.value   = std::format (L"${:02X}", value);
                out.meaning = std::format (L"Read {} -> {}  key='{}' strobe={}",
                                           out.address,
                                           out.value,
                                           PrintableChar (key),
                                           strobe ? 1 : 0);
                break;

            case InputEventType::KbdStrobe:
                address = src.payload.io.address;
                value   = src.payload.io.value;
                akd     = (src.payload.io.flags & InputEvent::kFlagAnyKeyDown) != 0;
                strobe  = (src.payload.io.flags & InputEvent::kFlagStrobe) != 0;
                out.address = std::format (L"${:04X}", address);
                out.value   = std::format (L"${:02X}", value);
                out.meaning = std::format (L"Clear strobe {} -> {}  AKD={} cleared={}",
                                           out.address,
                                           out.value,
                                           akd ? 1 : 0,
                                           strobe ? 1 : 0);
                break;

            case InputEventType::ButtonRead:
                address = src.payload.io.address;
                value   = src.payload.io.value;
                pressed = (value & s_kButtonPressedBit) != 0;
                button  = ButtonAnnotation (address);
                out.address = std::format (L"${:04X}", address);
                out.value   = std::format (L"${:02X}", value);
                out.meaning = std::format (L"Button read {} -> {}  pressed={}",
                                           out.address,
                                           out.value,
                                           pressed ? 1 : 0);
                if (button[0] != L'\0')
                {
                    out.meaning.append (L"  ");
                    out.meaning.append (button);
                }
                break;

            case InputEventType::PaddleTrigger:
            case InputEventType::PaddleRead:
                address = src.payload.io.address;
                value   = src.payload.io.value;
                out.address = std::format (L"${:04X}", address);
                out.value   = std::format (L"${:02X}", value);
                out.meaning = std::format (L"Paddle event {} -> {}", out.address, out.value);
                break;

            case InputEventType::EventsLost:
                out.meaning = std::format (L"??? {} events lost (ring overflow)", src.payload.lost.count);
                break;
        }
    }


    void ProjectOne (
        const InputEvent &                         src,
        std::deque<InputEventDisplay> &            deque,
        std::chrono::steady_clock::time_point      uptimeAnchor)
    {
        InputEventDisplay  entry;

        FormatInputEvent (src, uptimeAnchor, entry);
        deque.push_back (std::move (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

InputDebugPanel::InputDebugPanel()
{
    m_uptimeAnchor = std::chrono::steady_clock::now();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

InputDebugPanel::~InputDebugPanel()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::Create (
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
//  Show / Hide / Destroy / RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Show()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_SHOW);
        SetForegroundWindow (hwnd);
    }
}

void InputDebugPanel::Hide()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_HIDE);
    }
}

void InputDebugPanel::Destroy()
{
    m_window.Destroy();
}

HRESULT InputDebugPanel::RenderFrame()
{
    return m_window.Render();
}

void InputDebugPanel::SetTheme (const ChromeTheme * theme)
{
    m_window.SetTheme (theme);
}

LPCWSTR InputDebugPanel::GetWindowClassName() const
{
    return s_kpszClassName;
}

LPCWSTR InputDebugPanel::GetWindowTitle() const
{
    return s_kpszWindowTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Host lifecycle
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::OnHostCreated (
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

    ConfigureWidgets();
    RecomputeLayout();

Error:
    return hr;
}

void InputDebugPanel::OnHostDestroyed()
{
    m_text.UnbindBackBuffer();
    m_text.Shutdown();
    m_painter.Shutdown();
    ReleaseRenderTargets();
    m_swapChain.Reset();
    m_hwnd     = nullptr;
    m_titleBar = nullptr;
}

HRESULT InputDebugPanel::OnHostResize (int widthPx, int heightPx, UINT dpi)
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

    RecomputeLayout();

Error:
    return hr;
}

void InputDebugPanel::SetChromeTheme (TitleBar * titleBar, const ChromeTheme * theme)
{
    m_titleBar = titleBar;
    m_theme    = theme;
    RecomputeLayout();
}

SIZE InputDebugPanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};


    size.cx = MulDiv (s_kPreferredWidthDip,  (int) dpi, 96);
    size.cy = MulDiv (s_kPreferredHeightDip, (int) dpi, 96);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Swap chain helpers
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::EnsureSwapChain()
{
    HRESULT                                  hr      = S_OK;
    Microsoft::WRL::ComPtr<IDXGIDevice>     dxgiDev;
    Microsoft::WRL::ComPtr<IDXGIAdapter>    adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2>   factory;
    DXGI_SWAP_CHAIN_DESC1                   desc    = {};


    CBRA (m_device);
    CBRA (m_hwnd);
    BAIL_OUT_IF (m_swapChain != nullptr, S_OK);

    hr = m_device->QueryInterface (IID_PPV_ARGS (&dxgiDev));
    CHRA (hr);
    hr = dxgiDev->GetAdapter (&adapter);
    CHRA (hr);
    hr = adapter->GetParent (IID_PPV_ARGS (&factory));
    CHRA (hr);

    desc.Width       = (UINT) std::max (1, m_widthPx);
    desc.Height      = (UINT) std::max (1, m_heightPx);
    desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = s_kSwapBufferCount;
    desc.Scaling     = DXGI_SCALING_STRETCH;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;

    hr = factory->CreateSwapChainForHwnd (m_device, m_hwnd, &desc, nullptr, nullptr, &m_swapChain);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:
    return hr;
}

HRESULT InputDebugPanel::CreateBackBufferRtv()
{
    HRESULT                                      hr     = S_OK;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     buffer;


    CBRA (m_device);
    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&buffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (buffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

Error:
    return hr;
}

void InputDebugPanel::ReleaseRenderTargets()
{
    m_rtv.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout and configuration
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ConfigureWidgets()
{
    HRESULT  hr = S_OK;
    int      i  = 0;


    SeedDefaultColumns (m_columnsModel);

    m_showLabel.SetText (s_kpszShowLabel);

    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        m_categoryChecks[i].SetLabel (s_kpszCategoryLabels[i]);
        m_categoryChecks[i].SetChecked (true);
    }

    m_pauseButton.SetLabel (s_kpszPauseLabel);
    m_clearButton.SetLabel (s_kpszClearLabel);

    m_eventList.SetShowHeader (true);
    m_eventList.EnableStickyTail (true);
    m_eventList.SetDpi (m_dpi);
    m_eventList.SetTheme (m_theme);
    m_columnMenu.SetDpi (m_dpi);
    m_columnMenu.SetTheme (m_theme);
    m_columnMenu.SetOnSelect ([this] (int id)
    {
        auto & columns = m_columnsModel;
        if (id >= 0 && id < kInputColumnCount)
        {
            columns[id].visible = !columns[id].visible;
            m_eventList.SetColumns (PlanVisibleColumns (columns));
            PushListViewRows();
        }
    });

    m_tooltip.SetDpi (m_dpi);
}

void InputDebugPanel::RecomputeLayout()
{
    int  topOffset = 0;


    if (m_titleBar != nullptr)
    {
        topOffset = m_titleBar->GetTitleHeight();
    }

    m_showLabel.SetDpi (m_dpi);
    for (int i = 0; i < kInputCategoryCheckCount; i++) { m_categoryChecks[i].SetDpi (m_dpi); }
    m_pauseButton.SetDpi (m_dpi);
    m_clearButton.SetDpi (m_dpi);
    m_eventList.SetDpi (m_dpi);
    m_eventList.SetTheme (m_theme);
    m_columnMenu.SetDpi (m_dpi);
    m_columnMenu.SetTheme (m_theme);
    m_tooltip.SetDpi (m_dpi);

    m_layout = ComputeInputDebugPanelLayout (m_widthPx, m_heightPx, topOffset, m_dpi);
    LayoutWidgets();
}

void InputDebugPanel::LayoutWidgets()
{
    int  i = 0;


    m_showLabel.SetRect (m_layout.showLabel);
    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        m_categoryChecks[i].SetRect (m_layout.categoryChecks[i]);
    }

    m_pauseButton.Layout (m_layout.pauseButton);
    m_clearButton.Layout (m_layout.clearButton);
    m_eventList.SetRect (m_layout.listView);
    m_eventList.SetColumns (PlanVisibleColumns (m_columnsModel));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::Render()
{
    HRESULT                                  hr            = S_OK;
    float                                    clearColor[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    Microsoft::WRL::ComPtr<IDXGISurface>    surface;
    ChromeVisualState                       visual        = {};
    D3D11_VIEWPORT                          vp            = {};


    BAIL_OUT_IF (m_context == nullptr || m_rtv == nullptr || m_swapChain == nullptr, S_OK);

    DrainAndProject();

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

    hr = m_text.BeginDrawOffscreen();
    CHRA (hr);

    visual.dpi = m_dpi;
    if (m_titleBar != nullptr && m_theme != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, visual, *m_theme);
    }

    m_showLabel.Paint (m_painter, m_text);
    for (int i = 0; i < kInputCategoryCheckCount; i++)
    {
        m_categoryChecks[i].Paint (m_painter, m_text);
    }
    if (m_theme != nullptr)
    {
        m_pauseButton.Paint (m_painter, m_text, *m_theme);
        m_clearButton.Paint (m_painter, m_text, *m_theme);
    }
    m_eventList.Paint (m_painter, m_text);
    m_tooltip.Tick (NowMs());

    if (m_tooltip.IsVisible() || m_columnMenu.IsVisible())
    {
        hr = m_painter.End (m_rtv.Get());
        CHRA (hr);
        hr = m_text.EndDrawComposite();
        CHRA (hr);
        hr = m_painter.Begin (m_widthPx, m_heightPx);
        CHRA (hr);
        hr = m_text.BeginDrawOffscreen();
        CHRA (hr);
    }

    m_tooltip.Paint (m_painter, m_text);
    m_columnMenu.Paint (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDrawComposite();
    CHRA (hr);

    // If the D2D target was lost partway through this frame (the
    // swap-chain buffers were invalidated by a live resize), EndDraw
    // unbinds the target and the frame we just built is incomplete --
    // some text was flushed before the loss, the rest was dropped.
    // Presenting it would flash partially-painted content (blank rows,
    // missing buttons). Skip Present so the last good frame stays on
    // screen; the next Render rebinds (via the !IsTargetBound() path
    // above) and repaints cleanly.
    if (m_text.IsTargetBound())
    {
        hr = m_swapChain->Present (1, 0);
        CHRA (hr);
    }

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  Event projection
//
////////////////////////////////////////////////////////////////////////////////

InputEvent InputDebugPanel::MakeStampedEvent (InputEventCategory cat, InputEventType type) const noexcept
{
    InputEvent  e = {};


    e.category = cat;
    e.type     = type;
    e.cycle    = (m_cycleCounter != nullptr) ? *m_cycleCounter : 0;
    return e;
}

void InputDebugPanel::PublishToRing (const InputEvent & e)
{
    bool  pushed = false;


    pushed = m_ring.TryPush (e);
    if (!pushed)
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}

void InputDebugPanel::DrainAndProject()
{
    std::array<InputEvent, s_kDrainBatchSize>  batch       = {};
    uint32_t                                   lost        = 0;
    size_t                                     n           = 0;
    bool                                       changed     = false;
    InputEvent                                 lostEvent   = {};


    if (m_paused)
    {
        return;
    }

    if (!m_pendingHostEvents.empty())
    {
        for (const InputEvent & e : m_pendingHostEvents)
        {
            ProjectOne (e, m_events, m_uptimeAnchor);
        }
        m_pendingHostEvents.clear();
        changed = true;
    }

    lost = m_droppedSinceLastDrain.exchange (0, std::memory_order_relaxed);
    if (lost != 0)
    {
        lostEvent = MakeStampedEvent (InputEventCategory::System, InputEventType::EventsLost);
        lostEvent.payload.lost.count = lost;
        ProjectOne (lostEvent, m_events, m_uptimeAnchor);
        changed = true;
    }

    do
    {
        n = m_ring.Drain (batch.data(), (uint32_t) batch.size());
        for (size_t i = 0; i < n; i++)
        {
            ProjectOne (batch[i], m_events, m_uptimeAnchor);
        }
        changed = changed || (n != 0);
    }
    while (n == batch.size());

    while (m_events.size() > s_kDisplayDequeCap)
    {
        m_events.pop_front();
        changed = true;
    }

    if (changed)
    {
        RebuildFilteredIndices();
        PushListViewRows();
    }
}

void InputDebugPanel::RebuildFilteredIndices()
{
    size_t  i = 0;


    m_filteredIndices.clear();
    m_filteredIndices.reserve (m_events.size());

    for (i = 0; i < m_events.size(); i++)
    {
        if (MatchesFilter (m_events[i], m_filter))
        {
            m_filteredIndices.push_back (i);
        }
    }

    if (m_sortColumn >= 0)
    {
        SortByColumn (m_sortColumn);
    }
}

void InputDebugPanel::PushListViewRows()
{
    std::vector<std::vector<ListView::Cell>>  rows;
    int                                       oldSelected = m_eventList.SelectedRow();


    rows.reserve (m_filteredIndices.size());
    for (size_t eventIndex : m_filteredIndices)
    {
        std::vector<ListView::Cell>  row;

        row.resize (kInputColumnCount);
        for (int col = 0; col < kInputColumnCount; col++)
        {
            AppendColumnText (row[(size_t) col].text, m_events[eventIndex], col);
        }
        rows.push_back (std::move (row));
    }

    m_eventList.SetRows (std::move (rows));
    if (m_eventList.RowCount() > 0)
    {
        if (oldSelected < 0)
        {
            m_eventList.SetSelectedRow (m_eventList.RowCount() - 1);
        }
        else
        {
            m_eventList.SetSelectedRow (oldSelected);
        }
    }
}






////////////////////////////////////////////////////////////////////////////////
//
//  Filtering and controls
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ClearEvents()
{
    std::array<InputEvent, s_kClearDrainBatchSize>  scratch = {};


    m_events.clear();
    m_filteredIndices.clear();
    m_pendingHostEvents.clear();
    while (m_ring.Drain (scratch.data(), (uint32_t) scratch.size()) != 0)
    {
    }
    m_droppedSinceLastDrain.store (0, std::memory_order_relaxed);
    PushListViewRows();
}

void InputDebugPanel::OnFilterChanged()
{
    m_filter.showHost   = m_categoryChecks[0].Checked();
    m_filter.showGuest  = m_categoryChecks[1].Checked();
    m_filter.showSystem = m_categoryChecks[2].Checked();

    RebuildFilteredIndices();
    PushListViewRows();
}

void InputDebugPanel::UpdatePauseLabel()
{
    m_pauseButton.SetLabel (m_paused ? s_kpszResumeLabel : s_kpszPauseLabel);
}

void InputDebugPanel::SortByColumn (int absCol)
{
    auto compareText = [this, absCol] (size_t lhs, size_t rhs) -> bool
    {
        std::wstring  l;
        std::wstring  r;

        AppendColumnText (l, m_events[lhs], absCol);
        AppendColumnText (r, m_events[rhs], absCol);
        if (m_sortDescending)
        {
            return _wcsicmp (l.c_str(), r.c_str()) > 0;
        }
        return _wcsicmp (l.c_str(), r.c_str()) < 0;
    };


    if (absCol < 0 || absCol >= kInputColumnCount)
    {
        return;
    }

    if (m_sortColumn == absCol)
    {
        m_sortDescending = !m_sortDescending;
    }
    else
    {
        m_sortColumn     = absCol;
        m_sortDescending = false;
    }

    std::stable_sort (m_filteredIndices.begin(), m_filteredIndices.end(), compareText);
    m_eventList.SetSortIndicator (absCol, m_sortDescending);
    
}

void InputDebugPanel::ApplyListSelection()
{
    int  selected = m_eventList.SelectedRow();


    if (selected >= 0 && selected < (int) m_filteredIndices.size())
    {
        m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) selected];
    }
}

void InputDebugPanel::OnListSelectionMoved()
{
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mouse and keyboard
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnLButtonDown (int x, int y)
{
    int  i = 0;


    if (m_pauseButton.HitTest (x, y))
    {
        m_pauseButton.SetMouse (x, y, true);
        SetFocusIndex (kInputCategoryCheckCount);
        return;
    }

    if (m_clearButton.HitTest (x, y))
    {
        m_clearButton.SetMouse (x, y, true);
        SetFocusIndex (kInputCategoryCheckCount + 1);
        return;
    }

    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        if (m_categoryChecks[i].HitTest (x, y))
        {
            m_categoryChecks[i].OnLButtonDown (x, y);
            SetFocusIndex (i);
            return;
        }
    }

    if ((x >= m_layout.listView.left && x < m_layout.listView.right && y >= m_layout.listView.top && y < m_layout.listView.bottom))
    {
        int  lx        = x - m_layout.listView.left;
        int  ly        = y - m_layout.listView.top;
        int  resizeCol = m_eventList.HitTestColumnResize (lx, ly, 4);
        int  headerCol = m_eventList.HitTestHeaderColumn (lx, ly);
        int  row       = -1;

        if (resizeCol >= 0)
        {
            m_resizeColumn       = resizeCol;
            m_resizeStartXPx     = x;
            m_resizeStartWidthPx = m_eventList.ColumnEffectiveWidthPx ((size_t) resizeCol);
            SetFocusIndex (kInputCategoryCheckCount + 2);
            return;
        }

        if (headerCol >= 0)
        {
            SortByColumn (headerCol);
            SetFocusIndex (kInputCategoryCheckCount + 2);
            return;
        }

        row = m_eventList.HitTestRow (lx, ly);
        if (row >= 0)
        {
            m_eventList.SetSelectedRow (row);
        }
        SetFocusIndex (kInputCategoryCheckCount + 2);
    }
}

void InputDebugPanel::OnLButtonUp (int x, int y)
{
    int  i = 0;


    m_resizeColumn = -1;

    if (m_pauseButton.HitTest (x, y))
    {
        m_paused = !m_paused;
        UpdatePauseLabel();
    }
    m_pauseButton.SetMouse (x, y, false);

    if (m_clearButton.HitTest (x, y))
    {
        ClearEvents();
    }
    m_clearButton.SetMouse (x, y, false);

    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        if (m_categoryChecks[i].OnLButtonUp (x, y))
        {
            OnFilterChanged();
        }
    }

    if ((x >= m_layout.listView.left && x < m_layout.listView.right && y >= m_layout.listView.top && y < m_layout.listView.bottom))
    {
        int  row = m_eventList.HitTestRow (x - m_layout.listView.left, y - m_layout.listView.top);
        if (row >= 0)
        {
            m_eventList.SetSelectedRow (row);
            OnListSelectionMoved();
        }
    }
}

void InputDebugPanel::OnRButtonDown (int x, int y)
{
    if (m_eventList.HitTestHeaderColumn (x - m_layout.listView.left, y - m_layout.listView.top) >= 0)
    {
        ShowColumnMenu (x, y);
    }
}

void InputDebugPanel::OnMouseMove (int x, int y)
{
    int  newWidth = 0;


    if (m_resizeColumn >= 0)
    {
        newWidth = std::max (24, m_resizeStartWidthPx + (x - m_resizeStartXPx));
        m_columnsModel[(size_t) m_resizeColumn].savedWidth  = newWidth;
        m_columnsModel[(size_t) m_resizeColumn].userResized = true;
        m_eventList.SetColumns (PlanVisibleColumns (m_columnsModel));
        return;
    }

    UpdateTooltip (x, y);
}

void InputDebugPanel::OnMouseWheel (int x, int y, int delta)
{
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    m_eventList.ScrollByWheelDelta (delta);
}

bool InputDebugPanel::OnKey (WPARAM vk)
{
    bool  handled = true;


    switch (vk)
    {
        case VK_TAB:
            FocusCycle ((GetKeyState (VK_SHIFT) & 0x8000) ? -1 : 1);
            break;

        case VK_SPACE:
            if (m_focusIndex >= 0 && m_focusIndex < kInputCategoryCheckCount)
            {
                m_categoryChecks[m_focusIndex].SetChecked (!m_categoryChecks[m_focusIndex].Checked());
                OnFilterChanged();
            }
            else if (m_focusIndex == kInputCategoryCheckCount)
            {
                m_paused = !m_paused;
                UpdatePauseLabel();
            }
            else if (m_focusIndex == kInputCategoryCheckCount + 1)
            {
                ClearEvents();
            }
            break;

        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_HOME:
        case VK_END:
            if (vk == VK_UP) { m_eventList.SetSelectedRow (m_eventList.SelectedRow() - 1); }
            else if (vk == VK_DOWN) { m_eventList.SetSelectedRow (m_eventList.SelectedRow() + 1); }
            else if (vk == VK_PRIOR) { m_eventList.ScrollByRows (-m_eventList.VisibleRowCapacity()); }
            else if (vk == VK_NEXT) { m_eventList.ScrollByRows (m_eventList.VisibleRowCapacity()); }
            else if (vk == VK_HOME) { m_eventList.SetSelectedRow (0); }
            else if (vk == VK_END) { m_eventList.SetSelectedRow (m_eventList.RowCount() - 1); }
            OnListSelectionMoved();
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}

bool InputDebugPanel::OnChar (wchar_t ch)
{
    UNREFERENCED_PARAMETER (ch);
    return false;
}

void InputDebugPanel::Accept()
{
}

void InputDebugPanel::Cancel()
{
    Hide();
}

bool InputDebugPanel::IsContentActive() const
{
    return true;
}

HCURSOR InputDebugPanel::OnSetCursor (int x, int y)
{
    int  lx = x - m_layout.listView.left;
    int  ly = y - m_layout.listView.top;


    if (m_resizeColumn >= 0 || m_eventList.HitTestColumnResize (lx, ly, 4) >= 0)
    {
        return LoadCursor (nullptr, IDC_SIZEWE);
    }

    return nullptr;
}

void InputDebugPanel::UpdateTooltip (int x, int y)
{
    LPCWSTR  text = nullptr;
    int      i    = 0;


    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        if (m_categoryChecks[i].HitTest (x, y))
        {
            text = s_kpszCategoryTips[i];
        }
    }

    if (text == nullptr && m_pauseButton.HitTest (x, y))
    {
        text = L"Pause or resume live input logging";
    }
    if (text == nullptr && m_clearButton.HitTest (x, y))
    {
        text = L"Clear the input debug log";
    }

    if (text != nullptr)
    {
        RECT  anchor = { x, y, x + 1, y + 1 };
        m_tooltip.RequestShow (anchor, text, NowMs());
    }
    else
    {
        m_tooltip.RequestHide (NowMs());
    }
}

void InputDebugPanel::ShowColumnMenu (int anchorX, int anchorY)
{
    auto &                       columns  = m_columnsModel;
    std::vector<PopupMenu::Item> items;
    RECT                         hostRect = { 0, 0, m_widthPx, m_heightPx };
    int                          i        = 0;


    items.reserve (kInputColumnCount);
    for (i = 0; i < kInputColumnCount; i++)
    {
        PopupMenu::Item  item;

        item.label   = columns[i].headerText;
        item.checked = columns[i].visible;
        items.push_back (std::move (item));
    }

    m_columnMenu.Show (anchorX, anchorY, std::move (items), m_text, hostRect);
}

void InputDebugPanel::ClearAllWidgetFocus()
{
    int  i = 0;


    for (i = 0; i < kInputCategoryCheckCount; i++)
    {
        m_categoryChecks[i].SetFocused (false);
    }
    m_pauseButton.SetFocused (false);
    m_clearButton.SetFocused (false);
    m_eventList.SetListFocused (false);
}

int InputDebugPanel::DynamicStopCount() const
{
    return 1;
}

int InputDebugPanel::TotalStopCount() const
{
    return kInputCategoryCheckCount + 2 + DynamicStopCount();
}

void InputDebugPanel::FocusCycle (int direction)
{
    int  total = TotalStopCount();
    int  next  = 0;


    if (total <= 0)
    {
        return;
    }

    next = m_focusIndex;
    if (next < 0)
    {
        next = (direction > 0) ? 0 : total - 1;
    }
    else
    {
        next = (next + direction + total) % total;
    }

    SetFocusIndex (next);
}

void InputDebugPanel::SetFocusIndex (int index)
{
    ClearAllWidgetFocus();
    m_focusIndex = index;

    if (index >= 0 && index < kInputCategoryCheckCount)
    {
        m_categoryChecks[index].SetFocused (true);
    }
    else if (index == kInputCategoryCheckCount)
    {
        m_pauseButton.SetFocused (true);
    }
    else if (index == kInputCategoryCheckCount + 1)
    {
        m_clearButton.SetFocused (true);
    }
    else if (index == kInputCategoryCheckCount + 2)
    {
        m_eventList.SetListFocused (true);
    }
}

void InputDebugPanel::OnHeaderSortKey()
{
    SortByColumn (std::max (0, m_sortColumn));
}

void InputDebugPanel::OnDividerResizeKey (int direction)
{
    UNREFERENCED_PARAMETER (direction);
}

int64_t InputDebugPanel::NowMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IInputEventSink
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnKbdDataRead (Word address, Byte value, bool strobeSet)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::KbdDataRead);


    e.payload.io.address = address;
    e.payload.io.value   = value;
    e.payload.io.flags   = strobeSet ? InputEvent::kFlagStrobe : 0;
    PublishToRing (e);
}

void InputDebugPanel::OnKbdStrobe (Word address, Byte value, bool clearedStrobe)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::KbdStrobe);


    e.payload.io.address = address;
    e.payload.io.value   = value;
    e.payload.io.flags   = 0;
    if (clearedStrobe)
    {
        e.payload.io.flags |= InputEvent::kFlagStrobe;
    }
    if ((value & s_kAnyKeyDownBit) != 0)
    {
        e.payload.io.flags |= InputEvent::kFlagAnyKeyDown;
    }
    PublishToRing (e);
}

void InputDebugPanel::OnButtonRead (Word address, Byte value)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::ButtonRead);


    e.payload.io.address = address;
    e.payload.io.value   = value;
    e.payload.io.flags   = 0;
    PublishToRing (e);
}

void InputDebugPanel::OnHostAutoRepeat (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostAutoRepeat);


    e.payload.key.ascii = asciiChar;
    PublishToRing (e);
}

void InputDebugPanel::OnHostKeyDown (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostKeyDown);


    e.cycle = 0;
    e.payload.key.ascii = asciiChar;
    m_pendingHostEvents.push_back (e);
}

void InputDebugPanel::OnHostKeyUp (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostKeyUp);


    e.cycle = 0;
    e.payload.key.ascii = asciiChar;
    m_pendingHostEvents.push_back (e);
}

