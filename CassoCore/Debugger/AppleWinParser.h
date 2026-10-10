#pragma once

#include "Debugger/DebugCommand.h"
#include "Debugger/IDebugExpressionContext.h"

struct AppleWinCommand;
class AppleWinCommandTable;





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
    using Tokens = std::vector<std::string>;

    static AppleWinParseResult  Parse (const std::string & line, const IDebugExpressionContext & context);

    //  One expression as a word; a value outside $0000-$FFFF is an error.
    static bool  TryEvaluate (const std::string & text, const IDebugExpressionContext & context, Word & value, std::string & error);

    //  Argument forms another dialect's parser shares when it builds the same
    //  command: a range, a source line, search items, an id or *, an IF clause
    //  and a comparison.
    static bool  TryParseRange       (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool  TryParseSourceLine  (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command);
    static bool  TryParseSearchItems (const std::string & items, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool  TryParseIdOrAll     (const Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool  TryParseIfClause    (Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool  TryParseCondition   (const std::string & subject, const Tokens & tokens, size_t first, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);

private:

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
    static std::string  GetTextAfterFirstWord (const Arguments & args);
    static bool    TryParseShorthand  (const std::string & first, const Arguments & args, AppleWinParseResult & result);
    static bool    TryParseMoveShorthand (const std::string & upper, const Arguments & args, AppleWinParseResult & result);
    static bool    TryParseArguments  (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRunArguments      (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRegisterArguments (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseFlagArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseBreakpointArguments (const Arguments & source, DebugCommand & command, std::string & error);
    static bool    TryParseValueBreakpoint   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseBeamArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseFrameArguments    (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseRegisterCondition (const Tokens & tokens, DebugCommand & command, std::string & error);
    static bool    TryParseWatchpointArguments (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseMemoryArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseDataArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    IsBlockName               (const std::string & text);
    static bool    TryParseListArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseSymbolArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseOutputArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseEngineArguments   (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseSkipArguments     (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseCallsArguments    (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseHistoryArguments  (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseDecimal           (const std::string & text, uint64_t & value, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool    TryParseCount             (const std::string & text, uint32_t & value, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool    IsNumberOrRange           (const std::string & text, NumberSyntax syntax);
    static bool    TryParsePanelArguments    (const Arguments & args, DebugCommand & command, std::string & error);
    static bool    TryParseSkipRange  (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseValues     (const Tokens & tokens, size_t first, ValueWidth width, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool    TryParseSearchWord  (const std::string & word, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
};
