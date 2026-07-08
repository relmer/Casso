#include "Pch.h"

#include "Cpu65C02.h"

// Bit-addressed instruction families (RMBn/SMBn/BBRn/BBSn) are laid out at a
// fixed 0x10 stride from a base opcode, one per bit 0..7.
static constexpr Byte    s_kBitOpCount   = 8;
static constexpr Byte    s_kBitOpStride  = 0x10;
static constexpr Byte    s_kRmbBase      = 0x07;
static constexpr Byte    s_kSmbBase      = 0x87;
static constexpr Byte    s_kBbrBase      = 0x0F;
static constexpr Byte    s_kBbsBase      = 0x8F;
static constexpr Byte    s_kBitOpCycles  = 5;

static constexpr Byte    s_kOneByteNopCycles = 1;


////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeModeCycles
//
//  Compact table row for opcodes whose operation is fixed per group and that
//  take no register pointer (so the row can be a compile-time constant).
//
////////////////////////////////////////////////////////////////////////////////

struct OpcodeModeCycles
{
    Byte                                   opcode;
    GlobalAddressingMode::AddressingMode   mode;
    Byte                                   cycles;
};


// ADC/SBC across every addressing mode. Routed to the CMOS variants so decimal
// flags and the extra decimal cycle are correct regardless of mode.
static constexpr OpcodeModeCycles    s_kAdcModes[] =
{
    { 0x69, GlobalAddressingMode::Immediate,         2 },
    { 0x65, GlobalAddressingMode::ZeroPage,          3 },
    { 0x75, GlobalAddressingMode::ZeroPageX,         4 },
    { 0x6D, GlobalAddressingMode::Absolute,          4 },
    { 0x7D, GlobalAddressingMode::AbsoluteX,         4 },
    { 0x79, GlobalAddressingMode::AbsoluteY,         4 },
    { 0x61, GlobalAddressingMode::ZeroPageXIndirect, 6 },
    { 0x71, GlobalAddressingMode::ZeroPageIndirectY, 5 },
};

static constexpr OpcodeModeCycles    s_kSbcModes[] =
{
    { 0xE9, GlobalAddressingMode::Immediate,         2 },
    { 0xE5, GlobalAddressingMode::ZeroPage,          3 },
    { 0xF5, GlobalAddressingMode::ZeroPageX,         4 },
    { 0xED, GlobalAddressingMode::Absolute,          4 },
    { 0xFD, GlobalAddressingMode::AbsoluteX,         4 },
    { 0xF9, GlobalAddressingMode::AbsoluteY,         4 },
    { 0xE1, GlobalAddressingMode::ZeroPageXIndirect, 6 },
    { 0xF1, GlobalAddressingMode::ZeroPageIndirectY, 5 },
};

// STZ addressing forms.
static constexpr OpcodeModeCycles    s_kStzModes[] =
{
    { 0x64, GlobalAddressingMode::ZeroPage,  3 },
    { 0x74, GlobalAddressingMode::ZeroPageX, 4 },
    { 0x9C, GlobalAddressingMode::Absolute,  4 },
    { 0x9E, GlobalAddressingMode::AbsoluteX, 5 },
};

// Reserved multi-byte NOPs: (opcode, addressing mode determines byte count).
static constexpr OpcodeModeCycles    s_kReservedNops[] =
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
    InstallCmosOpcodes ();
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
//  InstallCmosOpcodes
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallCmosOpcodes ()
{
    InstallArithmetic ();
    InstallZeroPageInd ();
    InstallNewOps ();
    InstallBitOps ();
    InstallNops ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallArithmetic
//
//  Re-point every ADC/SBC opcode and BRK at the CMOS variants.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallArithmetic ()
{
    for (const OpcodeModeCycles & e : s_kAdcModes)
    {
        SetOpcode (e.opcode, "ADC", Microcode::AddWithCarryCmos, e.mode, nullptr, nullptr, e.cycles);
    }

    for (const OpcodeModeCycles & e : s_kSbcModes)
    {
        SetOpcode (e.opcode, "SBC", Microcode::SubtractWithCarryCmos, e.mode, nullptr, nullptr, e.cycles);
    }

    SetOpcode (0x00, "BRK", Microcode::BreakCmos, GlobalAddressingMode::SingleByteNoOperand, nullptr, nullptr, 7);
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallZeroPageInd
//
//  The eight (zp) forms of the ALU group.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallZeroPageInd ()
{
    static constexpr Byte    s_kZpIndCycles = 5;
    GlobalAddressingMode::AddressingMode  mode = GlobalAddressingMode::ZeroPageIndirect;

    SetOpcode (0x12, "ORA", Microcode::Or,                    mode, nullptr, nullptr, s_kZpIndCycles);
    SetOpcode (0x32, "AND", Microcode::And,                   mode, nullptr, nullptr, s_kZpIndCycles);
    SetOpcode (0x52, "EOR", Microcode::Xor,                   mode, nullptr, nullptr, s_kZpIndCycles);
    SetOpcode (0x72, "ADC", Microcode::AddWithCarryCmos,      mode, nullptr, nullptr, s_kZpIndCycles);
    SetOpcode (0x92, "STA", Microcode::Store,                 mode, &A,      nullptr, s_kZpIndCycles);
    SetOpcode (0xB2, "LDA", Microcode::Load,                  mode, nullptr, &A,      s_kZpIndCycles);
    SetOpcode (0xD2, "CMP", Microcode::Compare,               mode, &A,      nullptr, s_kZpIndCycles);
    SetOpcode (0xF2, "SBC", Microcode::SubtractWithCarryCmos, mode, nullptr, nullptr, s_kZpIndCycles);
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallNewOps
//
//  STZ, TSB/TRB, INC A/DEC A, PHX/PLX/PHY/PLY, BRA, BIT (new forms), and the
//  corrected/added JMP indirect modes.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallNewOps ()
{
    for (const OpcodeModeCycles & e : s_kStzModes)
    {
        SetOpcode (e.opcode, "STZ", Microcode::StoreZero, e.mode, nullptr, nullptr, e.cycles);
    }

    SetOpcode (0x04, "TSB", Microcode::TestAndSetBits,   GlobalAddressingMode::ZeroPage, nullptr, nullptr, 5);
    SetOpcode (0x0C, "TSB", Microcode::TestAndSetBits,   GlobalAddressingMode::Absolute, nullptr, nullptr, 6);
    SetOpcode (0x14, "TRB", Microcode::TestAndResetBits, GlobalAddressingMode::ZeroPage, nullptr, nullptr, 5);
    SetOpcode (0x1C, "TRB", Microcode::TestAndResetBits, GlobalAddressingMode::Absolute, nullptr, nullptr, 6);

    SetOpcode (0x1A, "INC", Microcode::Increment, GlobalAddressingMode::Accumulator, &A, nullptr, 2);
    SetOpcode (0x3A, "DEC", Microcode::Decrement, GlobalAddressingMode::Accumulator, &A, nullptr, 2);

    SetOpcode (0xDA, "PHX", Microcode::Push, GlobalAddressingMode::SingleByteNoOperand, &X,      nullptr, 3);
    SetOpcode (0xFA, "PLX", Microcode::Pull, GlobalAddressingMode::SingleByteNoOperand, nullptr, &X,      4);
    SetOpcode (0x5A, "PHY", Microcode::Push, GlobalAddressingMode::SingleByteNoOperand, &Y,      nullptr, 3);
    SetOpcode (0x7A, "PLY", Microcode::Pull, GlobalAddressingMode::SingleByteNoOperand, nullptr, &Y,      4);

    SetOpcode (0x80, "BRA", Microcode::BranchAlways, GlobalAddressingMode::Relative, nullptr, nullptr, 2);

    SetOpcode (0x34, "BIT", Microcode::BitTest,          GlobalAddressingMode::ZeroPageX, nullptr, nullptr, 4);
    SetOpcode (0x3C, "BIT", Microcode::BitTest,          GlobalAddressingMode::AbsoluteX, nullptr, nullptr, 4);
    SetOpcode (0x89, "BIT", Microcode::BitTestImmediate, GlobalAddressingMode::Immediate, nullptr, nullptr, 2);

    SetOpcode (0x6C, "JMP", Microcode::Jump, GlobalAddressingMode::JumpIndirectCmos,  nullptr, nullptr, 6);
    SetOpcode (0x7C, "JMP", Microcode::Jump, GlobalAddressingMode::AbsoluteXIndirect, nullptr, nullptr, 6);
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallBitOps
//
//  RMB0-7, SMB0-7 (zero page) and BBR0-7, BBS0-7 (zero page, relative).
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallBitOps ()
{
    Byte    i = 0;

    for (i = 0; i < s_kBitOpCount; ++i)
    {
        Byte offset = static_cast<Byte> (i * s_kBitOpStride);

        SetOpcode (static_cast<Byte> (s_kRmbBase + offset), "RMB", Microcode::ResetMemoryBit, GlobalAddressingMode::ZeroPage,         nullptr, nullptr, s_kBitOpCycles);
        SetOpcode (static_cast<Byte> (s_kSmbBase + offset), "SMB", Microcode::SetMemoryBit,   GlobalAddressingMode::ZeroPage,         nullptr, nullptr, s_kBitOpCycles);
        SetOpcode (static_cast<Byte> (s_kBbrBase + offset), "BBR", Microcode::BitBranchReset, GlobalAddressingMode::ZeroPageRelative, nullptr, nullptr, s_kBitOpCycles);
        SetOpcode (static_cast<Byte> (s_kBbsBase + offset), "BBS", Microcode::BitBranchSet,   GlobalAddressingMode::ZeroPageRelative, nullptr, nullptr, s_kBitOpCycles);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InstallNops
//
//  The 65C02 defines every opcode. Install the reserved multi-byte NOP forms,
//  then fill any opcode still marked illegal with a single-byte NOP so nothing
//  traps as an undefined instruction.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu65C02::InstallNops ()
{
    int    i = 0;

    for (const OpcodeModeCycles & e : s_kReservedNops)
    {
        SetOpcode (e.opcode, "NOP", Microcode::NoOperation, e.mode, nullptr, nullptr, e.cycles);
    }

    for (i = 0; i <= 0xFF; ++i)
    {
        if (!instructionSet[i].isLegal)
        {
            SetOpcode (static_cast<Byte> (i), "NOP", Microcode::NoOperation, GlobalAddressingMode::SingleByteNoOperand, nullptr, nullptr, s_kOneByteNopCycles);
        }
    }
}
