#pragma once

//  For ParseStatus, which every mode reports through, and for the engine
//  commands, which a GSSquared line reaches by their bare AppleWin names.
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
//  EVERY COMMAND IS AN APPLEWIN LINE UNDERNEATH. A line is rewritten into the
//  AppleWin command with the same effect and parsed by AppleWinParser, so a
//  breakpoint set here is the breakpoint `BPL` lists, and the two modes
//  cannot drift apart. Numbers are rewritten with a `$`, since GSSquared's
//  are always hex and never symbols.
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
    static void         ParseNoArguments   (Line & line, const std::string & appleWin);
    static void         ParseAppleWin      (Line & line, const std::string & appleWin, const std::string & word);
    static bool         TryParseAddress    (Line & line, const std::string & token, Word & address);
    static bool         TryParseRange      (Line & line, const std::string & token, Word & first, Word & last);
    static bool         TryParseHex        (const std::string & text, size_t maxDigits, Word & value);
    static bool         TryGetIfClause     (const Tokens & tokens, size_t first, std::string & clause);
    static void         SetInvalid         (Line & line, const std::string & error);
    static void         SetNotAvailable    (Line & line, const std::string & error);
    static std::string  FormatHex          (Word value);
    static std::string  FormatRange        (Word first, Word last);
    static std::string  ToLower            (const std::string & text);
    static std::string  Unquote            (const std::string & text);
    static Tokens       Split              (const std::string & text);
};
