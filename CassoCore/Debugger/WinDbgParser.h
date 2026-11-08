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
//  equivalent cannot parse to different engine operations. What an AppleWin
//  line cannot carry -- a step's start address, a backquoted file name, a
//  pattern x looks up whatever its first word -- is set on the parsed
//  command afterward.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgParser
{
public:
    static WinDbgParseResult  Parse (const std::string & line, const IDebugExpressionContext & context);

    static std::span<const WinDbgCommand>    GetCommands   ();
    static std::span<const WinDbgExclusion>  GetExclusions ();

private:
    using Tokens = std::vector<std::string>;

    //  What one command's arguments were rewritten to, before AppleWinParser,
    //  and what is set on the parsed command after it.
    struct Rewrite
    {
        std::string                 appleWinLine;
        std::string                 error;
        bool                        isDeferred = false;
        std::optional<Word>         startPc;
        std::optional<std::string>  sourceFile;
        std::optional<std::string>  lookup;
    };

    static Tokens       Split                (const std::string & text);
    static std::string  ToLower              (const std::string & text);
    static std::string  Trim                 (const std::string & text);
    static std::string  Join                 (const Tokens & tokens, size_t first);
    static std::string  GetTail              (const std::string & text, size_t index);
    static std::string  NormalizeNumbers     (const std::string & text);
    static std::string  NormalizeExpression  (const std::string & text);
    static std::string  NormalizeEngineLine  (const std::string & text);
    static std::string  NormalizeSearchItems (const std::string & text);
    static std::string  GetDecimalBudget     (const std::string & token);

    static bool  IsComment             (const std::string & line);
    static bool  HasSeparator          (const std::string & line);
    static bool  IsOperatorEdge        (char ch);
    static bool  TryFindExclusion      (const std::string & name, const WinDbgExclusion *& exclusion);
    static void  SetExclusion          (const std::string & name, const WinDbgExclusion & exclusion, WinDbgParseResult & result);
    static bool  TryParseEngine        (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result);
    static void  ApplyRewrite          (const std::string & name, const Rewrite & rewrite, DebugCommand & command);
    static bool  TryRewrite            (const std::string & name, const std::string & rest, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteRun         (const std::string & name, const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteStart       (const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite, size_t & next);
    static bool  TryRewriteBreakpoints (const std::string & name, const std::string & rest, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteBreakpoint  (const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteSourceLine  (const std::string & inner, const std::string & after, Rewrite & rewrite);
    static bool  TryRewriteAccess      (const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteMemory      (const std::string & name, const std::string & rest, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteDump        (const std::string & name, const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteEnter       (const std::string & name, const Tokens & args, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteText        (const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteRange       (const std::string & name, const std::string & normalized, const IDebugExpressionContext & context, Rewrite & rewrite);
    static bool  TryRewriteOther       (const std::string & name, const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteRegister    (const std::string & rest, Rewrite & rewrite);
    static bool  TryRewriteDisassemble (const Tokens & args, Rewrite & rewrite);
    static bool  TryRewriteLookup      (const std::string & rest, Rewrite & rewrite);
    static bool  TrySplitLength        (const Tokens & args, size_t first, std::string & length, size_t & next);
    static bool  TryEvaluate           (const std::string & text, const IDebugExpressionContext & context, uint32_t & value, std::string & error);
    static bool  TryEvaluateAddress    (const std::string & text, const IDebugExpressionContext & context, uint32_t & value, std::string & error);
};
