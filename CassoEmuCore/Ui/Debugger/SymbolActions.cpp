#include "Pch.h"

#include "Ui/Debugger/SymbolActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetUnloadDebugFile
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetUnloadDebugFile (CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ShowSource, "SRC CLEAR", mode);



    action.command.text = "CLEAR";
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetLoadSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetLoadSymbols (const std::string & path, CommandMode mode)
{
    std::string     quoted = std::format ("\"{}\"", path);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::LoadSymbols, "SYM LOAD " + quoted, mode);



    action.command.text = quoted;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetMarkData
//
//  The directive's name is the session's, whatever the mode: the handler
//  reads the data's kind from it, and a digit after DB or DW is how many
//  to a line. With a name, the range is given that name, as "DB name
//  range" does.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetMarkData (const std::string & directive, Word first, Word last, CommandMode mode, const std::string & name)
{
    static constexpr std::pair<const char *, DebugVerb>  kVerbs[] =
    {
        { "DB", DebugVerb::DefineBytes }, { "DW", DebugVerb::DefineWords }, { "DA", DebugVerb::DefineAddress },
        { "ASC", DebugVerb::DefineText }, { "DF", DebugVerb::DefineFloat },
    };
    DebugVerb       verb   = DebugVerb::DefineBytes;
    DebuggerAction  action;



    for (const auto & [word, each] : kVerbs)
    {
        verb = (directive == word) ? each : verb;
    }

    if (directive.starts_with ("DW"))
    {
        verb = DebugVerb::DefineWords;
    }

    action = DebuggerActions::Make (verb, name.empty() ? std::format ("{} {:04X}:{:04X}", directive, first, last) : std::format ("{} {} {:04X}:{:04X}", directive, name, first, last), mode);

    action.command.text       = name;
    action.command.sourceName = directive;
    action.command.a1         = first;
    action.command.a2         = last;
    action.command.hasA1      = true;
    action.command.hasA2      = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetClearData
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetClearData (Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::RemoveData, std::format ("X {:04X}:{:04X}", first, last), mode);



    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetAddSymbol
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetAddSymbol (const std::string & name, Word address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddSymbol, std::format ("SYM {} = {:04X}", name, address), mode);



    action.command.text  = name;
    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetEnableSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetEnableSymbols (const std::string & table, bool enable, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::EnableSymbols, std::format ("SYM{} {}", table, enable ? "ON" : "OFF"), mode);



    action.command.count       = enable ? 1 : 0;
    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetClearSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetClearSymbols (const std::string & table, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearSymbols, std::format ("SYM{} CLEAR", table), mode);



    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetSaveSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetSaveSymbols (const std::string & table, const std::string & path, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SaveSymbols, std::format ("SYM{} SAVE \"{}\"", table, path), mode);



    action.command.text        = std::format ("\"{}\"", path);
    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolActions::GetRemoveSymbol
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SymbolActions::GetRemoveSymbol (const std::string & table, const std::string & name, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::RemoveSymbol, std::format ("SYM{} ! {}", table, name), mode);



    action.command.text        = name;
    action.command.symbolTable = table;
    return action;
}
