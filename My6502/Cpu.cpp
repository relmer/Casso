#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Group00.h"
#include "Group01.h"
#include "Utils.h"



Cpu::Cpu ()
{
    InitializeInstructionSet ();
}



void Cpu::Reset ()
{
    /*
    PC = 0xFFFC;
    SP = 0x0100;
    */

    status.status = 0;

    A = 0;
    X = 0;
    Y = 0;

    memset (memory, 0, sizeof (memory));




    // Test code
    PC = 0x8000;
    SP = 0x0100;

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

    memory[addr++] = 0xA9;  // LDA, #immediate
    memory[addr++] = 0xCF;  // 
    memory[addr++] = 0x8D;  // STA, $0034
    memory[addr++] = 0x34;  // 
    memory[addr++] = 0x00;  // 
    memory[addr++] = 0xA9;  // LDA, #immediate
    memory[addr++] = 0xF0;  // 
    memory[addr++] = 0x24;  // BIT, zp
    memory[addr++] = 0x34;  // 
    memory[addr++] = 0xA4;  // LDY, $34
    memory[addr++] = 0x34;  //
    memory[addr++] = 0xC0;  // CPY, #CF
    memory[addr++] = 0xCF;  //




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
}



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



void Cpu::PrintSingleStepInfo (Word initialPC, Byte opcode, const OperandInfo & operandInfo)
{
    static constexpr char flags[][8] =
    {
        ".......",
        "CZIDBVN"
    };

    // Print the registers and the opcode byte
    std::printf ("SP: %04x  A: %04X  X: %04X  Y: %04X  %c%c%c%c%c%c%c    [%04X] %02X ",
                 SP,
                 A,
                 X,
                 Y,
                 flags[status.flags.c][0],
                 flags[status.flags.z][1],
                 flags[status.flags.i][2],
                 flags[status.flags.d][3],
                 flags[status.flags.b][4],
                 flags[status.flags.v][5],
                 flags[status.flags.n][6],
                 initialPC,
                 opcode);

    PrintOperandBytes (initialPC, opcode);

    std::printf ("%s ", instructionSet[opcode].instructionName);

    PrintOperandAndComment (opcode, operandInfo);
}



void Cpu::PrintOperandBytes (Word initialPC, Byte opcode)
{
    switch (instructionSet[opcode].globalAddressingMode)
    {
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::Immediate:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageX:
        std::printf ("%02X           ", memory[initialPC + 1]);
        break;

    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::AbsoluteX:
        std::printf ("%02X %02X        ", memory[initialPC + 1], memory[initialPC + 2]);
        break;
    }
}



void Cpu::PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo)
{
    if (!instructionSet[opcode].isLegal)
    {
        return;
    }

    // print the operand and comment if applicable
    switch (instructionSet[opcode].globalAddressingMode)
    {
    case GlobalAddressingMode::ZeroPageXIndirect:
        printf ("($%02X,X) ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
        break;

    case GlobalAddressingMode::ZeroPage:
        printf ("$%02X     ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case GlobalAddressingMode::Immediate:
        printf ("#$%02X", operandInfo.operand);
        break;

    case GlobalAddressingMode::Absolute:
        switch (opcode)
        {
        case 0x4C:
            // Handle JMP absolute which is encoded like an immediate
            printf ("$%04X", operandInfo.location);
            break;

        case 0x6C:
            // Handle JMP (indirect) which is encoded as absolute, but is actually indirect
            printf ("($%04X) ; $%04X", operandInfo.location, operandInfo.operand);
            break;

        default:
            printf ("$%04X   ; $%02X", operandInfo.location, operandInfo.operand);
            break;
        }
        break;

    case GlobalAddressingMode::ZeroPageIndirectY:
        printf ("($%02X),Y ; ($%04X) = $%02X", operandInfo.location, operandInfo.effectiveAddress, operandInfo.operand);
        break;

    case GlobalAddressingMode::ZeroPageX:
        printf ("$%02X,X   ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case GlobalAddressingMode::AbsoluteY:
        printf ("$%04X,Y ; $%02X", operandInfo.location, operandInfo.operand);
        break;

    case GlobalAddressingMode::AbsoluteX:
        printf ("$%04X,X ; $%02X", operandInfo.location, operandInfo.operand);
        break;
    }
}



void Cpu::FetchOperand (Microcode microcode, OperandInfo & operandInfo)
{
    operandInfo.location         = 0;
    operandInfo.effectiveAddress = 0;
    operandInfo.operand          = 0;

    if (microcode.isSingleByte || !microcode.isLegal)
    {
        return;
    }

    // Advance the program counter to the operand byte
    ++PC;

    switch (microcode.globalAddressingMode)
    {
    case GlobalAddressingMode::Immediate:         FetchOperandImmediate         (operandInfo);            break;
    case GlobalAddressingMode::ZeroPage:          FetchOperandZeroPage          (operandInfo);            break;
    case GlobalAddressingMode::ZeroPageX:         FetchOperandZeroPageX         (operandInfo);            break;
    case GlobalAddressingMode::Absolute:          FetchOperandAbsolute          (operandInfo, microcode); break;
    case GlobalAddressingMode::AbsoluteX:         FetchOperandAbsoluteX         (operandInfo);            break;
    case GlobalAddressingMode::AbsoluteY:         FetchOperandAbsoluteY         (operandInfo);            break;
    case GlobalAddressingMode::ZeroPageXIndirect: FetchOperandZeroPageXIndirect (operandInfo);            break;
    case GlobalAddressingMode::ZeroPageIndirectY: FetchOperandZeroPageIndirectY (operandInfo);            break;

    default:
        std::printf ("Unhandled addressing mode %d\n", microcode.instruction.asBits.addressingMode);
        break;
    }
}



void Cpu::FetchOperandZeroPageXIndirect (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = memory[PC];
    operandInfo.effectiveAddress = memory[(operandInfo.location + X) & 0xFF]
                                 | memory[(operandInfo.location + X + 1) & 0xFF] << 8;
    operandInfo.operand          = memory[operandInfo.effectiveAddress];
}



void Cpu::FetchOperandZeroPage (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = memory[PC];
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.operand          = memory[operandInfo.effectiveAddress];
}



void Cpu::FetchOperandImmediate (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location = memory[PC];
    operandInfo.operand  = operandInfo.location;
}



void Cpu::FetchOperandAbsolute (Cpu::OperandInfo & operandInfo, Microcode & microcode)
{
    operandInfo.location         = memory[PC];
    operandInfo.location        |= memory[++PC] << 8;
    operandInfo.effectiveAddress = operandInfo.location;

    switch (microcode.instruction.asByte)
    {
    case 0x4C:
        // Handle JMP absolute which is encoded like an immediate
        operandInfo.operand = operandInfo.location;
        break;

    case 0x6C:
        // Handle JMP (indirect) which is encoded as absolute, but is actually indirect
        operandInfo.effectiveAddress = memory[operandInfo.location]
                                     | memory[operandInfo.location + 1] << 8;
        operandInfo.operand          = operandInfo.effectiveAddress;
        break;

    default:
        operandInfo.operand = memory[operandInfo.effectiveAddress];
        break;
    }
}



void Cpu::FetchOperandZeroPageIndirectY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = memory[PC];
    operandInfo.effectiveAddress = memory[operandInfo.location]
                                 | memory[operandInfo.location + 1] << 8;
    operandInfo.effectiveAddress += Y;
    operandInfo.operand          = memory[operandInfo.effectiveAddress];
}



void Cpu::FetchOperandZeroPageX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = memory[PC];
    operandInfo.effectiveAddress = operandInfo.location + X;
    operandInfo.operand          = memory[operandInfo.effectiveAddress];
}



void Cpu::FetchOperandAbsoluteY (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location          = memory[PC];
    operandInfo.location         |= memory[++PC] << 8;
    operandInfo.effectiveAddress  = operandInfo.location;
    operandInfo.effectiveAddress += Y;
    operandInfo.operand           = memory[operandInfo.effectiveAddress];
}



void Cpu::FetchOperandAbsoluteX (Cpu::OperandInfo & operandInfo)
{
    operandInfo.location         = memory[PC];
    operandInfo.location        |= memory[++PC] << 8;
    operandInfo.effectiveAddress = operandInfo.location;
    operandInfo.effectiveAddress += X;
    operandInfo.operand          = memory[operandInfo.effectiveAddress];
}



void Cpu::ExecuteInstruction (Microcode microcode, const OperandInfo & operandInfo)
{
    switch (microcode.operation)
    {
    case Microcode::AddWithCarry:       CpuOperations::AddWithCarry      (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::And:                CpuOperations::And               (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::BitTest:            CpuOperations::BitTest           (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Compare:            CpuOperations::Compare           (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);     break;
    case Microcode::Jump:               CpuOperations::Jump              (*this, microcode.instruction, operandInfo.operand);                   break;
    case Microcode::Load:               CpuOperations::Load              (*this, *microcode.pRegisterAffected, (Byte) operandInfo.operand);     break;
    case Microcode::Or:                 CpuOperations::Or                (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Store:              CpuOperations::Store             (*this, *microcode.pRegisterAffected, operandInfo.effectiveAddress);   break;
    case Microcode::SubtractWithCarry:  CpuOperations::SubtractWithCarry (*this, (Byte) operandInfo.operand);                                   break;
    case Microcode::Xor:                CpuOperations::Xor               (*this, (Byte) operandInfo.operand);                                   break;

    default:                            
        std::printf ("Unimplemented instruction:  %s\n", microcode.instructionName);                                
        break;
    }
}



void Cpu::InitializeInstructionSet ()
{
    InitializeGroup00 ();
    InitializeGroup01 ();

    PrintInstructionSet (0b00);
    PrintInstructionSet (0b01);
}



void Cpu::InitializeGroup00 ()
{
    using _00 = Group00;
    struct TableEntry
    {
        _00::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pRegisterAffected;
    };

    TableEntry table[] =
    {
        { _00::BIT,          _00::AMF_ZeroPage  | _00::AMF_Absolute,                      Microcode::BitTest, nullptr },
        { _00::JMP,          _00::AMF_Absolute,                                           Microcode::Jump,    nullptr },
        { _00::JMP_indirect, _00::AMF_Absolute,                                           Microcode::Jump,    nullptr },
        { _00::STY,          _00::AMF_ZeroPage  | _00::AMF_Absolute | _00::AMF_ZeroPageX, Microcode::Store,   nullptr },
        { _00::LDY,          _00::__AMF_AllModes,                                         Microcode::Load,    &Y      },
        { _00::CPY,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &Y      },
        { _00::CPX,          _00::AMF_Immediate | _00::AMF_ZeroPage | _00::AMF_Absolute,  Microcode::Compare, &X      },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_00::__AM_Count, _00::instructionName, entry.opcode, entry.addressingModeFlags, 0b00, entry.operation, entry.pRegisterAffected);
    }
}



void Cpu::InitializeGroup01 ()
{
    using _01 = Group01;
    struct TableEntry
    {
        _01::Opcode            opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pRegisterAffected;
    };

    TableEntry table[] =
    {
        { _01::ORA, _01::__AMF_AllModes,                         Microcode::Or,                nullptr },
        { _01::AND, _01::__AMF_AllModes,                         Microcode::And,               nullptr },
        { _01::EOR, _01::__AMF_AllModes,                         Microcode::Xor,               nullptr },
        { _01::ADC, _01::__AMF_AllModes,                         Microcode::AddWithCarry,      nullptr },
        { _01::STA, _01::__AMF_AllModes & ~(_01::AMF_Immediate), Microcode::Store,             &A      },
        { _01::LDA, _01::__AMF_AllModes,                         Microcode::Load,              &A      },
        { _01::CMP, _01::__AMF_AllModes,                         Microcode::Compare,           &A      },
        { _01::SBC, _01::__AMF_AllModes,                         Microcode::SubtractWithCarry, nullptr },
    };


    for (TableEntry entry : table)
    {
        CreateInstruction (_01::__AM_Count, _01::instructionName, entry.opcode, entry.addressingModeFlags, 0b01, entry.operation, entry.pRegisterAffected);
    }
}



void Cpu::CreateInstruction (uint32_t                      addressingModeMax,
                             const char           * const  instructionName[],
                             Byte                          opcode,
                             Byte                          addressingModeFlags,
                             Byte                          group,
                             Microcode::Operation          operation,
                             Byte                 *        pRegisterAffected)
{
    Byte globalAddressingMode = 0;
    Byte currentAddressingModeFlag = 1;

    while (globalAddressingMode < addressingModeMax)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction            = Instruction (opcode, globalAddressingMode, group);
            instructionSet[instruction.asByte] = Microcode   (instruction, instructionName[opcode], false, operation, pRegisterAffected);
        }

        ++globalAddressingMode;
        currentAddressingModeFlag <<= 1;
    }
}



void Cpu::PrintInstructionSet (int group)
{
    for (size_t i = 0; i < ARRAYSIZE (instructionSet); i++)
    {
        Byte opcode                        = instructionSet[i].instruction.asBits.opcode;
        Byte globalAddressingMode                = instructionSet[i].instruction.asBits.addressingMode;
        const char * pszInstructionName    = nullptr;
        const char * pszAddressingModeName = nullptr;

        if ((instructionSet[i].instruction.asBits.group != group) ||
            !instructionSet[i].isLegal)
        {
            continue;
        }

        switch (instructionSet[i].instruction.asBits.group)
        {
        case 0b00:
            pszInstructionName    = Group00::instructionName[opcode];
            pszAddressingModeName = GlobalAddressingMode::s_addressingModeName[Group00::s_addressingModeMap[globalAddressingMode]];
            break;

        case 0b01:
            pszInstructionName    = Group01::instructionName[opcode];
            pszAddressingModeName = Group01::addressingModeName[globalAddressingMode];
            break;

        //case 0b10:
        //    pszInstructionName = Group01::instructionName[opcode];
        //    pszAddressingModeName = Group01::addressingModeName[addressingMode];
        //    break;
        }

        std::printf ("Instruction %02X:  Group %02d, %s ($%02X) %s\n",
                        (unsigned int) i,
                        instructionSet[i].instruction.asBits.group,
                        pszInstructionName,
                        instructionSet[i].instruction.asByte,
                        pszAddressingModeName);
    }

    std::puts ("");
}



