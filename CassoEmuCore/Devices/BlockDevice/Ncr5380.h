#pragma once

#include "Pch.h"

class ScsiBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Ncr5380
//
//  NCR's 5380 SCSI bus controller as an initiator, from its data sheet:
//
//      reg  read                          write
//      0    current SCSI data             output data
//      1    initiator command             initiator command
//      2    mode                          mode
//      3    target command                target command
//      4    current SCSI bus status       select enable
//      5    bus and status                start DMA send
//      6    input data                    start DMA target receive
//      7    reset parity/interrupts       start DMA initiator receive
//
//  Arbitration completes as soon as the bus is free, since the chip is the
//  bus's only initiator. In DMA mode the chip asserts DRQ while the target
//  requests a byte in the phase the target command register specifies, and
//  a DACK access moves one byte with its REQ/ACK handshake. A request in
//  another phase raises the interrupt and ends the transfer.
//
////////////////////////////////////////////////////////////////////////////////

class Ncr5380
{
public:
    //  Initiator command register.
    static constexpr Byte  kIcrAssertRst   = 0x80;
    static constexpr Byte  kIcrAip         = 0x40;
    static constexpr Byte  kIcrLa          = 0x20;
    static constexpr Byte  kIcrAssertAck   = 0x10;
    static constexpr Byte  kIcrAssertBsy   = 0x08;
    static constexpr Byte  kIcrAssertSel   = 0x04;
    static constexpr Byte  kIcrAssertAtn   = 0x02;
    static constexpr Byte  kIcrAssertData  = 0x01;

    //  Mode register.
    static constexpr Byte  kModeBlockDma   = 0x80;
    static constexpr Byte  kModeTarget     = 0x40;
    static constexpr Byte  kModeMonitorBsy = 0x04;
    static constexpr Byte  kModeDma        = 0x02;
    static constexpr Byte  kModeArbitrate  = 0x01;

    //  Current SCSI bus status.
    static constexpr Byte  kBusRst         = 0x80;
    static constexpr Byte  kBusBsy         = 0x40;
    static constexpr Byte  kBusReq         = 0x20;
    static constexpr Byte  kBusMsg         = 0x10;
    static constexpr Byte  kBusCd          = 0x08;
    static constexpr Byte  kBusIo          = 0x04;
    static constexpr Byte  kBusSel         = 0x02;

    //  Bus and status.
    static constexpr Byte  kBsrEndOfDma    = 0x80;
    static constexpr Byte  kBsrDrq         = 0x40;
    static constexpr Byte  kBsrIrq         = 0x10;
    static constexpr Byte  kBsrPhaseMatch  = 0x08;
    static constexpr Byte  kBsrBusyError   = 0x04;
    static constexpr Byte  kBsrAtn         = 0x02;
    static constexpr Byte  kBsrAck         = 0x01;

    static constexpr Byte  kRegisterMask   = 0x07;

    explicit Ncr5380 (ScsiBus & bus) : m_bus (bus) {}

    Byte  ReadRegister  (int reg);
    void  WriteRegister (int reg, Byte value);

    //  The pseudo-DMA port: a DACK read or write of one byte.
    Byte  ReadDma  ();
    void  WriteDma (Byte value);

    bool  IsDrq  ();
    bool  IsIrq  () const { return m_irq; }

    //  The chip's reset input.
    void  Reset ();

private:
    enum class DmaMode : Byte
    {
        None,
        Send,
        InitiatorReceive,
    };

    void  Drive              ();
    void  UpdateArbitration  ();
    bool  IsPhaseMatch       () const;
    Byte  GetBusStatus       () const;
    Byte  GetBusAndStatus    ();
    void  CheckPhaseMismatch ();
    void  PulseAck           ();

    ScsiBus  & m_bus;
    Byte       m_odr         = 0;
    Byte       m_icr         = 0;
    Byte       m_mode        = 0;
    Byte       m_tcr         = 0;
    Byte       m_ser         = 0;
    Byte       m_idr         = 0;
    bool       m_arbitrating = false;
    bool       m_irq         = false;
    bool       m_busyError   = false;
    DmaMode    m_dma         = DmaMode::None;
};
