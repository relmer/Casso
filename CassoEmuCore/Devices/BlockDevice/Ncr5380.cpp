#include "Pch.h"

#include "Devices/BlockDevice/Ncr5380.h"
#include "Devices/BlockDevice/ScsiBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
////////////////////////////////////////////////////////////////////////////////

Byte Ncr5380::ReadRegister (int reg)
{
    constexpr Byte  kIcrWritable = 0x9F;
    constexpr Byte  kTcrMask     = 0x0F;
    Byte            value        = 0;



    UpdateArbitration();

    switch (reg & kRegisterMask)
    {
        case 0:
            value = m_bus.GetData();
            break;

        case 1:
            value = (Byte) ((m_icr & kIcrWritable) | (m_arbitrating ? kIcrAip : 0));
            break;

        case 2:
            value = m_mode;
            break;

        case 3:
            value = (Byte) (m_tcr & kTcrMask);
            break;

        case 4:
            value = GetBusStatus();
            break;

        case 5:
            value = GetBusAndStatus();
            break;

        case 6:
            value = m_idr;
            break;

        default:
            m_irq       = false;
            m_busyError = false;
            break;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::WriteRegister (int reg, Byte value)
{
    constexpr Byte  kIcrWritable = 0x9F;
    constexpr Byte  kTcrMask     = 0x0F;



    switch (reg & kRegisterMask)
    {
        case 0:
            m_odr = value;
            break;

        case 1:
            m_icr = (Byte) (value & kIcrWritable);

            if ((m_icr & kIcrAssertRst) != 0)
            {
                m_irq = true;
            }

            break;

        case 2:
            m_mode = value;

            if ((m_mode & kModeArbitrate) == 0)
            {
                m_arbitrating = false;
            }

            if ((m_mode & kModeDma) == 0)
            {
                m_dma = DmaMode::None;
            }

            break;

        case 3:
            m_tcr = (Byte) (value & kTcrMask);
            break;

        case 4:
            m_ser = value;
            break;

        case 5:
            m_dma = ((m_mode & kModeDma) != 0) ? DmaMode::Send : DmaMode::None;
            break;

        case 6:
            break;

        default:
            m_dma = ((m_mode & kModeDma) != 0) ? DmaMode::InitiatorReceive : DmaMode::None;
            break;
    }

    Drive();
    UpdateArbitration();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDma
//
//  A DACK read: the byte the target offers, taken with one REQ/ACK
//  handshake. With no request pending, the last byte is read again.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ncr5380::ReadDma()
{
    bool  isDrq = IsDrq();



    if (isDrq && m_dma == DmaMode::InitiatorReceive)
    {
        m_idr = m_bus.GetData();
        PulseAck();
    }

    return m_idr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteDma
//
//  A DACK write: the byte goes on the bus with one REQ/ACK handshake.
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::WriteDma (Byte value)
{
    bool  isDrq = IsDrq();



    m_odr = value;

    if (isDrq && m_dma == DmaMode::Send)
    {
        PulseAck();
    }
    else
    {
        Drive();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDrq
//
////////////////////////////////////////////////////////////////////////////////

bool Ncr5380::IsDrq()
{
    bool  isDmaMode = (m_mode & kModeDma) != 0;



    CheckPhaseMismatch();

    return m_dma != DmaMode::None && isDmaMode && m_bus.IsReq() && IsPhaseMatch();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::Reset()
{
    m_odr         = 0;
    m_icr         = 0;
    m_mode        = 0;
    m_tcr         = 0;
    m_ser         = 0;
    m_idr         = 0;
    m_arbitrating = false;
    m_irq         = false;
    m_busyError   = false;
    m_dma         = DmaMode::None;

    Drive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Drive
//
//  Puts the chip's lines on the bus from its registers. Arbitration drives
//  BSY and the chip's ID on the data lines.
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::Drive()
{
    ScsiInitiatorLines  lines;



    lines.data      = m_odr;
    lines.driveData = (m_icr & kIcrAssertData) != 0 || m_arbitrating;
    lines.bsy       = (m_icr & kIcrAssertBsy)  != 0 || m_arbitrating;
    lines.sel       = (m_icr & kIcrAssertSel)  != 0;
    lines.atn       = (m_icr & kIcrAssertAtn)  != 0;
    lines.ack       = (m_icr & kIcrAssertAck)  != 0;
    lines.rst       = (m_icr & kIcrAssertRst)  != 0;

    m_bus.SetInitiatorLines (lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateArbitration
//
//  With ARBITRATE set, the chip takes the bus as soon as it is free.
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::UpdateArbitration()
{
    bool  wantsBus = (m_mode & kModeArbitrate) != 0 && !m_arbitrating;
    bool  isFree   = !m_bus.IsBsy() && !m_bus.IsSel();



    if (wantsBus && isFree)
    {
        m_arbitrating = true;
        Drive();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPhaseMatch
//
//  The bus's MSG, C/D and I/O lines against the target command register's.
//
////////////////////////////////////////////////////////////////////////////////

bool Ncr5380::IsPhaseMatch() const
{
    constexpr Byte  kPhaseBits  = 0x07;
    constexpr int   kPhaseShift = 2;
    Byte            busPhase    = (Byte) ((GetBusStatus() >> kPhaseShift) & kPhaseBits);



    return busPhase == (m_tcr & kPhaseBits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBusStatus
//
////////////////////////////////////////////////////////////////////////////////

Byte Ncr5380::GetBusStatus() const
{
    Byte  value = 0;



    value |= m_bus.IsRst() ? kBusRst : 0;
    value |= m_bus.IsBsy() ? kBusBsy : 0;
    value |= m_bus.IsReq() ? kBusReq : 0;
    value |= m_bus.IsMsg() ? kBusMsg : 0;
    value |= m_bus.IsCd()  ? kBusCd  : 0;
    value |= m_bus.IsIo()  ? kBusIo  : 0;
    value |= m_bus.IsSel() ? kBusSel : 0;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBusAndStatus
//
////////////////////////////////////////////////////////////////////////////////

Byte Ncr5380::GetBusAndStatus()
{
    Byte  value = 0;



    value |= IsDrq()          ? kBsrDrq        : 0;
    value |= m_irq            ? kBsrIrq        : 0;
    value |= IsPhaseMatch()   ? kBsrPhaseMatch : 0;
    value |= m_busyError      ? kBsrBusyError  : 0;
    value |= m_bus.IsAtn()    ? kBsrAtn        : 0;
    value |= m_bus.IsAck()    ? kBsrAck        : 0;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckPhaseMismatch
//
//  A target that requests in a phase other than the one the transfer is for
//  raises the interrupt, and the transfer ends. With MONITOR BUSY set, a
//  target that lets go of BSY is a busy error.
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::CheckPhaseMismatch()
{
    bool  isMonitoring = (m_mode & kModeMonitorBsy) != 0;



    if (m_dma != DmaMode::None && m_bus.IsReq() && !IsPhaseMatch())
    {
        m_irq = true;
        m_dma = DmaMode::None;
    }

    if (isMonitoring && !m_bus.IsBsy())
    {
        m_busyError = true;
        m_irq       = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PulseAck
//
//  One byte's handshake: ACK asserted with the chip's byte on the data lines
//  for a send, then released.
//
////////////////////////////////////////////////////////////////////////////////

void Ncr5380::PulseAck()
{
    ScsiInitiatorLines  lines;



    lines.data      = m_odr;
    lines.driveData = m_dma == DmaMode::Send;
    lines.bsy       = (m_icr & kIcrAssertBsy) != 0;
    lines.atn       = (m_icr & kIcrAssertAtn) != 0;
    lines.ack       = true;

    m_bus.SetInitiatorLines (lines);

    lines.ack = false;
    m_bus.SetInitiatorLines (lines);

    Drive();
}
