#pragma once

#include "Cpu.h"



class CpuOperations
{
public:
    CpuOperations () = delete;

    static void Or           (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void And          (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Xor          (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void AddWithCarry (Cpu & cpu, Byte & registerAffected, Byte operand);

}; 
