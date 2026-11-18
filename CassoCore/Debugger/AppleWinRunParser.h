#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArguments.h"
#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunParser
//
//  The arguments of the commands that run and change the CPU: go, trace
//  and step, registers, flags, the engine commands, the step filter, the
//  call stack and the history.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinRunParser
{
public:
    using Tokens = AppleWinArguments::Tokens;

    static bool  TryParseRunArguments      (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseFlagArguments     (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseEngineArguments   (const AppleWinArguments & args, DebugCommand & command, std::string & error);

private:
    static bool  TryParseRegisterArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseSkipArguments     (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseSkipRange         (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool  TryParseCallsArguments    (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseHistoryArguments  (const AppleWinArguments & args, DebugCommand & command, std::string & error);
};
