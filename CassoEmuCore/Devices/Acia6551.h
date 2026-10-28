#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/IInterruptController.h"
#include "Core/IMachineState.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Devices/SerialByteQueue.h"

class MemoryBus;
class IAciaEndpoint;
class ICommEventSink;
class InputJournal;
class HostInputGate;

struct DeviceConfig;
struct InputRecord;

enum class IoEventKind : uint8_t;





////////////////////////////////////////////////////////////////////////////////
//
//  Acia6551
//
//  6551 ACIA (Asynchronous Communications Interface Adapter) as used by the
//  Apple //c built-in serial ports and the Super Serial Card. Four registers
//  occupy a slot's I/O page at $C088 + slot*16:
//
//      +0  Data     read = receiver data (RDR), write = transmit data (TDR)
//      +1  Status   read = status, write = program reset
//      +2  Command  DTR, receiver-IRQ enable, transmitter control, parity
//      +3  Control  baud rate, receiver clock, word length, stop bits
//
//  Transmit and receive both take one character time per byte, measured in
//  CPU cycles against the 1.8432 MHz crystal both the //c and the SSC use.
//  Transmit is double-buffered as on the part: a data write fills the
//  transmit data register, which moves into an idle shift register at once,
//  so TDRE sets again until a second byte is written while the first is
//  shifting. Each byte is passed to the attachment when its shift ends.
//
//  Received bytes wait in a queue any thread may fill, and land in the
//  single receive data register one character time apart; a byte landing
//  over an unread one sets overrun, as on the part. The queue is not a FIFO
//  the guest can see.
//
//  A host attachment's bytes arrive from outside the machine, so the
//  emulation thread takes them into the receive queue at an instruction
//  boundary and, while the input journal is attached, journals each one
//  there; in a replay they come from the journal. Behind live, while
//  HostInputGate::IsHeld returns true, the bytes wait. A byte a loopback plug
//  returns follows from what the chip sent, so a replay sends it again; it
//  goes straight into the queue.
//
//  The IRQ status bit (bit 7) is a latch: set when an enabled interrupt
//  condition occurs, cleared by reading the status register. Whether the
//  latch drives the CPU's IRQ line depends on the board's wiring (the SSC's
//  SW2-6), set through SetIrqConnected.
//
////////////////////////////////////////////////////////////////////////////////

class Acia6551 : public MemoryDevice, public IMachineState, public IDiagnosticsProvider
{
public:
    // Slot I/O geometry: slot n decodes $C080 + n*16; the ACIA sits at
    // offset 8 within that page.
    static constexpr Word  kSlotIoBase    = 0xC080;
    static constexpr Word  kSlotIoStride  = 0x10;
    static constexpr Word  kAciaRegOffset = 0x08;

    static constexpr Word  kRegData       = 0;
    static constexpr Word  kRegStatus     = 1;
    static constexpr Word  kRegCommand    = 2;
    static constexpr Word  kRegControl    = 3;
    static constexpr Word  kRegisterCount = 4;

    // The serial log's port: a card's slot number, or a built-in port's
    // number with this bit set.
    static constexpr Byte  kCommPortBuiltIn = 0x80;

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
    static constexpr Byte    kCommandTicOff        = 0x00;
    static constexpr Byte    kCommandTicTxIrqOn    = 0x04;
    static constexpr Byte    kCommandTicTxIrqOff   = 0x08;
    static constexpr Byte    kCommandTicBreak      = 0x0C;
    static constexpr Byte    kCommandEcho          = 0x10;
    static constexpr Byte    kCommandParityEnable  = 0x20;
    static constexpr Byte    kCommandParityMask    = 0xE0;

    // Control register bits.
    static constexpr Byte    kControlBaudMask     = 0x0F;
    static constexpr Byte    kControlRxClockBaud  = 0x10;
    static constexpr Byte    kControlWordLenMask  = 0x60;
    static constexpr Byte    kControlWordLenShift = 5;
    static constexpr Byte    kControlStopBits     = 0x80;

    // Timing bases. The CPU clock defaults to the NTSC Apple II's and is set
    // from the machine config by the builder.
    static constexpr uint32_t  kCrystalHz         = 1843200;
    static constexpr uint32_t  kDefaultCpuClockHz = 1022727;

    explicit                 Acia6551 (Word baseAddress);

    // MemoryDevice
    Byte    Read       (Word address) override;
    bool    TryPeekIo  (Word address, Byte & value) override;
    void    Write      (Word address, Byte value) override;
    Word    GetStart   () const override { return m_baseAddress; }
    Word    GetEnd     () const override { return m_ioEnd; }
    void    Reset      () override;
    void    SoftReset  () override;
    void    PowerCycle (Prng & prng) override;

    // Register the ACIA's IRQ source with the interrupt controller. The
    // controller is caller-owned and outlives the device.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // Whether the IRQ latch drives the CPU's IRQ line. Defaults to true.
    void    SetIrqConnected (bool connected);

    // Timing. Tick advances the shift register and receive delivery; the
    // machine calls it once per instruction with the cycles it took.
    void    SetCpuClockHz (uint32_t hz);
    void    Tick          (uint32_t cycles);

    // Route transmitted bytes. Caller-owned; pass nullptr to detach. Any
    // change empties the receive queue and restores CTS, DSR and DCD to true.
    // Emulation thread only.
    void    SetEndpoint (IAciaEndpoint * endpoint);

    // While muted, as in a replay of what was sent live, transmitted bytes
    // go to a loopback plug only. Emulation thread.
    void    SetOutputMuted (bool isMuted) { m_isOutputMuted = isMuted; }

    // Called by an attachment, from any thread.
    void    EnqueueReceivedBytes  (const Byte * bytes, size_t count);
    void    SetModemInputs        (bool cts, bool dsr, bool dcd);
    size_t  GetQueuedReceiveCount () const { return m_rxInbox.GetCount() + m_rxQueue.GetCount(); }

    // Called by a loopback plug, on the emulation thread, with a byte the
    // chip itself sent.
    void    EnqueueLoopedBackByte (Byte value);

    // A resume from the past, on the emulation thread: the bytes the chip
    // took from its attachment after the point go to the receiver again,
    // behind the ones restored with the point.
    void    RequeueReceivedBytes  (std::span<const Byte> bytes) { m_rxQueue.Push (bytes.data(), bytes.size()); }

    // Reverse execution: the journal for the bytes taken from an attachment,
    // or null, with this chip's index among the machine's serial chips; the
    // gate checked before they are taken; and, in a replay, a journaled byte
    // put back into the receive queue. Emulation thread.
    void    SetInputJournal  (InputJournal * journal, uint16_t chipIndex);
    void    SetHostInputGate (const HostInputGate * gate) { m_inputGate = gate; }
    bool    ApplyInput       (const InputRecord & record);

    // The debugger's serial log: settings, line changes, each byte sent and
    // received and each overrun go to `sink`, or nowhere when it is null,
    // as this chip's port. Emulation thread.
    void    SetCommEventSink (ICommEventSink * sink);
    void    SetCommPort      (Byte port) { m_commPort = port; }
    Byte    GetCommPort      () const    { return m_commPort; }

    // Inspectors for tests and board wiring.
    Byte    GetStatus  () const { return m_status; }
    Byte    GetCommand () const { return m_command; }
    Byte    GetControl () const { return m_control; }
    int     GetWordLengthBits () const;
    bool    IsCtsTrue  () const { return m_cts; }
    bool    IsDtrOn    () const { return (m_command & kCommandDtr) != 0; }
    bool    IsRtsOn    () const;

    // Character time in CPU cycles for the transmitter's current format.
    double  GetTransmitCharacterCycles () const;

    // Bits per second for a control-register baud code, 0 being crystal / 16.
    static double  GetBaudRate (int code);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // IMachineState: the registers, the transmitter and receiver in flight,
    // the bytes waiting to be received, the modem lines and the modem control
    // already reported. The endpoint, the IRQ source and the clock are
    // wiring, bytes already handed to the endpoint are output and are not
    // taken back, and the interrupt controller saves whether the line is held.
    // Version 1 has the four registers only.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', 'C', 'I', 'A');
    static constexpr uint16_t  kStateVersion = 2;

    // IDiagnosticsProvider, for a port that is a device by itself, as each
    // of the //c's is: the id and title come from the builder. A card with
    // a 6551 adds its rows to the card's panel through AddDiagnostics.
    void         SetDiagnosticsName  (std::string id, std::string title) { m_diagnosticsId = std::move (id); m_diagnosticsTitle = std::move (title); }
    std::string  GetDiagnosticsId    () const override { return m_diagnosticsId; }
    std::string  GetDiagnosticsTitle () const override { return m_diagnosticsTitle; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;
    void         AddIoRegisters      (std::vector<DiagnosticsIoRegister> & registers) const override;

    // The four registers, each description after owner: a card's name and
    // slot, or a port's title.
    void         AddRegisterDescriptions (std::vector<DiagnosticsIoRegister> & registers, const std::string & owner) const;
    void         AddDiagnostics      (DiagnosticsSnapshot & snapshot) const;

private:
    // Fixed point: cycle counts are stored scaled by 2^16 so pacing never
    // accumulates rounding.
    static constexpr int     kFixedShift = 16;

    void    ResetState             (bool hardware);
    Byte    ReadData               ();
    Byte    ReadStatus             ();
    void    WriteData              (Byte value);
    void    WriteCommand           (Byte value);
    void    RaiseIrq               ();
    void    LowerIrq               ();
    bool    IsRxIrqEnabled         () const;
    bool    IsTxIrqEnabled         () const;
    bool    CanLoadTransmitter     () const;
    void    TryLoadTransmitter     ();
    void    AdvanceTransmitter     (int64_t fixedCycles);
    void    TakeHostBytes          (uint32_t cycles);
    void    AdvanceReceiver        (int64_t fixedCycles);
    void    LandReceivedByte       (Byte value);
    void    ApplyPendingModemInputs();
    void    NotifyModemControl     ();
    int64_t GetCharacterFixedCycles (double baud) const;
    double  GetReceiveBaud         () const;
    Byte    GetLineBits            () const;
    void    NoteLines              ();
    void    EmitComm               (IoEventKind kind, Byte value);

    Word                     m_baseAddress = 0;
    Word                     m_ioEnd       = 0;

protected:
    Byte                     m_status  = 0;
    Byte                     m_command = 0;
    Byte                     m_control = 0;
    Byte                     m_rxData  = 0;

private:
    Byte                     m_tdr         = 0;
    bool                     m_tdrFull     = false;
    Byte                     m_tsr         = 0;
    bool                     m_tsrBusy     = false;
    int64_t                  m_txRemaining = 0;

    SerialByteQueue          m_rxInbox;                // an attachment's bytes, until the emulation thread takes them
    SerialByteQueue          m_rxQueue;
    bool                     m_rxActive    = false;
    int64_t                  m_rxRemaining = 0;

    bool          m_cts             = true;
    bool          m_dsr             = true;
    bool          m_dcd             = true;
    atomic<bool>  m_pendingCts      = true;
    atomic<bool>  m_pendingDsr      = true;
    atomic<bool>  m_pendingDcd      = true;
    atomic<bool>  m_modemInputDirty = false;

    bool                     m_notifiedDtr   = false;
    bool                     m_notifiedRts   = false;
    bool                     m_notifiedBreak = false;

    uint32_t                 m_cpuClockHz = kDefaultCpuClockHz;

    IInterruptController *    m_ic           = nullptr;
    IrqSourceId               m_irqSource    = 0;
    bool                      m_irqBound     = false;
    bool                      m_irqConnected = true;

    IAciaEndpoint *           m_endpoint      = nullptr;
    bool                      m_isOutputMuted = false;

    InputJournal         *    m_inputJournal = nullptr;
    uint16_t                  m_chipIndex    = 0;
    const HostInputGate  *    m_inputGate    = nullptr;

    ICommEventSink       *    m_commSink     = nullptr;
    Byte                      m_commPort     = 0;
    Byte                      m_notedLines   = 0;       // the lines in the last serial log entry

    std::string               m_diagnosticsId;
    std::string               m_diagnosticsTitle;
};
