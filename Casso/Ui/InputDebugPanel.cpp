#include "Pch.h"

#include "InputDebugPanel.h"

#include "Chrome/CassoTheme.h"
#include "Core/DxuiAbsoluteLayout.h"
#include "Window/DxuiHostWindow.h"





constexpr LPCWSTR  s_kpszClassName   = L"Casso.InputDebug.Panel";
constexpr LPCWSTR  s_kpszWindowTitle = L"Casso - Input events";

constexpr int      s_kPreferredWidthDip  = 960;
constexpr int      s_kPreferredHeightDip = 600;
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
    // The content widgets are adopted into the host window's root panel
    // (not into this panel) in Create() once the host exists, so the
    // host paint pump walks and paints them. The constructor only seeds
    // the Uptime anchor; every other member default-initializes.
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
//  Stands up a full-ownership DxuiHostWindow (borderless chrome, close-
//  only caption, host-owned swap chain / paint pump), installs this
//  panel as its IDxuiHostClient, and adopts every content widget into
//  the host root so the host paint pump paints them. Idempotent -- a
//  second call while already open is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme    * theme)
{
    HRESULT                       hr        = S_OK;
    DxuiHostWindow::CreateParams  params;
    RECT                          clientRc  = {};


    BAIL_OUT_IF (m_host != nullptr, S_OK);

    CBRAEx (hInstance, E_INVALIDARG);
    CBRAEx (device,    E_INVALIDARG);
    CBRAEx (context,   E_INVALIDARG);

    m_device  = device;
    m_context = context;
    m_theme   = theme;

    params.title             = s_kpszWindowTitle;
    params.hInstance         = hInstance;
    params.ownerHwnd         = hwndOwner;
    params.borderless        = true;
    params.resizable         = true;
    params.roundedCorners    = true;
    params.darkMode          = true;
    params.createSwapChain   = true;
    params.captionStyle      = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride = s_kpszClassName;
    params.initialSizeDip    = { s_kPreferredWidthDip, s_kPreferredHeightDip };

    m_host = std::make_unique<DxuiHostWindow>();
    m_host->SetClient (this);

    hr = m_host->Create (params);
    CHRF (hr, m_host.reset());

    m_hwnd = m_host->Hwnd();
    m_dpi  = m_host->Scaler().Dpi();

    // Adopt every content widget into the host root so the host paint
    // pump walks and paints them. The tooltip and column menu are NOT
    // adopted -- they render through the host popup pool so they can
    // escape the client rect.
    m_host->Root().SetLayout (std::make_unique<DxuiAbsoluteLayout>());
    m_host->Root().Adopt (m_emuLabel);
    m_host->Root().Adopt (m_hostLabel);
    for (DxuiLabel & label : m_pairLabel)
    {
        m_host->Root().Adopt (label);
    }

    m_host->Root().Adopt (m_allCheck);
    m_host->Root().Adopt (m_emuKeyboardCheck);
    m_host->Root().Adopt (m_joystickCheck);
    m_host->Root().Adopt (m_paddleCheck);
    m_host->Root().Adopt (m_hostKeyboardCheck);
    for (DxuiDropdown & dropdown : m_pairView)
    {
        m_host->Root().Adopt (dropdown);
    }

    m_host->Root().Adopt (m_pauseButton);
    m_host->Root().Adopt (m_clearButton);
    m_host->Root().Adopt (m_copyButton);
    m_host->Root().Adopt (m_eventList);

    m_host->SetTheme (m_theme);

    m_columnMenu.SetPopupHost (m_host.get());
    m_tooltip.SetPopupHost (m_host.get());

    ConfigureWidgets();

    if (GetClientRect (m_hwnd, &clientRc))
    {
        m_widthPx  = std::max (1, (int) (clientRc.right  - clientRc.left));
        m_heightPx = std::max (1, (int) (clientRc.bottom - clientRc.top));
    }

    RecomputeLayout();

    ShowWindow (m_hwnd, SW_SHOWNORMAL);
    SetForegroundWindow (m_hwnd);

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
    HWND  hwnd = (m_host != nullptr) ? m_host->Hwnd() : nullptr;



    if (hwnd == nullptr)
    {
        return;
    }

    ShowWindow (hwnd, IsIconic (hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow (hwnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::Hide()
{
    HWND  hwnd = (m_host != nullptr) ? m_host->Hwnd() : nullptr;



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
    m_host.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
//  Public per-frame entry point invoked by the EmulatorShell render
//  loop. Drains the event ring into the display rows, advances the
//  list / tooltip timers, then invalidates the host window so its
//  WM_PAINT pump repaints the adopted widget tree.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputDebugPanel::RenderFrame()
{
    HRESULT  hr  = S_OK;
    int64_t  now = NowMs();



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    DrainAndProject();

    // Drive scrollbar auto-repeat for any held arrow / track press and
    // the tooltip open / close dwell timers.
    m_eventList.Tick (now);
    m_tooltip.Tick   (now);

    InvalidateRect (m_host->Hwnd(), nullptr, FALSE);
    UpdateWindow   (m_host->Hwnd());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::SetTheme (const CassoTheme * theme)
{
    m_theme = theme;
    if (m_host != nullptr)
    {
        m_host->SetTheme (m_theme);
    }

    m_focusMgr.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  Captures the mouse and takes focus so a drag that begins on a
//  scrollbar thumb or a column-resize handle keeps receiving moves once
//  the cursor leaves the client, then routes the press to OnMouse. The
//  host does no capture / focus bookkeeping of its own, so the panel
//  owns it here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    int  x = (int) (short) LOWORD (lParam);
    int  y = (int) (short) HIWORD (lParam);



    UNREFERENCED_PARAMETER (wParam);

    if (m_host != nullptr)
    {
        SetCapture (m_host->Hwnd());
        SetFocus   (m_host->Hwnd());
    }

    return DispatchClientMouse (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Releases the drag capture taken on button-down and routes the
//  release to OnMouse.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    int  x = (int) (short) LOWORD (lParam);
    int  y = (int) (short) HIWORD (lParam);



    UNREFERENCED_PARAMETER (wParam);

    ReleaseCapture();

    return DispatchClientMouse (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonDown
//
//  Takes focus and routes the secondary press to OnMouse, which raises
//  the column-header context menu.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnRButtonDown (WPARAM wParam, LPARAM lParam)
{
    int  x = (int) (short) LOWORD (lParam);
    int  y = (int) (short) HIWORD (lParam);



    UNREFERENCED_PARAMETER (wParam);

    if (m_host != nullptr)
    {
        SetFocus (m_host->Hwnd());
    }

    return DispatchClientMouse (DxuiMouseEventKind::Down, DxuiMouseButton::Right, x, y, 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    int  x = (int) (short) LOWORD (lParam);
    int  y = (int) (short) HIWORD (lParam);



    UNREFERENCED_PARAMETER (wParam);

    return DispatchClientMouse (DxuiMouseEventKind::Move, DxuiMouseButton::None, x, y, 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
//  WM_MOUSEWHEEL reports the point in SCREEN coordinates, so map it back
//  to client px before dispatch. The signed notch count is normalized to
//  wheel notches (+1 per notch up) to match the DxuiMouseEvent contract.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    POINT  pt         = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    float  wheelDelta = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;



    UNREFERENCED_PARAMETER (horizontal);

    if (m_host != nullptr)
    {
        ScreenToClient (m_host->Hwnd(), &pt);
    }

    return DispatchClientMouse (DxuiMouseEventKind::Wheel, DxuiMouseButton::None, pt.x, pt.y, wheelDelta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return DispatchClientKey (DxuiKeyEventKind::Down, vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnChar (WPARAM ch, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    return DispatchClientKey (DxuiKeyEventKind::Char, ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
//  Shows the horizontal-resize cursor while a column drag is live or the
//  cursor is parked on a header-edge resize handle; otherwise defers to
//  the host. Only plain client area is reclassified -- NC areas (resize
//  edges, caption) keep the host's own cursor handling.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnSetCursor (WORD hitTest)
{
    POINT  cursor = {};
    int    relX   = 0;
    int    relY   = 0;


    if (hitTest != HTCLIENT) { return DxuiMessageResult::NotHandled; }

    if (m_host == nullptr || GetCursorPos (&cursor) == FALSE)
    {
        return DxuiMessageResult::NotHandled;
    }

    ScreenToClient (m_host->Hwnd(), &cursor);
    relX = cursor.x - m_layout.listView.left;
    relY = cursor.y - m_layout.listView.top;

    if (m_eventList.IsResizingColumn() ||
        m_eventList.HitTestColumnResize (relX, relY, 4) >= 0)
    {
        SetCursor (LoadCursorW (nullptr, IDC_SIZEWE));
        return DxuiMessageResult::Handled;
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
//  Fires after the host has finished its own layout response; caches the
//  final client size and DPI, then re-runs the panel's layout so the
//  adopted widgets track the new bounds.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnSize (UINT widthPx, UINT heightPx)
{
    m_widthPx  = std::max (1, (int) widthPx);
    m_heightPx = std::max (1, (int) heightPx);
    if (m_host != nullptr)
    {
        m_dpi = m_host->Scaler().Dpi();
    }

    RecomputeLayout();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnGetMinMax
//
//  Clamps the OS minimum track size to the panel's preferred client size
//  scaled to the current DPI. The window is borderless, so client size
//  and window size coincide.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnGetMinMax (MINMAXINFO * info)
{
    if (info == nullptr) { return DxuiMessageResult::NotHandled; }

    info->ptMinTrackSize.x = MulDiv (s_kPreferredWidthDip,  (int) m_dpi, 96);
    info->ptMinTrackSize.y = MulDiv (s_kPreferredHeightDip, (int) m_dpi, 96);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
//  Non-modal: the close box hides the window and keeps the HWND (and the
//  filter / event-ring state) alive so EmulatorShell can re-Show it.
//  Consumes the close so DefWindowProc never destroys the window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::OnClose()
{
    Hide();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
//  Releases any live popup back to the pool and drops host-derived
//  pointers before the host (and its popup pool) tear down. Does NOT
//  call PostQuitMessage -- this is a secondary window.
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnDestroy()
{
    m_columnMenu.Hide();
    m_columnMenu.SetPopupHost (nullptr);

    m_tooltip.HideImmediate();
    m_tooltip.SetPopupHost (nullptr);

    m_hwnd = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDpiChanged
//
//  Fires after the host has applied the OS-suggested rect; refreshes the
//  cached DPI and re-runs layout so the DPI-scaled slots track the new
//  scale.
//
////////////////////////////////////////////////////////////////////////////////

void InputDebugPanel::OnDpiChanged (UINT newDpi)
{
    m_dpi = newDpi;
    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchClientMouse
//
//  Builds a DxuiMouseEvent from client-px coordinates plus the live
//  modifier-key state and routes it through the panel's OnMouse, mapping
//  the bool result onto the host client Handled / NotHandled contract.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::DispatchClientMouse (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta)
{
    DxuiMouseEvent  ev;



    ev.kind        = kind;
    ev.button      = button;
    ev.positionDip = { x, y };
    ev.wheelDelta  = wheelDelta;
    ev.shift       = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl        = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt         = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    return this->OnMouse (ev) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchClientKey
//
//  Builds a DxuiKeyEvent from a virtual-key / character code plus the
//  live modifier-key state and routes it through the panel's OnKey,
//  mapping the bool result onto the host client Handled / NotHandled
//  contract.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult InputDebugPanel::DispatchClientKey (DxuiKeyEventKind kind, WPARAM code)
{
    DxuiKeyEvent  ev;



    ev.kind  = kind;
    ev.vk    = code;
    ev.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    ev.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    ev.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    return this->OnKey (ev) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
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
    m_focusMgr.Attach  (&m_host->Root());
    m_focusMgr.SetTheme (m_theme);
    m_focusMgr.Rebuild();
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


    if (m_host != nullptr)
    {
        topOffset = m_host->CaptionHeightPx();
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
    m_focusMgr.Rebuild();
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
                    if (!m_pairView[0].IsOpen()) { m_focusMgr.SetFocused (&m_pairView[0]); }
                    return true;
                }
                if (m_pairView[1].OnMouse (ev))
                {
                    if (!m_pairView[1].IsOpen()) { m_focusMgr.SetFocused (&m_pairView[1]); }
                    return true;
                }

                if (m_pauseButton.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_pauseButton);
                    return true;
                }

                if (m_clearButton.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_clearButton);
                    return true;
                }

                if (m_copyButton.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_copyButton);
                    return true;
                }

                if (m_allCheck.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_allCheck);
                    return true;
                }

                if (m_emuKeyboardCheck.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_emuKeyboardCheck);
                    return true;
                }

                if (m_joystickVisible && m_joystickCheck.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_joystickCheck);
                    return true;
                }

                if (m_paddleVisible && m_paddleCheck.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_paddleCheck);
                    return true;
                }

                if (m_hostKeyboardCheck.OnMouse (ev))
                {
                    m_focusMgr.SetFocused (&m_hostKeyboardCheck);
                    return true;
                }

                if ((x >= m_layout.listView.left && x < m_layout.listView.right && y >= m_layout.listView.top && y < m_layout.listView.bottom))
                {
                    // The list owns all in-list routing (scrollbar arrows /
                    // thumb / track, column resize, header-click sort, row
                    // select) via OnMouse and reports outcomes through the
                    // callbacks wired at setup. OnLButtonDown holds the
                    // Win32 capture for the full press, so any drag the
                    // list starts keeps receiving moves after the cursor
                    // leaves the client.
                    (void) ForwardMouseToList (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, 0.0f);
                    m_focusMgr.SetFocused (&m_eventList);
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
                // unconditionally. OnLButtonUp releases the Win32 capture
                // before routing this release.
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
    IDxuiControl *  focused  = nullptr;


    // Char events carry no panel semantics (no text entry surface); only
    // key-down drives focus traversal, activation and list navigation.
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    // Tab / Shift+Tab: automatic focus traversal over the panel's visible
    // focusables (the framework tree walk) -- no bespoke per-stop list.
    if (vk == VK_TAB)
    {
        m_focusMgr.HandleKey ((GetKeyState (VK_SHIFT) & 0x8000) ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
        return true;
    }

    // Activation / value keys go to the focused control first: a focused
    // checkbox / button self-activates on Space / Enter (firing its wired
    // callback) and a focused dropdown steers its open popup.
    focused = m_focusMgr.Focused();
    if (focused != nullptr && focused->OnKey (ev))
    {
        return true;
    }

    switch (vk)
    {
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
    IDxuiTextRenderer          *  textRenderer = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    RECT                         hostRect = { 0, 0, m_widthPx, m_heightPx };
    int                          i        = 0;


    // The host owns no paint pump in adopt / synthetic mode, so it exposes
    // no text renderer to measure / lay the menu out with -- bail rather
    // than dereference a null renderer.
    if (textRenderer == nullptr)
    {
        return;
    }

    items.reserve (kInputColumnCount);
    for (i = 0; i < kInputColumnCount; i++)
    {
        DxuiPopupMenu::Item  item;

        item.label   = columns[i].headerText;
        item.checked = columns[i].visible;
        items.push_back (std::move (item));
    }

    m_columnMenu.Show (anchorX, anchorY, std::move (items), *textRenderer, hostRect);
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





