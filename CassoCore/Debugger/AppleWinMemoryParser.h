#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArguments.h"
#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinMemoryParser
//
//  The arguments of the memory and assembler commands, and of the data
//  commands that mark memory as bytes, words, addresses or text.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinMemoryParser
{
public:
    using Tokens = AppleWinArguments::Tokens;

    static bool  TryParseMemoryArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseDataArguments   (const AppleWinArguments & args, DebugCommand & command, std::string & error);

private:
    static bool  IsBlockName             (const std::string & text);
};
