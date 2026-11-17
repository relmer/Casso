#include "Pch.h"

#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusDiskPort
//
////////////////////////////////////////////////////////////////////////////////

Apple2cPlusDiskPort::Apple2cPlusDiskPort()
{
    m_port.GetIwm().AttachDrive (0, &m_drives[0]);
    m_port.GetIwm().AttachDrive (1, &m_drives[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Apple2cPlusDiskPort
//
////////////////////////////////////////////////////////////////////////////////

Apple2cPlusDiskPort::~Apple2cPlusDiskPort() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptMig
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::AdoptMig (std::unique_ptr<MigChip> mig)
{
    m_mig      = mig.get();
    m_ownedMig = std::move (mig);

    m_mig->SetIwmReset ([this] () { ResetIwm(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2cPlusDiskPort::Read (Word address)
{
    return m_port.Access (address & kSwitchMask, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::Write (Word address, Byte value)
{
    m_port.Access (address & kSwitchMask, true, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnIwmAccess
//
//  The IWM follows every access. A read returns its byte unless the
//  5.25-inch drives were enabled for the access and Q7 is off: the
//  controller has the floppy's data and status registers, and the write
//  handshake is the IWM's whatever drive is enabled. ROM 05 waits on the
//  handshake while it probes the SmartPort bus with drive 1 selected.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::OnIwmAccess (int offset, bool isWrite, Byte value, Byte & outValue)
{
    outValue = m_port.Access (offset, isWrite, value);

    return !m_floppies || m_port.GetIwm().IsWriting();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreFloppiesEnabled
//
//  Called before each access, with the IWM's state from before it. A
//  5.25-inch drive has no 3.5 select input: that pin is ground on the Disk
//  IIc's connector. At startup, ROM 05 reads drive 1 with 3.5 select off; a
//  5.25-inch drive returns nibbles there, and only then does ROM 05 run the
//  slot 6 firmware. An Apple 3.5 Drive ahead of it on the chain is drive 1
//  while 3.5 select is on.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::AreFloppiesEnabled()
{
    int   drive  = m_port.GetIwm().GetDrive();
    bool  hasBus = m_port.GetDriveCount() > 0 && (m_port.GetBus().IsEnabledAt (0) || m_port.GetBus().IsReset());
    bool  is35   = &GetDriven (drive) != &m_port.GetBus();



    m_floppies = drive < m_floppyCount && !is35 && !hasBus;

    return m_floppies;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppy35EventSink
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::SetFloppy35EventSink (IFloppy35EventSink * sink)
{
    m_internal.SetFloppy35EventSink (sink);

    for (Sony35Drive & external : m_externals)
    {
        external.SetFloppy35EventSink (sink);
    }

    m_port.SetFloppy35EventSink (sink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInternal
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::IsInternal() const
{
    return m_mig != nullptr && m_mig->IsInternalOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriven
//
//  What IWM drive `drive` (0 or 1) drives: the internal drive, an external
//  Apple 3.5 Drive or the SmartPort bus.
//
////////////////////////////////////////////////////////////////////////////////

IDiskDriveBus & Apple2cPlusDiskPort::GetDriven (int drive)
{
    if (drive == kInternalDrive && IsInternal())
    {
        return m_internal;
    }

    if (drive < m_externalCount && m_mig != nullptr && m_mig->IsSel35())
    {
        return m_externals[(size_t) drive];
    }

    return m_port.GetBus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncHeads
//
//  The MIG's head select is each 3.5-inch drive's SEL line.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::SyncHeads()
{
    if (m_mig == nullptr)
    {
        return;
    }

    m_internal.SetSel (m_mig->IsHeadOne());

    for (int i = 0; i < m_externalCount; i++)
    {
        m_externals[(size_t) i].SetSel (m_mig->IsHeadOne());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncEnables
//
//  The internal drive is enabled while the IWM enables drive 2 and the
//  MIG's internal drive is on; an Apple 3.5 Drive on the port while the IWM
//  enables its drive with 3.5 select on.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::SyncEnables()
{
    bool  internal = IsInternal();
    bool  sel35    = m_mig != nullptr && m_mig->IsSel35();



    m_internal.SetEnabled (m_iwmEnabled[kInternalDrive] && internal);

    for (int i = 0; i < m_externalCount; i++)
    {
        m_externals[(size_t) i].SetEnabled (m_iwmEnabled[(size_t) i] && sel35 && !(i == kInternalDrive && internal));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::Sync
//
////////////////////////////////////////////////////////////////////////////////

IDiskDriveBus & Apple2cPlusDiskPort::PortDrive::Sync()
{
    IDiskDriveBus        & driven = m_diskPort.GetDriven (m_drive);
    std::array<bool, 4>  & phases = m_diskPort.m_phases;



    m_diskPort.SyncHeads();
    m_diskPort.SyncEnables();

    if (&driven != m_driven)
    {
        driven.SetPhases (phases[0], phases[1], phases[2], phases[3]);
        m_driven = &driven;
    }

    return driven;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::SetPhases
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::PortDrive::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    m_diskPort.m_phases = { ph0, ph1, ph2, ph3 };

    Sync().SetPhases (ph0, ph1, ph2, ph3);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::GetSense
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::PortDrive::GetSense() const
{
    return m_diskPort.GetDriven (m_drive).GetSense();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::ReadCell
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2cPlusDiskPort::PortDrive::ReadCell (uint64_t cellEndNs)
{
    return Sync().ReadCell (cellEndNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::PortDrive::WriteCell (Byte bit, uint64_t cellEndNs)
{
    m_diskPort.GetDriven (m_drive).WriteCell (bit, cellEndNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::Advance
//
//  Time passes for the 3.5-inch drives at this drive's place, driven or
//  not, so a motor spins down and a step finishes.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::PortDrive::Advance (uint64_t elapsedNs)
{
    if (m_drive == kInternalDrive)
    {
        m_diskPort.m_internal.Advance (elapsedNs);
    }

    if (m_drive < m_diskPort.m_externalCount)
    {
        m_diskPort.m_externals[(size_t) m_drive].Advance (elapsedNs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::GetCellNs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple2cPlusDiskPort::PortDrive::GetCellNs() const
{
    return m_diskPort.GetDriven (m_drive).GetCellNs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::GetQuietUntil
//
//  The heads are brought to the MIG's head select first, as a read brings
//  them, since a side the disk doesn't have reads 0.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Apple2cPlusDiskPort::PortDrive::GetQuietUntil() const
{
    m_diskPort.SyncHeads();

    return m_diskPort.GetDriven (m_drive).GetQuietUntil();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortDrive::SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::PortDrive::SetEnabled (bool isEnabled)
{
    m_diskPort.m_iwmEnabled[(size_t) m_drive] = isEnabled;
    m_diskPort.SyncEnables();
}
