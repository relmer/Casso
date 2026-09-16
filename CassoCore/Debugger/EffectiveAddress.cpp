#include "Pch.h"

#include "Debugger/EffectiveAddress.h"

#include "Debugger/IDebugExpressionContext.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddress::Predict
//
//  Decodes the opcode and its operand bytes, resolves the addressing mode with
//  the 6502's own wrapping rules, and lists what would be touched. An opcode
//  the table does not define, or bytes the peek cannot read, give an error
//  and an empty prediction.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EffectiveAddress::Predict (
    const Microcode               * instructionSet,
    Word                            pc,
    const Cpu6502Registers        & registers,
    const IDebugExpressionContext & memory,
    AccessPrediction              & prediction)
{
    HRESULT            hr        = S_OK;
    const Microcode  * microcode = nullptr;
    Byte               opcode    = 0;
    Byte               lo        = 0;
    Byte               hi        = 0;
    Word               operand   = 0;
    Word               pointer   = 0;
    Word               effective = 0;
    Byte               size      = 0;
    bool               isRead    = false;
    PredictedAccess    access    = PredictedAccess::Read;



    prediction.touches.clear();

    CBRAEx (instructionSet, E_UNEXPECTED);

    isRead = memory.TryPeek (pc, opcode);
    CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    microcode = &instructionSet[opcode];
    CBREx (microcode->isLegal, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    size = OpcodeTable::GetOperandSize (microcode->globalAddressingMode);

    if (size >= 1)
    {
        isRead = memory.TryPeek ((Word) (pc + 1), lo);
        CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    if (size >= 2)
    {
        isRead = memory.TryPeek ((Word) (pc + 2), hi);
        CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    operand = (Word) (lo | (hi << 8));
    access  = ClassifyOperand (*microcode);

    switch (microcode->globalAddressingMode)
    {
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageRelative:
        effective = lo;
        break;

    case GlobalAddressingMode::ZeroPageX:
        effective = (Word) ((lo + registers.x) & kZeroPageMask);
        break;

    case GlobalAddressingMode::ZeroPageY:
        effective = (Word) ((lo + registers.y) & kZeroPageMask);
        break;

    case GlobalAddressingMode::Absolute:
        effective = operand;
        break;

    case GlobalAddressingMode::AbsoluteX:
        effective = (Word) (operand + registers.x);
        break;

    case GlobalAddressingMode::AbsoluteY:
        effective = (Word) (operand + registers.y);
        break;

    // (zp,X): the pointer is at zp+X, wrapping within the zero page.
    case GlobalAddressingMode::ZeroPageXIndirect:
        pointer = (Word) ((lo + registers.x) & kZeroPageMask);
        isRead  = TryPeekWord (memory, pointer, (Word) ((pointer + 1) & kZeroPageMask), effective);
        CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        prediction.touches.push_back ({ pointer,                                      PredictedAccess::Read });
        prediction.touches.push_back ({ (Word) ((pointer + 1) & kZeroPageMask),       PredictedAccess::Read });
        break;

    // (zp),Y and (zp): the pointer is at zp, wrapping within the zero page.
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageIndirect:
        pointer = lo;
        isRead  = TryPeekWord (memory, pointer, (Word) ((pointer + 1) & kZeroPageMask), effective);
        CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        prediction.touches.push_back ({ pointer,                                      PredictedAccess::Read });
        prediction.touches.push_back ({ (Word) ((pointer + 1) & kZeroPageMask),       PredictedAccess::Read });

        if (microcode->globalAddressingMode == GlobalAddressingMode::ZeroPageIndirectY)
        {
            effective = (Word) (effective + registers.y);
        }

        break;

    // JMP (abs) on the NMOS part fetches the high byte from the same page as
    // the low byte; the CMOS part fetches it from the next address.
    case GlobalAddressingMode::JumpIndirect:
        pointer = operand;
        prediction.touches.push_back ({ pointer,                                                        PredictedAccess::Read });
        prediction.touches.push_back ({ (Word) ((pointer & kPageMask) | ((pointer + 1) & kZeroPageMask)), PredictedAccess::Read });
        break;

    case GlobalAddressingMode::JumpIndirectCmos:
        pointer = operand;
        prediction.touches.push_back ({ pointer,              PredictedAccess::Read });
        prediction.touches.push_back ({ (Word) (pointer + 1), PredictedAccess::Read });
        break;

    case GlobalAddressingMode::AbsoluteXIndirect:
        pointer = (Word) (operand + registers.x);
        prediction.touches.push_back ({ pointer,              PredictedAccess::Read });
        prediction.touches.push_back ({ (Word) (pointer + 1), PredictedAccess::Read });
        break;

    default:
        break;
    }

    if (TouchesOperand (microcode->globalAddressingMode))
    {
        prediction.touches.push_back ({ effective, access });
    }

Error:
    if (FAILED (hr))
    {
        prediction.touches.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddress::TryPeekWord
//
////////////////////////////////////////////////////////////////////////////////

bool EffectiveAddress::TryPeekWord (const IDebugExpressionContext & memory, Word lo, Word hi, Word & value)
{
    Byte  low    = 0;
    Byte  high   = 0;
    bool  isRead = memory.TryPeek (lo, low) && memory.TryPeek (hi, high);



    value = (Word) (low | (high << 8));
    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddress::ClassifyOperand
//
//  Stores write, read-modify-writes do both, and everything else that has a
//  memory operand reads it.
//
////////////////////////////////////////////////////////////////////////////////

PredictedAccess EffectiveAddress::ClassifyOperand (const Microcode & microcode)
{
    switch (microcode.operation)
    {
    case Microcode::Store:
    case Microcode::StoreZero:
    case Microcode::StoreAccumulatorAndX:
        return PredictedAccess::Write;

    case Microcode::Increment:
    case Microcode::Decrement:
    case Microcode::ShiftLeft:
    case Microcode::ShiftRight:
    case Microcode::RotateLeft:
    case Microcode::RotateRight:
    case Microcode::TestAndSetBits:
    case Microcode::TestAndResetBits:
    case Microcode::ResetMemoryBit:
    case Microcode::SetMemoryBit:
    case Microcode::DecrementAndCompare:
    case Microcode::IncrementAndSubtract:
    case Microcode::ShiftLeftAndOr:
    case Microcode::RotateLeftAndAnd:
    case Microcode::ShiftRightAndXor:
    case Microcode::RotateRightAndAdd:
        return PredictedAccess::ReadWrite;

    default:
        return PredictedAccess::Read;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddress::TouchesOperand
//
//  The modes whose effective address is data the instruction accesses, as
//  opposed to a jump destination, a branch, or no memory at all.
//
////////////////////////////////////////////////////////////////////////////////

bool EffectiveAddress::TouchesOperand (GlobalAddressingMode::AddressingMode mode)
{
    switch (mode)
    {
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageIndirect:
    case GlobalAddressingMode::ZeroPageRelative:
        return true;

    default:
        return false;
    }
}
