#pragma once

#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion
//
//  What a parse failure says beyond the parser's own message. A word that is
//  another mode's command is answered with that mode's title and this mode's
//  equivalent with the same arguments, checked by parsing it here; when
//  several modes use the word, the first in the order AppleWin, Casso,
//  Monitor, GSSquared, WinDbg is the one given. A word no mode has is
//  answered with the closest command of this mode by spelling, when one is
//  close. Wrong arguments are answered with the command's syntax line.
//
//  Monitor mode is left as it is: its commands are characters, not words.
//
////////////////////////////////////////////////////////////////////////////////

class CommandSuggestion
{
public:
    //  Fills in the detail and suggestion of an unknown command, or the usage
    //  of one given wrong arguments; leaves any other reply alone.
    static void         Annotate        (Reply & reply, const std::string & text, CommandMode mode, const IDebugExpressionContext & context);

private:
    static bool         TryDescribeOtherModesWord (const std::string & word, const std::string & arguments, CommandMode mode, const IDebugExpressionContext & context, Reply & reply);
    static bool         TryFindOwner    (const std::string & word, CommandMode mode, CommandMode & owner, std::string & cassoNames);
    static bool         TryFindEquivalent (const std::string & cassoNames, const std::string & arguments, CommandMode mode, const IDebugExpressionContext & context, std::string & word, bool & argumentsFit);
    static bool         TryParseAs      (const std::string & line, CommandMode mode, const IDebugExpressionContext & context, DebugVerb & verb);
    static bool         IsRunnable      (CommandMode mode, const std::string & cassoName, const std::string & typed);
    static std::string  FindClosest     (const std::string & word, CommandMode mode);
    static std::string  FindUsage       (const std::string & word, CommandMode mode);
    static size_t       GetDistance     (const std::string & a, const std::string & b);
    static std::string  MatchCase       (const std::string & name, const std::string & typed);
    static std::string  ToLower         (const std::string & text);
};
