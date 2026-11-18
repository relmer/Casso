#include "Pch.h"

#include "Ui/Debugger/WatchActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetClearWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetClearWatch (int id, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearWatch, std::format ("WC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetEnableWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetEnableWatch (int id, bool enable, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (enable ? DebugVerb::EnableWatch : DebugVerb::DisableWatch, std::format ("{} {}", enable ? "WE" : "WD", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddZeroPagePointer (Byte address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddZeroPagePointer, std::format ("ZP {:02X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddZeroPagePointerAt
//
//  A pointer in the slot its name gives, ZP0 to ZP7, as an undone removal
//  puts it back.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddZeroPagePointerAt (int slot, Byte address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddZeroPagePointer, std::format ("ZP{} {:02X}", slot, address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetClearZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetClearZeroPagePointer (int slot, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearZeroPagePointer, std::format ("ZPC {}", slot), mode);



    action.command.count = (uint32_t) slot;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetEnableZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetEnableZeroPagePointer (int slot, bool enable, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (enable ? DebugVerb::EnableZeroPagePointer : DebugVerb::DisableZeroPagePointer,
                                                    std::format ("{} {}", enable ? "ZPE" : "ZPD", slot), mode);



    action.command.count = (uint32_t) slot;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddBookmark
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddBookmark (Word address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddBookmark, std::format ("BMA {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetClearBookmark
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetClearBookmark (int id, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearBookmark, std::format ("BMC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddWatch (Word address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddWatch, std::format ("W {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.command.text  = std::format ("{:04X}", address);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddWatch (const std::string & expression, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddWatch, "W EXPR " + expression, mode);



    action.command.hasA1        = true;
    action.command.isTypedWatch = true;
    action.operand              = expression;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions::GetAddWatchAt
//
//  The id goes in the count, one past it, so a count of zero is an add with
//  no id.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction WatchActions::GetAddWatchAt (int id, const std::string & expression, CommandMode mode)
{
    DebuggerAction  action = GetAddWatch (expression, mode);



    action.command.count = (uint32_t) id + 1;
    return action;
}
