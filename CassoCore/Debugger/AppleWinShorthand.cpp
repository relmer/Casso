#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/AppleWinShorthand.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinShorthand::TryParse
//
//  The classic Monitor forms AppleWin also accepts: `addr:bytes` deposits,
//  `addrG` sets PC and goes, `addrL` disassembles, and `dest<start.endM`
//  moves.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinShorthand::TryParse (const std::string & first, const AppleWinArguments & args, AppleWinParseResult & result)
{
    size_t        colon  = first.find (':');
    Tokens        values;
    std::string   upper  = AppleWinText::ToUpper (first);
    char          last   = upper.empty() ? '\0' : upper.back();
    std::string   head   = upper.size() > 1 ? upper.substr (0, upper.size() - 1) : std::string();



    if (colon != std::string::npos && colon > 0)
    {
        values = args.tokens;

        if (colon + 1 < first.size())
        {
            values.insert (values.begin(), first.substr (colon + 1));
        }

        result.command.verb = DebugVerb::EnterBytes;

        if (values.empty())
        {
            result.status = ParseStatus::Invalid;
            result.error  = "A deposit needs one or more values after the colon.";
            return true;
        }

        result.status       = AppleWinArgumentForms::TryEvaluate (first.substr (0, colon), *args.context, result.command.a1, result.error) &&
                              AppleWinArgumentForms::TryParseValues (values, 0, AppleWinArgumentForms::ValueWidth::BytesOrWords, *args.context, result.command, result.error)
                            ? ParseStatus::Ok : ParseStatus::Invalid;
        result.command.hasA1 = true;
        return true;
    }

    if (last == 'M' && TryParseMove (upper, args, result))
    {
        return true;
    }

    if (last == 'G' && AppleWinArgumentForms::TryEvaluate (head, *args.context, result.command.a3, result.error))
    {
        result.command.verb  = DebugVerb::Go;
        result.command.hasA3 = true;
        result.status        = ParseStatus::Ok;
        return true;
    }

    if (last == 'L' && AppleWinArgumentForms::TryEvaluate (head, *args.context, result.command.a1, result.error))
    {
        result.command.verb  = DebugVerb::Disassemble;
        result.command.hasA1 = true;
        result.status        = ParseStatus::Ok;
        return true;
    }

    result.error.clear();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinShorthand::TryParseMove
//
//  `dest<start.endM`, the Monitor's move, into the same command M produces:
//  the source range in a1/a2 and the destination in a3.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinShorthand::TryParseMove (const std::string & upper, const AppleWinArguments & args, AppleWinParseResult & result)
{
    size_t  less   = upper.find ('<');
    size_t  period = upper.find ('.', less == std::string::npos ? 0 : less);
    bool    isMove = less != std::string::npos && period != std::string::npos && less > 0 && period + 1 < upper.size() - 1;



    if (!isMove)
    {
        return false;
    }

    result.command.verb  = DebugVerb::MoveMemory;
    result.command.hasA1 = true;
    result.command.hasA2 = true;
    result.command.hasA3 = true;
    result.status        = AppleWinArgumentForms::TryEvaluate (upper.substr (0, less), *args.context, result.command.a3, result.error) &&
                           AppleWinArgumentForms::TryEvaluate (upper.substr (less + 1, period - less - 1), *args.context, result.command.a1, result.error) &&
                           AppleWinArgumentForms::TryEvaluate (upper.substr (period + 1, upper.size() - period - 2), *args.context, result.command.a2, result.error)
                         ? ParseStatus::Ok : ParseStatus::Invalid;

    if (result.status == ParseStatus::Ok && result.command.a2 < result.command.a1)
    {
        result.status = ParseStatus::Invalid;
        result.error  = std::format ("The range ends at ${:04X}, before its start at ${:04X}.", result.command.a2, result.command.a1);
    }

    return true;
}
