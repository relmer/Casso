#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/IInterruptController.h"

class MemoryBus;
class IAciaEndpoint;

struct DeviceConfig;




////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551
//
//  6551 ACIA (Asynchronous Communications Interface Adapter) as used by the
//  Apple //c built-in serial ports and the Super Serial Card. Four registers
//  occupy a slot's I/O page at $C088 + slot*16:
//
//      +0  Data     read = receiver data (RDR), write = transmit data (TDR)
//      +1  Status   read = status, write = programmed reset
//      +2  Command  DTR, receiver-IRQ enable, transmitter-IRQ control, parity
//      +3  Control  baud rate, word length, stop bits
//
//  Transmit is modelled as instantaneous: a byte written to the data register
//  is handed to the attached endpoint immediately and TDRE stays set. The IRQ
//  status bit (bit 7) is an event latch — set when a byte is received or the
//  transmitter empties (subject to the command-register enables) and cleared
//  by reading the status register.
//
//  This is the single ACIA implementation in the tree; the printer feature
//  (spec 015) consumes the same device rather than building its own.
//
////////////////////////////////////////////////////////////////////////////////

class Acia6551 : public MemoryDevice
{
public:
    // Slot I/O geometry: slot n decodes $C080 + n*16; the ACIA sits at
    // offset 8 within that page.
    static constexpr Word    kSlotIoBase   = 0xC080;
    static constexpr Word    kSlotIoStride = 0x10;
    static constexpr Word    kAciaRegOffset = 0x08;

    static constexpr Word    kRegData    = 0;
    static constexpr Word    kRegStatus  = 1;
    static constexpr Word    kRegCommand = 2;
    static constexpr Word    kRegControl = 3;
    static constexpr Word    kRegisterCount = 4;

    // Status register bits.
    static constexpr Byte    kStatusParityError  = 0x01;
    static constexpr Byte    kStatusFramingError = 0x02;
    static constexpr Byte    kStatusOverrun      = 0x04;
    static constexpr Byte    kStatusRxFull       = 0x08;
    static constexpr Byte    kStatusTxEmpty      = 0x10;
    static constexpr Byte    kStatusDcd          = 0x20;
    static constexpr Byte    kStatusDsr          = 0x40;
    static constexpr Byte    kStatusIrq          = 0x80;

    // Command register bits.
    static constexpr Byte    kCommandDtr           = 0x01;
    static constexpr Byte    kCommandRxIrqDisable  = 0x02;
    static constexpr Byte    kCommandTicMask       = 0x0C;
    static constexpr Byte    kCommandTicTxIrqOn    = 0x04;
    static constexpr Byte    kCommandEcho          = 0x10;
    static constexpr Byte    kCommandParityMask    = 0xE0;

    // Control register bits.
    static constexpr Byte    kControlBaudMask     = 0x0F;
    static constexpr Byte    kControlWordLenMask  = 0x60;
    static constexpr Byte    kControlWordLenShift = 5;
    static constexpr Byte    kControlStopBits     = 0x80;

    explicit                 Acia6551 (Word baseAddress);

    // MemoryDevice
    Byte    Read       (Word address) override;
    void    Write      (Word address, Byte value) override;
    Word    GetStart   () const override { return m_baseAddress; }
    Word    GetEnd     () const override { return m_ioEnd; }
    void    Reset      () override;
    void    SoftReset  () override;
    void    PowerCycle (Prng & prng) override;

    // Register the ACIA's IRQ source with the interrupt controller. The
    // controller is caller-owned and outlives the device.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // Route transmitted bytes. Caller-owned; pass nullptr to detach.
    void    SetEndpoint (IAciaEndpoint * endpoint) { m_endpoint = endpoint; }

    // Push one received byte into the ACIA (called by an endpoint).
    void    ReceiveByte (Byte value);

    // Inspectors for tests / firmware wiring.
    Byte    GetStatus  () const { return m_status; }
    Byte    GetCommand () const { return m_command; }
    Byte    GetControl () const { return m_control; }
    int     GetWordLengthBits () const;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void    ResetState      (bool hardware);
    void    ProgrammedReset ();
    Byte    ReadData        ();
    Byte    ReadStatus      ();
    void    WriteData       (Byte value);
    void    RaiseIrq        ();
    void    LowerIrq        ();
    bool    RxIrqEnabled    () const;
    bool    TxIrqEnabled    () const;

    Word                     m_baseAddress = 0;
    Word                     m_ioEnd       = 0;

    Byte                     m_status  = 0;
    Byte                     m_command = 0;
    Byte                     m_control = 0;
    Byte                     m_rxData  = 0;

    IInterruptController *    m_ic        = nullptr;
    IrqSourceId               m_irqSource = 0;
    bool                      m_irqBound  = false;

    IAciaEndpoint *           m_endpoint = nullptr;
};
