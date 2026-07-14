#include "Pch.h"

#include "Cpu65C02.h"

#include "GroupCmos.h"

static constexpr Byte    s_kOneByteNopCycles = 1;


////////////////////////////////////////////////////////////////////////////////
//
//  ReservedNop
//
//  A reserved opcode-map hole that the 65C02 executes as a NOP. Unlike the real
//  instructions (which live in GroupCmos keyed by mnemonic) these are pure fill:
//  the addressing mode only fixes how many operand bytes are consumed.
//
////////////////////////////////////////////////////////////////////////////////

struct ReservedNop
{
    Byte                                   opcode;
    GlobalAddressingMode::AddressingMode   mode;
    Byte                                   cycles;
};


static constexpr ReservedNop    s_kReservedNops[] =
{
    { 0x02, GlobalAddressingMode::Immediate, 2 },   // 2-byte, 2 cycle
    { 0x22, GlobalAddressingMode::Immediate, 2 },
    { 0x42, GlobalAddressingMode::Immediate, 2 },
    { 0x62, GlobalAddressingMode::Immediate, 2 },
    { 0x82, GlobalAddressingMode::Immediate, 2 },
    { 0xC2, GlobalAddressingMode::Immediate, 2 },
    { 0xE2, GlobalAddressingMode::Immediate, 2 },
    { 0x44, GlobalAddressingMode::ZeroPage,  3 },   // 2-byte, 3 cycle
    { 0x54, GlobalAddressingMode::ZeroPageX, 4 },   // 2-byte, 4 cycle
    { 0xD4, GlobalAddressingMode::ZeroPageX, 4 },
    { 0xF4, GlobalAddressingMode::ZeroPageX, 4 },
    { 0x5C, GlobalAddressingMode::Absolute,  8 },   // 3-byte, 8 cycle
    { 0xDC, GlobalAddressingMode::Absolute,  4 },   // 3-byte, 4 cycle
    { 0xFC, GlobalAddressingMode::Absolute,  4 },
};




////////////////////////////////////////////////////////////////////////////////
//
//  Cpu65C02
//
////////////////////////////////////////////////////////////////////////////////

Cpu65C02::Cpu65C02 (MemoryBus & memoryBus)
    : MemoryBusCpu (memoryBus)
{
    InitializeCmos ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DispatchVector
//
//  Hardware IRQ/NMI entry. The 65C02 clears the decimal flag on entry (the
//  pushed status still reflects the pre-entry flags, so the base prologue runs
//  first).
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::DispatchVector (Word vector, bool fromBrk)
{
    Cpu6502::DispatchVector (vector, fromBrk);

    status.flags.decimal = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetOpcode
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::SetOpcode (Byte                                  opcode,
                          const char                          * name,
                          Microcode::Operation                  operation,
                          GlobalAddressingMode::AddressingMode  mode,
                          Byte                                * pSourceRegister,
                          Byte                                * pDestinationRegister,
                          Byte                                  cycles)
{
    instructionSet[opcode]            = Microcode (Instruction (opcode), name, operation, mode, pSourceRegister, pDestinationRegister);
    instructionSet[opcode].baseCycles = cycles;
}




////////////////////////////////////////////////////////////////////////////////
//
//  InitializeCmos
//
//  Runs after the base ctor has built the NMOS table; each step below overrides
//  the affected entries with their 65C02 behavior.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InitializeCmos ()
{
    InitializeArithmetic ();
    ReclaimUndocumented ();
    InitializeCmosLeftovers ();
    InstallBitOps ();
    InitializeNops ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallBitOps
//
//  The Rockwell/WDC bit instructions RMBn/SMBn (zero-page read-modify-write)
//  and BBRn/BBSn (zero-page-bit test + relative branch). Apple's //c ROM 4
//  firmware uses them, so they are part of the tier Casso models. The bit
//  number is encoded in the opcode ((opcode >> 4) & 7); the operations read it
//  back from the microcode instruction byte. Installed before InitializeNops
//  so the illegal-slot NOP fill leaves them alone.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallBitOps ()
{
    static constexpr Byte    s_kBitOpCycles       = 5;
    static constexpr Byte    s_kBitBranchCycles   = 5;

    // The bit index is baked into the mnemonic (RMB0..RMB7 etc.), exactly as the
    // instructions are written in assembly, as well as into the opcode
    // ((opcode >> 4) & 7) which the operations read back. Distinct names let the
    // disassembler show the right bit and let the assembler's OpcodeTable tell the
    // eight variants apart (they would otherwise collapse onto one shared key).
    static constexpr const char * const s_kRmbNames[8] =
        { "RMB0", "RMB1", "RMB2", "RMB3", "RMB4", "RMB5", "RMB6", "RMB7" };
    static constexpr const char * const s_kSmbNames[8] =
        { "SMB0", "SMB1", "SMB2", "SMB3", "SMB4", "SMB5", "SMB6", "SMB7" };
    static constexpr const char * const s_kBbrNames[8] =
        { "BBR0", "BBR1", "BBR2", "BBR3", "BBR4", "BBR5", "BBR6", "BBR7" };
    static constexpr const char * const s_kBbsNames[8] =
        { "BBS0", "BBS1", "BBS2", "BBS3", "BBS4", "BBS5", "BBS6", "BBS7" };

    for (int n = 0; n < 8; ++n)
    {
        Byte    rmb = static_cast<Byte> (0x07 + n * 0x10);   // $07,$17,..,$77
        Byte    smb = static_cast<Byte> (0x87 + n * 0x10);   // $87,$97,..,$F7
        Byte    bbr = static_cast<Byte> (0x0F + n * 0x10);   // $0F,$1F,..,$7F
        Byte    bbs = static_cast<Byte> (0x8F + n * 0x10);   // $8F,$9F,..,$FF

        SetOpcode (rmb, s_kRmbNames[n], Microcode::ResetMemoryBit, GlobalAddressingMode::ZeroPage,         nullptr, nullptr, s_kBitOpCycles);
        SetOpcode (smb, s_kSmbNames[n], Microcode::SetMemoryBit,   GlobalAddressingMode::ZeroPage,         nullptr, nullptr, s_kBitOpCycles);
        SetOpcode (bbr, s_kBbrNames[n], Microcode::BitBranchReset, GlobalAddressingMode::ZeroPageRelative, nullptr, nullptr, s_kBitBranchCycles);
        SetOpcode (bbs, s_kBbsNames[n], Microcode::BitBranchSet,   GlobalAddressingMode::ZeroPageRelative, nullptr, nullptr, s_kBitBranchCycles);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReclaimUndocumented
//
//  The 65C02 has none of the NMOS undocumented opcodes. Blank the ones the base
//  Cpu::InitializeUndocumented() installed so that any the 65C02 redefines (e.g.
//  $04 -> TSB) are re-installed by the leftovers pass, while the rest ($CF) fall
//  through to the single-byte NOP fill. Keep this list in sync with
//  Cpu::InitializeUndocumented().
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::ReclaimUndocumented ()
{
    static constexpr Byte    s_kNmosUndocumented[] = { 0x04, 0xCF };

    for (Byte opcode : s_kNmosUndocumented)
    {
        instructionSet[opcode] = Microcode ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InitializeArithmetic
//
//  ADC/SBC/BRK keep their NMOS opcodes, modes and cycles and differ only in
//  behavior, so re-point them by operation instead of by opcode. This relies on
//  AddWithCarry / SubtractWithCarry / Break each being used by only the ADC /
//  SBC / BRK opcodes (true of the NMOS table), and automatically covers every
//  addressing mode. The extra decimal cycle is added at run time by the CMOS
//  operation itself.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InitializeArithmetic ()
{
    int    i = 0;

    for (i = 0; i <= 0xFF; ++i)
    {
        switch (instructionSet[i].operation)
        {
        case Microcode::AddWithCarry:
            instructionSet[i].operation = Microcode::AddWithCarryCmos;
            break;

        case Microcode::SubtractWithCarry:
            instructionSet[i].operation = Microcode::SubtractWithCarryCmos;
            break;

        case Microcode::Break:
            instructionSet[i].operation = Microcode::BreakCmos;
            break;

        default:
            break;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InitializeCmosLeftovers
//
//  The 65C02 additions that fill the NMOS opcode-map holes (see GroupCmos).
//  Mirrors Cpu::InitializeMisc: a table keyed by the mnemonic enum, with the
//  opcode byte and name pulled from GroupCmos::instruction[].
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InitializeCmosLeftovers ()
{
    struct TableEntry
    {
        GroupCmos::Opcode                      opcode;
        GlobalAddressingMode::AddressingMode   addressingMode;
        Microcode::Operation                   operation;
        Byte                                 * pSourceRegister;
        Byte                                 * pDestinationRegister;
        Byte                                   baseCycles;
    };

    TableEntry table[] =
    {
        { GroupCmos::STZ_ZP,      GlobalAddressingMode::ZeroPage,            Microcode::StoreZero,             nullptr, nullptr, 3 },
        { GroupCmos::STZ_ZPX,     GlobalAddressingMode::ZeroPageX,           Microcode::StoreZero,             nullptr, nullptr, 4 },
        { GroupCmos::STZ_ABS,     GlobalAddressingMode::Absolute,            Microcode::StoreZero,             nullptr, nullptr, 4 },
        { GroupCmos::STZ_ABSX,    GlobalAddressingMode::AbsoluteX,           Microcode::StoreZero,             nullptr, nullptr, 5 },

        { GroupCmos::TSB_ZP,      GlobalAddressingMode::ZeroPage,            Microcode::TestAndSetBits,        nullptr, nullptr, 5 },
        { GroupCmos::TSB_ABS,     GlobalAddressingMode::Absolute,            Microcode::TestAndSetBits,        nullptr, nullptr, 6 },
        { GroupCmos::TRB_ZP,      GlobalAddressingMode::ZeroPage,            Microcode::TestAndResetBits,      nullptr, nullptr, 5 },
        { GroupCmos::TRB_ABS,     GlobalAddressingMode::Absolute,            Microcode::TestAndResetBits,      nullptr, nullptr, 6 },

        { GroupCmos::ORA_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::Or,                    nullptr, nullptr, 5 },
        { GroupCmos::AND_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::And,                   nullptr, nullptr, 5 },
        { GroupCmos::EOR_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::Xor,                   nullptr, nullptr, 5 },
        { GroupCmos::ADC_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::AddWithCarryCmos,      nullptr, nullptr, 5 },
        { GroupCmos::STA_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::Store,                 &A,      nullptr, 5 },
        { GroupCmos::LDA_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::Load,                  nullptr, &A,      5 },
        { GroupCmos::CMP_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::Compare,               &A,      nullptr, 5 },
        { GroupCmos::SBC_ZPI,     GlobalAddressingMode::ZeroPageIndirect,    Microcode::SubtractWithCarryCmos, nullptr, nullptr, 5 },

        { GroupCmos::INC_A,       GlobalAddressingMode::Accumulator,         Microcode::Increment,             &A,      nullptr, 2 },
        { GroupCmos::DEC_A,       GlobalAddressingMode::Accumulator,         Microcode::Decrement,             &A,      nullptr, 2 },

        { GroupCmos::PHX,         GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                  &X,      nullptr, 3 },
        { GroupCmos::PLX,         GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                  nullptr, &X,      4 },
        { GroupCmos::PHY,         GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                  &Y,      nullptr, 3 },
        { GroupCmos::PLY,         GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                  nullptr, &Y,      4 },

        { GroupCmos::BRA,         GlobalAddressingMode::Relative,            Microcode::BranchAlways,          nullptr, nullptr, 2 },

        { GroupCmos::BIT_ZPX,     GlobalAddressingMode::ZeroPageX,           Microcode::BitTest,               nullptr, nullptr, 4 },
        { GroupCmos::BIT_ABSX,    GlobalAddressingMode::AbsoluteX,           Microcode::BitTest,               nullptr, nullptr, 4 },
        { GroupCmos::BIT_IMM,     GlobalAddressingMode::Immediate,           Microcode::BitTestImmediate,      nullptr, nullptr, 2 },

        { GroupCmos::JMP_IND,     GlobalAddressingMode::JumpIndirectCmos,    Microcode::Jump,                  nullptr, nullptr, 6 },
        { GroupCmos::JMP_ABSXIND, GlobalAddressingMode::AbsoluteXIndirect,   Microcode::Jump,                  nullptr, nullptr, 6 },
    };

    for (TableEntry entry : table)
    {
        Byte    opcode = GroupCmos::instruction[entry.opcode].instruction;

        SetOpcode (opcode,
                   GroupCmos::instruction[entry.opcode].name,
                   entry.operation,
                   entry.addressingMode,
                   entry.pSourceRegister,
                   entry.pDestinationRegister,
                   entry.baseCycles);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InitializeNops
//
//  The 65C02 defines every opcode. Install the reserved multi-byte NOP forms,
//  then fill any opcode still marked illegal with a single-byte NOP so nothing
//  traps as an undefined instruction.
//
//  Casso models the Rockwell R65C02 -- the CMOS core plus the Rockwell bit ops
//  (RMB/SMB/BBR/BBS, installed by InstallBitOps), which Apple's //c ROM 4 and
//  the Enhanced //e firmware use. WDC's WAI/STP ($CB/$DB) are NOT part of the
//  Rockwell parts Apple shipped, so those two slots remain single-byte,
//  single-cycle NOPs (canonical per 6502.org / Klaus Dormann), reached by the
//  illegal-slot fill below. The Harte match is the rockwell65c02 corpus.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InitializeNops ()
{
    int    i = 0;

    // These reserved / undefined-opcode slots execute and disassemble as NOPs
    // (real CMOS behavior), but they are opcode-map fill, not instructions anyone
    // writes -- so hide them from the assembler. Otherwise the highest such slot
    // would shadow the canonical NOP ($EA) when OpcodeTable inverts by mnemonic.
    for (const ReservedNop & e : s_kReservedNops)
    {
        SetOpcode (e.opcode, "NOP", Microcode::NoOperation, e.mode, nullptr, nullptr, e.cycles);
        instructionSet[e.opcode].assemblerHidden = true;
    }

    for (i = 0; i <= 0xFF; ++i)
    {
        if (!instructionSet[i].isLegal)
        {
            SetOpcode (static_cast<Byte> (i), "NOP", Microcode::NoOperation, GlobalAddressingMode::SingleByteNoOperand, nullptr, nullptr, s_kOneByteNopCycles);
            instructionSet[i].assemblerHidden = true;
        }
    }
}
