#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"
#include "Debugger/AppleWinMemoryParser.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinMemoryParser::TryParseMemoryArguments
//
//  `M dest src,len`, `MC dest src,len`, `F range value` or `F start end
//  value`, `S range items`, `ME addr bytes`, `MEW addr words`, `BLOAD file
//  addr[,len]`, and a range for D, U and A.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinMemoryParser::TryParseMemoryArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    DebugCommand  source;
    size_t        count      = args.tokens.size();
    size_t        valuesFrom = 1;
    bool          isThreeArg = false;



    switch (command.verb)
    {
    case DebugVerb::MoveMemory:
    case DebugVerb::CompareMemory:
        if (count < 2)
        {
            error = std::format ("{} needs a destination and a source range.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        command.hasA3 = true;
        return AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a3, error) &&
               AppleWinArgumentForms::TryParseRange (args.tokens[1], *args.context, command, error);

    // F start end value is the three-argument form AppleWin also takes.
    case DebugVerb::FillMemory:
        isThreeArg = count >= 3 && args.tokens[0].find_first_of (",:") == std::string::npos;
        valuesFrom = isThreeArg ? 2 : 1;

        if (isThreeArg)
        {
            command.hasA1 = true;
            command.hasA2 = true;

            if (!AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error) ||
                !AppleWinArgumentForms::TryEvaluate (args.tokens[1], *args.context, command.a2, error))
            {
                return false;
            }

            // An end before the start is a value in the end's place, as in
            // `F 300 AA BB`, not a one-byte fill.
            if (command.a2 < command.a1)
            {
                error = std::format ("{}'s end address ${:04X} is before its start ${:04X}.", AppleWinText::ToUpper (command.sourceName), command.a2, command.a1);
                return false;
            }

            return AppleWinArgumentForms::TryParseValues (args.tokens, valuesFrom, AppleWinArgumentForms::ValueWidth::Bytes, *args.context, command, error);
        }

        if (count < 2)
        {
            error = std::format ("{} needs a range and one or more values.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return AppleWinArgumentForms::TryParseRange (args.tokens[0], *args.context, command, error) &&
               AppleWinArgumentForms::TryParseValues (args.tokens, valuesFrom, AppleWinArgumentForms::ValueWidth::Bytes, *args.context, command, error);

    case DebugVerb::SearchMemory:
    case DebugVerb::SearchHex:
        if (count < 2)
        {
            error = std::format ("{} needs a range and what to search for.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return AppleWinArgumentForms::TryParseRange (args.tokens[0], *args.context, command, error) &&
               AppleWinArgumentForms::TryParseSearchItems (args.rest.substr (args.rest.find (args.tokens[0]) + args.tokens[0].size()), *args.context, command, error);

    case DebugVerb::EnterBytes:
    case DebugVerb::EnterWords:
    case DebugVerb::PatchBytes:
        command.hasA1 = count > 0;

        if (count < 2)
        {
            error = std::format ("{} needs an address and one or more values.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        return AppleWinArgumentForms::TryEvaluate (args.tokens[0], *args.context, command.a1, error) &&
               AppleWinArgumentForms::TryParseValues (args.tokens, 1, command.verb == DebugVerb::EnterWords ? AppleWinArgumentForms::ValueWidth::Words : AppleWinArgumentForms::ValueWidth::BytesOrWords, *args.context, command, error);

    case DebugVerb::LoadBinary:
    case DebugVerb::SaveBinary:
        if (count == 0)
        {
            error = std::format ("{} needs a file name.", AppleWinText::ToUpper (command.sourceName));
            return false;
        }

        // A quoted file name may hold spaces; the range follows the quote.
        if (args.tokens[0][0] == '"')
        {
            size_t       open  = args.rest.find ('"');
            size_t       close = args.rest.find ('"', open + 1);
            size_t       gap   = 0;
            std::string  tail;
            Tokens       after;



            if (close == std::string::npos)
            {
                error = "The file name needs a closing quote.";
                return false;
            }

            command.text = args.rest.substr (open + 1, close - open - 1);
            tail         = args.rest.substr (close + 1);

            // A format word may follow the closing quote: "my file.bin",RAW.
            if (!tail.empty() && tail[0] == ',')
            {
                gap           = tail.find_first_of (" \t");
                command.text += tail.substr (0, gap);
                tail          = (gap == std::string::npos) ? std::string() : tail.substr (gap);
            }

            after = AppleWinText::Split (tail);
            return after.empty() || AppleWinArgumentForms::TryParseRange (after[0], *args.context, command, error);
        }

        command.text = args.tokens[0];
        return count < 2 || AppleWinArgumentForms::TryParseRange (args.tokens[1], *args.context, command, error);

    case DebugVerb::SaveText:
        command.text = args.rest;
        return true;

    default:
        return count == 0 || AppleWinArgumentForms::TryParseRange (args.tokens[0], *args.context, command, error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinMemoryParser::TryParseDataArguments
//
//  The data directives take `[name] [addr | range]` or `name = addr`, with
//  or without spaces around the `=`. With two arguments the first is the
//  block's name, even one that would also evaluate, such as a register or a
//  hex word. A lone argument that evaluates is the address; otherwise it is
//  the name. B takes nothing; X takes a range. A name argument that could
//  not be a name is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinMemoryParser::TryParseDataArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error)
{
    std::string  spaced;
    Tokens       tokens;
    size_t       count = 0;



    if (command.verb == DebugVerb::ListData || args.tokens.empty())
    {
        return true;
    }

    if (command.verb == DebugVerb::RemoveData)
    {
        return AppleWinArgumentForms::TryParseRange (args.tokens[0], *args.context, command, error);
    }

    for (char ch : args.rest)
    {
        spaced += (ch == '=') ? std::string (" = ") : std::string (1, ch);
    }

    tokens = AppleWinText::Split (spaced);
    count  = tokens.size();

    if (count == 3 && tokens[1] == "=")
    {
        tokens.erase (tokens.begin() + 1);
        count = 2;
    }

    if (count > 2 || tokens[0] == "=" || (count == 2 && tokens[1] == "="))
    {
        error = std::format ("{} takes [name] [addr | range] or name = addr.", AppleWinText::ToUpper (command.sourceName));
        return false;
    }

    if (count == 1 && AppleWinArgumentForms::TryParseRange (tokens[0], *args.context, command, error))
    {
        return true;
    }

    if (!IsBlockName (tokens[0]))
    {
        if (count == 2)
        {
            error = std::format ("{} takes [name] [addr | range] or name = addr.", AppleWinText::ToUpper (command.sourceName));
        }

        return false;
    }

    error.clear();
    command.text  = tokens[0];
    command.hasA1 = false;
    command.hasA2 = false;

    return count == 1 || AppleWinArgumentForms::TryParseRange (tokens[1], *args.context, command, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinMemoryParser::IsBlockName
//
//  A letter or underscore, then letters, digits and underscores.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinMemoryParser::IsBlockName (const std::string & text)
{
    bool  isName = !text.empty() && (isalpha ((unsigned char) text[0]) || text[0] == '_');



    for (size_t i = 1; isName && i < text.size(); ++i)
    {
        isName = isalnum ((unsigned char) text[i]) || text[i] == '_';
    }

    return isName;
}
