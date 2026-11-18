#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/Disk2Clock.h"
#include "Machines/Apple2/Common/Disk2IwmPort.h"
#include "Machines/Apple2/Common/Disk2Lines.h"
#include "Machines/Apple2/Common/Disk2Recorder.h"
#include "Machines/Apple2/Common/Disk2Signals.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"




//  The drive select, Q6 and Q7: the switches the IWM has whatever drive is
//  enabled.
static constexpr int  s_kFirstSelectSwitch = 0xA;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Disk2IwmPort
//
////////////////////////////////////////////////////////////////////////////////

Disk2IwmPort::Disk2IwmPort (
    Disk2Lines     & lines,
    Disk2Drives    & drives,
    Disk2Signals   & signals,
    Disk2Recorder  & recorder,
    Disk2Clock     & clock) :
    m_lines    (lines),
    m_drives   (drives),
    m_signals  (signals),
    m_recorder (recorder),
    m_clock    (clock),
    m_buses    { Disk2IwmDriveBus { lines, drives[0], 0 }, Disk2IwmDriveBus { lines, drives[1], 1 } }
{
    m_iwm.AttachDrive (0, &m_buses[0]);
    m_iwm.AttachDrive (1, &m_buses[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Access
//
//  An access to the //c's IWM. With the drives not enabled, only the drive
//  select, Q6 and Q7 change, and a read returns 0. A write then goes to the
//  device that has the port, so it doesn't load the mode register. A byte
//  the CPU stored in write mode with the motor on is a latch load, and a
//  byte the IWM finished reading goes to the recorder.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2IwmPort::Access (int offset, bool isWrite, Byte value, IDiskPortHook * hook)
{
    bool          isEnabled = hook == nullptr || hook->AreFloppiesEnabled();
    Byte          result    = 0;
    Byte          fresh     = 0;
    Disk2Drive  * drive     = nullptr;



    if (!isEnabled && m_iwm.IsEnabled())
    {
        m_iwm.Disable();
    }

    if (isEnabled)
    {
        result = m_iwm.Access (offset, isWrite, value, GetNow());
    }
    else if (offset >= s_kFirstSelectSwitch)
    {
        (void) m_iwm.Access (offset, false, 0, GetNow());
    }

    m_lines.SelectDrive (m_iwm.GetDrive());
    m_lines.SetQ6       (m_iwm.IsQ6());
    m_lines.SetQ7       (m_iwm.IsWriting());

    if (isEnabled)
    {
        ReportSwitch (offset);
    }

    SyncMotor();

    drive = &m_lines.GetActive();

    if (isEnabled && isWrite && m_iwm.IsQ6() && m_iwm.IsWriting() && m_iwm.IsMotorSwitchOn())
    {
        drive->LoadWriteLatch (value, m_lines.IsMotorOn());
        m_recorder.NoteWrite (*drive, m_lines.GetActiveDrive(), value, m_lines.IsMotorOn());
    }

    if (isEnabled && !isWrite && !m_iwm.IsQ6() && !m_iwm.IsWriting() && m_iwm.ConsumeFreshByte (fresh))
    {
        drive->GetEngine().CountReadNibble();
        m_recorder.NoteRead (*drive, m_lines.GetActiveDrive(), fresh, 0, m_clock.GetNow());
    }

    return isEnabled ? result : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::TryPeek
//
//  Access's byte for a read, then the port hook's: the IWM's byte with the
//  drives enabled, 0 without them, or the hook's when the access is the
//  hook's. False where the IWM or the hook cannot work it out.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2IwmPort::TryPeek (int offset, const IDiskPortHook * hook, Byte & value) const
{
    bool  isEnabled = hook == nullptr || hook->AreFloppiesEnabled();
    bool  isPorts   = false;
    bool  canPeek   = true;
    Byte  port      = 0;



    value = 0;

    if (isEnabled)
    {
        canPeek = m_iwm.TryPeekAccess (offset, value);
    }

    if (hook != nullptr)
    {
        canPeek = canPeek && hook->TryPeekIwmAccess (offset, isPorts, port);

        if (isPorts)
        {
            value = port;
        }
    }

    return canPeek;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Tick
//
//  While a drive is enabled, its cells move with the CPU, so its motor stops
//  when the IWM's timer runs out. With no cycle source, the IWM's time moves
//  here.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Tick (uint32_t cpuCycles)
{
    if (!m_clock.HasSource())
    {
        m_cycles += cpuCycles;
    }

    if (m_iwm.IsEnabled())
    {
        m_iwm.AdvanceTo (GetNow());
        SyncMotor();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Reset()
{
    m_buses[0].RestoreEnabled (false);
    m_buses[1].RestoreEnabled (false);
    m_iwm.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::SaveState
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::SaveState (StateWriter & writer) const
{
    m_iwm.SaveState (writer);
    writer.WriteBool   (m_buses[0].IsEnabled());
    writer.WriteBool   (m_buses[1].IsEnabled());
    writer.WriteUInt64 (m_cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2IwmPort::LoadState (StateReader & reader)
{
    HRESULT  hr       = S_OK;
    bool     enabled1 = false;
    bool     enabled2 = false;



    hr = m_iwm.LoadState (reader);
    CHR (hr);

    reader.ReadBool   (enabled1);
    reader.ReadBool   (enabled2);
    reader.ReadUInt64 (m_cycles);

    hr = reader.GetResult();
    CHR (hr);

    m_buses[0].RestoreEnabled (enabled1);
    m_buses[1].RestoreEnabled (enabled2);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::GetNow
//
//  The IWM's time, moved on by the CPU's cycles since the last access.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Disk2IwmPort::GetNow()
{
    uint64_t  elapsed = 0;



    if (m_clock.TakeElapsed (elapsed))
    {
        m_cycles += elapsed;
    }

    return m_cycles * Disk2IwmDriveBus::kNsPerCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::SyncMotor
//
//  The motor runs while either drive is enabled. This runs after an access
//  rather than at each enable, since a drive select disables one drive
//  before it enables the other.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::SyncMotor()
{
    bool  isOn = m_buses[0].IsEnabled() || m_buses[1].IsEnabled();



    if (isOn && !m_lines.IsMotorOn())
    {
        m_lines.SetMotorOn (true);
        m_signals.OnMotorEngaged();
    }
    else if (!isOn && m_lines.IsMotorOn())
    {
        m_lines.SetMotorOn (false);

        for (Disk2Drive & drive : m_drives)
        {
            drive.EndRead();
        }

        m_signals.OnMotorDisengaged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::ReportSwitch
//
//  The motor and drive select switches, for the event sinks.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::ReportSwitch (int offset)
{
    constexpr int  kMotorOff = 0x8;
    constexpr int  kMotorOn  = 0x9;



    if (offset == kMotorOff || offset == kMotorOn)
    {
        m_signals.OnMotorCommand (offset == kMotorOn);
    }
    else if (offset == s_kFirstSelectSwitch || offset == s_kFirstSelectSwitch + 1)
    {
        m_signals.OnDriveSelect (m_lines.GetActiveDrive());
    }
}
