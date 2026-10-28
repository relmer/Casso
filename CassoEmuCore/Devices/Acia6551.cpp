#include "Pch.h"

#include "Acia6551.h"
#include "IAciaEndpoint.h"
#include "Devices/Serial/SerialLineFormat.h"
#include "Core/MachineConfig.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Debugger/Events/ICommEventSink.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Shell/HostInputGate.h"





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

    NoteLines();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  Read's byte with RDRF and the IRQ latch left as they are.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::TryPeekIo (Word address, Byte & value)
{
    Word  reg = static_cast<Word> (address - m_baseAddress);



    switch (reg)
    {
    case kRegData:    value = m_rxData;  break;
    case kRegStatus:  value = m_status;  break;
    case kRegCommand: value = m_command; break;
    case kRegControl: value = m_control; break;
    default:          value = 0;         break;
    }

    return true;
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
        EmitComm (IoEventKind::SerialSettings, 0);
        break;

    case kRegControl:
        m_control = value;
        EmitComm (IoEventKind::SerialSettings, 0);
        break;

    default:
        break;
    }

    NoteLines();
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
    size_t   arrived     = m_rxInbox.GetCount();
    bool     idle        = !m_tsrBusy && !m_tdrFull && queued == 0 && arrived == 0 && !m_rxActive && !modemDirty;



    BAIL_OUT_IF (idle, S_OK);

    if (modemDirty)
    {
        ApplyPendingModemInputs();
    }

    if (arrived != 0)
    {
        TakeHostBytes (cycles);
    }

    AdvanceTransmitter (fixedCycles);
    AdvanceReceiver    (fixedCycles);

    NoteLines();

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
    m_rxInbox.Clear();
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
    m_rxInbox.Push (bytes, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnqueueLoopedBackByte
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::EnqueueLoopedBackByte (Byte value)
{
    m_rxQueue.Push (&value, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReceiveFromHostNow
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::ReceiveFromHostNow (Byte value, Byte errors)
{
    m_rxQueue.PushEntry (value, errors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputJournal
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetInputJournal (InputJournal * journal, uint16_t chipIndex)
{
    m_inputJournal = journal;
    m_chipIndex    = chipIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInput
//
//  A journaled byte goes back into the receive queue where the emulation
//  thread took it live, ahead of the receiver's tick, with the receive
//  errors in the record's data.
//
////////////////////////////////////////////////////////////////////////////////

bool Acia6551::ApplyInput (const InputRecord & record)
{
    bool  isApplied = record.kind == InputKind::AciaByte && record.detail == m_chipIndex;



    if (isApplied)
    {
        m_rxQueue.PushEntry (record.value, (Byte) record.data);
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::SetCommEventSink
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::SetCommEventSink (
    ICommEventSink  * sink,
    Byte              port)
{
    m_commSink   = sink;
    m_commPort   = port;
    m_notedLines = GetLineBits();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::GetLineBits
//
//  The lines on, as IoEventLog's kLine bits.
//
////////////////////////////////////////////////////////////////////////////////

Byte Acia6551::GetLineBits() const
{
    Byte  bits = 0;



    bits |= IsDtrOn()                  ? IoEventLog::kLineDtr : 0;
    bits |= IsRtsOn()                  ? IoEventLog::kLineRts : 0;
    bits |= m_cts                      ? IoEventLog::kLineCts : 0;
    bits |= m_dsr                      ? IoEventLog::kLineDsr : 0;
    bits |= m_dcd                      ? IoEventLog::kLineDcd : 0;
    bits |= (m_status & kStatusIrq)    ? IoEventLog::kLineIrq : 0;

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::NoteLines
//
//  A serial log entry for the lines, when they differ from the last one.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::NoteLines()
{
    Byte  bits = 0;



    if (m_commSink == nullptr)
    {
        return;
    }

    bits = GetLineBits();

    if (bits != m_notedLines)
    {
        m_notedLines = bits;
        EmitComm (IoEventKind::SerialLines, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::EmitComm
//
//  A received byte's receive errors go in the entry's reg.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::EmitComm (
    IoEventKind  kind,
    Byte         value,
    Byte         errors)
{
    IoEvent  event = IoEvent();



    if (m_commSink == nullptr)
    {
        return;
    }

    event.kind    = kind;
    event.address = m_baseAddress;
    event.chip    = m_commPort;
    event.value   = value;
    event.reg     = errors;
    event.control = m_control;
    event.command = m_command;
    event.lines   = GetLineBits();

    m_commSink->OnCommEvent (event, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeHostBytes
//
//  Moves an attachment's bytes into the receive queue and, while the input
//  journal is attached, journals each as made during the instruction just
//  run. Behind live, when HostInputGate::IsHeld returns true, the journal
//  supplies the bytes, so these wait.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::TakeHostBytes (uint32_t cycles)
{
    Byte  value = 0;



    if (m_inputGate != nullptr && m_inputGate->IsHeld())
    {
        return;
    }

    while (m_rxInbox.TryPop (value))
    {
        m_rxQueue.Push (&value, 1);

        if (m_inputJournal != nullptr)
        {
            m_inputJournal->RecordObserved (m_inputJournal->GetCycle() - cycles, InputKind::AciaByte, value, m_chipIndex, 0);
        }
    }
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
//  GetLineFormat
//
////////////////////////////////////////////////////////////////////////////////

SerialLineFormat Acia6551::GetLineFormat() const
{
    return SerialLineFormat::FromRegisters (m_control, m_command);
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
//  SerialLineFormat's table; code 0 runs at the crystal divided by 16.
//
////////////////////////////////////////////////////////////////////////////////

double Acia6551::GetBaudRate (int code)
{
    return SerialLineFormat::GetBaudRate (code);
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

    EmitComm (IoEventKind::SerialSent, m_tsr);

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

        if (m_endpoint != nullptr && (!m_isOutputMuted || m_endpoint->IsLoopback()))
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
    Byte     errors     = 0;
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
        if (m_rxQueue.TryPop (value, errors))
        {
            LandReceivedByte (value, errors);
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
//  length. Its parity and framing errors replace the last byte's, as the
//  6551 reports them per character. A byte landing over an unread one, or
//  one the PC lost bytes before, sets overrun. In echo mode the byte is also
//  sent back out when the shift register is idle.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::LandReceivedByte (Byte value, Byte errors)
{
    constexpr int   kByteBits  = 8;
    constexpr Byte  kCharError = kStatusParityError | kStatusFramingError;
    int             wordBits   = GetWordLengthBits();
    Byte            mask       = static_cast<Byte> ((1u << wordBits) - 1);



    if (wordBits >= kByteBits)
    {
        mask = 0xFF;
    }

    if (m_status & kStatusRxFull)
    {
        m_status |= kStatusOverrun;
        EmitComm (IoEventKind::SerialOverrun, m_rxData);
    }

    m_rxData  = static_cast<Byte> (value & mask);
    m_status  = static_cast<Byte> ((m_status & ~kCharError) | (errors & kCharError) | kStatusRxFull);
    m_status |= errors & kRxErrorOverrun;

    EmitComm (IoEventKind::SerialReceived, m_rxData, errors);

    if (IsRxIrqEnabled())
    {
        RaiseIrq();
    }

    if ((m_command & kCommandEcho) && !m_tsrBusy && m_cts)
    {
        m_tsr         = m_rxData;
        m_tsrBusy     = true;
        m_txRemaining = GetCharacterFixedCycles (GetBaudRate (m_control & kControlBaudMask));

        EmitComm (IoEventKind::SerialSent, m_tsr);
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
//  One character at `baud` in fixed-point CPU cycles, from the half bits the
//  format has per character.
//
////////////////////////////////////////////////////////////////////////////////

int64_t Acia6551::GetCharacterFixedCycles (double baud) const
{
    int     halfBits = GetLineFormat().GetHalfBitsPerCharacter();
    double  cycles   = static_cast<double> (m_cpuClockHz) * static_cast<double> (halfBits) / (2.0 * baud);



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





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Acia6551::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    std::vector<SerialByteQueue::Entry>  waiting;



    m_rxQueue.CopyTo (waiting);

    writer.WriteByte (m_status);
    writer.WriteByte (m_command);
    writer.WriteByte (m_control);
    writer.WriteByte (m_rxData);

    writer.WriteByte   (m_tdr);
    writer.WriteBool   (m_tdrFull);
    writer.WriteByte   (m_tsr);
    writer.WriteBool   (m_tsrBusy);
    writer.WriteUInt64 ((uint64_t) m_txRemaining);
    writer.WriteBool   (m_rxActive);
    writer.WriteUInt64 ((uint64_t) m_rxRemaining);
    writer.WriteUInt32 ((uint32_t) waiting.size());

    for (const SerialByteQueue::Entry & entry : waiting)
    {
        writer.WriteByte (entry.value);
        writer.WriteByte (entry.errors);
    }

    writer.WriteBool (m_cts);
    writer.WriteBool (m_dsr);
    writer.WriteBool (m_dcd);
    writer.WriteBool (m_pendingCts.load());
    writer.WriteBool (m_pendingDsr.load());
    writer.WriteBool (m_pendingDcd.load());
    writer.WriteBool (m_modemInputDirty.load());
    writer.WriteBool (m_notifiedDtr);
    writer.WriteBool (m_notifiedRts);
    writer.WriteBool (m_notifiedBreak);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Acia6551::LoadState (StateReader & reader)
{
    constexpr uint16_t  kFirstWithTiming = 2;
    constexpr uint32_t  kMaxWaiting      = 1u << 20;
    HRESULT             hr               = S_OK;
    uint16_t            version          = 0;
    uint64_t            remaining        = 0;
    uint32_t            count            = 0;
    bool                flag             = false;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (m_status);
    reader.ReadByte (m_command);
    reader.ReadByte (m_control);
    reader.ReadByte (m_rxData);

    //  A version 1 state has no transmitter or receiver in flight.
    m_tdr         = 0;
    m_tdrFull     = false;
    m_tsr         = 0;
    m_tsrBusy     = false;
    m_txRemaining = 0;
    m_rxActive    = false;
    m_rxRemaining = 0;
    m_rxQueue.Clear();

    if (version >= kFirstWithTiming)
    {
        reader.ReadByte   (m_tdr);
        reader.ReadBool   (m_tdrFull);
        reader.ReadByte   (m_tsr);
        reader.ReadBool   (m_tsrBusy);
        reader.ReadUInt64 (remaining);
        m_txRemaining = (int64_t) remaining;
        reader.ReadBool   (m_rxActive);
        reader.ReadUInt64 (remaining);
        m_rxRemaining = (int64_t) remaining;
        reader.ReadUInt32 (count);
        CBREx (count <= kMaxWaiting, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        LoadWaitingBytes (reader, version, count);

        reader.ReadBool (m_cts);
        reader.ReadBool (m_dsr);
        reader.ReadBool (m_dcd);
        reader.ReadBool (flag);
        m_pendingCts = flag;
        reader.ReadBool (flag);
        m_pendingDsr = flag;
        reader.ReadBool (flag);
        m_pendingDcd = flag;
        reader.ReadBool (flag);
        m_modemInputDirty = flag;
        reader.ReadBool (m_notifiedDtr);
        reader.ReadBool (m_notifiedRts);
        reader.ReadBool (m_notifiedBreak);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadWaitingBytes
//
//  The bytes waiting to be received: a byte each before version 3, a byte
//  and its receive errors from version 3 on.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::LoadWaitingBytes (
    StateReader  & reader,
    uint16_t       version,
    uint32_t       count)
{
    constexpr uint16_t  kFirstWithErrors = 3;
    uint32_t            i                = 0;
    Byte                value            = 0;
    Byte                errors           = 0;



    for (i = 0; i < count; i++)
    {
        reader.ReadByte (value);

        if (version >= kFirstWithErrors)
        {
            reader.ReadByte (errors);
        }

        m_rxQueue.PushEntry (value, errors);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    AddDiagnostics (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::AddIoRegisters
//
//  A port that is a device by itself, as each of the //c's, under its
//  title.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::AddIoRegisters (std::vector<DiagnosticsIoRegister> & registers) const
{
    AddRegisterDescriptions (registers, m_diagnosticsTitle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551::AddRegisterDescriptions
//
//  The panel's labels for the registers. A write to the status register
//  resets the chip.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::AddRegisterDescriptions (std::vector<DiagnosticsIoRegister> & registers, const std::string & owner) const
{
    registers.push_back ({ (Word) (m_baseAddress + kRegData),    owner + ": 6551 data register; read the received byte, write a byte to send" });
    registers.push_back ({ (Word) (m_baseAddress + kRegStatus),  owner + ": 6551 status register; a write resets the chip" });
    registers.push_back ({ (Word) (m_baseAddress + kRegCommand), owner + ": 6551 command register: parity, echo, transmit interrupt and DTR" });
    registers.push_back ({ (Word) (m_baseAddress + kRegControl), owner + ": 6551 control register: baud rate, word length and stop bits" });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDiagnostics
//
//  The three registers as a guest read returns them, without the side
//  effects of the read, decoded bit by bit and into the format they set; then the
//  modem lines, what is waiting and what is on the connector.
//
////////////////////////////////////////////////////////////////////////////////

void Acia6551::AddDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    DiagnosticsGroup  registers { "6551", {} };
    DiagnosticsGroup  lines     { "Modem lines", {} };
    DiagnosticsGroup  traffic   { "Traffic", {} };
    std::string       attached  = "none";



    if (m_endpoint != nullptr)
    {
        attached = m_endpoint->IsLoopback() ? "loopback plug" : "attached";
    }

    registers.rows.push_back (MakeByteRow ("Status",  m_status,  { "IRQ", "DSR", "DCD", "TX EMPTY", "RX FULL", "OVERRUN", "FRAMING", "PARITY" }));
    registers.rows.push_back (MakeByteRow ("Command", m_command, { "PMC1", "PMC0", "PME", "ECHO", "TIC1", "TIC0", "IRD", "DTR" }));
    registers.rows.push_back (MakeByteRow ("Control", m_control, { "SBN", "WL1", "WL0", "RCS", "SBR3", "SBR2", "SBR1", "SBR0" }));
    registers.rows.push_back (MakeTextRow ("Baud",        std::format ("{:g}", GetBaudRate (m_control & kControlBaudMask))));
    registers.rows.push_back (MakeTextRow ("Word length", std::format ("{}", GetWordLengthBits())));
    registers.rows.push_back (MakeTextRow ("Stop bits",   (m_control & kControlStopBits) ? "2" : "1"));
    registers.rows.push_back (MakeHexRow  ("Received",    m_rxData, kByteDigits));

    lines.rows.push_back (MakeFlagRow ("DTR", IsDtrOn()));
    lines.rows.push_back (MakeFlagRow ("RTS", IsRtsOn()));
    lines.rows.push_back (MakeFlagRow ("CTS", m_cts));
    lines.rows.push_back (MakeFlagRow ("DSR", m_dsr));
    lines.rows.push_back (MakeFlagRow ("DCD", m_dcd));

    traffic.rows.push_back (MakeFlagRow ("Sending",       m_tsrBusy || m_tdrFull));
    traffic.rows.push_back (MakeTextRow ("Bytes waiting", std::format ("{}", GetQueuedReceiveCount())));
    traffic.rows.push_back (MakeTextRow ("Attachment",    attached));

    snapshot.groups.push_back (std::move (registers));
    snapshot.groups.push_back (std::move (lines));
    snapshot.groups.push_back (std::move (traffic));
}
