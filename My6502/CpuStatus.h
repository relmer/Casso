#pragma once



// NV1B DIZC
struct CpuFlags
{
    Byte c         : 1; // Carry
    Byte z         : 1; // Zero
    Byte i         : 1; // Interrupt Disable
    Byte d         : 1; // BCD Mode
    Byte b         : 1; // Break
    Byte alwaysOne : 1; // Always 1
    Byte v         : 1; // Overflow
    Byte n         : 1; // Negative
};



union CpuStatus
{
    Byte        status;
    CpuFlags    flags;
};


