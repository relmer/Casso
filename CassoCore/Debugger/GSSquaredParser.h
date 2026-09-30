#pragma once

//  For ParseStatus, which every mode reports through, for the engine commands,
//  which a GSSquared line reaches by their bare AppleWin names, and for
//  TryEvaluate, which reads a register value and a file:line target.
#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugCommand.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParseResult
//
//  A list rather than one command, because `watch 6.7` watches each address
//  of the range and Casso's watches are one address each.
//
//  isIdOrAddress marks a `nobp` whose number is an id if an entry has that
//  id and an address otherwise, which only the session's tables can decide:
//  the command carries the number as a decimal id in count and as a hex
//  address in a1.
//
////////////////////////////////////////////////////////////////////////////////

struct GSSquaredParseResult
{
    ParseStatus                status        = ParseStatus::Empty;
    std::vector<DebugCommand>  commands;
    std::string                error;
    bool                       isIdOrAddress = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredCommand
//
//  One command word in GSSquared mode. reason is null for a word Casso
//  carries out, and the second line of the not-available error otherwise.
//
////////////////////////////////////////////////////////////////////////////////

struct GSSquaredCommand
{
    const char  * name;
    const char  * reason;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredParser
//
//  One GSSquared debugger line to the commands it means.
//
//  GSSquared classifies each space-separated token by its form -- hex is a
//  number, `addr:` deposits, `a.b` is a range, `bank/addr` is a bank-qualified
//  address, a quoted token is a string -- and looks anything else up in a
//  flat table of words. The first token decides the command.
//
//  Each command is built here as the DebugCommand with the same effect as
//  its AppleWin counterpart, the same verb and the same fields, so a
//  breakpoint set here is the breakpoint `BPL` lists. No line is rewritten
//  into another mode's text. GSSquared's numbers are always hex and never
//  symbols, so they are read here rather than evaluated.
//
//  Casso's engine commands are bare names here, as in AppleWin mode: `/` is
//  GSSquared's bank separator and cannot be the marker. Only bank 00 exists
//  on an Apple II, so `00/300` is $0300 and any other bank is refused.
//
////////////////////////////////////////////////////////////////////////////////

class GSSquaredParser
{
public:
    static GSSquaredParseResult  Parse (const std::string & line, const IDebugExpressionContext & context);

    //  Every GSSquared command word, in GSSquared's own table order, plus
    //  Casso's additions for scripts: `s`, `o`, `r` and `g`.
    static std::span<const GSSquaredCommand>  GetCommands ();

private:
    using Tokens = std::vector<std::string>;

    //  One line in progress: the tokens, and what the line becomes.
    struct Line
    {
        Tokens                           tokens;
        GSSquaredParseResult           & result;
        const IDebugExpressionContext  & context;
    };

    static bool         TryParseForm       (Line & line);
    static bool         TryParseWord       (Line & line);
    static void         ParseDeposit       (Line & line, const std::string & address, const Tokens & values);
    static void         ParseBreakpoint    (Line & line);
    static void         ParseDataBreakpoint (Line & line, bool isIo);
    static void         ParseClearBreakpoint (Line & line);
    static void         ParseWatch         (Line & line);
    static void         ParseFile          (Line & line, bool isLoad);
    static void         ParseSymbols       (Line & line, const std::string & word);
    static void         ParseRegister      (Line & line);
    static void         ParsePanel         (Line & line, bool isClose);
    static void         ParseNoArguments   (Line & line, DebugVerb verb);
    static void         ParseHelp          (Line & line);
    static void         ParseMove          (Line & line, const std::string & word, Word first, Word last, Word dest);
    static void         ParseCassoCommand  (Line & line, const std::string & text);
    static void         AddSymbolCommand   (Line & line, DebugVerb verb, const std::string & text);
    static DebugCommand MakeCommand        (DebugVerb verb, const std::string & word);
    static void         AddCommand         (Line & line, const DebugCommand & command);
    static void         AddRange           (Line & line, DebugCommand command, Word first, Word last, bool hasLast);
    static bool         TryParseColonTarget (Line & line, const std::string & token, DebugCommand & command);
    static bool         TryParseIdOrAll    (Line & line, const std::string & token, DebugCommand & command);
    static bool         TryParseId         (const std::string & text, uint32_t & value);
    static bool         TryParseIfExpression (Line & line, bool hasIf, const std::string & expression, DebugCommand & command);
    static bool         TryParseAddress    (Line & line, const std::string & token, Word & address);
    static bool         TryParseRange      (Line & line, const std::string & token, Word & first, Word & last);
    static bool         TryParseHex        (const std::string & text, size_t maxDigits, Word & value);
    static bool         TryGetIfClause     (const Tokens & tokens, size_t first, std::string & expression);
    static void         SetInvalid         (Line & line, const std::string & error);
    static void         SetNotAvailable    (Line & line, const std::string & error);
    static std::string  FormatHex          (Word value);
    static std::string  ToUpper            (const std::string & text);
    static std::string  ToLower            (const std::string & text);
    static std::string  Unquote            (const std::string & text);
    static Tokens       Split              (const std::string & text);
};
