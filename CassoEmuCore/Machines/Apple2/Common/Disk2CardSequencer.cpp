#include "Pch.h"

#include "Machines/Apple2/Common/Disk2CardSequencer.h"
#include "Machines/Apple2/Common/Disk2Clock.h"
#include "Machines/Apple2/Common/Disk2Lines.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"
#include "Machines/Apple2/Common/Disk2Recorder.h"
#include "Machines/Apple2/Common/Disk2Signals.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"




//  The drive select, Q6 and Q7: the switches that change with the drives
//  not enabled.
static constexpr int  s_kFirstSelectSwitch = 0xA;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Disk2CardSequencer
//
////////////////////////////////////////////////////////////////////////////////

Disk2CardSequencer::Disk2CardSequencer (
    Disk2Lines     & lines,
    Disk2Drives    & drives,
    Disk2Signals   & signals,
    Disk2Recorder  & recorder,
    Disk2Clock     & clock) :
    m_lines    (lines),
    m_drives   (drives),
    m_signals  (signals),
    m_recorder (recorder),
    m_clock    (clock)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Read
//
//  The switch's side effects, then the data path Q6 and Q7 select. With the
//  drives not enabled, only the drive select, Q6 and Q7 change, and the read
//  returns 0.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2CardSequencer::Read (int offset, IDiskPortHook * hook)
{
    bool  isEnabled = false;



    CatchUpToCpu();

    isEnabled = PollFloppyEnable (hook);

    if (isEnabled || offset >= s_kFirstSelectSwitch)
    {
        HandleSwitch (offset);
    }

    return isEnabled ? HandleReadDispatch() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Write
//
//  The switch's side effects fire on a write too. With Q6 and Q7 on, the
//  byte loads the write latch.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::Write (int offset, Byte value, IDiskPortHook * hook)
{
    bool  isEnabled = false;



    CatchUpToCpu();

    isEnabled = PollFloppyEnable (hook);

    if (isEnabled || offset >= s_kFirstSelectSwitch)
    {
        HandleSwitch (offset);
    }

    if (isEnabled && m_lines.IsQ7() && m_lines.IsQ6())
    {
        GetActive().LoadWriteLatch (value, m_lines.IsMotorOn());
        m_recorder.NoteWrite (GetActive(), m_lines.GetActiveDrive(), value, m_lines.IsMotorOn());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::TryPeek
//
//  Q6, Q7 and the drive as they are after the switch, then the byte
//  HandleReadDispatch returns for those: the data latch, run forward to the
//  CPU's cycle on a copy of the drive; the sense line; or 0 with Q7 on.
//  False while the drive is writing, which a copy of it cannot run without
//  writing to the disk.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2CardSequencer::TryPeek (int offset, Byte & value)
{
    bool          q6           = m_lines.IsQ6();
    bool          q7           = m_lines.IsQ7();
    int           drive        = m_lines.GetActiveDrive();
    uint64_t      delta        = 0;
    bool          isSpinningUp = false;
    Byte          latch        = 0;
    Disk2Drive  * peeked       = nullptr;



    switch (offset)
    {
        case 0xA: drive = 0;     break;
        case 0xB: drive = 1;     break;
        case 0xC: q6    = false; break;
        case 0xD: q6    = true;  break;
        case 0xE: q7    = false; break;
        case 0xF: q7    = true;  break;
        default:                 break;
    }

    peeked = &m_drives[(size_t) drive];

    if (TryGetFixedByte (q6, q7, *peeked, value))
    {
        return true;
    }

    if (peeked->GetEngine().IsWriteMode())
    {
        return false;
    }

    if (drive == m_lines.GetActiveDrive())
    {
        delta = m_clock.GetPending();
    }

    //  A motor turned on from rest starts the spin-up, which reads $80.
    isSpinningUp = m_motorSpinupRemaining > 0 || (offset == 0x9 && !m_lines.IsMotorOn());
    latch        = isSpinningUp ? 0 : m_peek.Peek (peeked->GetEngine(), drive, m_clock.GetLastSync(), delta);
    value        = GetLatchByte (isSpinningUp, latch);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Tick
//
//  The motor's timers run off the instruction's cycles. The spin-up drains
//  first; the bit cursor moves through it, since the rotational position
//  must be right when the window closes. With a cycle source, each access
//  runs the drive on (CatchUpToCpu), so the drive moves here only without
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::Tick (uint32_t cpuCycles)
{
    if (m_motorSpinupRemaining > 0)
    {
        m_motorSpinupRemaining = (cpuCycles >= m_motorSpinupRemaining) ? 0 : m_motorSpinupRemaining - cpuCycles;
    }

    if (m_motorSpindownCycles > 0)
    {
        if (cpuCycles >= m_motorSpindownCycles)
        {
            StopMotor();
        }
        else
        {
            m_motorSpindownCycles -= cpuCycles;
        }
    }

    if (!m_clock.HasSource())
    {
        GetActive().GetEngine().Tick (cpuCycles);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::Reset()
{
    Restore (0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::IsMotorAtSpeed
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2CardSequencer::IsMotorAtSpeed() const
{
    return m_lines.IsMotorOn() && m_motorSpinupRemaining == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::Restore
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::Restore (uint32_t spindownCycles, uint32_t spinupRemaining)
{
    m_motorSpindownCycles  = spindownCycles;
    m_motorSpinupRemaining = spinupRemaining;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::PollFloppyEnable
//
//  Whether the drives are enabled for this access: always, with no port
//  hook. A drive whose enable drops stops at the next tick, without the
//  spin-down a motor-off command starts.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2CardSequencer::PollFloppyEnable (IDiskPortHook * hook)
{
    bool  isEnabled = hook == nullptr || hook->AreFloppiesEnabled();



    if (!isEnabled && m_lines.IsMotorOn())
    {
        m_motorSpindownCycles = 1;
    }

    return isEnabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::CatchUpToCpu
//
//  Runs the selected drive on to the CPU's bus cycle, so the switch handlers
//  see the drive where the CPU's time puts it.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::CatchUpToCpu()
{
    uint64_t  elapsed = 0;



    if (m_clock.TakeElapsed (elapsed) && elapsed > 0)
    {
        GetActive().GetEngine().Tick (static_cast<uint32_t> (elapsed));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::HandleSwitch
//
//  The sixteen switches: four phase magnets off and on, motor off and on,
//  drive 1 and 2, Q6 off and on, Q7 off and on.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::HandleSwitch (int offset)
{
    constexpr int  kMotorOff = 0x8;
    constexpr int  kMotorOn  = 0x9;



    m_switchAccesses++;

    if (offset < kMotorOff)
    {
        m_lines.SetPhase (offset >> 1, (offset & 1) != 0);
        return;
    }

    switch (offset)
    {
        case kMotorOff: CommandMotorOff(); break;
        case kMotorOn:  CommandMotorOn();  break;
        case 0xA:       SelectDrive (0);   break;
        case 0xB:       SelectDrive (1);   break;

        case 0xC:
            m_lines.SetQ6 (false);
            GetActive().GetEngine().SetShiftLoadMode (false);
            break;

        case 0xD:
            m_lines.SetQ6 (true);
            GetActive().GetEngine().SetShiftLoadMode (true);
            break;

        case 0xE: SetQ7 (false); break;
        case 0xF: SetQ7 (true);  break;
        default:                 break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::CommandMotorOff
//
//  The disk spins for a second after the command, so rotational sync
//  survives DOS RWTS retries and back-to-back commands (UTAIIe ch. 9).
//  The motor stops when the spin-down runs out (Tick). Every access is a
//  motor-off command for the sinks, one that changes nothing included.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::CommandMotorOff()
{
    if (m_lines.IsMotorOn() && m_motorSpindownCycles == 0)
    {
        m_motorSpindownCycles = kMotorSpindownCycles;
    }

    UpdateEngineWriteMode();
    m_signals.OnMotorCommand (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::CommandMotorOn
//
//  Cancels a spin-down, so the engine produces nibbles across the motor-off
//  and motor-on toggle DOS issues between sectors. Only a motor started
//  from rest is an engage, for the motor sound, and starts the spin-up:
//  during the spin-down the disk turns at 300 RPM. The drive
//  enabled now, the phases already on pull its head.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::CommandMotorOn()
{
    bool  isFromRest = !m_lines.IsMotorOn();



    m_motorSpindownCycles = 0;
    m_lines.SetMotorOn (true);
    GetActive().GetEngine().SetMotorOn (true);
    UpdateEngineWriteMode();

    if (isFromRest)
    {
        m_motorSpinupRemaining = kMotorSpinupCycles;
        m_lines.PullHead();
    }

    m_signals.OnMotorCommand (true);

    if (isFromRest)
    {
        m_signals.OnMotorEngaged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::SelectDrive
//
//  A drive selected while the motor runs is enabled at once, so the phases
//  on pull its head. The select goes to the sinks after the engines change.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::SelectDrive (int drive)
{
    m_lines.SelectDrive (drive);
    UpdateEngineSelection();

    if (m_lines.IsMotorOn())
    {
        m_lines.PullHead();
    }

    m_signals.OnDriveSelect (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::SetQ7
//
//  Q7 selects reading or writing. Writing turned on with a write-protected
//  disk in the drive sends the nibbles nowhere; RWTS checks the sense line
//  first, so only a program that does not check gets there.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::SetQ7 (bool isOn)
{
    if (isOn != m_lines.IsQ7())
    {
        m_signals.OnWriteMode (m_lines.GetActiveDrive(), isOn);
    }

    if (isOn && !m_lines.IsQ7() && GetActive().IsWriteProtected())
    {
        m_signals.OnWriteProtected();
    }

    m_lines.SetQ7 (isOn);
    UpdateEngineWriteMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::HandleReadDispatch
//
//  Q7 off, Q6 off: the data latch. Q7 off, Q6 on: the write-protect sense in
//  bit 7. Q7 on: 0, since the write path loads the latch through Write.
//
//  Each nibble the drive assembles goes to the recorder once, on the edge
//  where it arrives, not on every poll: the 6502 spins on LDA $C0EC, BPL
//  until bit 7 is set, and a sample per poll would flood the address-mark
//  watcher with repeats and half-assembled bytes. During the spin-up the CPU
//  reads $80, and the latch's nibble goes to the recorder, flagged.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2CardSequencer::HandleReadDispatch()
{
    Disk2Drive  & drive   = GetActive();
    Byte          result  = 0;
    Byte          nibble  = 0;
    uint8_t       fresh   = 0;
    bool          isFresh = false;



    if (TryGetFixedByte (m_lines.IsQ6(), m_lines.IsQ7(), drive, result))
    {
        return result;
    }

    nibble  = drive.GetEngine().ReadLatch();
    isFresh = drive.GetEngine().ConsumeFreshNibble (fresh);

    if (isFresh)
    {
        m_recorder.NoteRead (drive,
                             m_lines.GetActiveDrive(),
                             fresh,
                             (m_motorSpinupRemaining > 0) ? Disk2NibbleRecord::kSpinUp : 0,
                             m_clock.GetNow());
    }

    result = GetLatchByte (m_motorSpinupRemaining > 0, nibble);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::TryGetFixedByte
//
//  The byte a read returns when it isn't the data latch's, which a read and
//  a peek both select here: 0 with Q7 on, the write-protect sense in bit 7
//  with Q6 on. False with both off, for the latch.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2CardSequencer::TryGetFixedByte (bool q6, bool q7, const Disk2Drive & drive, Byte & value)
{
    value = 0;

    if (q7)
    {
        return true;
    }

    if (q6)
    {
        value = drive.IsWriteProtected() ? 0x80 : 0x00;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::StopMotor
//
//  The spin-down ran out: the motor stops, and only now does the motor
//  sound fade and the disengage go to the sinks.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::StopMotor()
{
    m_motorSpindownCycles = 0;
    m_lines.SetMotorOn (false);

    for (Disk2Drive & drive : m_drives)
    {
        drive.GetEngine().SetMotorOn (false);
        drive.EndRead();
    }

    m_signals.OnMotorDisengaged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::UpdateEngineSelection
//
//  The drive select changes which engine has the motor on. The other engine
//  stops; the selected one takes the motor, its drive's head position and
//  Q6. Each drive has a separate stepper, so selecting a drive moves
//  neither head.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::UpdateEngineSelection()
{
    Disk2Drive  & other = m_drives[(size_t) (m_lines.GetActiveDrive() ^ 1)];



    other.GetEngine().SetMotorOn (false);
    GetActive().GetEngine().SetMotorOn (m_lines.IsMotorOn());
    GetActive().GetEngine().SetCurrentTrack (GetActive().GetQuarterTrack());
    GetActive().GetEngine().SetShiftLoadMode (m_lines.IsQ6());
    UpdateEngineWriteMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::UpdateEngineWriteMode
//
//  Whether the selected drive takes the cells the data latch shifts out: Q7
//  on. A card's drive is enabled through the spin-down, so it writes there.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2CardSequencer::UpdateEngineWriteMode()
{
    GetActive().GetEngine().SetWriteMode (m_lines.IsQ7());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer::GetActive
//
////////////////////////////////////////////////////////////////////////////////

Disk2Drive & Disk2CardSequencer::GetActive()
{
    return m_lines.GetActive();
}
