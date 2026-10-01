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
//  A WinDbg-mode command and the AppleWin-mode command whose DebugCommand it
//  builds; an excluded WinDbg command and the family it belongs to.
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
//  Each command's DebugCommand is built here, with the same verb and fields
//  AppleWin mode gives its equivalent, so the two cannot reach different
//  engine operations; the sweep test holds them to that. Replies and errors
//  quote the command as typed in WinDbg mode. A `!` engine command is parsed
//  by AppleWinParser, with its numbers' WinDbg prefixes read as AppleWin's.
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

    //  One command as it is built, or why it could not be.
    struct Build
    {
        DebugCommand  command;
        std::string   error;
        bool          isDeferred = false;
    };

    static Tokens       Split                    (const std::string & text);
    static std::string  ToLower                  (const std::string & text);
    static std::string  ToUpper                  (const std::string & text);
    static std::string  GetTail                  (const std::string & text, size_t first);
    static std::string  StripBackquotes          (const std::string & text);
    static bool         HasIf                    (const Tokens & tokens);

    static bool  TryFindExclusion           (const std::string & name, const WinDbgExclusion *& exclusion);
    static bool  TryParseEngine             (const std::string & line, const IDebugExpressionContext & context, WinDbgParseResult & result);
    static bool  TryBuild                   (const std::string & name, const Tokens & args, const std::string & rest, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildStep               (const Tokens & args, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildIdOrAll            (const std::string & name, const Tokens & args, Build & build);
    static bool  TryBuildBytes              (const Tokens & args, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildEnter              (const std::string & name, const Tokens & args, bool isWords, const IDebugExpressionContext & context, Build & build);
    static bool  TryAddValues               (const Tokens & tokens, size_t first, bool isWords, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool  TryBuildDump               (const std::string & name, const Tokens & args, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildAccess             (const Tokens & args, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildBreakpoint         (const Tokens & args, const std::string & rest, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildAddressBreakpoint  (const std::string & name, Tokens tokens, const std::string & size, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildWatchpoint         (Tokens tokens, const std::string & size, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildAccessRange        (const std::string & text, const std::string & size, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool  TrySetLength               (Word address, uint32_t length, DebugCommand & command, std::string & error);
    static bool  TryBuildRegister           (const std::string & rest, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildText               (const Tokens & args, const std::string & rest, const IDebugExpressionContext & context, Build & build);
    static bool  TryBuildRange              (const std::string & name, const Tokens & args, const std::string & rest, const IDebugExpressionContext & context, Build & build);
    static bool  TrySplitLength             (const Tokens & args, size_t first, std::string & length, size_t & next);
    static bool  TryEvaluate                (const std::string & text, const IDebugExpressionContext & context, uint32_t & value, std::string & error);
};
