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
    { 0xDB, GlobalAddressingMode::Immediate, 3 },   // WAI slot: 2-byte NOP (base tier)
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
    InitializeCmosLeftovers ();
    InitializeNops ();
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
//  This is deliberately the base CMOS tier, matching the parts Apple shipped
//  (a GTE/WDC/Rockwell mix whose common instruction set omits the extensions):
//  the Rockwell bit ops (RMB/SMB/BBR/BBS) and WDC's WAI/STP are NOT installed,
//  so they behave as NOPs. Their operations/modes still exist in the CassoCore
//  CMOS superset, inert, ready for a future Rockwell/WDC variant.
//
//  These NOPs are NOT all single-byte. Harte's synertek65c02 vectors show the
//  base part still consumes each slot's operand bytes: the $x7 column (RMB/SMB)
//  is a 2-byte NOP, the $xF column (BBR/BBS) is 3-byte, and $DB (WAI) is 2-byte
//  (handled in s_kReservedNops). Only the truly empty $x3/$xB slots and $CB are
//  single-byte. Cycle counts mirror the displaced instruction's slot timing;
//  conformance checks final state, not cycles.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InitializeNops ()
{
    int    i = 0;

    for (const ReservedNop & e : s_kReservedNops)
    {
        SetOpcode (e.opcode, "NOP", Microcode::NoOperation, e.mode, nullptr, nullptr, e.cycles);
    }

    // $x7 (RMB/SMB slots): 2-byte NOPs.  $xF (BBR/BBS slots): 3-byte NOPs.
    for (i = 0x07; i <= 0xF7; i += 0x10)
    {
        SetOpcode ((Byte) i, "NOP", Microcode::NoOperation, GlobalAddressingMode::ZeroPage, nullptr, nullptr, 5);
    }

    for (i = 0x0F; i <= 0xFF; i += 0x10)
    {
        SetOpcode ((Byte) i, "NOP", Microcode::NoOperation, GlobalAddressingMode::Absolute, nullptr, nullptr, 5);
    }

    for (i = 0; i <= 0xFF; ++i)
    {
        if (!instructionSet[i].isLegal)
        {
            SetOpcode (static_cast<Byte> (i), "NOP", Microcode::NoOperation, GlobalAddressingMode::SingleByteNoOperand, nullptr, nullptr, s_kOneByteNopCycles);
        }
    }
}
