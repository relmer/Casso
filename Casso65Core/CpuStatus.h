#pragma once





// NV1B DIZC
struct CpuFlags
{
    Byte carry            : 1; // Carry
    Byte zero             : 1; // Zero
    Byte interruptDisable : 1; // Interrupt Disable
    Byte decimal          : 1; // BCD Mode
    Byte brk              : 1; // Break
    Byte alwaysOne        : 1; // Always 1
    Byte overflow         : 1; // Overflow
    Byte negative         : 1; // Negative
};





union CpuStatus
{
    Byte        status;
    CpuFlags    flags;
};


