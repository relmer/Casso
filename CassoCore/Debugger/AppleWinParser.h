#pragma once

#include "Debugger/AppleWinArguments.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParseStatus / AppleWinParseResult
//
////////////////////////////////////////////////////////////////////////////////

enum class ParseStatus
{
    Ok,
    Empty,
    Unknown,
    NotAvailable,
    WindowOnly,
    Invalid,
};

struct AppleWinParseResult
{
    ParseStatus    status = ParseStatus::Empty;
    DebugCommand   command;
    std::string    error;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParser
//
//  One AppleWin-mode line to one DebugCommand. Addresses and values are
//  evaluated when the line is parsed, through the context; a breakpoint
//  condition is parsed and stored, to be evaluated before each instruction.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinParser
{
public:
    static AppleWinParseResult  Parse (const std::string & line, const IDebugExpressionContext & context);

private:
    static bool  TryParseArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
};
