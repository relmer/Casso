#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArguments.h"

struct AppleWinParseResult;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinShorthand
//
//  The classic Monitor forms AppleWin also accepts, in place of a command:
//  `addr:bytes` deposits, `addrG`, `addrL`, and the `dest<start.endM` move.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinShorthand
{
public:
    using Tokens = AppleWinArguments::Tokens;

    static bool  TryParse     (const std::string & first, const AppleWinArguments & args, AppleWinParseResult & result);

private:
    static bool  TryParseMove (const std::string & upper, const AppleWinArguments & args, AppleWinParseResult & result);
};
