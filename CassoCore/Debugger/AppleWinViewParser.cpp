#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"
#include "Debugger/AppleWinViewParser.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser::TryParseOutputArguments
//
//  CALC evaluates one expression; the rest take the line as text.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinViewParser::TryParseOutputArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    if (command.verb == DebugVerb::Calculate)
    {
        if (args.tokens.empty())
        {
            error = "CALC needs an expression.";
            return false;
        }

        command.hasA1 = true;
        return AppleWinArgumentForms::TryEvaluate (args.rest, *args.context, command.a1, error);
    }

    command.text = args.rest;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser::TryParseListArguments
//
//  Watches, zero-page pointers and bookmarks: an address to add, an id or *
//  to clear, enable, disable or go to. ZP0-ZP7 and P0-P4 carry a slot.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinViewParser::TryParseListArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  name  = AppleWinText::ToUpper (command.sourceName);
    char         digit = name.back();



    switch (command.verb)
    {
    case DebugVerb::AddWatch:
    case DebugVerb::AddZeroPagePointer:
    case DebugVerb::AddBookmark:
        if (isdigit ((unsigned char) digit) && (name.starts_with ("ZP") || name.starts_with ("P")))
        {
            command.count = (uint32_t) (digit - '0');
        }

        command.hasA1 = !args.tokens.empty();

        //  With EXPR, the text after it is the watch, resolved once to what
        //  it is: a register, a symbol or an expression, read at each stop.
        if (command.verb == DebugVerb::AddWatch && args.tokens.size() > 1 && AppleWinText::ToUpper (args.tokens[0]) == "EXPR")
        {
            command.text         = args.rest.substr (args.rest.find_first_not_of (" \t", args.tokens[0].size()));
            command.isTypedWatch = true;
            return true;
        }

        //  A watch's text also goes to the session, which reads it as a BASIC
        //  variable where it is one; text that is not an address may be one.
        if (command.verb == DebugVerb::AddWatch && !args.tokens.empty())
        {
            command.text = args.rest;
            return AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error) || IsBasicReferenceForm (args.rest, error);
        }

        return args.tokens.empty() || AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error);

    case DebugVerb::ClearWatch:
    case DebugVerb::DisableWatch:
    case DebugVerb::EnableWatch:
    case DebugVerb::ClearZeroPagePointer:
    case DebugVerb::DisableZeroPagePointer:
    case DebugVerb::EnableZeroPagePointer:
    case DebugVerb::ClearBookmark:
        return AppleWinArgumentForms::TryParseIdOrAll (args.tokens, command, error, args.context->GetNumberSyntax());

    case DebugVerb::GoToBookmark:
        if (args.tokens.empty())
        {
            error = "Give a bookmark id.";
            return false;
        }

        if (args.tokens[0] == "*")
        {
            error = std::format ("{} takes one id, not *.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return AppleWinArgumentForms::TryParseIdOrAll (args.tokens, command, error, args.context->GetNumberSyntax());

    default:
        command.text = args.rest;
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser::TryParseSymbolArguments
//
//  `SYM` alone reports counts. `SYM<table>` takes `CLEAR`, `LOAD "file"
//  [,offset]`, `SAVE "file"`, `ON`, `OFF`, `name = addr`, `! name` or
//  `~ name`, or a name or address to look up. The table, the word after
//  SYM, goes in symbolTable.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinViewParser::TryParseSymbolArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    static constexpr const char * kPrefix = "SYM";
    size_t                        equals  = args.rest.find ('=');
    std::string                   first   = args.tokens.empty() ? std::string() : AppleWinText::ToUpper (args.tokens[0]);
    std::string                   name    = AppleWinText::ToUpper (command.sourceName);



    if (command.verb == DebugVerb::LookupSymbol && name.starts_with (kPrefix))
    {
        command.symbolTable = name.substr (strlen (kPrefix));
    }

    if (command.verb != DebugVerb::LookupSymbol)
    {
        command.text = args.rest;
        return true;
    }

    if (args.tokens.empty())
    {
        command.verb = DebugVerb::ShowSymbolInfo;
        return true;
    }

    // An '=' is an assignment, even to a keyword, unless it falls in the file
    // name after LOAD or SAVE.
    if (equals != std::string::npos && !((first == "LOAD" || first == "SAVE") && AppleWinText::Split (args.rest.substr (0, equals)).size() > 1))
    {
        command.verb  = DebugVerb::AddSymbol;
        command.text  = AppleWinText::Split (args.rest.substr (0, equals)).empty() ? std::string() : AppleWinText::Split (args.rest.substr (0, equals))[0];
        command.hasA1 = true;

        if (command.text.empty())
        {
            error = "SYM name = addr needs a name.";
            return false;
        }

        return AppleWinArgumentForms::TryEvaluate (args.rest.substr (equals + 1), *args.context, command.a1, error);
    }

    if (first == "CLEAR")
    {
        command.verb = DebugVerb::ClearSymbols;
        return true;
    }

    if (first == "ON" || first == "OFF")
    {
        command.verb  = DebugVerb::EnableSymbols;
        command.count = (first == "ON") ? 1 : 0;
        return true;
    }

    if (first == "LOAD" || first == "SAVE")
    {
        command.verb = (first == "LOAD") ? DebugVerb::LoadSymbols : DebugVerb::SaveSymbols;
        command.text = AppleWinArgumentForms::GetTextAfterFirstWord (args);

        if (command.text.empty())
        {
            error = std::format ("{} needs a file name.", first);
            return false;
        }

        return true;
    }

    //  SYM name ! removes too, the way the command reads aloud.
    if (args.tokens.size() == 2 && (args.tokens[1] == "!" || args.tokens[1] == "~"))
    {
        command.verb = DebugVerb::RemoveSymbol;
        command.text = args.tokens[0];
        return true;
    }

    if (first == "!" || first == "~")
    {
        command.verb = DebugVerb::RemoveSymbol;
        command.text = args.tokens.size() > 1 ? args.tokens[1] : std::string();

        if (command.text.empty())
        {
            error = std::format ("SYM {} needs the name of the symbol to remove.", first);
            return false;
        }

        return true;
    }

    command.text = args.rest;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser::IsBasicReferenceForm
//
//  BASIC and a name, or a name written as an Applesoft variable or array
//  element: letters and digits after a letter, a $ or %, and subscripts.
//  The error is cleared for one.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinViewParser::IsBasicReferenceForm (const std::string & text, std::string & error)
{
    static const std::regex  s_kForm (R"(^\s*(BASIC\s+)?[A-Za-z][A-Za-z0-9]*[$%]?(\(\s*\d+(\s*,\s*\d+)*\s*\))?\s*$)", std::regex::icase);



    if (!std::regex_match (text, s_kForm))
    {
        return false;
    }

    error.clear();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser::TryParsePanelArguments
//
//  PANEL and PANEL LIST list the device panels; PANEL name, PANEL OPEN name
//  or PANEL name OPEN opens one, and PANEL CLOSE name or PANEL name CLOSE
//  closes it. The name is the rest of the line, so a title that holds a
//  space works; which panels exist is the window's to know.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinViewParser::TryParsePanelArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  first = args.tokens.empty() ? std::string() : AppleWinText::ToUpper (args.tokens[0]);
    std::string  last  = args.tokens.empty() ? std::string() : AppleWinText::ToUpper (args.tokens.back());
    bool         close = first == "CLOSE";
    bool         open  = first == "OPEN";
    Tokens       name  = args.tokens;



    if (args.tokens.empty() || (first == "LIST" && args.tokens.size() == 1))
    {
        command.verb = DebugVerb::ListPanels;
        return true;
    }

    if ((close || open) && args.tokens.size() < 2)
    {
        error = "Use PANEL LIST to list the device panels, PANEL name or PANEL OPEN name to open one, or PANEL CLOSE name to close one.";
        return false;
    }

    //  The verb may come first or, as in PANEL name CLOSE, last.
    if (close || open)
    {
        name.erase (name.begin());
    }
    else if (args.tokens.size() >= 2 && (last == "CLOSE" || last == "OPEN"))
    {
        close = last == "CLOSE";
        name.pop_back();
    }

    //  A panel's title can hold a space ("Disk II"), so every word left is
    //  the name.
    command.verb = close ? DebugVerb::ClosePanel : DebugVerb::OpenPanel;
    command.text = AppleWinText::Join (name, 0);
    return true;
}
