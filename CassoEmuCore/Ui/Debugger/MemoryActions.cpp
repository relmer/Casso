#include "Pch.h"

#include "Ui/Debugger/MemoryActions.h"
#include "Ui/Debugger/DebuggerCommandLines.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetPoke
//
//  One byte, which must match in full, as a typed deposit's values do.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetPoke (Word address, Byte value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = DebuggerActions::Make (DebugVerb::EnterBytes, DebuggerCommandLines::GetPokeLine (address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { value };
    action.command.mask   = { kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetSetRegister
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetSetRegister (const std::string & name, Word value, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetRegister, std::format ("R {} {:02X}", name, value), mode);



    action.command.text  = name;
    action.command.a1    = value;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetSetRegister
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetSetRegister (const std::string & name, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetRegister, std::format ("R {} {}", name, value), mode);



    action.command.text  = name;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetPatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetPatch (Word address, std::span<const Byte> bytes, CommandMode mode, std::span<const Byte> required)
{
    std::string     line = std::format ("PATCH {:04X}", address);
    DebuggerAction  action;



    for (Byte value : bytes)
    {
        line += std::format (" {:02X}", value);
    }

    action               = DebuggerActions::Make (DebugVerb::PatchBytes, line, mode);
    action.command.a1    = address;
    action.command.hasA1 = true;
    action.command.values.assign (bytes.begin(), bytes.end());
    action.required.assign (required.begin(), required.end());
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetLoadBinary
//
//  As the parser reads `BLOAD "path",FORMAT addr,len`: the name without its
//  quotes, then the format after a comma, and the last address, not the
//  length.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetLoadBinary (const std::string & path, const std::string & format, std::optional<Word> address,
                                             std::optional<uint32_t> length, CommandMode mode)
{
    std::string     suffix = format.empty() ? std::string() : "," + format;
    std::string     range;
    DebuggerAction  action;



    if (address.has_value())
    {
        range = std::format (" {:04X}", *address) + (length.has_value() ? std::format (",{:X}", *length) : std::string());
    }

    action = DebuggerActions::Make (DebugVerb::LoadBinary, DebuggerCommandLines::GetLineWithFileName ("BLOAD ", path) + suffix + range, mode);

    action.command.text  = path + suffix;
    action.command.a1    = address.value_or (0);
    action.command.hasA1 = address.has_value();

    if (address.has_value() && length.has_value())
    {
        action.command.a2    = (Word) (*address + *length - 1);
        action.command.hasA2 = true;
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetSaveBinary
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetSaveBinary (const std::string & path, Word address, uint32_t length, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SaveBinary, DebuggerCommandLines::GetLineWithFileName ("BSAVE ", path) + std::format (" {:04X},{:X}", address, length), mode);



    action.command.text  = path;
    action.command.a1    = address;
    action.command.hasA1 = true;
    action.command.a2    = (Word) (address + length - 1);
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetPushByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetPushByte (Byte value, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::PushStack, std::format ("PUSH {:02X}", value), mode);



    action.command.values = { value };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetPopByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetPopByte (CommandMode mode)
{
    return DebuggerActions::Make (DebugVerb::PopStack, "POP", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetPopWord
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetPopWord (CommandMode mode)
{
    return DebuggerActions::Make (DebugVerb::PopStackWord, "PPOP", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetSearchMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetSearchMemory (const std::string & items, CommandMode mode)
{
    static constexpr Word  kLastAddress = 0xFFFF;
    DebuggerAction         action       = DebuggerActions::Make (DebugVerb::SearchMemory, "S 0000:FFFF " + items, mode);



    //  The whole address space; the items are evaluated when the action runs.
    action.command.a1    = 0;
    action.command.a2    = kLastAddress;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    action.operand       = items;
    action.isResult      = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetFillMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetFillMemory (Word first, Word last, std::span<const Byte> bytes, CommandMode mode)
{
    std::string     line = std::format ("F {:04X}:{:04X}", first, last);
    DebuggerAction  action;



    for (Byte each : bytes)
    {
        line += std::format (" {:02X}", each);
    }

    action               = DebuggerActions::Make (DebugVerb::FillMemory, line, mode);
    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    action.command.values.assign (bytes.begin(), bytes.end());
    action.command.mask.assign (bytes.size(), 0xFF);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetMoveMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetMoveMemory (Word destination, Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::MoveMemory, std::format ("M {:04X} {:04X}:{:04X}", destination, first, last), mode);



    SetRangeFrom (action.command, destination, first, last);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetCompareMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetCompareMemory (Word other, Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::CompareMemory, std::format ("MC {:04X} {:04X}:{:04X}", other, first, last), mode);



    SetRangeFrom (action.command, other, first, last);
    action.isResult = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetEnterWord / GetEnterByte
//
//  A value given as a number is written whole, as a typed deposit's values
//  are; one given as text is evaluated when the action runs.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetEnterWord (Word address, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::EnterWords, std::format ("MEW {:04X} {}", address, value), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetEnterWord
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetEnterWord (Word address, Word value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = DebuggerActions::Make (DebugVerb::EnterWords, std::format ("MEW {:04X} {:04X}", address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { (Byte) value, (Byte) (value >> 8) };
    action.command.mask   = { kFullMask, kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetEnterByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetEnterByte (Word address, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::EnterBytes, std::format ("MEB {:04X} {}", address, value), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::GetEnterByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction MemoryActions::GetEnterByte (Word address, Byte value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = DebuggerActions::Make (DebugVerb::EnterBytes, std::format ("MEB {:04X} {:02X}", address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { value };
    action.command.mask   = { kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions::SetRangeFrom
//
//  M's and MC's fields: the other address in a3 and the range in a1 and a2.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryActions::SetRangeFrom (DebugCommand & command, Word other, Word first, Word last)
{
    command.a1    = first;
    command.a2    = last;
    command.a3    = other;
    command.hasA1 = true;
    command.hasA2 = true;
    command.hasA3 = true;
}
