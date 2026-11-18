#include "Pch.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinMemoryParser.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/AppleWinRunParser.h"
#include "Debugger/AppleWinShorthand.h"
#include "Debugger/AppleWinViewParser.h"
#include "Debugger/AppleWinText.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::Parse
//
////////////////////////////////////////////////////////////////////////////////

AppleWinParseResult AppleWinParser::Parse (const std::string & line, const IDebugExpressionContext & context)
{
    AppleWinParseResult  result;
    AppleWinArguments    args     = { nullptr, AppleWinText::Split (line), std::string(), &context };
    std::string          name;
    size_t               nameEnd  = 0;
    size_t               restFrom = 0;



    if (args.tokens.empty())
    {
        return result;
    }

    name     = args.tokens[0];
    nameEnd  = line.find (name) + name.size();
    restFrom = line.find_first_not_of (" \t", nameEnd);
    args.rest = (restFrom == std::string::npos) ? std::string() : line.substr (restFrom);
    args.tokens.erase (args.tokens.begin());

    result.command.sourceName = name;
    args.entry                = AppleWinCommandTable::Find (name);

    if (args.entry == nullptr)
    {
        if (!AppleWinShorthand::TryParse (name, args, result))
        {
            result.status = ParseStatus::Unknown;
            result.error  = std::format ("{} is not a command.", name);
        }

        return result;
    }

    if (args.entry->availability == CommandAvailability::NotAvailable)
    {
        result.status = ParseStatus::NotAvailable;
        result.error  = args.entry->reason;
        return result;
    }

    if (args.entry->availability == CommandAvailability::WindowOnly)
    {
        result.status = ParseStatus::WindowOnly;
        result.error  = std::format ("{} needs the debugger window.", AppleWinText::ToUpper (name));
        return result;
    }

    result.command.verb = args.entry->verb;
    result.status       = TryParseArguments (args, result.command, result.error) ? ParseStatus::Ok : ParseStatus::Invalid;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser::TryParseArguments
//
//  Commands with no arguments accept and ignore anything after the name, as
//  AppleWin does.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinParser::TryParseArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    switch (args.entry->family)
    {
    case AppleWinCommandFamily::Cpu:         return AppleWinRunParser::TryParseRunArguments                   (args, command, error);
    case AppleWinCommandFamily::Flags:       return AppleWinRunParser::TryParseFlagArguments                  (args, command, error);
    case AppleWinCommandFamily::Breakpoints:
    case AppleWinCommandFamily::Video:       return AppleWinBreakpointParser::TryParseBreakpointArguments     (args, command, error);
    case AppleWinCommandFamily::Memory:
    case AppleWinCommandFamily::Assembler:   return AppleWinMemoryParser::TryParseMemoryArguments             (args, command, error);
    case AppleWinCommandFamily::Data:        return AppleWinMemoryParser::TryParseDataArguments               (args, command, error);
    case AppleWinCommandFamily::Watch:
    case AppleWinCommandFamily::ZeroPage:
    case AppleWinCommandFamily::Bookmarks:   return AppleWinViewParser::TryParseListArguments                 (args, command, error);
    case AppleWinCommandFamily::Symbols:     return AppleWinViewParser::TryParseSymbolArguments               (args, command, error);
    case AppleWinCommandFamily::Output:      return AppleWinViewParser::TryParseOutputArguments               (args, command, error);
    case AppleWinCommandFamily::Engine:      return AppleWinRunParser::TryParseEngineArguments                (args, command, error);
    default:
        command.text = args.rest;
        return true;
    }
}
