#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Ehm.h"
#include "Group00.h"
#include "Group01.h"
#include "Group10.h"
#include "GroupMisc.h"
#include "Utils.h"


////////////////////////////////////////////////////////////////////////////////
//
//  Cpu
//
////////////////////////////////////////////////////////////////////////////////

Cpu::Cpu () :
    memory         (memSize, 0),
    instructionSet (256)
{
    InitializeInstructionSet ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::Reset ()
{
    /*
    PC = 0xFFFC;
    SP = 0x00;
    */

    status.status          = 0;
    status.flags.alwaysOne = 1;

    A = 0;
    X = 0;
    Y = 0;

    std::fill (memory.begin (), memory.end (), Byte (0));



    // Test code
    PC = 0x8000;
    SP = 0xFF;

    Word addr = PC;


    // Immediate
    //memory[addr++] = 0x09;  // ORA
    //memory[addr++] = 0x80;  // Immediate
    //memory[addr++] = 0xA9;  // LDA 
    //memory[addr++] = 0x80;  // Immediate
    //memory[addr++] = 0x69;  // ADC
    //memory[addr++] = 0x11;  // Immediate
    //memory[addr++] = 0x85;  // STA
    //memory[addr++] = 0x11;  // Zeropage offset
    //memory[addr++] = 0xA9;  // LDA
    //memory[addr++] = 0x00;  // Immediate
    //memory[addr++] = 0xA5;  // LDA
    //memory[addr++] = 0x11;  // Zeropage offset
    //memory[addr++] = 0xA9;  // LDA
    //memory[addr++] = 0x40;  // Immediate
    //memory[addr++] = 0xC9;  // CMP
    //memory[addr++] = 0x40;  // Immediate
    //memory[addr++] = 0xC9;  // CMP
    //memory[addr++] = 0x41;  // Immediate
    //memory[addr++] = 0xC9;  // CMP
    //memory[addr++] = 0x3F;  // Immediate

    //memory[addr++] = 0xA9;  // LDA 
    //memory[addr++] = 0x80;  // Immediate
    //memory[addr++] = 0xE9;  // SBC
    //memory[addr++] = 0x01;  // Immediate

    //Word loop = addr + 3;
    //memory[addr++] = 0x4C;  // JMP 
    //memory[addr++] = loop; 
    //memory[addr++] = loop >> 8;

    //Word loop = addr;
    //memory[addr++] = 0x6C;  // JMP 
    //memory[addr++] = 0x00;
    //memory[addr++] = 0x01;
    //memory[0x0100] = loop;
    //memory[0x0101] = loop >> 8;

    //memory[addr++] = 0xA9;  // LDA, #immediate
    //memory[addr++] = 0xF0;  // 
    //memory[addr++] = 0x24;  // BIT, zp
    //memory[addr++] = 0x00;  // 
    //memory[0x0000] = 0xCF;

    //memory[addr++] = 0xA9;  // LDA, #immediate
    //memory[addr++] = 0xCF;  // 
    //memory[addr++] = 0x8D;  // STA, $0034
    //memory[addr++] = 0x34;  // 
    //memory[addr++] = 0x00;  // 
    //memory[addr++] = 0xA9;  // LDA, #immediate
    //memory[addr++] = 0xF0;  // 
    //memory[addr++] = 0x24;  // BIT, zp
    //memory[addr++] = 0x34;  // 
    //memory[addr++] = 0xA4;  // LDY, $34
    //memory[addr++] = 0x34;  //
    //memory[addr++] = 0xC0;  // CPY, #CF
    //memory[addr++] = 0xCF;  //




    //memory[addr++] = 0x09;  // ORA
    //memory[addr++] = 0x39;  // AND
    //memory[addr++] = 0x49;  // EOR
    //memory[addr++] = 0xA5;  // Immediate
    //memory[addr++] = 0x29;  // AND
    //memory[addr++] = 0x49;  // EOR
    //memory[addr++] = 0xFF;  // Immediate

    // Zero page + X, indirect
    //X = 0x11;
    //memory[addr++] = 0x01;  // ORA
    //memory[addr++] = 0x11;  // ($0011,X)
    //memory[memory[addr - 1] + X]     = 0x45;
    //memory[memory[addr - 1] + X + 1] = 0x23;
    //memory[0x2345] = 0x55;

    // Zero page indirect, Y
    //Y = 0x22;
    //memory[addr++] = 0x11;  // ORA
    //memory[addr++] = 0x33;  // (22),Y
    //memory[memory[addr - 1]] = 0x56;
    //memory[memory[addr - 1] + 1] = 0x34;
    //memory[0x3456 + Y] = 0x44;

    // Zero page
    //memory[addr++] = 0x05;  // ORA
    //memory[addr++] = 0x55;  // ($0055)
    //memory[memory[addr - 1]] = 0x44;

    // Zero page, X
    //X = 0x77;
    //memory[addr++] = 0x15;  // ORA
    //memory[addr++] = 0x66;  // ($0055)
    //memory[(memory[addr - 1] + X) & 0xFF] = 0xEE;

    // Absolute
    //memory[addr++] = 0x0D;  // ORA
    //memory[addr++] = 0x34;  // NB:  Little endian
    //memory[addr++] = 0x12;  // ($1234)
    //memory[0x1234] = 0x55;

    // Absolute, X
    //X = 0x06;
    //memory[addr++] = 0x1D;  // ORA
    //memory[addr++] = 0x34;  // NB:  Little endian
    //memory[addr++] = 0x12;  // ($1234)
    //memory[0x123A] = 0x99;

    // Absolute, Y
    //Y = 0x0C;
    //memory[addr++] = 0x19;  // ORA
    //memory[addr++] = 0x34;  // NB:  Little endian
    //memory[addr++] = 0x12;  // ($1234)
    //memory[0x1240] = 0xAA;

    // LDX, DEX, BMI, BPL, INX
    memory[addr++] = 0xA2;      // LDX, #immediate
    memory[addr++] = 0x03;      // a non-negative number
    memory[addr++] = 0xCA;      // DEX
    memory[addr++] = 0x30;      // BMI
    memory[addr++] = 0x02;      // Branch offset
    memory[addr++] = 0x10;      // BPL
    memory[addr++] = ~0x05 + 1; // Branch offset -5
    memory[addr++] = 0xA2;      // LDX, #immediate
    memory[addr++] = ~0x02 + 1; // -2
    memory[addr++] = 0xE8;      // INX
    memory[addr++] = 0x30;      // BMI
    memory[addr++] = ~0x03 + 1; // Branch offset -3
    memory[addr++] = 0xA9;      // LDA, #immediate
    memory[addr++] = 0x00;      // a zero number
}





////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::Run ()
{
    do
    {
        Word        initialPC   = PC;
        Byte        opcode      = memory[PC];
        Microcode   microcode   = instructionSet[opcode];
        OperandInfo operandInfo = { 0 };

        FetchOperand        (microcode, operandInfo);
        PrintSingleStepInfo (initialPC, opcode, operandInfo);

        if (!instructionSet[opcode].isLegal)
        {
            break;
        }

        ++PC;

        ExecuteInstruction  (instructionSet[opcode], operandInfo);

        std::printf ("\n");
    }
    while (1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepOne
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::StepOne ()
{
    Byte        opcode      = memory[PC];
    Microcode   microcode   = instructionSet[opcode];
    OperandInfo operandInfo = { 0 };

    FetchOperand (microcode, operandInfo);
    ++PC;
    ExecuteInstruction (microcode, operandInfo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintSingleStepInfo
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintSingleStepInfo (Word initialPC, Byte opcode, const OperandInfo & operandInfo)
{
    static constexpr char flags[][8] =
    {
        ".......",
        "CZIDBVN"
    };

    // Print the registers and the opcode byte
    std::printf ("SP: %02x  A: %04X  X: %04X  Y: %04X  %c%c%c%c%c%c%c    [%04X] %02X ",
                 SP,
                 A,
                 X,
                 Y,
                 flags[status.flags.carry][0],
                 flags[status.flags.zero][1],
                 flags[status.flags.interruptDisable][2],
                 flags[status.flags.decimal][3],
                 flags[status.flags.brk][4],
                 flags[status.flags.overflow][5],
                 flags[status.flags.negative][6],
                 initialPC,
                 opcode);

    PrintOperandBytes (initialPC, opcode);

    std::printf ("%s ", instructionSet[opcode].instructionName);

    PrintOperandAndComment (opcode, operandInfo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintOperandBytes
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintOperandBytes (Word initialPC, Byte opcode)
{
    switch (instructionSet[opcode].globalAddressingMode)
    {
        case GlobalAddressingMode::Accumulator:
        case GlobalAddressingMode::SingleByteNoOperand:
            std::printf ("             ");
            break;

        case GlobalAddressingMode::Immediate:
        case GlobalAddressingMode::ZeroPage:
        case GlobalAddressingMode::ZeroPageIndirectY:
        case GlobalAddressingMode::ZeroPageXIndirect:
        case GlobalAddressingMode::ZeroPageX:
        case GlobalAddressingMode::ZeroPageY:
            std::printf ("%02X           ", memory[initialPC + 1]);
            break;

        case GlobalAddressingMode::Absolute:
        case GlobalAddressingMode::AbsoluteX:
        case GlobalAddressingMode::AbsoluteY:
        case GlobalAddressingMode::JumpAbsolute:
        case GlobalAddressingMode::JumpIndirect:
        case GlobalAddressingMode::Relative:
            std::printf ("%02X %02X        ", memory[initialPC + 1], memory[initialPC + 2]);
            break;

        default:
            ASSERT (false);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintOperandAndComment
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo)
{
    if (!instructionSet[opcode].isLegal)
    {
        return;
    }

    // print the operand and comment if applicable
    switch (instructionSet[opcode].globalAddressingMode)
    {
        case GlobalAddressingMode::Absolute:
            printf ("$%04X   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::AbsoluteX:
            printf ("$%04X,X ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::AbsoluteY:
            printf ("$%04X,Y ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::Immediate:
            printf ("#$%02X", operandInfo.operand);
            break;

        case GlobalAddressingMode::JumpAbsolute:
            printf ("$%04X", operandInfo.location);
            break;

        case GlobalAddressingMode::JumpIndirect:
            printf ("($%04X) ; $%04X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::Relative:
            printf ("$%02X     ; $%04X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPage:
            printf ("$%02X     ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageXIndirect:
            printf ("($%02X,X) ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageIndirectY:
            printf ("($%02X),Y ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageX:
            printf ("$%02X,X   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        case GlobalAddressingMode::ZeroPageY:
            printf ("$%02X,Y   ; $%02X", operandInfo.location, operandInfo.operand);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperand
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperand (Microcode microcode, OperandInfo & operandInfo)
{
    operandInfo.location         = 0;
    operandInfo.effectiveAddress = 0;
    operandInfo.operand          = 0;

    if (!microcode.isLegal)
    {
        ASSERT (false);
        return;
    }

    if (microcode.globalAddressingMode == GlobalAddressingMode::SingleByteNoOperand ||
        microcode.globalAddressingMode == GlobalAddressingMode::Accumulator)
    {
        return;
    }

    // Advance the program counter to the operand byte
    ++PC;

    switch (microcode.globalAddressingMode)
    {
    case GlobalAddressingMode::Absolute:          FetchOperandAbsolute          (operandInfo, microcode);    break;
    case GlobalAddressingMode::AbsoluteX:         FetchOperandAbsoluteX         (operandInfo);               break;
    case GlobalAddressingMode::AbsoluteY:         FetchOperandAbsoluteY         (operandInfo);               break;
    case GlobalAddressingMode::Immediate:         FetchOperandImmediate         (operandInfo);               break;
    case GlobalAddressingMode::JumpAbsolute:      FetchOperandJumpAbsolute      (operandInfo);               break;
    case GlobalAddressingMode::JumpIndirect:      FetchOperandJumpIndirect      (operandInfo);               break;
    case GlobalAddressingMode::Relative:          FetchOperandRelative          (operandInfo);               break;
    case GlobalAddressingMode::ZeroPage:          FetchOperandZeroPage          (operandInfo);               break;
    case GlobalAddressingMode::ZeroPageX:         FetchOperandZeroPageX         (operandInfo);               break;
    case GlobalAddressingMode::ZeroPageY:         FetchOperandZeroPageY         (operandInfo);               break;
    case GlobalAddressingMode::ZeroPageXIndirect: FetchOperandZeroPageXIndirect (operandInfo);               break;
    case GlobalAddressingMode::ZeroPageIndirectY: FetchOperandZeroPageIndirectY (operandInfo);               break;

    default:
        std::printf ("Unhandled addressing mode %d\n", microcode.instruction.asBits.addressingMode);
        ASSERT (false);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageXIndirect
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = ReadWord (operandInfo.location + X);
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPage
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPage (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandImmediate
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandImmediate (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location = ReadByte (PC);
    operandInfo.operand  = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpAbsolute
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpAbsolute (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = operandInfo.location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandJumpIndirect
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandJumpIndirect (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = ReadWord (operandInfo.location);
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandRelative
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandRelative (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = (PC + 1) + (SByte) operandInfo.location;
    operandInfo.operand          = operandInfo.effectiveAddress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsolute
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsolute (Cpu::OperandInfo & operandInfo, Microcode & microcode)
{
    operandInfo.location         = ReadWord (PC++);
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageIndirectY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageIndirectY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = ReadByte (PC);
    operandInfo.effectiveAddress  = ReadWord (operandInfo.location);
    operandInfo.effectiveAddress += Y;
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageX
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = operandInfo.location + X;
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandZeroPageY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandZeroPageY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = ReadByte (PC);
    operandInfo.effectiveAddress = operandInfo.location + Y;
    operandInfo.operand          = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteY
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = ReadWord (PC++);
    operandInfo.effectiveAddress  = operandInfo.location;
    operandInfo.effectiveAddress += Y;
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FetchOperandAbsoluteX
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::FetchOperandAbsoluteX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = ReadWord (PC++);
    operandInfo.effectiveAddress  = operandInfo.location;
    operandInfo.effectiveAddress += X;
    operandInfo.operand           = ReadByte (operandInfo.effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteInstruction
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::ExecuteInstruction (Microcode microcode, const OperandInfo & operandInfo)
{
    Byte * pAccumulator = nullptr;

    if (microcode.globalAddressingMode == GlobalAddressingMode::Accumulator)
    {
        pAccumulator = &A;
    }

    switch (microcode.operation)
    {
    case Microcode::AddWithCarry:         CpuOperations::AddWithCarry         (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::And:                  CpuOperations::And                  (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::BitTest:              CpuOperations::BitTest              (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Branch:               CpuOperations::Branch               (*this, microcode.instruction, operandInfo.operand);                   break;
    case Microcode::Break:                CpuOperations::Break                (*this);                                                               break;
    case Microcode::Compare:              CpuOperations::Compare              (*this, *microcode.pSourceRegister, (Byte) operandInfo.operand);       break;
    case Microcode::Decrement:            CpuOperations::Decrement            (*this, microcode.pSourceRegister, operandInfo.effectiveAddress);      break;
    case Microcode::Increment:            CpuOperations::Increment            (*this, microcode.pSourceRegister, operandInfo.effectiveAddress);      break;
    case Microcode::Jump:                 CpuOperations::Jump                 (*this, microcode.instruction, operandInfo.operand);                   break;
    case Microcode::Load:                 CpuOperations::Load                 (*this, *microcode.pDestinationRegister, (Byte) operandInfo.operand);  break;
    case Microcode::NoOperation:          CpuOperations::NoOperation          (*this);                                                               break;
    case Microcode::Or:                   CpuOperations::Or                   (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Pull:                 CpuOperations::Pull                 (*this, microcode.pDestinationRegister);                               break;
    case Microcode::Push:                 CpuOperations::Push                 (*this, microcode.pSourceRegister);                                    break;
    case Microcode::ReturnFromInterrupt:  CpuOperations::ReturnFromInterrupt  (*this);                                                               break;
    case Microcode::ReturnFromSubroutine: CpuOperations::ReturnFromSubroutine (*this);                                                               break;
    case Microcode::RotateLeft:           CpuOperations::RotateLeft           (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::RotateRight:          CpuOperations::RotateRight          (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::SetFlag:              CpuOperations::SetFlag              (*this, microcode.instruction);                                        break;
    case Microcode::ShiftLeft:            CpuOperations::ShiftLeft            (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::ShiftRight:           CpuOperations::ShiftRight           (*this, pAccumulator, operandInfo.effectiveAddress);                   break;
    case Microcode::Store:                CpuOperations::Store                (*this, *microcode.pSourceRegister, operandInfo.effectiveAddress);     break;
    case Microcode::SubtractWithCarry:    CpuOperations::SubtractWithCarry    (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Transfer:             CpuOperations::Transfer             (*this, microcode.pSourceRegister, microcode.pDestinationRegister);    break;
    case Microcode::Xor:                  CpuOperations::Xor                  (*this, (Byte) operandInfo.operand);                                   break;

    default:                          
        std::printf ("Unimplemented instruction:  %s\n", microcode.instructionName);                                
        ASSERT (false);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushByte
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PushByte (Byte value)
{
    WriteByte (stackAddress + SP--, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushWord
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::PushWord (Word value)
{
    PushByte (value >> 8);
    PushByte (value & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopByte
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::PopByte ()
{
    return ReadByte(stackAddress + ++SP);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopWord
//
////////////////////////////////////////////////////////////////////////////////

Word Cpu::PopWord ()
{
    Byte lo = PopByte ();
    Byte hi = PopByte ();
    return lo | (hi << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::WriteByte (Word address, Byte value)
{
    memory[address] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::WriteWord (Word address, Word value)
{
    memory[address]     = value & 0xFF;
    memory[address + 1] = value >> 8;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte Cpu::ReadByte (Word address)
{
    return memory[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word Cpu::ReadWord (Word address)
{
    return memory[address] | 
           memory[address + 1] << 8;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeInstructionSet
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeInstructionSet ()
{
    InitializeGroup00 ();
    InitializeGroup01 ();
    InitializeGroup10 ();
    InitializeMisc ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup00
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup00 ()
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
        { _00::STY,          _00::AMF_ZeroPage  | _00::AMF_Absolute | _00::AMF_ZeroPageX, Microcode::Store,   nullptr, nullptr },
        { _00::LDY,          _00::__AMF_AllModes,                                         Microcode::Load,    nullptr, &Y      },
        { _00::CPY,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &Y,      nullptr },
        { _00::CPX,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &X,      nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_00::__AM_Count, _00::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group00, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup01
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup01 ()
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
        { _01::STA, _01::__AMF_AllModes & ~(_01::AMF_Immediate), Microcode::Store,             &A,      nullptr },
        { _01::LDA, _01::__AMF_AllModes,                         Microcode::Load,              nullptr, &A      },
        { _01::CMP, _01::__AMF_AllModes,                         Microcode::Compare,           &A,      nullptr },
        { _01::SBC, _01::__AMF_AllModes,                         Microcode::SubtractWithCarry, nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_01::__AM_Count, _01::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group01, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeGroup10
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeGroup10 ()
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
        { _10::ASL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftLeft,   &A,      nullptr },
        { _10::ROL, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateLeft,  &A,      nullptr },
        { _10::LSR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::ShiftRight,  &A,      nullptr },
        { _10::ROR, _10::__AMF_AllModes & ~(_10::AMF_Immediate),                     Microcode::RotateRight, &A,      nullptr },
        { _10::STX, _10::AMF_ZeroPage | _10::AMF_Absolute | _10::AMF_ZeroPageX,      Microcode::Store,       &X,      nullptr },
        { _10::LDX, _10::__AMF_AllModes & ~(_10::AMF_Absolute),                      Microcode::Load,        nullptr, &X      },
        { _10::DEC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Absolute), Microcode::Decrement,   nullptr, nullptr },
        { _10::INC, _10::__AMF_AllModes & ~(_10::AMF_Immediate | _10::AMF_Absolute), Microcode::Increment,   nullptr, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_10::__AM_Count, _10::instructionName, entry.opcode, entry.addressingModeFlags, Microcode::Group10, entry.operation, entry.pSourceRegister, entry.pDestinationRegister);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeMisc
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::InitializeMisc ()
{
    struct TableEntry
    {
        GroupMisc::Opcode                      opcode;
        GlobalAddressingMode::AddressingMode   addressingMode;
        Microcode::Operation                   operation;
        Byte                                 * pSourceRegister;
        Byte                                 * pDestinationRegister;
    };

    TableEntry table[] =
    {
        { GroupMisc::BPL, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BMI, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BVC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BVS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BCC, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BCS, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BNE, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        { GroupMisc::BEQ, GlobalAddressingMode::Relative,            Microcode::Branch,               nullptr,        nullptr        },
        
        { GroupMisc::BRK, GlobalAddressingMode::SingleByteNoOperand, Microcode::Break,                nullptr,        nullptr        },
        { GroupMisc::JSR, GlobalAddressingMode::JumpAbsolute,        Microcode::Jump,                 nullptr,        nullptr        },
        { GroupMisc::RTI, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromInterrupt,  nullptr,        nullptr        },
        { GroupMisc::RTS, GlobalAddressingMode::SingleByteNoOperand, Microcode::ReturnFromSubroutine, nullptr,        nullptr        },
         
        { GroupMisc::PHP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 &status.status, nullptr        },
        { GroupMisc::PLP, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        &status.status },
        { GroupMisc::PHA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Push,                 &A,             nullptr        },
        { GroupMisc::PLA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Pull,                 nullptr,        &A             },
        { GroupMisc::DEY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            &Y,             nullptr        },
        { GroupMisc::TAY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &A,             &Y             },
        { GroupMisc::INY, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            &Y,             nullptr        },
        { GroupMisc::INX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Increment,            &X,             nullptr        },
         
        { GroupMisc::CLC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::SEC, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::CLI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::SEI, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::TYA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &Y,             &A             },
        { GroupMisc::CLV, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::CLD, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
        { GroupMisc::SED, GlobalAddressingMode::SingleByteNoOperand, Microcode::SetFlag,              &status.status, nullptr        },
         
        { GroupMisc::TXA, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &X,             &A             },
        { GroupMisc::TXS, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &X,             &SP            },
        { GroupMisc::TAX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &A,             &X             },
        { GroupMisc::TSX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Transfer,             &SP,            &X             },
        { GroupMisc::DEX, GlobalAddressingMode::SingleByteNoOperand, Microcode::Decrement,            &X,             nullptr        },
        { GroupMisc::NOP, GlobalAddressingMode::SingleByteNoOperand, Microcode::NoOperation,          nullptr,        nullptr        },
    };


    for (TableEntry entry : table)
    {
        Instruction instruction            = Instruction (GroupMisc::instruction[entry.opcode].instruction);
        instructionSet[instruction.asByte] = Microcode (instruction, GroupMisc::instruction[entry.opcode].name, entry.operation, entry.addressingMode, entry.pSourceRegister, entry.pDestinationRegister);
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateInstruction
//
////////////////////////////////////////////////////////////////////////////////

void Cpu::CreateInstruction (uint32_t                      addressingModeMax,
                             const char           * const  instructionName[],
                             Byte                          opcode,
                             Byte                          addressingModeFlags,
                             Byte                          group,
                             Microcode::Operation          operation,
                             Byte                 *        pSourceRegister,
                             Byte                 *        pDestinationRegister)
{
    Byte addressingMode = 0;
    Byte currentAddressingModeFlag = 1;

    while (addressingMode < addressingModeMax)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction            = Instruction (opcode, addressingMode, group);
            instructionSet[instruction.asByte] = Microcode   (instruction, instructionName[opcode], operation, pSourceRegister, pDestinationRegister);
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::LoadBinary (const std::string & filename, Word address)
{
    HRESULT       hr      = S_OK;
    std::ifstream file      (filename, std::ios::binary);
    bool          fLoaded = false;

    CBRA (file.is_open ());

    fLoaded = LoadBinary (file, address);
    CBR  (fLoaded);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinary
//
////////////////////////////////////////////////////////////////////////////////

bool Cpu::LoadBinary (std::istream & stream, Word address)
{
    HRESULT hr = S_OK;

    // Determine stream size
    stream.seekg (0, std::ios::end);
    auto size = stream.tellg ();
    stream.seekg (0, std::ios::beg);

    CBRA (!stream.bad ());
    CBR  (size >= 0 && (size_t) size <= memSize - address);

    // Read directly into CPU memory — no intermediate buffer
    stream.read (reinterpret_cast<char *>(memory.data () + address), size);

    CBRA (!stream.bad ());

Error:
    return SUCCEEDED (hr);
}
