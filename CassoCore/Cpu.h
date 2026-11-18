#pragma once

#include "CpuFetchObservers.h"
#include "CpuOperandInfo.h"
#include "CpuStatus.h"
#include "CpuTrace.h"
#include "Microcode.h"





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
    Byte                GetP              () const            { return status.status; }
    const Microcode &   GetMicrocode      (Byte opcode) const { return instructionSet[opcode]; }
    const Microcode *   GetInstructionSet () const            { return instructionSet.data (); }

    void StepOne                  ();
    Byte GetLastInstructionCycles () const                   { return m_lastCycles; }
    Byte GetLastPenalties         () const                   { return m_lastPenalties; }
    bool TryGetLastBranch         (Word & from) const        { from = m_lastBranchFrom; return m_hasLastBranch; }
    bool TryGetLastBranchTarget   (Word & to) const          { to = m_lastBranchTo; return m_hasLastBranchTo; }

    // The address of the instruction StepOne is executing, or last executed,
    // for a device that reports which instruction touched it.
    const Word * GetInstructionStartPtr () const             { return &m_instructionStart; }

    Byte          PeekByte  (Word address) const       { return memory[address]; }
    void          PokeByte  (Word address, Byte value) { memory[address] = value; }
    Word          PeekWord  (Word address) const       { return memory[address] | (memory[(Word) (address + 1)] << 8); }
    const Byte  * GetMemory () const                   { return memory.data (); }

    // Load a raw binary file into memory at the specified address.
    // E_INVALIDARG if the file cannot be opened, or if the image does not fit
    // within the 64 KB address space starting at `address`; E_FAIL if the read
    // itself goes bad. On failure, memory contents are left unchanged.
    HRESULT LoadBinary (const std::string & filename, Word address);

    // Stream-based overload. Reads all remaining bytes from `stream` into
    // memory starting at `address`. Used directly by unit tests to avoid
    // touching the filesystem; the filename overload is a thin wrapper.
    HRESULT LoadBinary (std::istream & stream, Word address);

    // The trace of recent instructions, and what each opcode fetch goes to.
    CpuTrace                  & GetTrace          ()       { return m_trace; }
    const CpuTrace            & GetTrace          () const { return m_trace; }
    CpuFetchObservers         & GetFetchObservers ()       { return m_fetchObservers; }

protected:
    static bool  ReadsTarget                   (const Microcode & microcode);
    Byte         GetIndirectPointerHigh        (const Microcode & microcode, const CpuOperandInfo & operandInfo) const;
    void         FetchOperand                  (const Microcode & microcode, CpuOperandInfo & operandInfo);
    void         FetchOperandAbsoluteX         (CpuOperandInfo & operandInfo);
    void         FetchOperandAbsoluteY         (CpuOperandInfo & operandInfo);
    void         FetchOperandZeroPageX         (CpuOperandInfo & operandInfo);
    void         FetchOperandZeroPageY         (CpuOperandInfo & operandInfo);
    void         FetchOperandZeroPageIndirectY (CpuOperandInfo & operandInfo);
    void         FetchOperandAbsolute          (CpuOperandInfo & operandInfo);
    void         FetchOperandImmediate         (CpuOperandInfo & operandInfo);
    void         FetchOperandJumpAbsolute      (CpuOperandInfo & operandInfo);
    void         FetchOperandJumpIndirect      (CpuOperandInfo & operandInfo);
    void         FetchOperandRelative          (CpuOperandInfo & operandInfo);
    void         FetchOperandZeroPage          (CpuOperandInfo & operandInfo);
    void         FetchOperandZeroPageXIndirect (CpuOperandInfo & operandInfo);

    // 65C02 addressing-mode fetches. Selected purely by the CMOS instruction
    // table; the NMOS table never references them, so NMOS decode is unchanged.
    // FetchOperandJumpIndirectCmos is the page-boundary-correct JMP ($xxFF).
    void FetchOperandZeroPageIndirect  (CpuOperandInfo & operandInfo);
    void FetchOperandAbsoluteXIndirect (CpuOperandInfo & operandInfo);
    void FetchOperandZeroPageRelative  (CpuOperandInfo & operandInfo);
    void FetchOperandJumpIndirectCmos  (CpuOperandInfo & operandInfo);

    void ExecuteInstruction            (const Microcode & microcode, const CpuOperandInfo & operandInfo);

    // Called for an instruction flagged isExtendedIndirect, after the pointer
    // is read and before the data access: the zero page offset of the
    // pointer's high byte and the address the pointer and index produced. A
    // derived core that redirects the access sets m_isExtendedAccess, and
    // StepOne calls EndExtendedAccess once the instruction completes.
    virtual void ResolveExtendedIndirect (Byte pointerHigh, Word effectiveAddress) { (void) pointerHigh; (void) effectiveAddress; }
    virtual void EndExtendedAccess       ()                                        {}

    bool  m_isExtendedAccess = false;

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

    //  A fetch bit for each byte a page maps, by page; null where none. Set
    //  by a CPU on a bus that keeps them, and set at each opcode fetch.
    uint8_t * const * m_fetchTable = nullptr;
    virtual Word ReadWord      (Word address);

    void InitializeInstructionSet ();
    


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
    std::vector<Microcode>  instructionSet  {std::vector<Microcode> (256)};
    Byte                    m_lastCycles    = 0;
    Byte                    m_lastPenalties = 0;

    // The address of the last instruction that sent PC somewhere other than
    // the next instruction: a taken branch, a jump, a call, a return or BRK.
    Word                  m_lastBranchFrom = 0;
    bool                  m_hasLastBranch  = false;

    //  Where it sent PC; unknown after loading a state from a build without it.
    Word                  m_lastBranchTo    = 0;
    bool                  m_hasLastBranchTo = false;

    Word                  m_instructionStart = 0;

public:
    // The penalty kinds the last instruction paid, one bit each, each worth one
    // cycle already counted in m_lastCycles. The profiler subtracts them to find
    // the base cost and bills them to their own buckets.
    static constexpr Byte  kPenaltyPageCross   = 0x01;
    static constexpr Byte  kPenaltyBranchTaken = 0x02;
    static constexpr Byte  kPenaltyBranchCross = 0x04;

protected:
    // The trace reads mnemonics from instructionSet, which is declared
    // above it so it is constructed first.
    CpuFetchObservers        m_fetchObservers;
    CpuTrace                 m_trace;

    // Where TracePush reads the operand bytes: a derived CPU whose read
    // table can be unpublished page by page (a watched bus) points it at a
    // table that always has the mapped pages. Null means the read table.
    Byte * const *           m_tracePeekPages   = nullptr;

    // Forced inline: it runs once per instruction while a fetch is observed.
    __forceinline void ObserveFetch (Byte opcode)
    {
        m_fetchObservers.ObserveWatch (opcode, PC, SP);

        if (m_trace.IsEnabled())
        {
            TracePush (opcode);
        }

        m_fetchObservers.ObserveSink (opcode, PC);
    }

    void TracePush (Byte opcode);

    // Side-effect-free read for the trace, routed through the same read-page
    // table the fetch uses so operands come from the bank that executed.
    Byte  PeekForTrace (Word address) const;
};
