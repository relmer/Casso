#pragma once

#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugCommand.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParseResult
//
//  label is the first line of the error when it is not the one the status
//  implies: an excluded WinDbg command says it has no meaning on this
//  machine rather than that it is unavailable.
//
////////////////////////////////////////////////////////////////////////////////

struct WinDbgParseResult
{
    ParseStatus    status = ParseStatus::Empty;
    DebugCommand   command;
    std::string    error;
    std::string    label;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgCommand / WinDbgExclusion
//
//  A WinDbg-mode command and the AppleWin-mode command with the same engine
//  effect; an excluded WinDbg command and the family it belongs to.
//
////////////////////////////////////////////////////////////////////////////////

struct WinDbgCommand
{
    const char  * name;
    const char  * appleWinName;
};

struct WinDbgExclusion
{
    const char  * name;
    const char  * family;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgParser
//
//  One WinDbg-mode line to one DebugCommand.
//
//  Each command is rewritten as the AppleWin-mode line with the same effect
//  and handed to AppleWinParser, so a WinDbg command and its AppleWin
//  equivalent cannot parse to different engine operations.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgParser
{
public:
    static WinDbgParseResult  Parse (const std::string & line, const IDebugExpressionContext & context);

    static std::span<const WinDbgCommand>    GetCommands   ();
    static std::span<const WinDbgExclusion>  GetExclusions ();

    //  Whether `!name` reaches this AppleWin-mode command: the engine
    //  commands, which have no WinDbg original.
    static bool  IsEngineCommand (const AppleWinCommand & entry);

private:
    using Tokens = std::vector<std::string>;

    //  What one command's arguments were rewritten to, before AppleWinParser.
    struct Rewrite
    {
        std::string  appleWinLine;
        std::string  error;
        bool         isDeferred = false;
    };

    static Tokens       Split             (const std::string & text);
    static std::string  ToLower           (const std::string & text);
    static std::string  Join              (const Tokens & tokens, size_t first);
    static std::string  NormalizeNumbers  (const std::string & text);
    static std::string  StripBackquotes   (const std::string & text);

    static bool  TryFindExclusion      (const std::string & name, const WinDbgExclusion *& exclusion);
    static bool  TryParseEngine        (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result);
    static bool  TryRewrite            (const std::string & name, const Tokens & args, const std::string & rest, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteDump        (const std::string & name, const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite);    static bool  TryRewriteAccess      (const Tokens & args, Rewrite & rewrite);
    static bool  TryRewriteBreakpoint  (const Tokens & args, const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteRegister    (const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteText        (const Tokens & args, const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteRange       (const std::string & name, const Tokens & args, Rewrite & rewrite);
    static bool  TrySplitLength        (const Tokens & args, size_t first, std::string & length, size_t & next);
    static bool  TryEvaluate           (const std::string & text, const IDebugExpressionContext & context, uint32_t & value, std::string & error);
};
