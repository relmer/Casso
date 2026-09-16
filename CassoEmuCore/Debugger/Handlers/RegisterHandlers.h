#pragma once

#include "Debugger/IDebugCommandHandler.h"

class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers
//
//  R and REGISTER, the flag commands, the stack commands POP, PPOP and PUSH,
//  and STACK.
//
////////////////////////////////////////////////////////////////////////////////

class RegisterHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    static StackData  MakeStackData (IDebugTarget & target);

private:
    static constexpr Word  kStackPage = 0x0100;
    static constexpr Byte  kStackTop  = 0xFF;
    static constexpr Word  kMaxByte   = 0xFF;

    static void  ShowRegisters (IDebugTarget & target, Reply & reply);
    static void  SetRegister   (IDebugTarget & target, const DebugCommand & command, Reply & reply);
    static void  ChangeFlag    (IDebugTarget & target, const DebugCommand & command, bool isSet, Reply & reply);
    static void  Pop           (IDebugTarget & target, bool isWord, Reply & reply);
    static void  Push          (IDebugTarget & target, const DebugCommand & command, Reply & reply);
    static bool  TryGetFlagBit (char letter, Byte & bit);
    static Byte  PeekStack     (IDebugTarget & target, Byte sp);
};
