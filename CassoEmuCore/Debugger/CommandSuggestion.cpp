#include "Pch.h"

#include "Debugger/CommandSuggestion.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/CassoCommandReference.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/WinDbgParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::Annotate
//
////////////////////////////////////////////////////////////////////////////////

void CommandSuggestion::Annotate (Reply & reply, const std::string & text, CommandMode mode, const IDebugExpressionContext & context)
{
    size_t       start     = text.find_first_not_of (" \t");
    size_t       end       = 0;
    size_t       rest      = 0;
    std::string  word;
    std::string  arguments;
    std::string  closest;



    if (mode == CommandMode::Monitor || start == std::string::npos)
    {
        return;
    }

    end  = text.find_first_of (" \t", start);
    word = text.substr (start, end == std::string::npos ? std::string::npos : end - start);
    rest = (end == std::string::npos) ? std::string::npos : text.find_first_not_of (" \t", end);

    if (rest != std::string::npos)
    {
        arguments = text.substr (rest);
        arguments.erase (arguments.find_last_not_of (" \t") + 1);
    }

    if (reply.status == CommandStatus::Error && reply.error.label == "invalid arguments")
    {
        reply.error.usage = FindUsage (word, mode);
        return;
    }

    if (reply.status != CommandStatus::Unknown || TryDescribeOtherModesWord (word, arguments, mode, context, reply))
    {
        return;
    }

    closest = FindClosest (word, mode);

    if (!closest.empty())
    {
        reply.suggestion   = arguments.empty() ? closest : closest + " " + arguments;
        reply.error.detail = std::format ("{} is not a command. The closest is {}.", word, reply.suggestion);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::TryDescribeOtherModesWord
//
//  The suggestion is offered only when it parses here as the same command;
//  an equivalent whose arguments are written differently is given by its
//  word alone.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandSuggestion::TryDescribeOtherModesWord (const std::string & word, const std::string & arguments, CommandMode mode, const IDebugExpressionContext & context, Reply & reply)
{
    CommandMode  owner        = CommandMode::AppleWin;
    std::string  cassoNames;
    std::string  equivalent;
    std::string  title;
    bool         argumentsFit = false;



    if (!TryFindOwner (word, mode, owner, cassoNames))
    {
        return false;
    }

    title = CommandModeHelp::GetTitle (owner);

    if (!TryFindEquivalent (cassoNames, arguments, mode, context, equivalent, argumentsFit))
    {
        reply.error.detail = std::format ("{} is {} {} command. {} has no equivalent.",
                                          word, title.starts_with ('A') ? "an" : "a", title, CommandModeHelp::GetTitle (mode));
        return true;
    }

    equivalent = MatchCase (equivalent, word);

    if (argumentsFit)
    {
        reply.suggestion = arguments.empty() ? equivalent : equivalent + " " + arguments;
    }

    reply.error.detail = std::format ("{} is {} {} command. {}'s is {}.",
                                      word, title.starts_with ('A') ? "an" : "a", title, CommandModeHelp::GetTitle (mode),
                                      argumentsFit ? reply.suggestion : equivalent);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::TryFindOwner
//
//  The first mode, in the order AppleWin, Casso, Monitor, GSSquared,
//  WinDbg, whose own command the word is, and the Casso commands that word
//  stands for. AppleWin and Casso share one table: an engine command is
//  Casso's, any other AppleWin's.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandSuggestion::TryFindOwner (const std::string & word, CommandMode mode, CommandMode & owner, std::string & cassoNames)
{
    static constexpr CommandMode  s_kOrder[] =
    {
        CommandMode::AppleWin,
        CommandMode::Casso,
        CommandMode::Monitor,
        CommandMode::GSSquared,
        CommandMode::WinDbg,
    };
    const AppleWinCommand         * command    = AppleWinCommandTable::Find (word);
    bool                            isEngine   = AppleWinCommandTable::IsEngineCommand (word);
    bool                            isAppleWin = (mode == CommandMode::AppleWin || mode == CommandMode::Casso);



    for (CommandMode other : s_kOrder)
    {
        const CommandModeHelp::Entry  * entry = nullptr;



        if (other == mode)
        {
            continue;
        }

        if (other == CommandMode::AppleWin || other == CommandMode::Casso)
        {
            if (isAppleWin || command == nullptr || command->verb == DebugVerb::None || isEngine != (other == CommandMode::Casso))
            {
                continue;
            }

            owner      = other;
            cassoNames = (command->aliasOf != nullptr) ? command->aliasOf : command->name;
            return true;
        }

        entry = CommandModeHelp::Find (other, word);

        if (entry != nullptr && entry->word[0] != '\0')
        {
            owner      = other;
            cassoNames = entry->casso;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::TryFindEquivalent
//
//  The first of the Casso commands this mode runs, as the mode types it.
//  argumentsFit says the mode parses it with the arguments typed as that
//  same command.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandSuggestion::TryFindEquivalent (const std::string & cassoNames, const std::string & arguments, CommandMode mode, const IDebugExpressionContext & context, std::string & word, bool & argumentsFit)
{
    std::istringstream       names (cassoNames);
    std::string              name;
    std::string              typed;
    std::string              first;
    const AppleWinCommand  * command = nullptr;
    DebugVerb                verb    = DebugVerb::None;



    argumentsFit = false;

    while (names >> name)
    {
        command = AppleWinCommandTable::Find (name);
        typed   = (mode == CommandMode::AppleWin || mode == CommandMode::Casso) ? name : CommandModeHelp::GetTypedName (mode, name);

        if (command == nullptr || !IsRunnable (mode, name, typed))
        {
            continue;
        }

        if (first.empty())
        {
            first = typed;
        }

        if (TryParseAs (arguments.empty() ? typed : typed + " " + arguments, mode, context, verb) && verb == command->verb)
        {
            word         = typed;
            argumentsFit = true;
            return true;
        }
    }

    word = first;
    return !first.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::TryParseAs
//
////////////////////////////////////////////////////////////////////////////////

bool CommandSuggestion::TryParseAs (const std::string & line, CommandMode mode, const IDebugExpressionContext & context, DebugVerb & verb)
{
    switch (mode)
    {
    case CommandMode::GSSquared:
    {
        GSSquaredParseResult  parsed = GSSquaredParser::Parse (line, context);

        verb = parsed.commands.empty() ? DebugVerb::None : parsed.commands.front().verb;
        return parsed.status == ParseStatus::Ok;
    }

    case CommandMode::WinDbg:
    {
        WinDbgParseResult  parsed = WinDbgParser::Parse (line, context);

        verb = parsed.command.verb;
        return parsed.status == ParseStatus::Ok;
    }

    case CommandMode::Monitor:
        return false;

    default:
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, context);

        verb = parsed.command.verb;
        return parsed.status == ParseStatus::Ok;
    }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::IsRunnable
//
//  The mode's own word for a command always runs; its marker form only when
//  the mode reaches that command.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandSuggestion::IsRunnable (CommandMode mode, const std::string & cassoName, const std::string & typed)
{
    std::string  markerForm = std::string (CommandModeHelp::GetMarker (mode)) + cassoName;



    if (mode == CommandMode::AppleWin || mode == CommandMode::Casso)
    {
        return true;
    }

    if (_stricmp (typed.c_str(), markerForm.c_str()) != 0)
    {
        return true;
    }

    return CommandModeHelp::IsCassoCommandReachable (mode, cassoName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::FindClosest
//
//  The mode's command nearest the word by spelling, counting a swap of two
//  neighboring letters as one change. Nothing when the nearest is more
//  changes away than one for a word of four letters or fewer, or two for a
//  longer one.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandSuggestion::FindClosest (const std::string & word, CommandMode mode)
{
    static constexpr size_t  s_kShortWord   = 4;
    std::string              lower          = ToLower (word);
    std::string              best;
    size_t                   bestDistance   = SIZE_MAX;
    std::vector<std::string> candidates;



    if (mode == CommandMode::AppleWin || mode == CommandMode::Casso)
    {
        for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
        {
            if (command.verb != DebugVerb::None && command.availability != CommandAvailability::NotAvailable)
            {
                candidates.push_back (command.name);
            }
        }
    }
    else
    {
        for (const CommandModeHelp::Entry & entry : CommandModeHelp::GetEntries (mode))
        {
            if (entry.word[0] != '\0')
            {
                candidates.push_back (entry.word);
            }
        }
    }

    for (const std::string & candidate : candidates)
    {
        size_t  distance = GetDistance (lower, ToLower (candidate));



        if (distance < bestDistance)
        {
            bestDistance = distance;
            best         = candidate;
        }
    }

    if (best.empty() || bestDistance > (word.size() <= s_kShortWord ? 1u : 2u) || bestDistance >= word.size())
    {
        return std::string();
    }

    return MatchCase (best, word);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::FindUsage
//
//  The syntax help shows for the word: the mode's own entry, or the Casso
//  command it reaches bare or after its marker.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandSuggestion::FindUsage (const std::string & word, CommandMode mode)
{
    std::string                           marker = CommandModeHelp::GetMarker (mode);
    const CommandModeHelp::Entry        * entry  = nullptr;
    const CassoCommandReference::Entry  * casso  = nullptr;



    if (mode != CommandMode::AppleWin && mode != CommandMode::Casso)
    {
        if (!marker.empty() && word.size() > marker.size() && word.starts_with (marker))
        {
            casso = CassoCommandReference::Find (word.substr (marker.size()));
            return (casso != nullptr) ? marker + casso->syntax : std::string();
        }

        entry = CommandModeHelp::Find (mode, word);

        if (entry != nullptr && entry->word[0] != '\0')
        {
            return entry->syntax;
        }
    }

    casso = CassoCommandReference::Find (word);
    return (casso != nullptr) ? std::string (casso->syntax) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::GetDistance
//
//  Optimal string alignment: insertions, deletions, substitutions and swaps
//  of neighboring characters each count one.
//
////////////////////////////////////////////////////////////////////////////////

size_t CommandSuggestion::GetDistance (const std::string & a, const std::string & b)
{
    std::vector<std::vector<size_t>>  d (a.size() + 1, std::vector<size_t> (b.size() + 1, 0));
    size_t                            cost = 0;



    for (size_t i = 0; i <= a.size(); i++)
    {
        d[i][0] = i;
    }

    for (size_t j = 0; j <= b.size(); j++)
    {
        d[0][j] = j;
    }

    for (size_t i = 1; i <= a.size(); i++)
    {
        for (size_t j = 1; j <= b.size(); j++)
        {
            cost    = (a[i - 1] == b[j - 1]) ? 0 : 1;
            d[i][j] = std::min ({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });

            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
            {
                d[i][j] = std::min (d[i][j], d[i - 2][j - 2] + 1);
            }
        }
    }

    return d[a.size()][b.size()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::MatchCase
//
//  A word typed in lowercase gets the suggestion in lowercase; otherwise the
//  name keeps the case its table gives it.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandSuggestion::MatchCase (const std::string & name, const std::string & typed)
{
    bool  hasUpper = std::any_of (typed.begin(), typed.end(), [] (char c) { return c >= 'A' && c <= 'Z'; });



    return hasUpper ? name : ToLower (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestion::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandSuggestion::ToLower (const std::string & text)
{
    std::string  lower = text;



    std::transform (lower.begin(), lower.end(), lower.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });
    return lower;
}
