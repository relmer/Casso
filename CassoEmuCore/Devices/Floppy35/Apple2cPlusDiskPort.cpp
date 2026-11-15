#include "Pch.h"

#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusDiskPort
//
//  The IWM's drive 1 is the disk port's bus; drive 2 is this class.
//
////////////////////////////////////////////////////////////////////////////////

Apple2cPlusDiskPort::Apple2cPlusDiskPort()
{
    m_port.GetIwm().AttachDrive (1, this);
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
//  SetPhases
//
//  After a switch between the internal drive and the port, the newly
//  driven one is set to the IWM's phases.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    m_phases = { ph0, ph1, ph2, ph3 };
    SyncHead();

    if (IsInternal())
    {
        m_internal.SetPhases (ph0, ph1, ph2, ph3);
    }
    else
    {
        m_port.GetBus().SetPhases (ph0, ph1, ph2, ph3);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSense
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::GetSense() const
{
    return IsInternal() ? m_internal.GetSense() : m_port.GetBus().GetSense();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCell
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2cPlusDiskPort::ReadCell (uint64_t cellEndNs)
{
    SyncHead();

    return IsInternal() ? m_internal.ReadCell (cellEndNs) : m_port.GetBus().ReadCell (cellEndNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::WriteCell (Byte bit, uint64_t cellEndNs)
{
    if (IsInternal())
    {
        m_internal.WriteCell (bit, cellEndNs);
    }
    else
    {
        m_port.GetBus().WriteCell (bit, cellEndNs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCellNs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple2cPlusDiskPort::GetCellNs() const
{
    return IsInternal() ? m_internal.GetCellNs() : SmartPortBus::kCellNs;
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
//  IIc's connector. At startup, drive 1 is read with 3.5 select on; a
//  5.25-inch drive returns nibbles there, and only then does ROM 05 run the
//  slot 6 firmware.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusDiskPort::AreFloppiesEnabled()
{
    int   drive  = m_port.GetIwm().GetDrive();
    bool  hasBus = m_port.GetDriveCount() > 0 && (m_port.GetBus().IsEnabledAt (0) || m_port.GetBus().IsReset());



    m_floppies = drive < m_floppyCount && !(drive == kInternalDrive && IsInternal()) && !hasBus;

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
//  SyncHead
//
//  The MIG's head select is the internal drive's SEL line. After a switch
//  to the internal drive, it is set to the IWM's phases.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusDiskPort::SyncHead()
{
    bool  internal = IsInternal();



    if (m_mig != nullptr)
    {
        m_internal.SetSel (m_mig->IsHeadOne());
    }

    if (internal && !m_wasInternal)
    {
        m_internal.SetPhases (m_phases[0], m_phases[1], m_phases[2], m_phases[3]);
    }

    m_wasInternal = internal;
}
