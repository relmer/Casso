#include "Pch.h"

#include "Debugger/Handlers/RegisterHandlers.h"

#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool RegisterHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget & target = session.GetTarget();



    switch (command.verb)
    {
    case DebugVerb::ShowRegisters: ShowRegisters (target, reply);                 return true;
    case DebugVerb::SetRegister:   SetRegister   (target, command, reply);        return true;
    case DebugVerb::SetFlag:       ChangeFlag    (target, command, true,  reply); return true;
    case DebugVerb::ClearFlag:     ChangeFlag    (target, command, false, reply); return true;
    case DebugVerb::PopStack:      Pop           (target, false, reply);          return true;
    case DebugVerb::PopStackWord:  Pop           (target, true,  reply);          return true;
    case DebugVerb::PushStack:     Push          (target, command, reply);        return true;
    case DebugVerb::ShowStack:     reply.data = MakeStackData (target);           return true;
    default:                                                                      return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::MakeStackData
//
//  The stack pointer and the bytes above it, from $01FF down to SP+1.
//
////////////////////////////////////////////////////////////////////////////////

StackData RegisterHandlers::MakeStackData (IDebugTarget & target)
{
    Cpu6502Registers  registers = target.GetRegisters();
    StackData         data;



    data.sp = registers.sp;

    for (int offset = kStackTop; offset > registers.sp; --offset)
    {
        data.entries.push_back ({ (Word) (kStackPage + offset), PeekStack (target, (Byte) offset) });
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::ShowRegisters
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHandlers::ShowRegisters (IDebugTarget & target, Reply & reply)
{
    reply.data = RegistersData { target.GetRegisters() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::SetRegister
//
//  A, X, Y and P take a byte. S takes a byte, or a stack address $0100-$01FF
//  whose low byte is the pointer. PC takes a word. The reply shows the
//  registers as they now are.
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHandlers::SetRegister (IDebugTarget & target, const DebugCommand & command, Reply & reply)
{
    static constexpr Word  kStackLast = 0x01FF;
    Cpu6502Registers       registers  = target.GetRegisters();
    Word                   value      = command.a1;
    bool                   isStack    = command.text == "S";
    bool                   isByte     = command.text != "PC";
    bool                   isStackAddress = isStack && value >= kStackPage && value <= kStackLast;



    if (isByte && value > kMaxByte && !isStackAddress)
    {
        reply.SetError (CommandStatus::Error, "value out of range",
                        std::format ("{} takes a byte, $00-$FF.", command.text));
        return;
    }

    if      (command.text == "A")  { registers.a  = (Byte) value; }
    else if (command.text == "X")  { registers.x  = (Byte) value; }
    else if (command.text == "Y")  { registers.y  = (Byte) value; }
    else if (command.text == "P")  { registers.p  = (Byte) value; }
    else if (command.text == "S")  { registers.sp = (Byte) value; }
    else if (command.text == "PC") { registers.pc = value; }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("{} is not a register. The registers are A, X, Y, P, S, and PC.", command.text));
        return;
    }

    target.SetRegisters (registers);
    reply.data = RegistersData { registers };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::ChangeFlag
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHandlers::ChangeFlag (IDebugTarget & target, const DebugCommand & command, bool isSet, Reply & reply)
{
    Cpu6502Registers  registers = target.GetRegisters();
    Byte              bit       = 0;



    if (command.text.size() != 1 || !TryGetFlagBit (command.text[0], bit))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "A flag is one of C, Z, I, D, B, R, V, or N.");
        return;
    }

    registers.p = isSet ? (Byte) (registers.p | bit) : (Byte) (registers.p & ~bit);
    target.SetRegisters (registers);
    reply.data = RegistersData { registers };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::Pop
//
//  POP pulls a byte into A. PPOP pulls a word into PC as it is on the stack,
//  without the increment RTS applies.
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHandlers::Pop (IDebugTarget & target, bool isWord, Reply & reply)
{
    static constexpr int  kByteBits  = 8;
    Cpu6502Registers      registers  = target.GetRegisters();
    Byte                  low        = 0;
    Byte                  high       = 0;



    if (isWord)
    {
        low  = PeekStack (target, (Byte) (registers.sp + 1));
        high = PeekStack (target, (Byte) (registers.sp + 2));
        registers.sp = (Byte) (registers.sp + 2);
        registers.pc = (Word) (low | (high << kByteBits));
    }
    else
    {
        registers.sp = (Byte) (registers.sp + 1);
        registers.a  = PeekStack (target, registers.sp);
    }

    target.SetRegisters (registers);
    reply.data = RegistersData { registers };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::Push
//
//  Each byte goes to $0100+SP, and SP moves down, as PHA does.
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHandlers::Push (IDebugTarget & target, const DebugCommand & command, Reply & reply)
{
    Cpu6502Registers  registers = target.GetRegisters();



    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PUSH takes one or more bytes.");
        return;
    }

    for (Byte value : command.values)
    {
        target.TryPoke ((Word) (kStackPage + registers.sp), value);
        registers.sp = (Byte) (registers.sp - 1);
    }

    target.SetRegisters (registers);
    reply.data = MakeStackData (target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::TryGetFlagBit
//
////////////////////////////////////////////////////////////////////////////////

bool RegisterHandlers::TryGetFlagBit (char letter, Byte & bit)
{
    static constexpr std::string_view  kLetters = "CZIDBRVN";
    size_t                             index    = kLetters.find (letter);



    if (index == std::string_view::npos)
    {
        return false;
    }

    bit = (Byte) (1 << index);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlers::PeekStack
//
////////////////////////////////////////////////////////////////////////////////

Byte RegisterHandlers::PeekStack (IDebugTarget & target, Byte sp)
{
    Byte  value = 0;



    target.TryPeek ((Word) (kStackPage + sp), value);
    return value;
}
