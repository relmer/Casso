#include "Pch.h"

#include "Debugger/Handlers/BreakpointGroupHandlers.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::Group
//
//  BPGROUP lists the groups and their breakpoints; BPGROUP name #[,#...]
//  puts breakpoints in a group, making it; BPGROUP ! #[,#...] takes them out
//  of theirs; ENABLE, DISABLE, UNGROUP and DELETE with a name act on a
//  group, and RENAME old new renames it. A name with spaces is quoted.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointGroupHandlers::Group (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<std::string>  words = SplitGroupWords (command.text);
    std::vector<int>          ids;
    MessageData               message;
    BreakpointListData        list;
    bool                      isOut = false;



    if (words.empty())
    {
        BreakpointHandlers::ListAll (session, list);

        for (const std::string & group : GetGroups (session))
        {
            std::string  line = std::format ("Group \"{}\":", group);

            for (const BreakpointInfo & info : list.breakpoints)
            {
                line += BreakpointTable::IsSameGroup (info.group, group) ? std::format (" #{}", info.id) : std::string();
            }

            message.lines.push_back (line);
        }

        if (message.lines.empty())
        {
            message.lines.push_back ("No breakpoint groups.");
        }

        reply.data = message;
        return;
    }

    if (words.size() >= 2 && words[0] != "!" && !std::isdigit ((unsigned char) words[1][0]) && words[1][0] != '#')
    {
        ActOnGroup (session, words, reply);
        return;
    }

    if (!TryParseIds (session, words, 1, ids, reply))
    {
        return;
    }

    isOut = words[0] == "!";

    for (int id : ids)
    {
        (void) SetGroup (session, id, isOut ? std::string() : words[0]);
    }

    session.OnStopConditionsChanged();

    reply.data = MessageData { { isOut ? std::format ("{} breakpoint{} taken out of {} group.", ids.size(), (ids.size() == 1) ? "" : "s", (ids.size() == 1) ? "its" : "their")
                                       : std::format ("{} breakpoint{} put in group \"{}\".", ids.size(), (ids.size() == 1) ? "" : "s", FindGroup (session, words[0])) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::ActOnGroup
//
//  ENABLE, DISABLE, UNGROUP or DELETE name, or RENAME old new.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointGroupHandlers::ActOnGroup (DebugSession & session, const std::vector<std::string> & words, Reply & reply)
{
    BreakpointListData  list;
    std::string         action   = words[0];
    std::string         group    = FindGroup (session, words[1]);
    std::vector<int>    ids;
    std::string         done;
    bool                isRename = false;
    bool                isKnown  = false;



    std::ranges::transform (action, action.begin(), [] (char ch) { return (char) toupper ((unsigned char) ch); });

    isRename = action == "RENAME";
    isKnown  = action == "ENABLE" || action == "DISABLE" || action == "UNGROUP" || action == "DELETE" || isRename;

    if (!isKnown || words.size() != (isRename ? 3u : 2u))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BPGROUP takes a name and breakpoint ids, ! and ids, ENABLE, DISABLE, UNGROUP or DELETE and a name, or RENAME and two names.");
        return;
    }

    if (group.empty())
    {
        reply.SetError (CommandStatus::Error, "no such group", std::format ("There is no breakpoint group \"{}\". BPGROUP lists them.", words[1]));
        return;
    }

    if (isRename && !FindGroup (session, words[2]).empty() && !BreakpointTable::IsSameGroup (group, words[2]))
    {
        reply.SetError (CommandStatus::Error, "group exists", std::format ("There is a breakpoint group \"{}\" already.", FindGroup (session, words[2])));
        return;
    }

    BreakpointHandlers::ListAll (session, list);

    for (const BreakpointInfo & info : list.breakpoints)
    {
        if (BreakpointTable::IsSameGroup (info.group, group))
        {
            ids.push_back (info.id);
        }
    }

    //  A rename takes every breakpoint out of the group first, so the new
    //  name is written as typed, even when it differs from the old in case
    //  alone.
    for (int id : ids)
    {
        if (action == "ENABLE" || action == "DISABLE")
        {
            (void) (session.GetBreakpoints().TrySetEnabled (id, action == "ENABLE") || session.GetWatchpoints().TrySetEnabled (id, action == "ENABLE"));
        }
        else if (action == "DELETE")
        {
            (void) (session.GetBreakpoints().TryClear (id) || session.GetWatchpoints().TryClear (id));
        }
        else
        {
            (void) SetGroup (session, id, std::string());
        }
    }

    for (int id : ids)
    {
        if (isRename)
        {
            (void) SetGroup (session, id, words[2]);
        }
    }

    session.OnStopConditionsChanged();

    done = (action == "ENABLE")  ? std::string ("enabled")
         : (action == "DISABLE") ? std::string ("disabled")
         : (action == "UNGROUP") ? std::string ("ungrouped")
         : (action == "DELETE")  ? std::string ("deleted")
         :                         std::format ("renamed \"{}\"", words[2]);

    reply.data = MessageData { { std::format ("Group \"{}\" {}: {} breakpoint{}.", group, done, ids.size(), (ids.size() == 1) ? "" : "s") } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::TryParseIds
//
//  Decimal ids, each with or without #, apart by spaces or commas, every one
//  a breakpoint's; otherwise the reply has an error for the first that isn't.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointGroupHandlers::TryParseIds (DebugSession & session, const std::vector<std::string> & words, size_t first, std::vector<int> & ids, Reply & reply)
{
    BreakpointInfo  info;



    for (size_t i = first; i < words.size(); i++)
    {
        std::string  word = words[i];
        size_t       at   = 0;

        std::ranges::replace (word, ',', ' ');

        while (at < word.size())
        {
            size_t  end = word.find (' ', at);
            size_t  num = (word[at] == '#') ? at + 1 : at;
            int     id  = 0;

            end = (end == std::string::npos) ? word.size() : end;

            if (end == at)
            {
                at++;
                continue;
            }

            if (num == end || std::from_chars (word.data() + num, word.data() + end, id).ptr != word.data() + end)
            {
                reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("\"{}\" is not a breakpoint id.", word.substr (at, end - at)));
                return false;
            }

            if (!BreakpointHandlers::TryFindInfo (session, id, info))
            {
                BreakpointHandlers::SetNoSuch (session, reply, id);
                return false;
            }

            ids.push_back (id);
            at = end + 1;
        }
    }

    if (ids.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BPGROUP takes the ids of the breakpoints to group.");
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::GetGroups
//
//  Every group a breakpoint or watchpoint is in, in id order of the first
//  one in each.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> BreakpointGroupHandlers::GetGroups (DebugSession & session)
{
    BreakpointListData        list;
    std::vector<std::string>  groups;



    BreakpointHandlers::ListAll (session, list);

    for (const BreakpointInfo & info : list.breakpoints)
    {
        if (!info.group.empty() && std::ranges::none_of (groups, [&info] (const std::string & each) { return BreakpointTable::IsSameGroup (each, info.group); }))
        {
            groups.push_back (info.group);
        }
    }

    return groups;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::FindGroup
//
//  The group a name matches without regard to case, as its breakpoints have
//  it; empty for none.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointGroupHandlers::FindGroup (DebugSession & session, const std::string & group)
{
    for (const std::string & each : GetGroups (session))
    {
        if (BreakpointTable::IsSameGroup (each, group))
        {
            return each;
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::SetGroup
//
//  A name that matches a group is written as that group has it, so the
//  group's name is as first typed. False for no such breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointGroupHandlers::SetGroup (DebugSession & session, int id, const std::string & group)
{
    std::string  written = FindGroup (session, group);



    written = written.empty() ? group : written;

    return session.GetBreakpoints().TrySetGroup (id, written) || session.GetWatchpoints().TrySetGroup (id, written);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers::SplitGroupWords
//
//  Words apart by spaces, a quoted run being one word without its quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> BreakpointGroupHandlers::SplitGroupWords (const std::string & text)
{
    std::vector<std::string>  words;
    size_t                    at    = 0;



    while (at < text.size())
    {
        size_t  end = 0;

        if (isspace ((unsigned char) text[at]))
        {
            at++;
            continue;
        }

        if (text[at] == '"')
        {
            end = text.find ('"', at + 1);
            end = (end == std::string::npos) ? text.size() : end;
            words.push_back (text.substr (at + 1, end - at - 1));
            at  = end + 1;
            continue;
        }

        end = text.find_first_of (" \t", at);
        end = (end == std::string::npos) ? text.size() : end;
        words.push_back (text.substr (at, end - at));
        at  = end;
    }

    return words;
}
