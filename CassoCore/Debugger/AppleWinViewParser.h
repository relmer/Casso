#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArguments.h"
#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinViewParser
//
//  The arguments of the listing and display commands: watches, zero page
//  and bookmarks, symbols, output, and panels.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinViewParser
{
public:
    using Tokens = AppleWinArguments::Tokens;

    static bool  TryParseListArguments   (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseSymbolArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParseOutputArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool  TryParsePanelArguments  (const AppleWinArguments & args, DebugCommand & command, std::string & error);

private:
    static bool  IsBasicReferenceForm    (const std::string & text, std::string & error);
};
