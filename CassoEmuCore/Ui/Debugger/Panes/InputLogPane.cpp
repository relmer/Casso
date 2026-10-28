#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/IoEventLog.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Ui/Debugger/Panes/InputLogPane.h"
#include "Debugger/IoEventLog.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetLaneShown
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::SetLaneShown (Lane lane, bool isShown)
{
    m_lanes[lane] = isShown;
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPairJoystick
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::SetPairJoystick (int pair, bool isJoystick)
{
    m_isPairJoystick[(size_t) (pair & 1)] = isJoystick;
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseButtonAtC063
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::SetMouseButtonAtC063 (bool isMouseButton)
{
    if (isMouseButton != m_isMouseButtonAtC063)
    {
        m_isMouseButtonAtC063 = isMouseButton;
        MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOptionsText
//
//  lanes=11111;pairs=11;columnLayout=101111 0,0,0,0,0,0
//
////////////////////////////////////////////////////////////////////////////////

std::string InputLogPane::GetOptionsText() const
{
    std::string  lanes;



    for (bool isShown : m_lanes)
    {
        lanes += isShown ? '1' : '0';
    }

    return std::format ("lanes={};pairs={}{};columnLayout={}", lanes, m_isPairJoystick[0] ? 1 : 0, m_isPairJoystick[1] ? 1 : 0, GetColumnsText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOptionsText
//
//  Fields missing or not understood are left as they were.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::SetOptionsText (const std::string & text)
{
    size_t       start = 0;
    size_t       end   = 0;
    size_t       equal = 0;
    size_t       i     = 0;
    std::string  field;
    std::string  key;
    std::string  value;



    while (start < text.size())
    {
        end   = text.find (';', start);
        end   = (end == std::string::npos) ? text.size() : end;
        field = text.substr (start, end - start);
        equal = field.find ('=');
        key   = field.substr (0, equal);
        value = (equal == std::string::npos) ? std::string() : field.substr (equal + 1);

        if (key == "lanes")
        {
            for (i = 0; i < value.size() && i < kLaneCount; i++)
            {
                m_lanes[i] = value[i] == '1';
            }
        }
        else if (key == "pairs")
        {
            for (i = 0; i < value.size() && i < m_isPairJoystick.size(); i++)
            {
                m_isPairJoystick[i] = value[i] == '1';
            }
        }
        else if (key == "columnLayout")
        {
            SetColumnsText (value);
        }

        start = end + 1;
    }

    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLaneLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputLogPane::GetLaneLabel (Lane lane)
{
    static constexpr const wchar_t * kLabels[kLaneCount] = { L"Keyboard", L"Joystick", L"Paddle", L"Keyboard", L"Mouse" };



    return (lane < kLaneCount) ? kLabels[lane] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLaneTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputLogPane::GetLaneTip (Lane lane)
{
    static constexpr const wchar_t * kTips[kLaneCount] =
    {
        L"List the guest's keyboard soft-switch reads ($C000, $C010)",
        L"List game-port reads for pairs viewed as a joystick",
        L"List game-port reads for pairs viewed as paddles",
        L"List the host's keys, auto-repeat and pastes",
        L"List the host's mouse moves and buttons",
    };



    return (lane < kLaneCount) ? kTips[lane] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGamePort
//
//  The game port has two pairs, and an event's pair is given differently by
//  type: a host axis by its index, a guest read by its soft switch, a host
//  button by its line. The //e's Shift key reads through button 2, so it is
//  pair 1's. The PTRIG strobe arms both pairs at once. Anything else is
//  neither pair.
//
////////////////////////////////////////////////////////////////////////////////

InputLogPane::GamePort InputLogPane::GetGamePort (const InputEvent & event)
{
    Word      address = event.payload.io.address;
    GamePort  port    = GamePort::None;



    switch (event.type)
    {
        case InputEventType::HostPaddle:
            port = (address < kAxesPerPair) ? GamePort::Pair0 : (address < kAxisCount) ? GamePort::Pair1 : GamePort::None;
            break;

        case InputEventType::HostButton:
            switch ((InputLine) address)
            {
                case InputLine::OpenApple:
                case InputLine::ClosedApple:
                case InputLine::GamePortButton0:
                case InputLine::GamePortButton1:  port = GamePort::Pair0; break;
                case InputLine::Shift:
                case InputLine::GamePortButton2:  port = GamePort::Pair1; break;
                default:                                                  break;
            }

            break;

        case InputEventType::ButtonRead:
            port = (address == kOpenApple || address == kClosedApple) ? GamePort::Pair0 : (address == kButton2) ? GamePort::Pair1 : GamePort::None;
            break;

        case InputEventType::PaddleRead:
            port = (address == kPaddle0 || address == kPaddle1) ? GamePort::Pair0 : (address == kPaddle2 || address == kPaddle3) ? GamePort::Pair1 : GamePort::None;
            break;

        case InputEventType::PaddleTrigger:
            port = GamePort::Both;
            break;

        default:
            break;
    }

    return port;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputLogPane::GetLineName (uint16_t line)
{
    const wchar_t  * name = L"?";



    switch ((InputLine) line)
    {
        case InputLine::OpenApple:                name = L"Open Apple";      break;
        case InputLine::ClosedApple:              name = L"Closed Apple";    break;
        case InputLine::Shift:                    name = L"Shift";           break;
        case InputLine::EightyColumnSwitch:       name = L"80/40 switch";    break;
        case InputLine::GamePortButton0:          name = L"BTN0";            break;
        case InputLine::GamePortButton1:          name = L"BTN1";            break;
        case InputLine::GamePortButton2:          name = L"BTN2";            break;
        case InputLine::JoyportAttached:          name = L"Joyport";         break;
        case InputLine::JoyportPaddlesConnected:  name = L"Joyport paddles"; break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSwitchLine
//
//  A line that stays where it is set, ON or OFF, rather than a button
//  pressed and let go.
//
////////////////////////////////////////////////////////////////////////////////

bool InputLogPane::IsSwitchLine (uint16_t line)
{
    InputLine  each = (InputLine) line;



    return each == InputLine::EightyColumnSwitch || each == InputLine::JoyportAttached || each == InputLine::JoyportPaddlesConnected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatEvent
//
//  Host axes are labeled by the pair's view: X and Y for a pair viewed as a
//  joystick, a paddle number each for one viewed as paddles. A button reads as
//  pressed when bit 7 is set, but the //c's mouse button at $C063 reads as
//  pressed when it is clear.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::FormatEvent (
    const InputEvent  & event,
    std::wstring      & outAddress,
    std::wstring      & outValue,
    std::wstring      & outMeaning) const
{
    Word      address = event.payload.io.address;
    uint8_t   value   = event.payload.io.value;
    uint8_t   flags   = event.payload.io.flags;
    int       axis    = 0;
    int       pair    = 0;
    bool      isDown  = false;
    bool      isMouse = false;



    outAddress.clear();
    outValue.clear();
    outMeaning.clear();

    switch (event.type)
    {
        case InputEventType::HostKeyDown:
            outValue   = EventLogText::FormatByteChar (event.payload.key.ascii);
            outMeaning = L"Key down: " + outValue;
            break;

        case InputEventType::HostKeyUp:
            outMeaning = L"Keys up";
            break;

        case InputEventType::HostAutoRepeat:
            outValue   = EventLogText::FormatByteChar (event.payload.key.ascii);
            outMeaning = L"Auto-repeat: " + outValue;
            break;

        case InputEventType::HostPaste:
            outValue   = EventLogText::FormatByteChar (event.payload.key.ascii);
            outMeaning = L"Paste: " + outValue;
            break;

        case InputEventType::HostPaddle:
            axis = address;
            pair = axis / kAxesPerPair;

            if (IsPairJoystick (pair))
            {
                outAddress = std::format (L"JOY{} {}", pair, (axis % kAxesPerPair == 0) ? L"X" : L"Y");
            }
            else
            {
                outAddress = std::format (L"PDL{}", axis);
            }

            outValue   = std::format (L"{}", value);
            outMeaning = std::format (L"HOST {} = {}", outAddress, value);
            break;

        case InputEventType::HostButton:
            outAddress = GetLineName (address);
            outValue   = IsSwitchLine (address) ? ((value != 0) ? L"ON" : L"OFF") : ((value != 0) ? L"DOWN" : L"UP");
            outMeaning = std::format (L"HOST {} {}", outAddress, outValue);
            break;

        case InputEventType::HostMouseMove:
            outAddress = L"Mouse";
            outValue   = std::format (L"{}, {}", event.payload.motion.dx, event.payload.motion.dy);
            outMeaning = L"HOST mouse moved " + outValue;
            break;

        case InputEventType::HostMouseButton:
            outAddress = L"Mouse";
            outValue   = (value != 0) ? L"DOWN" : L"UP";
            outMeaning = L"HOST mouse button " + outValue;
            break;

        case InputEventType::HostSerialByte:
            outAddress = TextEncoding::NarrowToWide (IoEventLog::GetSerialPortName ((Byte) address));
            outValue   = EventLogText::FormatByteChar (value);
            outMeaning = L"HOST serial byte " + outValue + TextEncoding::NarrowToWide (IoEventLog::FormatReceiveErrors (flags));
            break;

        case InputEventType::HostSerialLines:
            outAddress = TextEncoding::NarrowToWide (IoEventLog::GetSerialPortName ((Byte) address));
            outValue   = TextEncoding::NarrowToWide (IoEventLog::FormatSerialLines (value));
            outMeaning = L"HOST serial lines " + outValue;
            break;

        case InputEventType::HostSerialLink:
            outAddress = TextEncoding::NarrowToWide (IoEventLog::GetSerialPortName ((Byte) address));
            outValue   = (value != 0) ? L"OPENED" : L"CLOSED";
            outMeaning = (value != 0) ? L"HOST serial connection opened" : L"HOST serial connection closed";
            break;

        case InputEventType::KbdDataRead:
            outAddress = std::format (L"${:04X}", address);
            outValue   = std::format (L"${:02X}", value);
            outMeaning = std::format (L"Read {} -> {}  key='{}' strobe={}", outAddress, outValue, EventLogText::GetPrintable (value),
                                      ((flags & InputEvent::kFlagStrobe) != 0) ? 1 : 0);
            break;

        case InputEventType::KbdStrobe:
            outAddress = std::format (L"${:04X}", address);
            outValue   = std::format (L"${:02X}", value);
            outMeaning = std::format (L"Clear strobe {} -> {}  AKD={} cleared={}", outAddress, outValue,
                                      ((flags & InputEvent::kFlagAnyKeyDown) != 0) ? 1 : 0, ((flags & InputEvent::kFlagStrobe) != 0) ? 1 : 0);
            break;

        case InputEventType::ButtonRead:
            isMouse    = address == kButton2 && m_isMouseButtonAtC063;
            isDown     = isMouse ? (value & kHighBit) == 0 : (value & kHighBit) != 0;
            outAddress = std::format (L"${:04X}", address);
            outValue   = std::format (L"${:02X}", value);
            outMeaning = std::format (L"Button read {} -> {}  pressed={}{}", outAddress, outValue, isDown ? 1 : 0,
                                      isMouse                  ? L"  mouse button"
                                      : address == kOpenApple   ? L"  Open Apple / button 0"
                                      : address == kClosedApple ? L"  Closed Apple / button 1"
                                      : address == kButton2     ? L"  Shift / button 2"
                                                                : L"");
            break;

        case InputEventType::PaddleTrigger:
            outAddress = std::format (L"${:04X}", address);
            outMeaning = std::format (L"PTRIG strobe {}  (arm game-port timers)", outAddress);
            break;

        case InputEventType::PaddleRead:
            outAddress = std::format (L"${:04X}", address);
            outValue   = std::format (L"${:02X}", value);
            outMeaning = std::format (L"Read PADDL{} {} -> {}  counting={}", address - kPaddle0, outAddress, outValue, ((value & kHighBit) != 0) ? 1 : 0);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnSpecs
//
////////////////////////////////////////////////////////////////////////////////

InputLogPane::ColumnSpecs InputLogPane::GetColumnSpecs() const
{
    return { { L"Time",      DxuiTextHAlign::Left, kFit, true  },
             { L"Cycle",     DxuiTextHAlign::Right, 110, false },
             { L"Source",    DxuiTextHAlign::Left,   60, true  },
             { L"Address",   DxuiTextHAlign::Left,   90, true  },
             { L"Value",     DxuiTextHAlign::Left,   80, true  },
             { L"Meaning",   DxuiTextHAlign::Left,    0, true  } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrepareView
//
//  Walks the rows from `from` on newest first, noting whether the next
//  paddle row is a read, so each PTRIG strobe is marked when a reported read
//  follows it.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::PrepareView (
    const std::vector<Row>  & rows,
    size_t                    from)
{
    bool    isReadNext = false;
    size_t  i          = 0;



    m_isTriggerRead.resize (rows.size(), false);
    std::fill (m_isTriggerRead.begin() + (ptrdiff_t) std::min (from, rows.size()), m_isTriggerRead.end(), false);

    for (i = rows.size(); i > from; i--)
    {
        if (rows[i - 1].kind != DebugLogRowKind::Event)
        {
            continue;
        }

        if (rows[i - 1].event.type == InputEventType::PaddleRead)
        {
            isReadNext = true;
        }
        else if (rows[i - 1].event.type == InputEventType::PaddleTrigger)
        {
            m_isTriggerRead[i - 1] = isReadNext;
            isReadNext             = false;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSettledCount
//
//  A PTRIG strobe is listed when a read follows it, so the last paddle row,
//  when it is a strobe, waits on the rows after it.
//
////////////////////////////////////////////////////////////////////////////////

size_t InputLogPane::GetSettledCount (
    const std::vector<Row>  & rows,
    size_t                    count) const
{
    size_t  i = 0;



    for (i = count; i > 0; i--)
    {
        if (rows[i - 1].kind != DebugLogRowKind::Event)
        {
            continue;
        }

        if (rows[i - 1].event.type == InputEventType::PaddleRead)
        {
            return count;
        }

        if (rows[i - 1].event.type == InputEventType::PaddleTrigger)
        {
            return i - 1;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Matches
//
//  A game-port event is listed by its pair's view's filter; the PTRIG
//  strobe by either filter, when a reported read follows it. Anything else
//  is listed by its source's keyboard filter, or the host's mouse filter.
//
////////////////////////////////////////////////////////////////////////////////

bool InputLogPane::Matches (
    const Row  & row,
    size_t       index) const
{
    const InputEvent  & event   = row.event;
    bool                isHost  = event.category == InputEventCategory::Host;
    bool                isMouse = event.type == InputEventType::HostMouseMove || event.type == InputEventType::HostMouseButton;
    bool                isShown = true;



    switch (GetGamePort (event))
    {
        case GamePort::None:
            isShown = isMouse ? m_lanes[kHostMouse] : m_lanes[isHost ? kHostKeyboard : kGuestKeyboard];
            break;

        case GamePort::Both:
            isShown = (m_lanes[kJoystick] || m_lanes[kPaddle]) && index < m_isTriggerRead.size() && m_isTriggerRead[index];
            break;

        case GamePort::Pair0:
            isShown = m_lanes[IsPairJoystick (0) ? kJoystick : kPaddle];
            break;

        case GamePort::Pair1:
            isShown = m_lanes[IsPairJoystick (1) ? kJoystick : kPaddle];
            break;
    }

    return isShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeCells
//
////////////////////////////////////////////////////////////////////////////////

void InputLogPane::MakeCells (
    const Row  & row,
    Cells      & out) const
{
    out[kSource].text = (row.event.category == InputEventCategory::Host) ? L"Host" : L"Guest";

    FormatEvent (row.event, out[kAddress].text, out[kValue].text, out[kMeaning].text);
}
