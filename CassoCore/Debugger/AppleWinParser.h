#pragma once

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/DebugCommand.h"

class IDebugExpressionContext;





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
    using Tokens = std::vector<std::string>;

    struct Arguments
    {
        const AppleWinCommand         * entry;
        Tokens                          tokens;
        std::string                     rest;
        const IDebugExpressionContext * context;
    };

    // How a list of values is written: bytes only, words only, or bytes
    // where a value above $FF becomes two bytes, as MEB takes them.
    enum class ValueWidth
    {
        Bytes,
        Words,
        BytesOrWords,
    };

    static Tokens  Split              (const std::string & text);
    static std::string  ToUpper       (const std::string & text);
    static std::string  Join          (const Tokens & tokens, size_t first);
    static bool    TryParseShorthand  (const std::string & first, const Arguments & args, AppleWinParseResult & result);
    static bool    TryParseMoveShorthand (const std::string & upper, const Arguments & args, AppleWinParseResult & result);
    static bool    TryParseArguments  (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRunArguments      (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRegisterArguments (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseFlagArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseBreakpointArguments (const Arguments & source, DebugCommand & command, std::string & error);
    static bool    TryParseIfClause          (Tokens & tokens, DebugCommand & command, std::string & error);
    static bool    TryParseValueBreakpoint   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRegisterCondition (const Tokens & tokens, DebugCommand & command, std::string & error);
    static bool    TryParseWatchpointArguments (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseMemoryArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseDataArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseListArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseSymbolArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseOutputArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseEngineArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseSkipArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseCallsArguments    (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseHistoryArguments  (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseDecimal           (const std::string & text, uint64_t & value);
    static bool    TryParseSkipRange  (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryEvaluate        (const std::string & text, const IDebugExpressionContext & context, Word & value, std::string & error);
    static bool    TryParseRange      (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseSourceLine (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command);
    static bool    TryParseValues     (const Tokens & tokens, size_t first, ValueWidth width, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseSearchItems (const std::string & items, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseSearchWord  (const std::string & word, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseIdOrAll    (const Tokens & tokens, DebugCommand & command, std::string & error);
    static bool    TryParseCondition  (const std::string & subject, const Tokens & tokens, size_t first, DebugCommand & command, std::string & error);
};
