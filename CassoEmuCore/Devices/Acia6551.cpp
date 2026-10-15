#include "Pch.h"

#include "Acia6551.h"
#include "IAciaEndpoint.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kBaudRates
//
//  Bits per second for control-register baud codes 1-15, from the 6551
//  datasheet's baud-rate generator table. Code 0 is not in the table: it
//  selects the crystal divided by 16 (GetBaudRate).
//
////////////////////////////////////////////////////////////////////////////////

static constexpr double  s_kBaudRates[] =
{
    0.0,     50.0,    75.0,    109.92,  134.58,  150.0,   300.0,   600.0,
    1200.0,  1800.0,  2400.0,  3600.0,  4800.0,  7200.0,  9600.0,  19200.0,
};





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
        result = ReadData();
        break;

    case kRegStatus:
        result = ReadStatus();
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
//  A write to the status address is a program reset. A control write takes
//  effect from the next character; a character already shifting finishes at
//  the timing it started with.
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
        ResetState (false);
        break;

    case kRegCommand:
        WriteCommand (value);
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

void Acia6551::Reset()
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

void Acia6551::SoftReset()
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
//  SetIrqConnected
//
//  Connects or disconnects the IRQ latch from the CPU's line. A latch already
//  set drives the line as soon as it is connected.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetIrqConnected (bool connected)
{
    m_irqConnected = connected;

    if (m_irqBound && m_ic != nullptr)
    {
        if (connected && (m_status & kStatusIrq))
        {
            m_ic->Assert (m_irqSource);
        }
        else
        {
            m_ic->Clear (m_irqSource);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCpuClockHz
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetCpuClockHz (uint32_t hz)
{
    if (hz != 0)
    {
        m_cpuClockHz = hz;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advances the shift register and the receive delivery by `cycles` CPU
//  cycles. An idle chip, with nothing shifting, nothing waiting to shift,
//  nothing queued and no modem input change, returns after the first test.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::Tick (uint32_t cycles)
{
    HRESULT  hr          = S_OK;
    int64_t  fixedCycles = static_cast<int64_t> (cycles) << kFixedShift;
    bool     modemDirty  = m_modemInputDirty.load (memory_order_relaxed);
    size_t   queued      = m_rxQueue.GetCount();
    bool     idle        = !m_tsrBusy && !m_tdrFull && queued == 0 && !m_rxActive && !modemDirty;



    BAIL_OUT_IF (idle, S_OK);

    if (modemDirty)
    {
        ApplyPendingModemInputs();
    }

    AdvanceTransmitter (fixedCycles);
    AdvanceReceiver    (fixedCycles);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEndpoint
//
//  Any change of attachment empties the receive queue and restores the modem
//  inputs to true, which is what a disconnected 6551 reads. The current DTR
//  and RTS states are then passed to the new attachment.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetEndpoint (IAciaEndpoint * endpoint)
{
    m_rxQueue.Clear();
    m_rxActive    = false;
    m_rxRemaining = 0;

    m_pendingCts.store (true);
    m_pendingDsr.store (true);
    m_pendingDcd.store (true);
    m_modemInputDirty.store (false);
    m_cts = true;
    m_dsr = true;
    m_dcd = true;

    m_endpoint = endpoint;

    if (m_endpoint != nullptr)
    {
        m_notifiedDtr   = IsDtrOn();
        m_notifiedRts   = IsRtsOn();
        m_notifiedBreak = false;
        m_endpoint->OnModemControlChanged (m_notifiedDtr, m_notifiedRts);
    }

    TryLoadTransmitter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnqueueReceivedBytes
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::EnqueueReceivedBytes (const Byte * bytes, size_t count)
{
    m_rxQueue.Push (bytes, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetModemInputs
//
//  Stored for the emulation thread, which applies them at its next tick.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetModemInputs (bool cts, bool dsr, bool dcd)
{
    m_pendingCts.store (cts);
    m_pendingDsr.store (dsr);
    m_pendingDcd.store (dcd);
    m_modemInputDirty.store (true, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWordLengthBits
//
//  Control bits 6-5 select 8/7/6/5-bit words (00..11 respectively).
//
////////////////////////////////////////////////////////////////////////////////

int Acia6551::GetWordLengthBits() const
{
    constexpr int  kMaxWordBits = 8;
    int            selector     = (m_control & kControlWordLenMask) >> kControlWordLenShift;



    return kMaxWordBits - selector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRtsOn
//
//  RTS is asserted for every transmitter-control value but 00, and in echo
//  mode, which requires those bits clear.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::IsRtsOn() const
{
    return ((m_command & kCommandTicMask) != kCommandTicOff) || ((m_command & kCommandEcho) != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTransmitCharacterCycles
//
////////////////////////////////////////////////////////////////////////////////

double Acia6551::GetTransmitCharacterCycles() const
{
    int64_t  fixed = GetCharacterFixedCycles (GetBaudRate (m_control & kControlBaudMask));



    return static_cast<double> (fixed) / static_cast<double> (1LL << kFixedShift);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBaudRate
//
//  Code 0 runs at the crystal divided by 16: 115,200 baud on the 1.8432 MHz
//  crystal the //c and the Super Serial Card use.
//
////////////////////////////////////////////////////////////////////////////////

double Acia6551::GetBaudRate (int code)
{
    constexpr double  kExternalDivisor = 16.0;
    double            rate             = static_cast<double> (kCrystalHz) / kExternalDivisor;



    if (code > 0 && code < static_cast<int> (size (s_kBaudRates)))
    {
        rate = s_kBaudRates[code];
    }

    return rate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetState
//
//  A hardware (RESB) reset clears command and control, empties both transmit
//  registers, clears RDRF and the error and IRQ bits, and sets TDRE. A
//  program reset (a write to the status address) clears command bits 0-4 and
//  the same status bits, and does not alter control or command bits 5-7.
//
//  Neither empties the receive queue: bytes an attachment sent are on the
//  wire, not in the chip. DTR and RTS are passed to the attachment afterward.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ResetState (bool hardware)
{
    m_status = kStatusTxEmpty;
    m_rxData = 0;

    if (hardware)
    {
        m_command     = 0;
        m_control     = 0;
        m_tdrFull     = false;
        m_tsrBusy     = false;
        m_txRemaining = 0;
    }
    else
    {
        m_command &= kCommandParityMask;
    }

    m_rxActive    = false;
    m_rxRemaining = 0;

    if (!m_dsr)
    {
        m_status |= kStatusDsr;
    }

    if (!m_dcd)
    {
        m_status |= kStatusDcd;
    }

    LowerIrq();
    NotifyModemControl();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadData
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::ReadData()
{
    m_status &= ~(kStatusRxFull | kStatusOverrun);

    return m_rxData;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadStatus
//
//  Reading the status register clears the IRQ latch and de-asserts the line.
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::ReadStatus()
{
    Byte   snapshot = m_status;



    LowerIrq();

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteData
//
//  Fills the transmit data register and clears TDRE. The byte moves into the
//  shift register at once when the shift register is idle.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::WriteData (Byte value)
{
    m_tdr      = value;
    m_tdrFull  = true;
    m_status  &= ~kStatusTxEmpty;

    TryLoadTransmitter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCommand
//
//  Writing the command register can newly enable an interrupt whose
//  condition is already true, which then fires at once. A change to DTR, RTS
//  or break is passed to the attachment.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::WriteCommand (Byte value)
{
    m_command = value;

    if (IsTxIrqEnabled() && (m_status & kStatusTxEmpty))
    {
        RaiseIrq();
    }

    if (IsRxIrqEnabled() && (m_status & kStatusRxFull))
    {
        RaiseIrq();
    }

    NotifyModemControl();
    TryLoadTransmitter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RaiseIrq
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::RaiseIrq()
{
    m_status |= kStatusIrq;

    if (m_irqBound && m_irqConnected && (m_ic != nullptr))
    {
        m_ic->Assert (m_irqSource);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LowerIrq
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::LowerIrq()
{
    m_status &= ~kStatusIrq;

    if (m_irqBound && (m_ic != nullptr))
    {
        m_ic->Clear (m_irqSource);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRxIrqEnabled
//
//  Receiver interrupts require DTR set and the receiver-IRQ-disable bit clear.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::IsRxIrqEnabled() const
{
    return (m_command & kCommandDtr) && !(m_command & kCommandRxIrqDisable);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTxIrqEnabled
//
//  Transmitter interrupts require DTR set and the transmitter-control field set
//  to the "IRQ enabled, RTS low" pattern (01).
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::IsTxIrqEnabled() const
{
    return (m_command & kCommandDtr) && ((m_command & kCommandTicMask) == kCommandTicTxIrqOn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanLoadTransmitter
//
//  A byte moves into the shift register only while CTS is true and the
//  transmitter is on: transmitter-control 00 turns it off and 11 sends break
//  in place of data.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::CanLoadTransmitter() const
{
    Byte  tic = static_cast<Byte> (m_command & kCommandTicMask);



    return m_cts && tic != kCommandTicOff && tic != kCommandTicBreak;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryLoadTransmitter
//
//  Moves a waiting byte from the transmit data register into an idle shift
//  register, sets TDRE, and raises the transmitter interrupt when enabled.
//  A negative remainder from the character just finished is added to the new
//  one, so back-to-back characters do not drift.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::TryLoadTransmitter()
{
    HRESULT  hr       = S_OK;
    int64_t  carry    = 0;
    bool     canStart = !m_tsrBusy && m_tdrFull && CanLoadTransmitter();



    BAIL_OUT_IF (!canStart, S_OK);

    carry = (m_txRemaining < 0) ? m_txRemaining : 0;

    m_tsr         = m_tdr;
    m_tdrFull     = false;
    m_tsrBusy     = true;
    m_txRemaining = carry + GetCharacterFixedCycles (GetBaudRate (m_control & kControlBaudMask));
    m_status     |= kStatusTxEmpty;

    if (IsTxIrqEnabled())
    {
        RaiseIrq();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceTransmitter
//
//  Finishes the character in the shift register once its time has run out,
//  passes it to the attachment, and loads the next one if it is waiting.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::AdvanceTransmitter (int64_t fixedCycles)
{
    HRESULT  hr   = S_OK;
    Byte     sent = 0;



    if (!m_tsrBusy)
    {
        TryLoadTransmitter();
    }

    BAIL_OUT_IF (!m_tsrBusy, S_OK);

    m_txRemaining -= fixedCycles;

    while (m_tsrBusy && m_txRemaining <= 0)
    {
        sent      = m_tsr;
        m_tsrBusy = false;

        if (m_endpoint != nullptr)
        {
            m_endpoint->OnByteTransmitted (sent);
        }

        TryLoadTransmitter();
    }

    if (!m_tsrBusy)
    {
        m_txRemaining = 0;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceReceiver
//
//  Lands queued bytes one receiver character time apart while DTR is on. For
//  a byte queued to an idle receiver, the countdown is set on the next tick
//  and runs from the tick after it, so a byte queued partway through a tick
//  (a loopback plug returning one) is not charged for time that passed before
//  it was queued. With DTR off the countdown pauses and the queue is not
//  emptied.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::AdvanceReceiver (int64_t fixedCycles)
{
    HRESULT  hr         = S_OK;
    Byte     value      = 0;
    size_t   queued     = m_rxQueue.GetCount();
    bool     dtrOn      = IsDtrOn();
    bool     startedNow = false;



    BAIL_OUT_IF (!dtrOn,                     S_OK);
    BAIL_OUT_IF (!m_rxActive && queued == 0, S_OK);

    startedNow = !m_rxActive;

    if (startedNow)
    {
        m_rxActive    = true;
        m_rxRemaining = GetCharacterFixedCycles (GetReceiveBaud());
    }

    BAIL_OUT_IF (startedNow, S_OK);

    m_rxRemaining -= fixedCycles;

    while (m_rxActive && m_rxRemaining <= 0)
    {
        if (m_rxQueue.TryPop (value))
        {
            LandReceivedByte (value);
        }

        if (m_rxQueue.GetCount() == 0)
        {
            m_rxActive    = false;
            m_rxRemaining = 0;
        }
        else
        {
            m_rxRemaining += GetCharacterFixedCycles (GetReceiveBaud());
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LandReceivedByte
//
//  Moves one byte into the receive data register, masked to the word
//  length. A byte landing over an unread one sets overrun and replaces it.
//  In echo mode the byte is also sent back out when the shift register is
//  idle.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::LandReceivedByte (Byte value)
{
    constexpr int  kByteBits = 8;
    int            wordBits  = GetWordLengthBits();
    Byte           mask      = static_cast<Byte> ((1u << wordBits) - 1);



    if (wordBits >= kByteBits)
    {
        mask = 0xFF;
    }

    if (m_status & kStatusRxFull)
    {
        m_status |= kStatusOverrun;
    }

    m_rxData  = static_cast<Byte> (value & mask);
    m_status |= kStatusRxFull;

    if (IsRxIrqEnabled())
    {
        RaiseIrq();
    }

    if ((m_command & kCommandEcho) && !m_tsrBusy && m_cts)
    {
        m_tsr         = m_rxData;
        m_tsrBusy     = true;
        m_txRemaining = GetCharacterFixedCycles (GetBaudRate (m_control & kControlBaudMask));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPendingModemInputs
//
//  Applies CTS, DSR and DCD stored by SetModemInputs. DSR and DCD read 0 in
//  the status register while true; a change in either raises the interrupt
//  under the same enables as the receiver's. CTS turning true lets a waiting
//  byte start.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ApplyPendingModemInputs()
{
    bool  cts     = m_pendingCts.load();
    bool  dsr     = m_pendingDsr.load();
    bool  dcd     = m_pendingDcd.load();
    bool  changed = (dsr != m_dsr) || (dcd != m_dcd);



    m_modemInputDirty.store (false);

    m_cts = cts;
    m_dsr = dsr;
    m_dcd = dcd;

    m_status = static_cast<Byte> ((m_status & ~(kStatusDsr | kStatusDcd)) |
                                  (dsr ? 0 : kStatusDsr) |
                                  (dcd ? 0 : kStatusDcd));

    if (changed && IsRxIrqEnabled())
    {
        RaiseIrq();
    }

    TryLoadTransmitter();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyModemControl
//
//  Passes DTR, RTS and break to the attachment when they differ from the
//  values last passed.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::NotifyModemControl()
{
    HRESULT  hr       = S_OK;
    bool     dtr      = IsDtrOn();
    bool     rts      = IsRtsOn();
    bool     breakNow = (m_command & kCommandTicMask) == kCommandTicBreak;



    BAIL_OUT_IF (m_endpoint == nullptr, S_OK);

    if (dtr != m_notifiedDtr || rts != m_notifiedRts)
    {
        m_notifiedDtr = dtr;
        m_notifiedRts = rts;
        m_endpoint->OnModemControlChanged (dtr, rts);
    }

    if (breakNow != m_notifiedBreak)
    {
        m_notifiedBreak = breakNow;
        m_endpoint->OnBreakChanged (breakNow);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCharacterFixedCycles
//
//  One character at `baud` in fixed-point CPU cycles: a start bit, the word,
//  a parity bit when parity is enabled, and the stop bits. Two stop bits
//  become one and a half with a 5-bit word and no parity. Counted in half
//  bits so the half stop bit is exact.
//
////////////////////////////////////////////////////////////////////////////////

int64_t Acia6551::GetCharacterFixedCycles (double baud) const
{
    constexpr int  kMinWordBits      = 5;
    int            wordBits          = GetWordLengthBits();
    bool           parity            = (m_command & kCommandParityEnable) != 0;
    bool           twoStop           = (m_control & kControlStopBits) != 0;
    int            halfBits          = 2 * (1 + wordBits + (parity ? 1 : 0));
    double         cycles            = 0.0;



    if (!twoStop)
    {
        halfBits += 2;
    }
    else if (wordBits == kMinWordBits && !parity)
    {
        halfBits += 3;
    }
    else
    {
        halfBits += 4;
    }

    cycles = static_cast<double> (m_cpuClockHz) * static_cast<double> (halfBits) / (2.0 * baud);

    return static_cast<int64_t> (llround (cycles * static_cast<double> (1LL << kFixedShift)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReceiveBaud
//
//  Control bit 4 set runs the receiver at the transmit rate; clear runs it
//  at the crystal divided by 16.
//
////////////////////////////////////////////////////////////////////////////////

double Acia6551::GetReceiveBaud() const
{
    double  baud = GetBaudRate (0);



    if (m_control & kControlRxClockBaud)
    {
        baud = GetBaudRate (m_control & kControlBaudMask);
    }

    return baud;
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
