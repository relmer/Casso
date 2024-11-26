#pragma once



typedef unsigned char   byte;
typedef unsigned short  word;



struct Flags
{
    byte c : 1;
    byte z : 1;
    byte i : 1;
    byte d : 1;
    byte b : 1;
    byte v : 1;
    byte n : 1;
};


union Status
{
    byte    status;
    Flags   flags;
};



class Instruction
{
public:
    Instruction () 
    {
        asByte = 0;
    }


    Instruction (byte opcode, byte addressingMode, byte group) 
    {
        asBits.group          = group;   
        asBits.addressingMode = addressingMode; 
        asBits.opcode         = opcode;
    }

public:
    union
    {
        byte asByte;

        // Bitfields must be in reverse order (low to high)
        struct InstructionBits
        {   
            byte group          : 2;
            byte addressingMode : 3;
            byte opcode         : 3;
        } asBits;
    };
};



class Group01
{
protected:
    static constexpr byte group = 0b01;

public:
    enum Opcode
    {
        ORA,    // 000 = OR
        AND,    // 001 = AND
        EOR,    // 010 = XOR
        ADC,    // 011 = ADD with Carry
        STA,    // 100 = Store
        LDA,    // 101 = Load
        CMP,    // 110 = Compare
        SBC     // 111 = Subtract with Carry
    };

    static constexpr const char * opcodeName[] =
    { 
        "ORA", 
        "AND", 
        "EOR", 
        "ADC", 
        "STA", 
        "LDA", 
        "CMP", 
        "SBC" 
    };

    enum AddressingMode
    {
        ZeroPageXIndirect,  // 000 (Zero Page, X)
        ZeroPage,           // 001 Zero Page
        Immediate,          // 010 #Immediate
        Absolute,           // 011 Absolute
        ZeroPageIndirectY,  // 100 (Zero Page), Y
        ZeroPageX,          // 101 Zero Page, X
        AbsoluteY,          // 110 Absolute, Y
        AbsoluteX           // 111 Absolute, X
    };

    static constexpr const char * addressingModeName[] =
    {
        "(Zero Page, X)",
        "Zero Page",
        "#Immediate",
        "Absolute",
        "(Zero Page), Y",
        "Zero Page, X",
        "Absolute, Y",
        "Absolute, X"
    };

public:
    static Instruction CreateInstruction (Opcode opcode, AddressingMode addressingMode)
    {
        return Instruction (opcode, addressingMode, group);
    }
};




class Microcode
{
public:
    Microcode ()
        : isLegal (false)
    {
    }

    Microcode (Instruction instruction) : 
        isLegal     (true),
        instruction (instruction)
    {
    }

public:
    bool        isLegal;
    Instruction instruction;

    // microcode goes here
};



class Cpu
{
public:
    Cpu ();
    void Reset ();
    void Run ();

protected:
    void InitializeInstructionSet ();
    void InitializeGroup01 ();
    void PrintInstructionSet ();
    word Decode (byte opcode);
    byte FetchInstruction ();

protected:
    static constexpr size_t memSize = 64 * 1024;
    byte                    memory[memSize];

    word    pc;
    word    sp;

    Status  status;

    byte    a;
    byte    x;
    byte    y;

protected:
    Microcode instructionSet[256];
};

