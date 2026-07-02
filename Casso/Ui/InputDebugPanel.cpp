#include "Pch.h"

#include "InputDebugPanel.h"

#include "Chrome/CassoTheme.h"
#include "Window/DxuiHostWindow.h"





constexpr LPCWSTR  s_kpszClassName   = L"Casso.InputDebug.Panel";
constexpr LPCWSTR  s_kpszWindowTitle = L"Casso - Input events";

constexpr int      s_kPreferredWidthDip  = 960;
constexpr int      s_kPreferredHeightDip = 600;
constexpr UINT     s_kSwapBufferCount    = 2;
constexpr float    s_kLabelFontDip       = 13.0f;

// Minimum width (DIP) a column can be drag-resized to, and the DPI
// the DIP column widths are authored at.
constexpr int      s_kMinColWidthDip     = 24;
constexpr int      s_kColWidthBaseDpi    = 96;

constexpr uint32_t s_kDisplayDequeHighWater = 100000;
constexpr uint32_t s_kDisplayDequeLowWater  =  90000;
constexpr uint32_t s_kDrainBatchSize     = 256;
constexpr uint32_t s_kClearDrainBatchSize = 64;

constexpr Word     s_kOpenAppleAddress   = 0xC061;
constexpr Word     s_kClosedAppleAddress = 0xC062;
constexpr Word     s_kShiftButtonAddress = 0xC063;
constexpr Byte     s_kButtonPressedBit   = 0x80;
constexpr Byte     s_kAnyKeyDownBit      = 0x80;
constexpr Byte     s_kAsciiMask          = 0x7F;

constexpr Word     s_kPaddle0Address       = 0xC064;
constexpr Word     s_kPaddle1Address       = 0xC065;
constexpr Word     s_kPaddle2Address       = 0xC066;
constexpr Word     s_kPaddle3Address       = 0xC067;
constexpr Word     s_kPaddleTriggerAddress = 0xC070;
constexpr Byte     s_kPaddleCountingBit    = 0x80;
constexpr Word     s_kHostPair0FirstAxis   = 0;
constexpr Word     s_kHostPair1FirstAxis   = 2;
constexpr Word     s_kHostAxisCount        = 4;
constexpr Word     s_kHostButton0Index     = 0;
constexpr Word     s_kHostButton1Index     = 1;

constexpr LPCWSTR  s_kpszEmuLabel    = L"Emulator input:";
constexpr LPCWSTR  s_kpszHostLabel   = L"Host input:";
constexpr LPCWSTR  s_kpszAllLabel    = L"All";
constexpr LPCWSTR  s_kpszKeyboardLabel = L"Keyboard";
constexpr LPCWSTR  s_kpszJoystickLabel = L"Joystick";
constexpr LPCWSTR  s_kpszPaddleLabel = L"Paddle";
constexpr LPCWSTR  s_kpszPauseLabel  = L"Pause";
constexpr LPCWSTR  s_kpszResumeLabel = L"Resume";
constexpr LPCWSTR  s_kpszClearLabel  = L"Clear";
constexpr LPCWSTR  s_kpszCopyLabel   = L"Copy";

constexpr LPCWSTR  s_kpszPair0Label  = L"View PADDL0-PADDL1 as";
constexpr LPCWSTR  s_kpszPair1Label  = L"View PADDL2-PADDL3 as";

constexpr LPCWSTR  s_kpszPair0Items[2] = { L"Joystick 0", L"Paddles 0, 1" };
constexpr LPCWSTR  s_kpszPair1Items[2] = { L"Joystick 1", L"Paddles 2, 3" };

constexpr LPCWSTR  s_kpszAllTip      = L"DxuiToggle every emulator-input lane at once";
constexpr LPCWSTR  s_kpszEmuKbdTip   = L"Show guest keyboard soft-switch reads ($C000/$C010)";
constexpr LPCWSTR  s_kpszJoystickTip = L"Show game-port reads for pairs viewed as a joystick";
constexpr LPCWSTR  s_kpszPaddleTip   = L"Show game-port reads for pairs viewed as paddles";
constexpr LPCWSTR  s_kpszHostKbdTip  = L"Show host keyboard events from Windows and auto-repeat";

constexpr uint32_t s_kLabelArgb      = 0xFFB8C0CC;





////////////////////////////////////////////////////////////////////////////////
//
//  ArgbToFloat4
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
{
    outRgba[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
    outRgba[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
    outRgba[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
    outRgba[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCycleWithSeparators
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::FormatCycleWithSeparators (uint64_t value, wchar_t * out, size_t cap)
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





////////////////////////////////////////////////////////////////////////////////
//
//  FormatWallNow
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::FormatWallNow (wchar_t * out, size_t cap)
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





////////////////////////////////////////////////////////////////////////////////
//
//  FormatUptime
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::FormatUptime (
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





////////////////////////////////////////////////////////////////////////////////
//
//  PrintableChar
//
////////////////////////////////////////////////////////////////////////////////

wchar_t InputDebugPanel::PrintableChar (Byte value) noexcept
{
    if (value >= 0x20 && value <= 0x7E)
    {
        return (wchar_t) value;
    }

    return L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatByteChar
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputDebugPanel::FormatByteChar (Byte value)
{
    return std::format (L"${:02X} '{}'", value, PrintableChar (value));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputDebugPanel::SourceLabel (InputEventCategory category)
{
    switch (category)
    {
        case InputEventCategory::Host:   return L"Host";
        case InputEventCategory::Guest:  return L"Guest";
        case InputEventCategory::System: return L"System";
    }

    return L"?";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ButtonAnnotation
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR InputDebugPanel::ButtonAnnotation (Word address) noexcept
{
    switch (address)
    {
        case s_kOpenAppleAddress:   return L"Open-Apple/Btn0";
        case s_kClosedAppleAddress: return L"Closed-Apple/Btn1 (bow)";
        case s_kShiftButtonAddress: return L"Shift/Btn2";
        default:                    return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyGamePort
//
////////////////////////////////////////////////////////////////////////////////

InputGamePortClass InputDebugPanel::ClassifyGamePort (InputEventType type, Word address) noexcept
{
    switch (type)
    {
        case InputEventType::HostPaddle:
            if (address >= s_kHostPair0FirstAxis && address < s_kHostPair1FirstAxis)
            {
                return InputGamePortClass::Pair0;
            }
            if (address >= s_kHostPair1FirstAxis && address < s_kHostAxisCount)
            {
                return InputGamePortClass::Pair1;
            }
            return InputGamePortClass::None;

        case InputEventType::HostButton:
            switch (address)
            {
                case s_kHostButton0Index:
                case s_kHostButton1Index: return InputGamePortClass::Pair0;
                default:                  return InputGamePortClass::None;
            }

        case InputEventType::ButtonRead:
            switch (address)
            {
                case s_kOpenAppleAddress:
                case s_kClosedAppleAddress: return InputGamePortClass::Pair0;
                case s_kShiftButtonAddress: return InputGamePortClass::Pair1;
                default:                    return InputGamePortClass::None;
            }

        case InputEventType::PaddleRead:
            switch (address)
            {
                case s_kPaddle0Address:
                case s_kPaddle1Address:     return InputGamePortClass::Pair0;
                case s_kPaddle2Address:
                case s_kPaddle3Address:     return InputGamePortClass::Pair1;
                default:                    return InputGamePortClass::None;
            }

        case InputEventType::PaddleTrigger:
            return InputGamePortClass::Global;

        default:
            return InputGamePortClass::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatInputEvent
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::FormatInputEvent (
    const InputEvent &                         src,
    std::chrono::steady_clock::time_point      uptimeAnchor,
    const InputFilterState &                   filter,
    InputEventDisplay &                        out)
{
    Word     address = 0;
    Byte     value   = 0;
    Byte     key     = 0;
    bool     strobe  = false;
    bool     akd     = false;
    bool     pressed = false;
    bool     counting = false;
    int      axis    = 0;
    LPCWSTR  button  = nullptr;

    out.category = src.category;
    out.type     = src.type;
    out.gamePort = ClassifyGamePort (src.type, src.payload.io.address);
    out.cycle    = src.cycle;
    out.source   = SourceLabel (src.category);
    out.address.clear();
    out.value.clear();
    out.meaning.clear();

    FormatWallNow (out.wallStr.data(), out.wallStr.size());
    FormatUptime  (uptimeAnchor, out.uptimeStr.data(), out.uptimeStr.size());
    out.cycleStr[0] = L'\0';
    if (src.type != InputEventType::HostKeyDown &&
        src.type != InputEventType::HostKeyUp   &&
        src.type != InputEventType::HostPaddle  &&
        src.type != InputEventType::HostButton)
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

        case InputEventType::HostPaddle:
            axis  = (int) src.payload.io.address;
            value = src.payload.io.value;
            {
                int  pair = axis / 2;

                // Decode per the user's "View PADDL<n>-PADDL<m> as" choice:
                // a pair viewed as a joystick reads axis 0/2 as X and 1/3
                // as Y; viewed as paddles it stays PADDL<n>.
                if (pair >= 0 && pair < 2 && filter.pairIsJoystick[pair])
                {
                    LPCWSTR  xy = (axis % 2 == 0) ? L"X" : L"Y";

                    out.address = std::format (L"JOY{} {}", pair, xy);
                    out.value   = std::format (L"{}", value);
                    out.meaning = std::format (L"HOST JOY{} {} = {}", pair, xy, value);
                }
                else
                {
                    out.address = std::format (L"PDL{}", axis);
                    out.value   = std::format (L"{}", value);
                    out.meaning = std::format (L"HOST PDL{} = {}", axis, value);
                }
            }
            break;

        case InputEventType::HostButton:
            axis    = (int) src.payload.io.address;
            pressed = src.payload.io.value != 0;
            out.address = std::format (L"BTN{}", axis);
            out.value   = pressed ? L"DOWN" : L"UP";
            out.meaning = std::format (L"HOST BTN{} {}", axis, pressed ? L"DOWN" : L"UP");
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
            out.meaning = std::format (L"DxuiButton read {} -> {}  pressed={}",
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
            address = src.payload.io.address;
            out.address = std::format (L"${:04X}", address);
            out.meaning = std::format (L"PTRIG strobe {}  (arm game-port timers)", out.address);
            break;

        case InputEventType::PaddleRead:
            address  = src.payload.io.address;
            value    = src.payload.io.value;
            axis     = (int) (address - s_kPaddle0Address);
            counting = (value & s_kPaddleCountingBit) != 0;
            out.address = std::format (L"${:04X}", address);
            out.value   = std::format (L"${:02X}", value);
            out.meaning = std::format (L"Read PADDL{} {} -> {}  counting={}",
                                       axis,
                                       out.address,
                                       out.value,
                                       counting ? 1 : 0);
            break;

        case InputEventType::EventsLost:
            out.meaning = std::format (L"??? {} events lost (ring overflow)", src.payload.lost.count);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectOne
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ProjectOne (
    const InputEvent &                         src,
    std::deque<InputEventDisplay> &            deque,
    std::chrono::steady_clock::time_point      uptimeAnchor,
    const InputFilterState &                   filter)
{
    InputEventDisplay  entry;

    FormatInputEvent (src, uptimeAnchor, filter, entry);
    deque.push_back (std::move (entry));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

InputDebugPanel::InputDebugPanel()
{
    // Register each owned widget into the panel's child list via Adopt
    // so they participate in the IDxuiControl tree (Bounds, Visible,
    // focus, parent pointers). The widgets remain InputDebugPanel-owned
    // members; Adopt is non-owning. The chrome shell still drives
    // input/paint through the bespoke IChromedPanelContent shims;
    // collapsing the duality is deferred to a follow-up session that
    // also threads a popup host through to the pair-view dropdowns
    // and column menu.
    Adopt (m_emuLabel);
    Adopt (m_hostLabel);
    for (DxuiLabel & label : m_pairLabel)
    {
        Adopt (label);
    }
    Adopt (m_allCheck);
    Adopt (m_emuKeyboardCheck);
    Adopt (m_joystickCheck);
    Adopt (m_paddleCheck);
    Adopt (m_hostKeyboardCheck);
    for (DxuiDropdown & dropdown : m_pairView)
    {
        Adopt (dropdown);
    }
    Adopt (m_pauseButton);
    Adopt (m_clearButton);
    Adopt (m_copyButton);
    Adopt (m_eventList);
    Adopt (m_tooltip);
    Adopt (m_columnMenu);

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
    const CassoTheme    * theme)
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

void InputDebugPanel::Show()
{
    m_window.Activate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Hide()
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

void InputDebugPanel::Destroy()
{
    m_window.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::RenderFrame()
{
    // Drive scrollbar auto-repeat for any held arrow / track press.
    m_eventList.Tick (NowMs());

    return m_window.Render();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SetTheme (const CassoTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowClassName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR InputDebugPanel::GetWindowClassName() const
{
    return s_kpszClassName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR InputDebugPanel::GetWindowTitle() const
{
    return s_kpszWindowTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostCreated
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::OnHostCreated (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi,
    DxuiHostWindow       * captionHost,
    const CassoTheme    * theme)
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
    m_captionHost = captionHost;
    m_theme    = theme;

    hr = EnsureSwapChain();
    CHRA (hr);

    hr = m_painter.Initialize (device, context);
    CHRA (hr);

    hr = m_text.Initialize (device);
    CHRA (hr);

    ConfigureWidgets();
    RecomputeLayout();

    // Route the column right-click menu through the host popup pool so
    // it renders as a real top-level popup (not clipped to the panel).
    m_columnMenu.SetPopupHost (m_window.PopupHost());

    // Same for hover tooltips so the balloon can escape the panel edge
    // and occlude whatever is behind it.
    m_tooltip.SetPopupHost (m_window.PopupHost());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostDestroyed
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostDestroyed()
{
    // Release any live popup back to the pool and drop the host pointer
    // before the host (and its pool) are destroyed in OnDestroy.
    m_columnMenu.Hide();
    m_columnMenu.SetPopupHost (nullptr);

    m_tooltip.HideImmediate();
    m_tooltip.SetPopupHost (nullptr);

    m_text.UnbindBackBuffer();
    m_text.Shutdown();
    m_painter.Shutdown();
    ReleaseRenderTargets();
    m_swapChain.Reset();
    m_hwnd     = nullptr;
    m_captionHost = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostResize
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  SetChromeTheme
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SetChromeTheme (DxuiHostWindow * captionHost, const CassoTheme * theme)
{
    m_captionHost = captionHost;
    m_theme    = theme;
    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredClientSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE InputDebugPanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};


    size.cx = MulDiv (s_kPreferredWidthDip,  (int) dpi, 96);
    size.cy = MulDiv (s_kPreferredHeightDip, (int) dpi, 96);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSwapChain
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





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferRtv
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseRenderTargets
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ReleaseRenderTargets()
{
    m_rtv.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ConfigureWidgets()
{
    int  p = 0;


    SeedDefaultColumns (m_columnsModel);

    m_emuLabel.SetText      (s_kpszEmuLabel);
    m_hostLabel.SetText     (s_kpszHostLabel);
    m_emuLabel.SetColorArgb (s_kLabelArgb);
    m_hostLabel.SetColorArgb(s_kLabelArgb);

    m_allCheck.SetLabel          (s_kpszAllLabel);
    m_emuKeyboardCheck.SetLabel  (s_kpszKeyboardLabel);
    m_joystickCheck.SetLabel     (s_kpszJoystickLabel);
    m_paddleCheck.SetLabel       (s_kpszPaddleLabel);
    m_hostKeyboardCheck.SetLabel (s_kpszKeyboardLabel);

    m_allCheck.SetChecked          (true);
    m_emuKeyboardCheck.SetChecked  (true);
    m_joystickCheck.SetChecked     (true);
    m_paddleCheck.SetChecked       (true);
    m_hostKeyboardCheck.SetChecked (true);

    m_pairLabel[0].SetText      (s_kpszPair0Label);
    m_pairLabel[1].SetText      (s_kpszPair1Label);
    m_pairLabel[0].SetColorArgb (s_kLabelArgb);
    m_pairLabel[1].SetColorArgb (s_kLabelArgb);

    m_pairView[0].SetItems ({ s_kpszPair0Items[0], s_kpszPair0Items[1] });
    m_pairView[1].SetItems ({ s_kpszPair1Items[0], s_kpszPair1Items[1] });
    for (p = 0; p < 2; p++)
    {
        m_pairView[(size_t) p].SetSelected (m_filter.pairIsJoystick[p] ? 0 : 1);
    }

    m_pairView[0].SetSelect ([this] (int idx) { OnPairViewChanged (0, idx); });
    m_pairView[1].SetSelect ([this] (int idx) { OnPairViewChanged (1, idx); });

    m_pauseButton.SetLabel (s_kpszPauseLabel);
    m_clearButton.SetLabel (s_kpszClearLabel);
    m_copyButton.SetLabel  (s_kpszCopyLabel);

    m_eventList.SetShowHeader    (true);
    m_eventList.EnableStickyTail (true);
    m_eventList.SetDpi           (m_dpi);
    m_eventList.SetTheme         (m_theme);

    // The list owns its own scroll / thumb / column-resize / row-select
    // routing via OnMouse; these callbacks fold the semantic outcomes
    // back into the panel's model (selection -> detail pane, header
    // click -> sort, resize-drag end -> persisted column width).
    m_eventList.SetOnSelectionChanged ([this] (int) { ApplyListSelection(); });
    m_eventList.SetOnSortColumn       ([this] (int col) { SortByColumn (col); });
    m_eventList.SetOnColumnResized    ([this] (int col, int widthPx)
    {
        int  widthDip = 0;

        if (col >= 0 && col < kInputColumnCount)
        {
            // The drag width is physical px; savedWidth feeds widthDip,
            // which ComputeColumnLayout re-scales by the DPI -- convert
            // back so a 1px drag is a 1px resize (not 2px at 200%).
            widthDip = std::max (s_kMinColWidthDip, MulDiv (widthPx, s_kColWidthBaseDpi, (int) m_dpi));
            m_columnsModel[(size_t) col].savedWidth  = widthDip;
            m_columnsModel[(size_t) col].userResized = true;
            m_eventList.SetColumns (PlanVisibleColumns (m_columnsModel));
        }
    });

    // The buttons and checkboxes own their own press / hover / toggle
    // state through OnMouse; these callbacks fold each widget's outcome
    // back into the panel model -- the buttons fire their action on a
    // click-release, the checkboxes re-apply the filter (or the
    // all-toggle) whenever their checked state changes.
    m_pauseButton.SetClick          ([this] () { m_paused = !m_paused; UpdatePauseLabel(); });
    m_clearButton.SetClick          ([this] () { ClearEvents(); });
    m_copyButton.SetClick           ([this] () { CopyEventsToClipboard(); });
    m_allCheck.SetOnChange          ([this] (bool) { ApplyAllToggle(); });
    m_emuKeyboardCheck.SetOnChange  ([this] (bool) { OnFilterChanged(); });
    m_joystickCheck.SetOnChange     ([this] (bool) { OnFilterChanged(); });
    m_paddleCheck.SetOnChange       ([this] (bool) { OnFilterChanged(); });
    m_hostKeyboardCheck.SetOnChange ([this] (bool) { OnFilterChanged(); });

    m_columnMenu.SetDpi          (m_dpi);
    m_columnMenu.SetTheme        (m_theme);
    m_columnMenu.SetOnSelect     ([this] (int id)
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

    UpdatePairVisibility();
    SyncAllCheck();
    RebuildFocusOrder();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecomputeLayout
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::RecomputeLayout()
{
    int  topOffset = 0;
    int  p         = 0;


    if (m_captionHost != nullptr)
    {
        topOffset = m_captionHost->CaptionHeightPx();
    }

    m_emuLabel.SetDpi          (m_dpi);
    m_hostLabel.SetDpi         (m_dpi);
    m_allCheck.SetDpi          (m_dpi);
    m_emuKeyboardCheck.SetDpi  (m_dpi);
    m_joystickCheck.SetDpi     (m_dpi);
    m_paddleCheck.SetDpi       (m_dpi);
    m_hostKeyboardCheck.SetDpi (m_dpi);

    for (p = 0; p < 2; p++)
    {
        m_pairLabel[(size_t) p].SetDpi (m_dpi);
        m_pairView[(size_t) p].SetDpi  (m_dpi);
    }

    m_pauseButton.SetDpi      (m_dpi);
    m_clearButton.SetDpi      (m_dpi);
    m_copyButton.SetDpi       (m_dpi);
    m_eventList.SetDpi        (m_dpi);
    m_eventList.SetTheme      (m_theme);
    m_columnMenu.SetDpi       (m_dpi);
    m_columnMenu.SetTheme     (m_theme);
    m_tooltip.SetDpi          (m_dpi);
    m_tooltip.SetViewportSize (m_widthPx, m_heightPx);

    m_layout = ComputeInputDebugPanelLayout (m_widthPx,
                                             m_heightPx,
                                             topOffset,
                                             m_joystickVisible,
                                             m_paddleVisible,
                                             m_dpi);
    LayoutWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutWidgets
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::LayoutWidgets()
{
    int  p = 0;


    m_emuLabel.SetRect          (m_layout.emuLabel);
    m_hostLabel.SetRect         (m_layout.hostLabel);
    m_allCheck.SetRect          (m_layout.allCheck);
    m_emuKeyboardCheck.SetRect  (m_layout.emuKeyboardCheck);
    m_joystickCheck.SetRect     (m_layout.joystickCheck);
    m_paddleCheck.SetRect       (m_layout.paddleCheck);
    m_hostKeyboardCheck.SetRect (m_layout.hostKeyboardCheck);

    for (p = 0; p < 2; p++)
    {
        m_pairLabel[(size_t) p].SetRect (m_layout.pairLabel[p]);
        m_pairView[(size_t) p].SetRect  (m_layout.pairDropdown[p]);
    }

    m_pauseButton.Layout   (m_layout.pauseButton);
    m_clearButton.Layout   (m_layout.clearButton);
    m_copyButton.Layout    (m_layout.copyButton);
    m_eventList.SetRect    (m_layout.listView);
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
        ArgbToFloat4 (m_theme->titleBarBottom, clearColor);
        clearColor[3] = 1.0f;
    }

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float) m_widthPx;
    vp.Height   = (float) m_heightPx;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports        (1, &vp);
    m_context->OMSetRenderTargets    (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDrawOffscreen();
    CHRA (hr);

    visual.dpi = m_dpi;
    if (m_captionHost != nullptr && m_theme != nullptr)
    {
        m_captionHost->RenderCaption (m_painter, m_text, *m_theme);
    }

    if (m_theme != nullptr)
    {
        uint32_t  labelArgb = m_theme->ForegroundMuted();

        m_emuLabel.SetColorArgb     (labelArgb);
        m_hostLabel.SetColorArgb    (labelArgb);
        m_pairLabel[0].SetColorArgb (labelArgb);
        m_pairLabel[1].SetColorArgb (labelArgb);
        m_pairView[0].SetTheme (m_theme);
        m_pairView[1].SetTheme (m_theme);
        m_tooltip.SetTheme (*m_theme);
    }

    m_emuLabel.Paint  (m_painter, m_text);
    m_hostLabel.Paint (m_painter, m_text);
    if (m_theme != nullptr)
    {
        m_allCheck.Paint          (m_painter, m_text, *m_theme);
        m_emuKeyboardCheck.Paint  (m_painter, m_text, *m_theme);
        if (m_joystickVisible) { m_joystickCheck.Paint (m_painter, m_text, *m_theme); }
        if (m_paddleVisible)   { m_paddleCheck.Paint   (m_painter, m_text, *m_theme); }
        m_hostKeyboardCheck.Paint (m_painter, m_text, *m_theme);
    }
    m_pairLabel[0].Paint (m_painter, m_text);
    m_pairLabel[1].Paint (m_painter, m_text);
    m_pairView[0].PaintBase (m_painter, m_text);
    m_pairView[1].PaintBase (m_painter, m_text);
    if (m_theme != nullptr)
    {
        m_pauseButton.Paint (m_painter, m_text, *m_theme);
        m_clearButton.Paint (m_painter, m_text, *m_theme);
        m_copyButton.Paint  (m_painter, m_text, *m_theme);
    }
    m_eventList.Paint (m_painter, m_text);
    m_pairView[0].PaintMenu (m_painter, m_text);
    m_pairView[1].PaintMenu (m_painter, m_text);
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
//  MakeStampedEvent
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





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::PublishToRing (const InputEvent & e)
{
    bool  pushed = false;


    pushed = m_ring.TryPush (e);
    if (!pushed)
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainAndProject
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::DrainAndProject()
{
    std::array<InputEvent, s_kDrainBatchSize>  batch     = {};
    uint32_t                                   lost      = 0;
    size_t                                     n         = 0;
    size_t                                     baseSize  = 0;
    size_t                                     appended  = 0;
    bool                                       trimmed   = false;
    InputEvent                                 lostEvent = {};
    int64_t                                    ticks     = 0;


    if (m_resetAnchorPending.exchange (false, std::memory_order_acq_rel))
    {
        // A reset (Ctrl+R / power-cycle) was requested from the CPU
        // thread. Apply the staged Uptime anchor and clear the event
        // list HERE, on the render thread, so m_events, m_filteredIndices
        // and the DxuiListView rows are only ever touched by one thread.
        ticks = m_pendingAnchorTicks.load (std::memory_order_acquire);

        m_uptimeAnchor = std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (ticks));
        ClearEvents();
    }

    if (m_paused)
    {
        return;
    }

    baseSize = m_events.size();

    if (!m_pendingHostEvents.empty())
    {
        for (const InputEvent & e : m_pendingHostEvents)
        {
            ProjectOne (e, m_events, m_uptimeAnchor, m_filter);
        }
        m_pendingHostEvents.clear();
    }

    lost = m_droppedSinceLastDrain.exchange (0, std::memory_order_relaxed);
    if (lost != 0)
    {
        lostEvent = MakeStampedEvent (InputEventCategory::System, InputEventType::EventsLost);
        lostEvent.payload.lost.count = lost;
        ProjectOne (lostEvent, m_events, m_uptimeAnchor, m_filter);
    }

    do
    {
        n = m_ring.Drain (batch.data(), (uint32_t) batch.size());
        for (size_t i = 0; i < n; i++)
        {
            ProjectOne (batch[i], m_events, m_uptimeAnchor, m_filter);
        }
    }
    while (n == batch.size());

    appended = m_events.size() - baseSize;

    // Evict in batches at a high-water mark rather than trimming a single
    // row every frame. A per-frame pop_front would shift every deque index
    // and force an O(n) rebuild of the filtered view and DxuiListView rows on
    // each frame; batching keeps the steady-state streaming path append-only.
    if (m_events.size() > s_kDisplayDequeHighWater)
    {
        while (m_events.size() > s_kDisplayDequeLowWater)
        {
            m_events.pop_front();
        }
        trimmed = true;
    }

    if (!trimmed && appended == 0)
    {
        return;
    }

    // A trim renumbers every surviving deque index, and an active sort can
    // place new events anywhere in the order, so both demand a full rebuild.
    // The common streaming case (no trim, no sort) only appends the newly
    // projected rows, leaving the existing rows and indices untouched.
    if (trimmed || m_sortColumn >= 0)
    {
        RebuildFilteredIndices();
        PushListViewRows();
    }
    else
    {
        AppendNewEventRows (baseSize);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
//
////////////////////////////////////////////////////////////////////////////////

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
        ApplySort();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendNewEventRows
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::AppendNewEventRows (size_t startIndex)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    size_t                                    i = 0;


    // Caller guarantees no eviction happened this frame, so deque indices in
    // [startIndex, size) are stable and can be appended to m_filteredIndices
    // and the DxuiListView without disturbing the existing rows.
    rows.reserve (m_events.size() - startIndex);
    for (i = startIndex; i < m_events.size(); i++)
    {
        std::vector<DxuiListView::Cell>  row;

        if (!MatchesFilter (m_events[i], m_filter))
        {
            continue;
        }

        m_filteredIndices.push_back (i);
        row.resize (kInputColumnCount);
        for (int col = 0; col < kInputColumnCount; col++)
        {
            AppendColumnText (row[(size_t) col].text, m_events[i], col);
        }
        rows.push_back (std::move (row));
    }

    if (!rows.empty())
    {
        m_eventList.AppendRows (std::move (rows));
        m_eventList.UpdateAutoFitFromRows();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushListViewRows
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::PushListViewRows()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    int                                       oldSelected = m_eventList.GetSelectedRow();


    rows.reserve (m_filteredIndices.size());
    for (size_t eventIndex : m_filteredIndices)
    {
        std::vector<DxuiListView::Cell>  row;

        row.resize (kInputColumnCount);
        for (int col = 0; col < kInputColumnCount; col++)
        {
            AppendColumnText (row[(size_t) col].text, m_events[eventIndex], col);
        }
        rows.push_back (std::move (row));
    }

    m_eventList.SetRows (std::move (rows));
    m_eventList.UpdateAutoFitFromRows();
    if (oldSelected >= 0 && m_eventList.GetRowCount() > 0)
    {
        m_eventList.SetSelectedRow (oldSelected);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearEvents
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
    m_eventList.ResetAutoFit();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestResetAnchor
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept
{
    m_pendingAnchorTicks.store (anchor.time_since_epoch().count(), std::memory_order_release);
    m_resetAnchorPending.store (true, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterChanged
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnFilterChanged()
{
    m_filter.showEmuKeyboard  = m_emuKeyboardCheck.Checked();
    m_filter.showJoystick     = m_joystickCheck.Checked();
    m_filter.showPaddle       = m_paddleCheck.Checked();
    m_filter.showHostKeyboard = m_hostKeyboardCheck.Checked();

    SyncAllCheck();
    RebuildFilteredIndices();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPairViewChanged
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnPairViewChanged (int pair, int index)
{
    if (pair < 0 || pair > 1)
    {
        return;
    }

    m_filter.pairIsJoystick[pair] = (index == 0);

    UpdatePairVisibility();
    SyncAllCheck();
    RebuildFocusOrder();
    RecomputeLayout();
    RebuildFilteredIndices();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePairVisibility
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::UpdatePairVisibility()
{
    m_joystickVisible = m_filter.pairIsJoystick[0] || m_filter.pairIsJoystick[1];
    m_paddleVisible   = !m_filter.pairIsJoystick[0] || !m_filter.pairIsJoystick[1];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncAllCheck
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SyncAllCheck()
{
    bool  all = m_emuKeyboardCheck.Checked();


    if (m_joystickVisible) { all = all && m_joystickCheck.Checked(); }
    if (m_paddleVisible)   { all = all && m_paddleCheck.Checked(); }

    m_allCheck.SetChecked (all);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAllToggle
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ApplyAllToggle()
{
    bool  newState = m_allCheck.Checked();


    m_emuKeyboardCheck.SetChecked (newState);
    if (m_joystickVisible) { m_joystickCheck.SetChecked (newState); }
    if (m_paddleVisible)   { m_paddleCheck.SetChecked (newState); }

    OnFilterChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePauseLabel
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::UpdatePauseLabel()
{
    m_pauseButton.SetLabel (m_paused ? s_kpszResumeLabel : s_kpszPauseLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyEventsToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::CopyEventsToClipboard()
{
    std::vector<const InputEventDisplay *>  rows;
    std::wstring                            text;
    HGLOBAL                                 hGlobal = nullptr;
    void                                  * pBuf    = nullptr;


    rows.reserve (m_filteredIndices.size());
    for (size_t eventIndex : m_filteredIndices)
    {
        rows.push_back (&m_events[eventIndex]);
    }

    text = BuildClipboardText (rows, m_columnsModel);
    if (text.empty())
    {
        return;
    }

    if (!OpenClipboard (m_hwnd))
    {
        return;
    }

    if (!EmptyClipboard())
    {
        CloseClipboard();
        return;
    }

    hGlobal = GlobalAlloc (GMEM_MOVEABLE, (text.size() + 1) * sizeof (wchar_t));
    if (hGlobal == nullptr)
    {
        CloseClipboard();
        return;
    }

    pBuf = GlobalLock (hGlobal);
    if (pBuf == nullptr)
    {
        GlobalFree (hGlobal);
        CloseClipboard();
        return;
    }

    memcpy (pBuf, text.c_str(), (text.size() + 1) * sizeof (wchar_t));
    GlobalUnlock (hGlobal);

    if (SetClipboardData (CF_UNICODETEXT, hGlobal) == nullptr)
    {
        GlobalFree (hGlobal);
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SortByColumn
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SortByColumn (int absCol)
{
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

    ApplySort();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplySort
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ApplySort()
{
    auto compareText = [this] (size_t lhs, size_t rhs) -> bool
    {
        std::wstring  l;
        std::wstring  r;

        AppendColumnText (l, m_events[lhs], m_sortColumn);
        AppendColumnText (r, m_events[rhs], m_sortColumn);
        if (m_sortDescending)
        {
            return _wcsicmp (l.c_str(), r.c_str()) > 0;
        }
        return _wcsicmp (l.c_str(), r.c_str()) < 0;
    };


    if (m_sortColumn < 0 || m_sortColumn >= kInputColumnCount)
    {
        return;
    }

    std::stable_sort (m_filteredIndices.begin(), m_filteredIndices.end(), compareText);
    m_eventList.SetSortIndicator (m_sortColumn, m_sortDescending);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyListSelection
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ApplyListSelection()
{
    int  selected = m_eventList.GetSelectedRow();


    if (selected >= 0 && selected < (int) m_filteredIndices.size())
    {
        m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) selected];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnListSelectionMoved
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnListSelectionMoved()
{
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForwardMouseToList
//
//  Translates a client-px mouse event into the event list's widget-local
//  space and dispatches it through DxuiListView::OnMouse, which owns all
//  scroll / thumb / column-resize / row-select routing and raises the
//  panel's selection / sort / column-resize callbacks. Returns true when
//  the list consumed the event.
//
////////////////////////////////////////////////////////////////////////////////

bool InputDebugPanel::ForwardMouseToList (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta)
{
    DxuiMouseEvent  ev;


    ev.kind        = kind;
    ev.button      = button;
    ev.positionDip = { x - m_layout.listView.left, y - m_layout.listView.top };
    ev.wheelDelta  = wheelDelta;

    return m_eventList.OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool InputDebugPanel::OnMouse (const DxuiMouseEvent & ev)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            // While the list owns a drag (scrollbar thumb / column resize),
            // route moves to it. DxuiListView::OnMouse treats a non-Left
            // move while interacting as a release (its missed-button-up
            // safety net), so pass Left explicitly.
            if (m_eventList.IsInteracting())
            {
                (void) ForwardMouseToList (DxuiMouseEventKind::Move, DxuiMouseButton::Left, x, y, 0.0f);
                return true;
            }

            m_pairView[0].SetMouseHover (x, y);
            m_pairView[1].SetMouseHover (x, y);
            UpdateTooltip (x, y);
            return true;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left)
            {
                // The client-px widgets share the panel's coordinate space
                // (ev.positionDip == client px), so route the event straight
                // to each widget's OnMouse; the widget hit-tests itself and
                // reports whether it consumed the press.
                if (m_pairView[0].OnMouse (ev))
                {
                    if (!m_pairView[0].IsOpen()) { SetFocusToStop (InputFocusStop::Pair0Dropdown); }
                    return true;
                }
                if (m_pairView[1].OnMouse (ev))
                {
                    if (!m_pairView[1].IsOpen()) { SetFocusToStop (InputFocusStop::Pair1Dropdown); }
                    return true;
                }

                if (m_pauseButton.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::PauseButton);
                    return true;
                }

                if (m_clearButton.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::ClearButton);
                    return true;
                }

                if (m_copyButton.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::CopyButton);
                    return true;
                }

                if (m_allCheck.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::AllCheck);
                    return true;
                }

                if (m_emuKeyboardCheck.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::EmuKeyboardCheck);
                    return true;
                }

                if (m_joystickVisible && m_joystickCheck.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::JoystickCheck);
                    return true;
                }

                if (m_paddleVisible && m_paddleCheck.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::PaddleCheck);
                    return true;
                }

                if (m_hostKeyboardCheck.OnMouse (ev))
                {
                    SetFocusToStop (InputFocusStop::HostKeyboardCheck);
                    return true;
                }

                if ((x >= m_layout.listView.left && x < m_layout.listView.right && y >= m_layout.listView.top && y < m_layout.listView.bottom))
                {
                    // The list owns all in-list routing (scrollbar arrows /
                    // thumb / track, column resize, header-click sort, row
                    // select) via OnMouse and reports outcomes through the
                    // callbacks wired at setup. ChromedPanelWindow already
                    // holds the Win32 capture for the full press, so any
                    // drag the list starts keeps receiving moves without the
                    // panel managing capture itself.
                    (void) ForwardMouseToList (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, 0.0f);
                    SetFocusToStop (InputFocusStop::EventList);
                }

                return true;
            }

            if (ev.button == DxuiMouseButton::Right)
            {
                if (m_eventList.HitTestHeaderColumn (x - m_layout.listView.left, y - m_layout.listView.top) >= 0)
                {
                    ShowColumnMenu (x, y);
                }
                return true;
            }

            return false;

        case DxuiMouseEventKind::Up:
            if (ev.button == DxuiMouseButton::Left)
            {
                bool  wasInteracting = m_eventList.IsInteracting();



                // Finish any list drag (scrollbar thumb / column resize) the
                // list started on button-down. The pointer may have left the
                // list bounds mid-drag, so forward the release
                // unconditionally. ChromedPanelWindow owns the Win32 capture
                // and releases it after this returns.
                if (wasInteracting)
                {
                    (void) ForwardMouseToList (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, 0.0f);
                    return true;
                }

                if (m_pairView[0].OnMouse (ev)) { return true; }
                if (m_pairView[1].OnMouse (ev)) { return true; }

                // Route the release to each button / checkbox: the widget
                // clears its own press visual and, on a click-release over
                // itself, fires the callback wired at setup (button click /
                // checkbox change), which folds the outcome back into the
                // panel model.
                m_pauseButton.OnMouse (ev);
                m_clearButton.OnMouse (ev);
                m_copyButton.OnMouse (ev);

                m_allCheck.OnMouse (ev);
                m_emuKeyboardCheck.OnMouse (ev);
                if (m_joystickVisible) { m_joystickCheck.OnMouse (ev); }
                if (m_paddleVisible)   { m_paddleCheck.OnMouse (ev); }
                m_hostKeyboardCheck.OnMouse (ev);

                // A plain release inside the list finalizes the row activate;
                // the list already selected on button-down (raising
                // onSelectionChanged).
                if ((x >= m_layout.listView.left && x < m_layout.listView.right && y >= m_layout.listView.top && y < m_layout.listView.bottom))
                {
                    (void) ForwardMouseToList (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, 0.0f);
                }

                return true;
            }

            return false;

        case DxuiMouseEventKind::Wheel:
            // Forward to the list, which scrolls only when the pointer is
            // over it (standard control behavior) and converts notches back
            // to raw WHEEL_DELTA units internally.
            (void) ForwardMouseToList (DxuiMouseEventKind::Wheel, DxuiMouseButton::None, x, y, ev.wheelDelta);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool InputDebugPanel::OnKey (const DxuiKeyEvent & ev)
{
    WPARAM          vk       = (WPARAM) ev.vk;
    bool            handled  = true;
    InputFocusStop  focused  = InputFocusStop::EventList;
    bool            hasFocus = (m_focusIndex >= 0 && m_focusIndex < (int) m_focusStops.size());


    // Char events carry no panel semantics (no text entry surface); only
    // key-down drives focus cycling, space activation and list navigation.
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    if (hasFocus)
    {
        focused = m_focusStops[(size_t) m_focusIndex];

        if (focused == InputFocusStop::Pair0Dropdown && m_pairView[0].HandleKey (vk)) { return true; }
        if (focused == InputFocusStop::Pair1Dropdown && m_pairView[1].HandleKey (vk)) { return true; }
    }

    switch (vk)
    {
        case VK_TAB:
            FocusCycle ((GetKeyState (VK_SHIFT) & 0x8000) ? -1 : 1);
            break;

        case VK_SPACE:
            if (!hasFocus)
            {
                break;
            }
            switch (focused)
            {
                case InputFocusStop::AllCheck:
                    m_allCheck.SetChecked (!m_allCheck.Checked());
                    ApplyAllToggle();
                    break;

                case InputFocusStop::EmuKeyboardCheck:
                    m_emuKeyboardCheck.SetChecked (!m_emuKeyboardCheck.Checked());
                    OnFilterChanged();
                    break;

                case InputFocusStop::JoystickCheck:
                    m_joystickCheck.SetChecked (!m_joystickCheck.Checked());
                    OnFilterChanged();
                    break;

                case InputFocusStop::PaddleCheck:
                    m_paddleCheck.SetChecked (!m_paddleCheck.Checked());
                    OnFilterChanged();
                    break;

                case InputFocusStop::HostKeyboardCheck:
                    m_hostKeyboardCheck.SetChecked (!m_hostKeyboardCheck.Checked());
                    OnFilterChanged();
                    break;

                case InputFocusStop::PauseButton:
                    m_paused = !m_paused;
                    UpdatePauseLabel();
                    break;

                case InputFocusStop::ClearButton:
                    ClearEvents();
                    break;

                case InputFocusStop::CopyButton:
                    CopyEventsToClipboard();
                    break;

                default:
                    break;
            }
            break;

        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_HOME:
        case VK_END:
            if (vk == VK_UP) { m_eventList.SetSelectedRow (m_eventList.GetSelectedRow() - 1); }
            else if (vk == VK_DOWN) { m_eventList.SetSelectedRow (m_eventList.GetSelectedRow() + 1); }
            else if (vk == VK_PRIOR) { m_eventList.ScrollByRows (-m_eventList.GetVisibleRowCapacity()); }
            else if (vk == VK_NEXT) { m_eventList.ScrollByRows (m_eventList.GetVisibleRowCapacity()); }
            else if (vk == VK_HOME) { m_eventList.SetSelectedRow (0); }
            else if (vk == VK_END) { m_eventList.SetSelectedRow (m_eventList.GetRowCount() - 1); }
            OnListSelectionMoved();
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Accept()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Cancel()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsContentActive
//
////////////////////////////////////////////////////////////////////////////////

bool InputDebugPanel::IsContentActive() const
{
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
////////////////////////////////////////////////////////////////////////////////

HCURSOR InputDebugPanel::OnSetCursor (int x, int y)
{
    int  lx = x - m_layout.listView.left;
    int  ly = y - m_layout.listView.top;


    if (m_eventList.IsResizingColumn() || m_eventList.HitTestColumnResize (lx, ly, 4) >= 0)
    {
        return LoadCursor (nullptr, IDC_SIZEWE);
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateTooltip
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::UpdateTooltip (int x, int y)
{
    LPCWSTR  text   = nullptr;
    RECT     anchor = {};


    if (m_allCheck.HitTest (x, y))                               { text = s_kpszAllTip;      anchor = m_allCheck.Bounds();         }
    else if (m_emuKeyboardCheck.HitTest (x, y))                  { text = s_kpszEmuKbdTip;   anchor = m_emuKeyboardCheck.Bounds(); }
    else if (m_joystickVisible && m_joystickCheck.HitTest (x, y)) { text = s_kpszJoystickTip; anchor = m_joystickCheck.Bounds();    }
    else if (m_paddleVisible && m_paddleCheck.HitTest (x, y))     { text = s_kpszPaddleTip;   anchor = m_paddleCheck.Bounds();      }
    else if (m_hostKeyboardCheck.HitTest (x, y))                 { text = s_kpszHostKbdTip;  anchor = m_hostKeyboardCheck.Bounds(); }

    if (text == nullptr && m_pauseButton.HitTest (x, y))
    {
        text   = L"Pause or resume live input logging";
        anchor = m_pauseButton.Bounds();
    }
    if (text == nullptr && m_clearButton.HitTest (x, y))
    {
        text   = L"Clear the input debug log";
        anchor = m_clearButton.Bounds();
    }
    if (text == nullptr && m_copyButton.HitTest (x, y))
    {
        text   = L"Copy the visible input debug log to the clipboard";
        anchor = m_copyButton.Bounds();
    }

    if (text != nullptr)
    {
        m_tooltip.RequestShow (anchor, text, NowMs());
    }
    else
    {
        m_tooltip.RequestHide (NowMs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowColumnMenu
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ShowColumnMenu (int anchorX, int anchorY)
{
    auto &                       columns  = m_columnsModel;
    std::vector<DxuiPopupMenu::Item> items;
    RECT                         hostRect = { 0, 0, m_widthPx, m_heightPx };
    int                          i        = 0;


    items.reserve (kInputColumnCount);
    for (i = 0; i < kInputColumnCount; i++)
    {
        DxuiPopupMenu::Item  item;

        item.label   = columns[i].headerText;
        item.checked = columns[i].visible;
        items.push_back (std::move (item));
    }

    m_columnMenu.Show (anchorX, anchorY, std::move (items), m_text, hostRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearAllWidgetFocus
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ClearAllWidgetFocus()
{
    m_allCheck.SetFocused          (false);
    m_emuKeyboardCheck.SetFocused  (false);
    m_joystickCheck.SetFocused     (false);
    m_paddleCheck.SetFocused       (false);
    m_hostKeyboardCheck.SetFocused (false);
    m_pairView[0].SetFocused       (false);
    m_pairView[1].SetFocused       (false);
    m_pauseButton.SetFocused       (false);
    m_clearButton.SetFocused       (false);
    m_copyButton.SetFocused        (false);
    m_eventList.SetListFocused     (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFocusOrder
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::RebuildFocusOrder()
{
    InputFocusStop  prev     = InputFocusStop::EventList;
    bool            hadFocus = (m_focusIndex >= 0 && m_focusIndex < (int) m_focusStops.size());
    size_t          i        = 0;


    if (hadFocus)
    {
        prev = m_focusStops[(size_t) m_focusIndex];
    }

    m_focusStops.clear();
    m_focusStops.push_back (InputFocusStop::AllCheck);
    m_focusStops.push_back (InputFocusStop::EmuKeyboardCheck);
    if (m_joystickVisible) { m_focusStops.push_back (InputFocusStop::JoystickCheck); }
    if (m_paddleVisible)   { m_focusStops.push_back (InputFocusStop::PaddleCheck); }
    m_focusStops.push_back (InputFocusStop::HostKeyboardCheck);
    m_focusStops.push_back (InputFocusStop::Pair0Dropdown);
    m_focusStops.push_back (InputFocusStop::Pair1Dropdown);
    m_focusStops.push_back (InputFocusStop::PauseButton);
    m_focusStops.push_back (InputFocusStop::ClearButton);
    m_focusStops.push_back (InputFocusStop::CopyButton);
    m_focusStops.push_back (InputFocusStop::EventList);

    m_focusIndex = -1;
    if (hadFocus)
    {
        for (i = 0; i < m_focusStops.size(); i++)
        {
            if (m_focusStops[i] == prev)
            {
                m_focusIndex = (int) i;
                break;
            }
        }
    }

    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyFocus
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::ApplyFocus()
{
    ClearAllWidgetFocus();

    if (m_focusIndex < 0 || m_focusIndex >= (int) m_focusStops.size())
    {
        return;
    }

    switch (m_focusStops[(size_t) m_focusIndex])
    {
        case InputFocusStop::AllCheck:          m_allCheck.SetFocused (true);          break;
        case InputFocusStop::EmuKeyboardCheck:  m_emuKeyboardCheck.SetFocused (true);  break;
        case InputFocusStop::JoystickCheck:     m_joystickCheck.SetFocused (true);     break;
        case InputFocusStop::PaddleCheck:       m_paddleCheck.SetFocused (true);       break;
        case InputFocusStop::HostKeyboardCheck: m_hostKeyboardCheck.SetFocused (true); break;
        case InputFocusStop::Pair0Dropdown:     m_pairView[0].SetFocused (true);       break;
        case InputFocusStop::Pair1Dropdown:     m_pairView[1].SetFocused (true);       break;
        case InputFocusStop::PauseButton:       m_pauseButton.SetFocused (true);       break;
        case InputFocusStop::ClearButton:       m_clearButton.SetFocused (true);       break;
        case InputFocusStop::CopyButton:        m_copyButton.SetFocused (true);        break;
        case InputFocusStop::EventList:         m_eventList.SetListFocused (true);     break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusCycle
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::FocusCycle (int direction)
{
    int  total = (int) m_focusStops.size();
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

    m_focusIndex = next;
    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocusToStop
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SetFocusToStop (InputFocusStop stop)
{
    size_t  i = 0;


    for (i = 0; i < m_focusStops.size(); i++)
    {
        if (m_focusStops[i] == stop)
        {
            m_focusIndex = (int) i;
            ApplyFocus();
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeaderSortKey
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHeaderSortKey()
{
    SortByColumn (std::max (0, m_sortColumn));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDividerResizeKey
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnDividerResizeKey (int direction)
{
    UNREFERENCED_PARAMETER (direction);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t InputDebugPanel::NowMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKbdDataRead
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





////////////////////////////////////////////////////////////////////////////////
//
//  OnKbdStrobe
//
////////////////////////////////////////////////////////////////////////////////

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





////////////////////////////////////////////////////////////////////////////////
//
//  OnButtonRead
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnButtonRead (Word address, Byte value)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::ButtonRead);


    e.payload.io.address = address;
    e.payload.io.value   = value;
    e.payload.io.flags   = 0;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPaddleTrigger
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnPaddleTrigger (Word address)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::PaddleTrigger);


    e.payload.io.address = address;
    e.payload.io.value   = 0;
    e.payload.io.flags   = 0;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPaddleRead
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnPaddleRead (Word address, Byte value)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Guest, InputEventType::PaddleRead);


    e.payload.io.address = address;
    e.payload.io.value   = value;
    e.payload.io.flags   = 0;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostAutoRepeat
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostAutoRepeat (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostAutoRepeat);


    e.payload.key.ascii = asciiChar;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostKeyDown (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostKeyDown);


    e.cycle = 0;
    e.payload.key.ascii = asciiChar;
    m_pendingHostEvents.push_back (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostKeyUp
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostKeyUp (Byte asciiChar)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostKeyUp);


    e.cycle = 0;
    e.payload.key.ascii = asciiChar;
    m_pendingHostEvents.push_back (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostPaddle
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostPaddle (int axis, Byte value)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostPaddle);



    e.cycle = 0;
    e.payload.io.address = static_cast<Word> (axis);
    e.payload.io.value   = value;
    e.payload.io.flags   = 0;
    m_pendingHostEvents.push_back (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostButton
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnHostButton (int index, bool down)
{
    InputEvent  e = MakeStampedEvent (InputEventCategory::Host, InputEventType::HostButton);



    e.cycle = 0;
    e.payload.io.address = static_cast<Word> (index);
    e.payload.io.value   = down ? 1 : 0;
    e.payload.io.flags   = 0;
    m_pendingHostEvents.push_back (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Layout(RECT, scaler) for the
//  IDxuiControl tree. The chrome shell drives this panel's bespoke
//  RecomputeLayout / LayoutWidgets pipeline directly via
//  OnHostResize, so the adapter is intentionally a no-op. It exists
//  so an IDxuiControl-tree walk targeting the panel does not abort
//  on the pure virtual.
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UNREFERENCED_PARAMETER (boundsDip);
    UNREFERENCED_PARAMETER (scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Paint(IDxuiPainter, ...). The
//  chrome shell drives this panel's bespoke Render via the
//  IChromedPanelContent path, which composes against its own owned
//  m_painter / m_text. The adapter is a no-op for the same reason
//  Layout above is: the unified Dxui dispatch path does not yet
//  reach the chrome-hosted panel, so this hook stays inert.
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);
}




