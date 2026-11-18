#include "Pch.h"

#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "GroupMisc.h"
#include "InstructionSetBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InstructionSetBuilder
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetBuilder::InstructionSetBuilder (std::vector<Microcode> & instructionSet, const Registers & registers) :
    m_instructionSet (instructionSet),
    m_registers      (registers)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::Build
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::Build()
{
    InitializeGroup00();
    InitializeGroup01();
    InitializeGroup10();
    InitializeMisc();
    InitializeUndocumented();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InitializeGroup00
//
//  Builds the group-00 opcodes -- the 6502's cc=00 column: branches, flag
//  ops, the index-register loads and compares, JMP, BIT.
//
//  Each row is a mnemonic plus a BITMASK of the addressing modes it supports,
//  and CreateInstruction expands that into one table entry per mode. The mask
//  is the encoding: the 6502 derives an opcode from (mnemonic, mode) bit
//  fields, so a row saying which modes are legal produces exactly the opcodes
//  the hardware has, and the gaps are the illegal ones.
//
//  Source and destination register pointers are what let one Microcode
//  operation serve several mnemonics -- Load with m_registers.y is LDY, with m_registers.x is LDX --
//  instead of an operation per register.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::InitializeGroup00()
{
    using _00 = Group00;
    struct TableEntry
    {
        _00::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _00::BIT,          _00::AMF_ZeroPage  | _00::AMF_Absolute,                      Microcode::BitTest, nullptr, nullptr },
        { _00::JMP,          _00::AMF_Absolute,                                           Microcode::Jump,    nullptr, nullptr },
        { _00::JMP_indirect, _00::AMF_Absolute,                                           Microcode::Jump,    nullptr, nullptr },
        { _00::STY,          _00::AMF_ZeroPage  | _00::AMF_Absolute | _00::AMF_ZeroPageX, Microcode::Store,   m_registers.y,      nullptr },
        { _00::LDY,          _00::__AMF_AllModes,                                         Microcode::Load,    nullptr, m_registers.y      },
        { _00::CPY,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, m_registers.y,      nullptr },
        { _00::CPX,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, m_registers.x,      nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_00::__AM_Count, _00::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group00, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InitializeGroup01
//
//  The cc=01 column: the eight accumulator ALU operations, which are the only
//  group where nearly every mnemonic supports the full addressing-mode set --
//  hence __AMF_AllModes on almost every row.
//
//  STA is the exception, masking OFF immediate: storing to a literal has no
//  meaning, and the hardware leaves that encoding to something else.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::InitializeGroup01()
{
    using _01 = Group01;
    struct TableEntry
    {
        _01::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _01::ORA, _01::__AMF_AllModes,                         Microcode::Or,                nullptr, nullptr },
        { _01::AND, _01::__AMF_AllModes,                         Microcode::And,               nullptr, nullptr },
        { _01::EOR, _01::__AMF_AllModes,                         Microcode::Xor,               nullptr, nullptr },
        { _01::ADC, _01::__AMF_AllModes,                         Microcode::AddWithCarry,      nullptr, nullptr },
        { _01::STA, _01::__AMF_AllModes & ~(_01::AMF_Immediate), Microcode::Store,             m_registers.a,      nullptr },
        { _01::LDA, _01::__AMF_AllModes,                         Microcode::Load,              nullptr, m_registers.a      },
        { _01::CMP, _01::__AMF_AllModes,                         Microcode::Compare,           m_registers.a,      nullptr },
        { _01::SBC, _01::__AMF_AllModes,                         Microcode::SubtractWithCarry, nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_01::__AM_Count, _01::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group01, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InitializeGroup10
//
//  The cc=10 column: the read-modify-write shifts and rotates, plus the X
//  register's load/store and the memory increment/decrement.
//
//  Nearly every row masks OFF immediate, because these write their result
//  back -- there is nowhere to write to a literal. LDX is the exception (it
//  only reads) and instead masks off accumulator mode, since ASL A has meaning
//  but LDX A does not.
//
//  ASL / ROL / LSR / ROR carry m_registers.a as their source register, which is how
//  accumulator mode reaches the operation; CreateInstruction pairs that with
//  the mode flag to produce both the `ASL A` and `ASL addr` encodings.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::InitializeGroup10()
{
    using _10 = Group10;
    struct TableEntry
    {
        _10::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pSourceRegister;
        Byte                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { _10::ASL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftLeft,   m_registers.a,      nullptr },
        { _10::ROL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateLeft,  m_registers.a,      nullptr },
        { _10::LSR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftRight,  m_registers.a,      nullptr },
        { _10::ROR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateRight, m_registers.a,      nullptr },
        { _10::STX, _10::AMF_ZeroPage | _10::AMF_Absolute | _10::AMF_ZeroPageX,                     Microcode::Store,       m_registers.x,      nullptr },
        { _10::LDX, _10::__AMF_AllModes & ~(_10::AMF_Accumulator),                                  Microcode::Load,        nullptr, m_registers.x      },
        { _10::DEC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Accumulator),             Microcode::Decrement,   nullptr, nullptr },
        { _10::INC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Accumulator),             Microcode::Increment,   nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_10::__AM_Count, _10::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group10, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InitializeMisc
//
//  Everything the cc-column encoding does not reach: branches, flag sets and
//  clears, stack pushes and pulls, the register transfers, BRK / RTI / RTS /
//  JSR / NOP.
//
//  These carry an EXPLICIT addressing mode and cycle count per row rather than
//  a mode bitmask, because each is a single fixed opcode with no family of
//  variants -- there is nothing for CreateInstruction to expand. Their cycle
//  counts are irregular for the same reason (JSR 6, RTI 6, PHA 3), so they are
//  stated rather than derived.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::InitializeMisc()
{
    struct TableEntry
    {
        GroupMisc::Opcode                      opcode;
        GlobalAddressingMode::AddressingMode   addressingMode;
        Microcode::Operation                   operation;
        Byte                                 * pSourceRegister;
        Byte                                 * pDestinationRegister;
        Byte                                   baseCycles;
    };

    TableEntry table[] =
    {
        { GroupMisc::BPL, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BMI, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BVC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BVS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BCC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BCS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BNE, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        { GroupMisc::BEQ, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr,        2 },
        
        { GroupMisc::BRK, GlobalAddressingMode::SingleByteNoOperand, Microcode::Break,                nullptr,        nullptr,        7 },
        { GroupMisc::JSR, GlobalAddressingMode::JumpAbsolute,        Microcode::JumpSubroutine,       nullptr,        nullptr,        6 },
        { GroupMisc::RTI, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromInterrupt,  nullptr,        nullptr,        6 },
        { GroupMisc::RTS, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromSubroutine, nullptr,        nullptr,        6 },
         
        { GroupMisc::PHP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 m_registers.status, nullptr,        3 },
        { GroupMisc::PLP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        m_registers.status, 4 },
        { GroupMisc::PHA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 m_registers.a,             nullptr,        3 },
        { GroupMisc::PLA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        m_registers.a,             4 },
        { GroupMisc::DEY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            m_registers.y,             nullptr,        2 },
        { GroupMisc::TAY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.a,             m_registers.y,             2 },
        { GroupMisc::INY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            m_registers.y,             nullptr,        2 },
        { GroupMisc::INX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            m_registers.x,             nullptr,        2 },
         
        { GroupMisc::CLC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::SEC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::CLI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::SEI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::TYA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.y,             m_registers.a,             2 },
        { GroupMisc::CLV, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::CLD, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
        { GroupMisc::SED, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              m_registers.status, nullptr,        2 },
         
        { GroupMisc::TXA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.x,             m_registers.a,             2 },
        { GroupMisc::TXS, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.x,             m_registers.sp,            2 },
        { GroupMisc::TAX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.a,             m_registers.x,             2 },
        { GroupMisc::TSX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             m_registers.sp,            m_registers.x,             2 },
        { GroupMisc::DEX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            m_registers.x,             nullptr,        2 },
        { GroupMisc::NOP, GlobalAddressingMode::SingleByteNoOperand, Microcode::NoOperation,          nullptr,        nullptr,        2 },
    };


    for (TableEntry entry : table)
    {
        Instruction instruction            = Instruction (GroupMisc::instruction[entry.opcode].instruction);
        m_instructionSet[instruction.asByte] = Microcode (instruction, GroupMisc::instruction[entry.opcode].name, entry.operation, entry.addressingMode, entry.pSourceRegister, entry.pDestinationRegister);
        m_instructionSet[instruction.asByte].baseCycles = entry.baseCycles;
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::InitializeUndocumented
//
//  Registers the stable NMOS 6502 undocumented opcodes that real Apple II
//  software relies on. Each combined opcode fuses a memory step with an ALU
//  step and is dispatched to the matching CpuOperations method; the NOP
//  family just consumes its operand bytes. The unstable "magic constant"
//  opcodes (ANE, LXA, SHA, SHX, SHY, TAS) are deliberately left illegal.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::InitializeUndocumented()
{
    struct UndocumentedOpcode
    {
        Byte                                 opcode;
        const char                         * name;
        Microcode::Operation                 operation;
        GlobalAddressingMode::AddressingMode mode;
        Byte                                 cycles;
    };

    // Read-modify-write combos (SLO/RLA/SRE/RRA/DCP/ISC): standard 6502 RMW
    // timing. The +1 page-cross penalty is never applied (see isReadOp).
    static constexpr Byte s_kRmwZeroPage        = 5;
    static constexpr Byte s_kRmwZeroPageX       = 6;
    static constexpr Byte s_kRmwAbsolute        = 6;
    static constexpr Byte s_kRmwAbsoluteIndexed = 7;   // abs,X and abs,Y
    static constexpr Byte s_kRmwIndirect        = 8;   // (zp,X) and (zp),Y

    // SAX store timing.
    static constexpr Byte s_kSaxZeroPage  = 3;
    static constexpr Byte s_kSaxZeroPageY = 4;
    static constexpr Byte s_kSaxAbsolute  = 4;
    static constexpr Byte s_kSaxIndirectX = 6;

    // LAX load timing. abs,Y and (zp),Y add 1 on page cross at run time.
    static constexpr Byte s_kLaxZeroPage  = 3;
    static constexpr Byte s_kLaxZeroPageY = 4;
    static constexpr Byte s_kLaxAbsolute  = 4;
    static constexpr Byte s_kLaxAbsoluteY = 4;
    static constexpr Byte s_kLaxIndirectX = 6;
    static constexpr Byte s_kLaxIndirectY = 5;

    // NOP family timing. abs,X adds 1 on page cross at run time.
    static constexpr Byte s_kNopImplied   = 2;
    static constexpr Byte s_kNopImmediate = 2;
    static constexpr Byte s_kNopZeroPage  = 3;
    static constexpr Byte s_kNopZeroPageX = 4;
    static constexpr Byte s_kNopAbsolute  = 4;
    static constexpr Byte s_kNopAbsoluteX = 4;

    static constexpr UndocumentedOpcode s_kUndocumentedOpcodes[] =
    {
        { 0x03, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0x07, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0x0F, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0x13, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0x17, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0x1B, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0x1F, "SLO", Microcode::ShiftLeftAndOr,       GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0x23, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0x27, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0x2F, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0x33, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0x37, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0x3B, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0x3F, "RLA", Microcode::RotateLeftAndAnd,     GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0x43, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0x47, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0x4F, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0x53, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0x57, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0x5B, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0x5F, "SRE", Microcode::ShiftRightAndXor,     GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0x63, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0x67, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0x6F, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0x73, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0x77, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0x7B, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0x7F, "RRA", Microcode::RotateRightAndAdd,    GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0x83, "SAX", Microcode::StoreAccumulatorAndX, GlobalAddressingMode::ZeroPageXIndirect,   s_kSaxIndirectX },
        { 0x87, "SAX", Microcode::StoreAccumulatorAndX, GlobalAddressingMode::ZeroPage,            s_kSaxZeroPage },
        { 0x8F, "SAX", Microcode::StoreAccumulatorAndX, GlobalAddressingMode::Absolute,            s_kSaxAbsolute },
        { 0x97, "SAX", Microcode::StoreAccumulatorAndX, GlobalAddressingMode::ZeroPageY,           s_kSaxZeroPageY },
        { 0xA3, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::ZeroPageXIndirect,   s_kLaxIndirectX },
        { 0xA7, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::ZeroPage,            s_kLaxZeroPage },
        { 0xAF, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::Absolute,            s_kLaxAbsolute },
        { 0xB3, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::ZeroPageIndirectY,   s_kLaxIndirectY },
        { 0xB7, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::ZeroPageY,           s_kLaxZeroPageY },
        { 0xBF, "LAX", Microcode::LoadAccumulatorAndX,  GlobalAddressingMode::AbsoluteY,           s_kLaxAbsoluteY },
        { 0xC3, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0xC7, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0xCF, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0xD3, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0xD7, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0xDB, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0xDF, "DCP", Microcode::DecrementAndCompare,  GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0xE3, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::ZeroPageXIndirect,   s_kRmwIndirect },
        { 0xE7, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::ZeroPage,            s_kRmwZeroPage },
        { 0xEF, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::Absolute,            s_kRmwAbsolute },
        { 0xF3, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::ZeroPageIndirectY,   s_kRmwIndirect },
        { 0xF7, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::ZeroPageX,           s_kRmwZeroPageX },
        { 0xFB, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::AbsoluteY,           s_kRmwAbsoluteIndexed },
        { 0xFF, "ISC", Microcode::IncrementAndSubtract, GlobalAddressingMode::AbsoluteX,           s_kRmwAbsoluteIndexed },
        { 0x1A, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0x3A, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0x5A, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0x7A, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0xDA, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0xFA, "NOP", Microcode::NoOperation,          GlobalAddressingMode::SingleByteNoOperand, s_kNopImplied },
        { 0x04, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPage,            s_kNopZeroPage },
        { 0x44, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPage,            s_kNopZeroPage },
        { 0x64, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPage,            s_kNopZeroPage },
        { 0x14, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0x34, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0x54, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0x74, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0xD4, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0xF4, "NOP", Microcode::NoOperation,          GlobalAddressingMode::ZeroPageX,           s_kNopZeroPageX },
        { 0x80, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Immediate,           s_kNopImmediate },
        { 0x82, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Immediate,           s_kNopImmediate },
        { 0x89, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Immediate,           s_kNopImmediate },
        { 0xC2, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Immediate,           s_kNopImmediate },
        { 0xE2, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Immediate,           s_kNopImmediate },
        { 0x0C, "NOP", Microcode::NoOperation,          GlobalAddressingMode::Absolute,            s_kNopAbsolute },
        { 0x1C, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
        { 0x3C, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
        { 0x5C, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
        { 0x7C, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
        { 0xDC, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
        { 0xFC, "NOP", Microcode::NoOperation,          GlobalAddressingMode::AbsoluteX,           s_kNopAbsoluteX },
    };

    for (const UndocumentedOpcode & op : s_kUndocumentedOpcodes)
    {
        m_instructionSet[op.opcode]                 = Microcode (Instruction (op.opcode), op.name, op.operation, op.mode, nullptr, nullptr);
        m_instructionSet[op.opcode].baseCycles      = op.cycles;
        m_instructionSet[op.opcode].assemblerHidden = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetBuilder::CreateInstruction
//
//  Expands one table row into every opcode it covers, walking the addressing-
//  mode flags and emitting an instructionSet entry per set bit. This is what
//  lets the group tables list eight mnemonics instead of 256 opcodes.
//
//  It also assigns baseCycles, and that is the subtle part. The count comes
//  from the addressing mode, adjusted by what the operation DOES with memory:
//
//    isMemoryRmw  a read-modify-write to memory pays for the read, the
//                 modify and the write ($1234 ASL is 6 cycles where LDA is 4)
//                 -- but only in memory. Accumulator mode is 2, which is why
//                 isRmw is further qualified by the mode.
//
//    isStore      an indexed store always pays the page-crossing cycle: the
//                 hardware cannot know whether the page crossed until it has
//                 read, and it must write regardless. Baking it in here is
//                 what lets StepOne skip stores when applying that penalty --
//                 the two must agree, or indexed stores are mistimed by one
//                 cycle in every instruction.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionSetBuilder::CreateInstruction (uint32_t                      addressingModeMax,
                             const char           * const  instructionName[],
                             Byte                          opcode,
                             Byte                          addressingModeFlags,
                             Byte                          group,
                             Microcode::Operation          operation,
                             Byte                 *        pSourceRegister,
                             Byte                 *        pDestinationRegister)
{
    Byte  addressingMode            = 0;
    Byte  currentAddressingModeFlag = 1;



    while (addressingMode < addressingModeMax)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction  instruction = Instruction (opcode, addressingMode, group);
            bool         isStore     = false;
            bool         isMemoryRmw = false;
            Byte         cycles      = 0;
            m_instructionSet[instruction.asByte] = Microcode   (instruction, instructionName[opcode], operation, pSourceRegister, pDestinationRegister);

            // Compute base cycle count from addressing mode and operation type
            isStore = (operation == Microcode::Store);
            bool isRmw       = (operation == Microcode::ShiftLeft  || operation == Microcode::ShiftRight  ||
                                operation == Microcode::RotateLeft || operation == Microcode::RotateRight ||
                                operation == Microcode::Decrement  || operation == Microcode::Increment);
            isMemoryRmw = isRmw && (m_instructionSet[instruction.asByte].globalAddressingMode != GlobalAddressingMode::Accumulator);

            cycles = 2;

            switch (m_instructionSet[instruction.asByte].globalAddressingMode)
            {
            case GlobalAddressingMode::Immediate:           cycles = 2;                                        break;
            case GlobalAddressingMode::ZeroPage:            cycles = isMemoryRmw ? 5 : 3;                      break;
            case GlobalAddressingMode::ZeroPageX:           cycles = isMemoryRmw ? 6 : 4;                      break;
            case GlobalAddressingMode::ZeroPageY:           cycles = 4;                                        break;
            case GlobalAddressingMode::Absolute:            cycles = isMemoryRmw ? 6 : 4;                      break;
            case GlobalAddressingMode::AbsoluteX:           cycles = isMemoryRmw ? 7 : (isStore ? 5 : 4);     break;
            case GlobalAddressingMode::AbsoluteY:           cycles = isStore ? 5 : 4;                          break;
            case GlobalAddressingMode::ZeroPageXIndirect:   cycles = 6;                                        break;
            case GlobalAddressingMode::ZeroPageIndirectY:   cycles = isStore ? 6 : 5;                          break;
            case GlobalAddressingMode::Accumulator:         cycles = 2;                                        break;
            case GlobalAddressingMode::JumpAbsolute:        cycles = 3;                                        break;
            case GlobalAddressingMode::JumpIndirect:        cycles = 5;                                        break;
            case GlobalAddressingMode::Relative:            cycles = 2;                                        break;
            case GlobalAddressingMode::SingleByteNoOperand: cycles = 2;                                        break;
            default:                                                                                           break;
            }

            m_instructionSet[instruction.asByte].baseCycles = cycles;
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}
