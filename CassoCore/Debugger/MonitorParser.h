#pragma once

//  For ParseStatus, which both modes report through. The dependency is real
//  rather than incidental: a Monitor line opening with `/` is an AppleWin
//  line, and this parser is what says so.
#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/MonitorState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParseResult
//
//  One line's worth. The Monitor takes several commands on a line
//  (`300.30F 400.40F`), so this is a list rather than a command.
//
//  appleWinLine is what followed a leading `/`, and is the one case where
//  the Monitor parser produces no commands and no error: the line was never
//  the Monitor's to read.
//
////////////////////////////////////////////////////////////////////////////////

struct MonitorParseResult
{
    ParseStatus                status = ParseStatus::Empty;
    std::vector<DebugCommand>  commands;
    std::string                error;
    std::string                appleWinLine;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser
//
//  One Apple II Monitor line to the commands it means.
//
//  The Monitor is a line scanner rather than a command parser, and the
//  difference shows: hex digits accumulate, a delimiter shifts them along,
//  and a command character acts on whatever has accumulated. The same letter
//  means different things by position -- `S` alone steps, and `41<300.3FFS`
//  searches -- so nothing here dispatches on a name.
//
//  NO COMMAND LETTER IS A HEX DIGIT, which is what makes the scan
//  unambiguous: G, I, L, M, N, R, S, T, V and W all sit outside A-F, and the
//  control commands arrive as control codes rather than as their letters.
//
//  The state carries across lines, because the Monitor does: a bare Return
//  continues examining, and `:` after `^E` sets registers rather than memory.
//
//  Numbers are bare hex, with no expressions and no symbols. That is the
//  Monitor's own syntax, and it is why this parser needs no expression
//  context where the AppleWin one does.
//
//  The command characters are declared as data because a test decodes the
//  command table of every Apple II ROM Casso ships and asserts that every
//  entry is one of them (FR-027). The union works on every machine (FR-016):
//  no single ROM carries all of them.
//
////////////////////////////////////////////////////////////////////////////////

class MonitorParser
{
public:
    static MonitorParseResult  Parse (const std::string & line, MonitorState & state);

    // Every character the Monitor treats as a command, as a 7-bit code; a
    // control character is its $00-$1F code, and the rest are uppercase.
    static std::span<const Byte>  GetCommandCharacters ();
    static bool                   IsCommandCharacter   (Byte character);

private:
    //  What one line's scan has accumulated and not yet spent.
    struct Scan
    {
        std::optional<Word>  value;      // the digits just read
        std::optional<Word>  first;      // the left side of a `.`
        std::optional<Word>  dest;       // the left side of a `<`
        char                 op = 0;     // a pending `+` or `-`
    };

    static bool         TryReadCharacter (const std::string & line, size_t & index, char & character, std::string & error);
    static bool         TryHexDigit      (char character, int & digit);
    static bool         TryParseBytes    (const std::string & text, std::vector<Byte> & values);
    static void         ApplyRange       (const Scan & scan, DebugCommand & command);
    static void         FlushExamine     (Scan & scan, MonitorState & state, MonitorParseResult & result);
    static DebugCommand MakeCommand      (DebugVerb verb, char source);
    static std::string  Trim             (const std::string & text);
    static std::string  Unquote          (const std::string & text);
};
