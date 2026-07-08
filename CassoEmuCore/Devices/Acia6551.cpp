#include "Pch.h"

#include "Acia6551.h"
#include "IAciaEndpoint.h"
#include "Core/MachineConfig.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551
//
////////////////////////////////////////////////////////////////////////////////

Acia6551::Acia6551 (Word baseAddress)
{
    m_baseAddress = baseAddress;
    m_ioEnd       = static_cast<Word> (baseAddress + kRegisterCount - 1);

    ResetState (true);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Data and status reads have side effects (clearing RDRF and the IRQ latch
//  respectively); command and control reads are pure.
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::Read (Word address)
{
    Word   reg    = static_cast<Word> (address - m_baseAddress);
    Byte   result = 0;

    switch (reg)
    {
    case kRegData:
        result = ReadData ();
        break;

    case kRegStatus:
        result = ReadStatus ();
        break;

    case kRegCommand:
        result = m_command;
        break;

    case kRegControl:
        result = m_control;
        break;

    default:
        break;
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write to the status address is a programmed reset. Writing the command
//  register can newly enable an interrupt whose condition already holds, which
//  must fire immediately.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::Write (Word address, Byte value)
{
    Word   reg = static_cast<Word> (address - m_baseAddress);

    switch (reg)
    {
    case kRegData:
        WriteData (value);
        break;

    case kRegStatus:
        ProgrammedReset ();
        break;

    case kRegCommand:
        m_command = value;

        if (TxIrqEnabled () && (m_status & kStatusTxEmpty))
        {
            RaiseIrq ();
        }

        if (RxIrqEnabled () && (m_status & kStatusRxFull))
        {
            RaiseIrq ();
        }
        break;

    case kRegControl:
        m_control = value;
        break;

    default:
        break;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::Reset ()
{
    ResetState (true);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  The 6551's RESB pin is tied to the Apple reset line, so Ctrl-Reset clears
//  the ACIA registers just like a power cycle.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SoftReset ()
{
    ResetState (true);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::PowerCycle (Prng & prng)
{
    (void) prng;

    ResetState (true);
}




////////////////////////////////////////////////////////////////////////////////
//
//  AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Acia6551::AttachInterruptController (IInterruptController * ic)
{
    HRESULT   hr = S_OK;

    CBRAEx (ic, E_INVALIDARG);

    hr = ic->RegisterSource (m_irqSource);
    CHR (hr);

    m_ic       = ic;
    m_irqBound = true;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReceiveByte
//
//  Push one byte into the receiver. If the previous byte was never read the
//  overrun flag is set and the stale byte is overwritten.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ReceiveByte (Byte value)
{
    if (m_status & kStatusRxFull)
    {
        m_status |= kStatusOverrun;
    }

    m_rxData  = value;
    m_status |= kStatusRxFull;

    if (RxIrqEnabled ())
    {
        RaiseIrq ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetWordLengthBits
//
//  Control bits 6-5 select 8/7/6/5-bit words (00..11 respectively).
//
////////////////////////////////////////////////////////////////////////////////

int Acia6551::GetWordLengthBits () const
{
    int   selector = (m_control & kControlWordLenMask) >> kControlWordLenShift;

    return 8 - selector;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ResetState
//
//  Full register reset. A hardware/RESB reset clears command and control; a
//  programmed reset (write to the status address) leaves control untouched and
//  preserves only the command register's parity-mode bits.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ResetState (bool hardware)
{
    m_status = kStatusTxEmpty;
    m_rxData = 0;

    if (hardware)
    {
        m_command = 0;
        m_control = 0;
    }
    else
    {
        m_command &= kCommandParityMask;
    }

    LowerIrq ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  ProgrammedReset
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ProgrammedReset ()
{
    ResetState (false);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadData
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::ReadData ()
{
    m_status &= ~kStatusRxFull;

    return m_rxData;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadStatus
//
//  Reading the status register clears the IRQ latch and de-asserts the line.
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::ReadStatus ()
{
    Byte   snapshot = m_status;

    LowerIrq ();

    return snapshot;
}




////////////////////////////////////////////////////////////////////////////////
//
//  WriteData
//
//  Transmit is instantaneous: the byte is handed to the endpoint and the
//  transmitter is immediately empty again, re-firing the TX interrupt if
//  enabled.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::WriteData (Byte value)
{
    if (m_endpoint != nullptr)
    {
        m_endpoint->OnByteTransmitted (value);
    }

    m_status |= kStatusTxEmpty;

    if (TxIrqEnabled ())
    {
        RaiseIrq ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  RaiseIrq
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::RaiseIrq ()
{
    m_status |= kStatusIrq;

    if (m_irqBound && (m_ic != nullptr))
    {
        m_ic->Assert (m_irqSource);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  LowerIrq
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::LowerIrq ()
{
    m_status &= ~kStatusIrq;

    if (m_irqBound && (m_ic != nullptr))
    {
        m_ic->Clear (m_irqSource);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  RxIrqEnabled
//
//  Receiver interrupts require DTR set and the receiver-IRQ-disable bit clear.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::RxIrqEnabled () const
{
    return (m_command & kCommandDtr) && !(m_command & kCommandRxIrqDisable);
}




////////////////////////////////////////////////////////////////////////////////
//
//  TxIrqEnabled
//
//  Transmitter interrupts require DTR set and the transmitter-control field set
//  to the "IRQ enabled, RTS low" pattern (01).
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::TxIrqEnabled () const
{
    return (m_command & kCommandDtr) && ((m_command & kCommandTicMask) == kCommandTicTxIrqOn);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  The ACIA occupies the four registers at $C088 + slot*16.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> Acia6551::Create (const DeviceConfig & config, MemoryBus & bus)
{
    Word   base = 0;

    (void) bus;

    base = static_cast<Word> (kSlotIoBase + config.slot * kSlotIoStride + kAciaRegOffset);

    return make_unique<Acia6551> (base);
}
