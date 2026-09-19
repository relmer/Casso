#pragma once

#include "CpuStatus.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IOpcodeWatcher
//
//  Told by a Cpu of each fetch of an opcode it watches, and of the fetch after
//  each one, before the instruction runs: the registers are the ones it starts
//  from. An interrupt the CPU takes reads as opcode $00, BRK's, which is what
//  the 6502 forces into its instruction register to take one.
//
////////////////////////////////////////////////////////////////////////////////

class IOpcodeWatcher
{
public:
    virtual ~IOpcodeWatcher() = default;

    virtual void  OnWatchedFetch (Word pc, Byte sp, Byte opcode) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu
//
//  The 6502 core: registers, a private 64 KB memory array, and the microcode
//  table that drives execution.
//
//  This class owns its OWN memory rather than reading through a bus, which is
//  what makes it usable standalone -- the CLI runs programs against it with no
//  machine, no devices, and no configuration, and the conformance suites drive
//  it directly. The emulator layers a bus-backed CPU over it rather than the
//  other way around.
//
//  Execution is TABLE-DRIVEN through Microcode: an opcode indexes a row
//  carrying its operation, addressing mode, size, and cycle count, so
//  StepOne is one operand fetch plus one operation dispatch instead of a
//  256-case switch.
//
//  CpuOperations is a friend so the ALU primitives can act on registers and
//  flags directly. They are the CPU's own internals split out for readability,
//  not an external collaborator.
//
//  Peek and Poke are deliberately side-effect free and are what the
//  disassembler and debugger read through; nothing here dispatches to a
//  device, so inspecting memory can never disturb the machine.
//
////////////////////////////////////////////////////////////////////////////////

class Cpu
{
    friend class CpuOperations;

public:
    Cpu ();
    void Reset ();

    // Public accessors for CLI and external use
    Word                GetPC             () const            { return PC; }
    void                SetPC             (Word pc)           { PC = pc; }
    Byte                GetA              () const            { return A; }
    Byte                GetX              () const            { return X; }
    Byte                GetY              () const            { return Y; }
    Byte                GetSP             () const            { return SP; }
    const Microcode &   GetMicrocode      (Byte opcode) const { return instructionSet[opcode]; }
    const Microcode *   GetInstructionSet () const            { return instructionSet.data (); }

    void StepOne                  ();
    Byte GetLastInstructionCycles () const                   { return m_lastCycles; }
    Byte GetLastPenalties         () const                   { return m_lastPenalties; }
    Byte PeekByte                 (Word address) const       { return memory[address]; }
    void PokeByte                 (Word address, Byte value) { memory[address] = value; }
    Word PeekWord                 (Word address) const       { return memory[address] | (memory[(Word) (address + 1)] << 8); }
    const Byte * GetMemory        () const                   { return memory.data (); }

    // Load a raw binary file into memory at the specified address.
    // E_INVALIDARG if the file cannot be opened, or if the image does not fit
    // within the 64 KB address space starting at `address`; E_FAIL if the read
    // itself goes bad. On failure, memory contents are left unchanged.
    HRESULT LoadBinary (const std::string & filename, Word address);

    // Stream-based overload. Reads all remaining bytes from `stream` into
    // memory starting at `address`. Used directly by unit tests to avoid
    // touching the filesystem; the filename overload is a thin wrapper.
    HRESULT LoadBinary (std::istream & stream, Word address);

protected:
    struct OperandInfo
    {
        Word location;
        Word effectiveAddress;
        Word operand;
    };

    void PrintSingleStepInfo           (Word initialPC, Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandAndComment        (Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandBytes             (Word initialPC, Byte opcode);
    void FetchOperand                  (const Microcode & microcode, OperandInfo & operandInfo);
    void FetchOperandAbsoluteX         (Cpu::OperandInfo & operandInfo);
    void FetchOperandAbsoluteY         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageX         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageY         (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageIndirectY (Cpu::OperandInfo & operandInfo);
    void FetchOperandAbsolute          (Cpu::OperandInfo & operandInfo, const Microcode & microcode);
    void FetchOperandImmediate         (Cpu::OperandInfo & operandInfo);
    void FetchOperandJumpAbsolute      (Cpu::OperandInfo & operandInfo);
    void FetchOperandJumpIndirect      (Cpu::OperandInfo & operandInfo);
    void FetchOperandRelative          (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPage          (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo);

    // 65C02 addressing-mode fetches. Selected purely by the CMOS instruction
    // table; the NMOS table never references them, so NMOS decode is unchanged.
    // FetchOperandJumpIndirectCmos is the page-boundary-correct JMP ($xxFF).
    void FetchOperandZeroPageIndirect  (Cpu::OperandInfo & operandInfo);
    void FetchOperandAbsoluteXIndirect (Cpu::OperandInfo & operandInfo);
    void FetchOperandZeroPageRelative  (Cpu::OperandInfo & operandInfo);
    void FetchOperandJumpIndirectCmos  (Cpu::OperandInfo & operandInfo);

    void ExecuteInstruction            (const Microcode & microcode, const OperandInfo & operandInfo);

    // Stack operations
    void PushByte (Byte value);
    void PushWord (Word value);
    Byte PopByte  ();
    Word PopWord  ();

    // Memory operations. ReadByte is a non-virtual inline fast path: any page
    // with a non-null page-table entry (RAM $0000-$BFFF and, once the language
    // card wires them, ROM/LC RAM $D000-$FFFF) hits the table directly with no
    // indirect call -- the overwhelming majority of reads (instruction fetches
    // and most operands). Null pages -- I/O ($C000-$CFFF) and any unmapped
    // region -- fall through the virtual ReadByteSlow hook, which a derived
    // strategy (MemoryBusCpu) overrides to route through the emulator bus.
    // m_readPages is null on the standalone base CPU, so it always takes the
    // slow path into memory[].
    virtual void WriteByte     (Word address, Byte value);
    virtual void WriteWord     (Word address, Word value);
    Byte         ReadByte      (Word address)
    {
        if (m_readPages != nullptr)
        {
            Byte * page = m_readPages[address >> 8];

            if (page != nullptr)
            {
                return page[address & 0xFF];
            }
        }

        return ReadByteSlow (address);
    }

    virtual Byte ReadByteSlow  (Word address);
    virtual Word ReadWord      (Word address);

    void InitializeInstructionSet ();

    void InitializeGroup00 ();
    void InitializeGroup01 ();
    void InitializeGroup10 ();
    void InitializeMisc ();
    void InitializeUndocumented ();

    void CreateInstruction (uint32_t                        addressingModeMax, 
                            const char              * const instructionName[], 
                            Byte                            opcode, 
                            Byte                            addressingModeFlags, 
                            Byte                            group, 
                            Microcode::Operation            operation, 
                            Byte                    *       pSourceRegister,
                            Byte                    *       pDestinationRegister);
    


protected:
    static constexpr size_t memSize      = 64 * 1024;
    static constexpr size_t stackAddress = 0x0100;
    static constexpr Word   nmiVector    = 0xFFFA;
    static constexpr Word   resVector    = 0xFFFC;
    static constexpr Word   irqVector    = 0xFFFE;

    // Heap-allocated to keep the 64 KB 6502 address space off the stack
    // (otherwise every function that stack-allocates a Cpu blows past C6262's
    // 16 KB frame-size threshold during code analysis).
    std::vector<Byte>       memory {std::vector<Byte> (memSize, 0)};

    // Optional read fast-path page table (null on the standalone base CPU). A
    // derived strategy points this at its own 256-entry read-page map so RAM/
    // ROM reads bypass the virtual ReadByteSlow dispatch. Entries update in
    // place on banking changes; the pointer itself is set once at wire-up.
    Byte * const *          m_readPages = nullptr;

    Byte                    SP = 0;
    Word                    PC = 0;
    Byte                    A  = 0;
    Byte                    X  = 0;
    Byte                    Y  = 0;

    CpuStatus               status = {};

protected:
    // Heap-allocated for the same reason as `memory`: keeps the ~10 KB
    // instruction table off the stack of any function holding a Cpu.
    std::vector<Microcode> instructionSet {std::vector<Microcode> (256)};
    Byte                  m_lastCycles    = 0;
    Byte                  m_lastPenalties = 0;

public:
    // The penalty kinds the last instruction paid, one bit each, each worth one
    // cycle already counted in m_lastCycles. The profiler subtracts them to find
    // the base cost and bills them to their own buckets.
    static constexpr Byte  kPenaltyPageCross   = 0x01;
    static constexpr Byte  kPenaltyBranchTaken = 0x02;
    static constexpr Byte  kPenaltyBranchCross = 0x04;

    // Circular trace of recent instructions for post-mortem debugging.
    // Runtime-gated by m_traceEnabled and allocated only when tracing is
    // on, so it costs one predicted branch per step when disabled and is
    // available in every build config (driven by the --trace switch). In
    // Debug a small look-back ring is auto-enabled at construction so the
    // illegal-opcode stderr dump keeps working without --trace.
    //
    // The access fields hold the instruction's last data access, filled by
    // RecordTraceAccess while a bus reports every access (the debugger's
    // HISTORY); hasAccess stays false otherwise.
    struct TraceEntry
    {
        uint64_t  cycles;         // the cycle counter before the instruction
        Word      pc;
        Byte      opcode;
        Byte      op1;            // operand bytes at PC+1 and PC+2, read
        Byte      op2;            // through the bank the CPU executes from
        Byte      a;
        Byte      x;
        Byte      y;
        Byte      sp;
        Byte      p;
        Byte      intr;           // kTraceIntr*: this instruction is a handler entry
        bool      hasAccess;
        bool      accessIsWrite;
        Byte      accessData;
        Word      accessAddress;
    };

    // Interrupt-dispatch tags for the trace. The dispatch path stamps the
    // pending kind before the vector is taken; the next TracePush moves it
    // onto that entry (the handler's first instruction). This lets the dump
    // flag interrupt entries and summarize the rate -- an interrupt storm
    // (stuck IRQ line, unacknowledged source, ISR that never returns) is
    // otherwise invisible in a flat instruction trace.
    static constexpr Byte    kTraceIntrNone = 0;
    static constexpr Byte    kTraceIntrIrq  = 1;
    static constexpr Byte    kTraceIntrNmi  = 2;

protected:
    static constexpr size_t  kTraceDefaultLookback = 256;

    std::vector<TraceEntry>  m_trace;   // sized by EnableTrace
    size_t                   m_traceCapacity    = 0;
    size_t                   m_traceHead        = 0;   // next slot to write
    uint64_t                 m_traceCount       = 0;   // total entries pushed
    bool                     m_traceEnabled     = false;
    Byte                     m_pendingTraceIntr = kTraceIntrNone;

    // Where TracePush reads the cycle count and the operand bytes. A derived
    // CPU with a cycle counter points the first at it; one whose read table
    // can be unpublished page by page (a watched bus) points the second at a
    // table that always holds the mapped pages. Null means none and the read
    // table respectively.
    const uint64_t *         m_traceCycles      = nullptr;
    Byte * const *           m_tracePeekPages   = nullptr;

    // Called from the interrupt-dispatch path just before the vector is taken.
    // Cheap no-op while tracing is off (the common case).
    void MarkTraceInterrupt (Byte kind) { if (m_traceEnabled) m_pendingTraceIntr = kind; }

public:
    // Enable execution tracing with the given ring capacity (in entries).
    // Allocates the buffer and resets head/count. A capacity of 0 disables
    // tracing and frees the buffer.
    void     EnableTrace    (size_t capacity);
    bool     IsTraceEnabled () const { return m_traceEnabled; }
    uint64_t GetTraceCount  () const { return m_traceCount; }

    // Stops recording and keeps what the ring holds.
    void     StopTrace      ()       { m_traceEnabled = false; m_pendingTraceIntr = kTraceIntrNone; UpdateFetchObserved(); }

    // The opcodes, a 256-entry table, whose fetches the watcher is told of,
    // with the fetch after each; null for none. The table is read in place.
    void     SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher);

    // The entries the ring holds, oldest first.
    size_t   GetTraceSize     () const;
    bool     TryGetTraceEntry (size_t index, TraceEntry & entry) const;

    // A bus access, recorded as the last access of the instruction executing.
    void     RecordTraceAccess (Word address, Byte data, bool isWrite);

    // Write the recorded ring to a text file, oldest-first. `onProgress`
    // (may be empty) is invoked periodically with (entriesWritten,
    // totalEntries) so a UI can show progress. Returns true on success.
    // CassoCore stays UI-free; the caller owns any progress dialog.
    HRESULT  DumpTraceToFile (const std::wstring & path,
                              const std::function<void (uint64_t, uint64_t)> & onProgress) const;

protected:
    static constexpr Byte  kInterruptOpcode = 0x00;

    using OpcodeTable = std::array<bool, 0x100>;

    static constexpr OpcodeTable  s_kNoOpcodes  = {};
    static constexpr OpcodeTable  s_kAllOpcodes = [] { OpcodeTable all {}; all.fill (true); return all; }();

    // Whether a fetch is looked at at all: the trace is on, or an opcode watch
    // is set. One test per instruction while neither is.
    bool                     m_isFetchObserved  = false;

    // The watcher's table, and the one the next fetch is looked up in: the
    // watcher's, or every opcode for the fetch after a watched one.
    const bool             * m_watchOpcodes     = nullptr;
    const bool             * m_watchFetch       = s_kNoOpcodes.data();
    IOpcodeWatcher         * m_watcher          = nullptr;

    void UpdateFetchObserved () { m_isFetchObserved = m_traceEnabled || m_watcher != nullptr; }
    void ReportWatchedFetch  (Byte opcode);

    // Forced inline: it runs once per instruction while a fetch is observed.
    __forceinline void ObserveFetch (Byte opcode)
    {
        if (m_watchFetch[opcode])
        {
            ReportWatchedFetch (opcode);
        }

        if (m_traceEnabled)
        {
            TracePush (opcode);
        }
    }

    // An interrupt about to be taken in place of the instruction at PC.
    void ObserveInterrupt ()
    {
        if (m_isFetchObserved && m_watchFetch[kInterruptOpcode])
        {
            ReportWatchedFetch (kInterruptOpcode);
        }
    }

    void TracePush    (Byte opcode);

    // Side-effect-free read for the trace, routed through the same read-page
    // table the fetch uses so operands come from the bank that executed.
    Byte PeekForTrace (Word address) const;
    void DumpInstructionTrace (Byte faultOpcode, Word faultPC) const;
};
