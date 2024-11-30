#pragma once



// NV1B DIZC
struct CpuFlags
{
    Byte c         : 1;
    Byte z         : 1;
    Byte i         : 1;
    Byte d         : 1;
    Byte b         : 1;
    Byte alwaysOne : 1;
    Byte v         : 1;
    Byte n         : 1;
};



union CpuStatus
{
    Byte        status;
    CpuFlags    flags;
};


