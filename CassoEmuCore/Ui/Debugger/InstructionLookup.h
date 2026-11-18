#pragma once

#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup
//
//  The instructions around an address in the session's memory: the one that
//  starts there, its length, the one before it, where an RTS returns to and
//  the address an instruction's operand accesses.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionLookup
{
public:
    //  The instruction that starts at an address, or none at an I/O address.
    static std::optional<DisassemblyLine>  GetInstructionAt (DebugSession & session, Word address);

    static Word                 GetInstructionLength   (DebugSession & session, Word address);
    static Word                 GetPreviousInstruction (DebugSession & session, Word address);
    static std::optional<Word>  GetReturnAddress       (DebugSession & session);
    static std::optional<Word>  GetOperandAddress      (DebugSession & session, Word address);
};
