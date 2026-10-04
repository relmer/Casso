#include "Pch.h"

#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetAttached
//
//  UI thread. Attaching needs no reset: the next button read answers from
//  the Joyport. Detaching hands every read straight back to the device that
//  owns the line, whose staged buttons and paddles were kept current all
//  along, so nothing is left held.
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SetAttached (bool attached)
{
    m_isAttached.store (attached, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAttached
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::IsAttached() const
{
    return m_isAttached.load (memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPaddlesConnected
//
//  UI thread. Whether something stands in the rear sockets, which the
//  hardware reads at the paddle inputs whichever jacks the front switch
//  selects: a player on Joystick or Paddle beside a player in a jack.
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SetPaddlesConnected (bool isConnected)
{
    m_arePaddlesConnected.store (isConnected, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetJackSwitches
//
//  UI thread, from the game-port sink. Written whether or not the Joyport is
//  attached, so attaching mid-game reads the switches as they are now.
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SetJackSwitches (size_t jack, JoystickSwitches switches)
{
    HRESULT  hr = S_OK;



    CBRA (jack < JoyportJacks::kJackCount);

    m_jacks[jack].store (switches.to_ulong(), memory_order_release);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineReset
//
//  CPU thread, after the CPU has taken its own reset, so a power cycle stamps
//  the counter it has just zeroed. Stamps even while detached, so attaching
//  during the window does not cut it short.
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::OnMachineReset()
{
    m_resetCycle    = (m_cycleSource != nullptr) ? *m_cycleSource : 0;
    m_hasResetStamp = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryReadButton
//
//  CPU thread. Returns false when the reading device should answer as if no
//  Joyport were attached; otherwise gives the selected switch in `value`,
//  bit 7 clear for closed. The annunciators are read now, not at any earlier
//  sample, since a game sets them and reads the buttons back to back.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::TryReadButton (int index, Byte & value) const
{
    HRESULT           hr          = S_OK;
    bool              isAnswering = false;
    bool              isReleased  = false;
    size_t            jack        = 0;
    JoystickSwitches  switches;
    JoystickSwitch    selected    = JoystickSwitch::Fire;



    CBRA (index >= 0 && index < kButtonCount);

    isReleased = !LoadAttached() || IsInResetWindow();
    BAIL_OUT_IF (isReleased, S_OK);

    jack     = IsAnnunciatorOn (kAnnunciatorJack) ? JoyportJacks::kRightJack : JoyportJacks::kLeftJack;
    switches = JoystickSwitches (LoadJack (jack));
    selected = GetSelectedSwitch (index);

    value       = switches.test (static_cast<size_t> (selected)) ? kSwitchClosed : kSwitchOpen;
    isAnswering = true;

Error:
    return isAnswering;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDrivingPaddles
//
//  An Atari joystick has no potentiometers, so while the Joyport is attached
//  with nothing in the rear sockets the paddle inputs read as nothing
//  connected, reset window or not. With something there they read it.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::IsDrivingPaddles() const
{
    return LoadAttached() && !LoadPaddlesConnected();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadAttached
//
//  CPU thread. The attached setting as a guest read sees it. With a journal
//  attached the value goes to ObserveSetting; the recording itself is kept
//  out of line, so with the journal off this costs one null test.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::LoadAttached() const
{
    bool  isAttached = m_isAttached.load (memory_order_acquire);



    if (m_inputJournal != nullptr)
    {
        ObserveSetting (InputLine::JoyportAttached, isAttached);
    }

    return isAttached;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadPaddlesConnected
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::LoadPaddlesConnected() const
{
    bool  isConnected = m_arePaddlesConnected.load (memory_order_acquire);



    if (m_inputJournal != nullptr)
    {
        ObserveSetting (InputLine::JoyportPaddlesConnected, isConnected);
    }

    return isConnected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadJack
//
////////////////////////////////////////////////////////////////////////////////

unsigned long SiriusJoyport::LoadJack (size_t jack) const
{
    unsigned long  switches = m_jacks[jack].load (memory_order_acquire);



    if (m_inputJournal != nullptr)
    {
        ObserveJack (jack, switches);
    }

    return switches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveSetting
//
//  Journal attached. Records the attached setting or the rear sockets when
//  another thread changed it since the last read that saw it.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void SiriusJoyport::ObserveSetting (InputLine line, bool isOn) const
{
    bool  & observed = (line == InputLine::JoyportAttached) ? m_observedAttached : m_observedPaddles;



    if (isOn != observed)
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::Button, isOn ? 1 : 0, static_cast<uint16_t> (line), 0);
        observed = isOn;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveJack
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void SiriusJoyport::ObserveJack (size_t jack, unsigned long switches) const
{
    if (switches != m_observedJacks[jack])
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::JoyportJack, 0, static_cast<uint16_t> (jack), switches);
        m_observedJacks[jack] = switches;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SampleHostInputs
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SampleHostInputs()
{
    size_t  jack = 0;



    ObserveSetting (InputLine::JoyportAttached,         m_isAttached.load          (memory_order_acquire));
    ObserveSetting (InputLine::JoyportPaddlesConnected, m_arePaddlesConnected.load (memory_order_acquire));

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        ObserveJack (jack, m_jacks[jack].load (memory_order_acquire));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputJournal
//
//  The settings at the moment of attaching are what a keyframe taken then
//  holds, so they count as seen.
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SetInputJournal (InputJournal * journal)
{
    m_inputJournal = journal;
    SyncObservedInputs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInput
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::ApplyInput (const InputRecord & record)
{
    bool  isOn      = record.value != 0;
    bool  isApplied = true;



    if (record.kind == InputKind::JoyportJack && record.detail < JoyportJacks::kJackCount)
    {
        m_jacks[record.detail].store (static_cast<unsigned long> (record.data), memory_order_release);
        m_observedJacks[record.detail] = static_cast<unsigned long> (record.data);
    }
    else if (record.kind == InputKind::Button && record.detail == static_cast<uint16_t> (InputLine::JoyportAttached))
    {
        m_isAttached.store (isOn, memory_order_release);
        m_observedAttached = isOn;
    }
    else if (record.kind == InputKind::Button && record.detail == static_cast<uint16_t> (InputLine::JoyportPaddlesConnected))
    {
        m_arePaddlesConnected.store (isOn, memory_order_release);
        m_observedPaddles = isOn;
    }
    else
    {
        isApplied = false;
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncObservedInputs
//
////////////////////////////////////////////////////////////////////////////////

void SiriusJoyport::SyncObservedInputs()
{
    size_t  jack = 0;



    m_observedAttached = m_isAttached.load          (memory_order_acquire);
    m_observedPaddles  = m_arePaddlesConnected.load (memory_order_acquire);

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        m_observedJacks[jack] = m_jacks[jack].load (memory_order_acquire);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInResetWindow
//
//  Emulated cycles, not host time, so the window is the firmware's at any
//  speed setting. The //e reads $C061 577 cycles after /RESET; the window is
//  several hundred times that.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::IsInResetWindow() const
{
    uint64_t  now = (m_cycleSource != nullptr) ? *m_cycleSource : m_resetCycle;



    return m_hasResetStamp && (now - m_resetCycle) < kReleaseCycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAnnunciatorOn
//
//  No bank wired reads as every annunciator off, the power-on state.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::IsAnnunciatorOn (int index) const
{
    return m_annunciatorSource != nullptr && m_annunciatorSource->IsAnnunciatorOn (index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedSwitch
//
//  PB0 is always fire. PB1 and PB2 carry left and right with AN1 off, and up
//  and down with AN1 on.
//
////////////////////////////////////////////////////////////////////////////////

JoystickSwitch SiriusJoyport::GetSelectedSwitch (int index) const
{
    bool            isVertical = IsAnnunciatorOn (kAnnunciatorDirection);
    JoystickSwitch  selected   = JoystickSwitch::Fire;



    if (index == kButtonLeftOrUp)
    {
        selected = isVertical ? JoystickSwitch::Up : JoystickSwitch::Left;
    }
    else if (index != kButtonFire)
    {
        selected = isVertical ? JoystickSwitch::Down : JoystickSwitch::Right;
    }

    return selected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SiriusJoyport::SaveState (StateWriter & writer) const
{
    size_t  jack = 0;



    writer.BeginSection (kStateTag, kStateVersion);
    writer.WriteUInt32  (static_cast<uint32_t> (JoyportJacks::kJackCount));

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        writer.WriteUInt32 (static_cast<uint32_t> (m_jacks[jack].load (memory_order_acquire)));
    }

    writer.WriteUInt64 (m_resetCycle);
    writer.WriteBool   (m_hasResetStamp);
    writer.WriteBool   (m_isAttached.load          (memory_order_acquire));
    writer.WriteBool   (m_arePaddlesConnected.load (memory_order_acquire));

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  A saved jack holding a switch bit this build does not define is rejected.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SiriusJoyport::LoadState (StateReader & reader)
{
    constexpr uint32_t  kSwitchMask                     = (1u << static_cast<uint32_t> (JoystickSwitch::Count)) - 1;
    HRESULT             hr                              = S_OK;
    uint16_t            version                         = 0;
    uint32_t            jackCount                       = 0;
    uint32_t            jacks[JoyportJacks::kJackCount] = {};
    size_t              jack                            = 0;
    bool                isAttached                      = m_isAttached.load (memory_order_acquire);
    bool                arePaddlesConnected             = m_arePaddlesConnected.load (memory_order_acquire);



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt32 (jackCount);
    CBREx (jackCount == JoyportJacks::kJackCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        reader.ReadUInt32 (jacks[jack]);
    }

    reader.ReadUInt64 (m_resetCycle);
    reader.ReadBool   (m_hasResetStamp);

    if (version >= kAttachedVersion)
    {
        reader.ReadBool (isAttached);
        reader.ReadBool (arePaddlesConnected);
    }

    hr = reader.EndSection();
    CHR (hr);

    m_isAttached.store          (isAttached,          memory_order_release);
    m_arePaddlesConnected.store (arePaddlesConnected, memory_order_release);

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        CBREx ((jacks[jack] & ~kSwitchMask) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        m_jacks[jack].store (jacks[jack], memory_order_release);
    }

    SyncObservedInputs();

Error:
    return hr;
}
