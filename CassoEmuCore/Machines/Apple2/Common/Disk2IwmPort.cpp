#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/Disk2IwmPort.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Disk2IwmPort
//
////////////////////////////////////////////////////////////////////////////////

Disk2IwmPort::Disk2IwmPort (IDisk2IwmOwner & owner) :
    m_owner  (owner),
    m_drives { Drive { *this, 0 }, Drive { *this, 1 } }
{
    m_iwm.AttachDrive (0, &m_drives[0]);
    m_iwm.AttachDrive (1, &m_drives[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Access
//
//  An access to the //c's IWM. With the drives not enabled, only the drive
//  select, Q6 and Q7 change, and a read returns 0. A write then goes to the
//  device that has the port, so it doesn't load the mode register. A byte
//  the IWM finished reading goes to the owner.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2IwmPort::Access (int offset, bool isWrite, Byte value, IDiskPortHook * hook)
{
    bool  isEnabled = hook == nullptr || hook->AreFloppiesEnabled();
    Byte  result    = 0;
    Byte  fresh     = 0;



    if (!isEnabled && m_iwm.IsEnabled())
    {
        m_iwm.Disable();
    }

    if (isEnabled)
    {
        result = m_iwm.Access (offset, isWrite, value, GetNow());
    }
    else if (offset >= kFirstSelectSwitch)
    {
        (void) m_iwm.Access (offset, false, 0, GetNow());
    }

    m_owner.OnIwmSequencer (m_iwm.GetDrive(), m_iwm.IsQ6(), m_iwm.IsWriting());

    if (isEnabled)
    {
        m_owner.OnIwmSwitch (offset);
    }

    SyncMotor();

    if (isEnabled && isWrite && m_iwm.IsQ6() && m_iwm.IsWriting() && m_iwm.IsMotorSwitchOn())
    {
        m_owner.OnIwmNibbleWritten (value);
    }

    if (isEnabled && !isWrite && !m_iwm.IsQ6() && !m_iwm.IsWriting() && m_iwm.ConsumeFreshByte (fresh))
    {
        m_owner.OnIwmNibbleRead (fresh);
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
    bool  isEnabled = hook == nullptr || hook->PeekFloppiesEnabled();
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
//  when the IWM's timer runs out. isCounted: the IWM's time moves here,
//  since there is no cycle source.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Tick (uint32_t cpuCycles, bool isCounted)
{
    if (isCounted)
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
    m_enabled.fill (false);
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
    writer.WriteBool   (m_enabled[0]);
    writer.WriteBool   (m_enabled[1]);
    writer.WriteUInt64 (m_cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2IwmPort::LoadState (StateReader & reader)
{
    HRESULT  hr = S_OK;



    hr = m_iwm.LoadState (reader);
    CHR (hr);

    reader.ReadBool   (m_enabled[0]);
    reader.ReadBool   (m_enabled[1]);
    reader.ReadUInt64 (m_cycles);

    hr = reader.GetResult();

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



    if (m_owner.AdvanceIwmClock (elapsed))
    {
        m_cycles += elapsed;
    }

    return m_cycles * kNsPerCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::SyncMotor
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::SyncMotor()
{
    m_owner.OnIwmMotor (m_enabled[0] || m_enabled[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Drive::SetPhases
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Drive::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    m_port.m_owner.OnIwmPhases (m_drive, static_cast<uint8_t> ((ph0 ? 1 : 0) | (ph1 ? 2 : 0) | (ph2 ? 4 : 0) | (ph3 ? 8 : 0)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Drive::GetSense
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2IwmPort::Drive::GetSense() const
{
    return m_port.m_owner.IsIwmWriteProtected (m_drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Drive::ReadCell
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2IwmPort::Drive::ReadCell (uint64_t cellEndNs)
{
    UNREFERENCED_PARAMETER (cellEndNs);

    return m_port.m_owner.StepIwmCell (m_drive, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Drive::WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Drive::WriteCell (Byte bit, uint64_t cellEndNs)
{
    UNREFERENCED_PARAMETER (cellEndNs);

    (void) m_port.m_owner.StepIwmCell (m_drive, true, bit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort::Drive::SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void Disk2IwmPort::Drive::SetEnabled (bool isEnabled)
{
    m_port.m_enabled[(size_t) m_drive] = isEnabled;
    m_port.m_owner.OnIwmEnable (m_drive, isEnabled);
}
