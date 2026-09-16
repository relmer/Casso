#pragma once

#include "GlobalAddressingModes.h"
#include "I6502DebugInfo.h"
#include "Microcode.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  PredictedAccess / PredictedTouch / AccessPrediction
//
//  The memory an instruction would touch if it executed now: each address
//  with whether the instruction reads it, writes it, or both. Pointer bytes
//  an indirect mode fetches are reads in their own right and come first.
//
////////////////////////////////////////////////////////////////////////////////

enum class PredictedAccess
{
    Read,
    Write,
    ReadWrite,
};

struct PredictedTouch
{
    Word             address = 0;
    PredictedAccess  access  = PredictedAccess::Read;
};

struct AccessPrediction
{
    std::vector<PredictedTouch>  touches;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddress
//
//  Predicts, from the instruction at the program counter and the current
//  registers, which addresses it will access, so a watchpoint can stop before
//  the access happens. Operand bytes and pointers are read through the
//  side-effect-free peek, never the bus.
//
//  Only the instruction's own operand is predicted. Stack pushes and pulls,
//  interrupt vector reads, the fetch of the instruction bytes themselves, and
//  the extra bus cycles real hardware spends on indexed stores and page
//  crossings are not. A read-modify-write is one ReadWrite touch.
//
////////////////////////////////////////////////////////////////////////////////

class EffectiveAddress
{
public:
    static HRESULT  Predict (const Microcode               * instructionSet,
                             Word                            pc,
                             const Cpu6502Registers        & registers,
                             const IDebugExpressionContext & memory,
                             AccessPrediction              & prediction);

private:
    static constexpr Word  kZeroPageMask = 0x00FF;
    static constexpr Word  kPageMask     = 0xFF00;

    static bool             TryPeekWord     (const IDebugExpressionContext & memory, Word lo, Word hi, Word & value);
    static PredictedAccess  ClassifyOperand (const Microcode & microcode);
    static bool             TouchesOperand  (GlobalAddressingMode::AddressingMode mode);
};
