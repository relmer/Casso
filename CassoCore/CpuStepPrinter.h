#pragma once

#include "Pch.h"

#include "CpuOperandInfo.h"
#include "CpuStatus.h"

class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuStepPrinter
//
//  A single-step trace line for an instruction: registers, flags, address,
//  opcode bytes and disassembly, in fixed columns so a run diffs cleanly
//  against a reference trace. The registers, the instruction set and a
//  reader of memory are given; nothing in the CPU calls it today.
//
////////////////////////////////////////////////////////////////////////////////

class CpuStepPrinter
{
public:
    using ByteReader = std::function<Byte (Word address)>;

    struct Registers
    {
        Byte       a      = 0;
        Byte       x      = 0;
        Byte       y      = 0;
        Byte       sp     = 0;
        CpuStatus  status = {};
    };

    static void  PrintSingleStepInfo    (const Registers       & regs,
                                         Word                    initialPC,
                                         Byte                    opcode,
                                         const CpuOperandInfo  & operandInfo,
                                         const Microcode       * instructionSet,
                                         const ByteReader      & read);
    static void  PrintOperandBytes      (Word initialPC, Byte opcode, const Microcode * instructionSet, const ByteReader & read);
    static void  PrintOperandAndComment (Byte opcode, const CpuOperandInfo & operandInfo, const Microcode * instructionSet);
};
