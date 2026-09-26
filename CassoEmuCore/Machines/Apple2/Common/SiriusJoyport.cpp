#include "Pch.h"

#include "Machines/Apple2/Common/SiriusJoyport.h"
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

    isReleased = !IsAttached() || IsInResetWindow();
    BAIL_OUT_IF (isReleased, S_OK);

    jack     = IsAnnunciatorOn (kAnnunciatorJack) ? JoyportJacks::kRightJack : JoyportJacks::kLeftJack;
    switches = JoystickSwitches (m_jacks[jack].load (memory_order_acquire));
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
//  the paddle inputs read as nothing connected, reset window or not.
//
////////////////////////////////////////////////////////////////////////////////

bool SiriusJoyport::IsDrivingPaddles() const
{
    return IsAttached();
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
