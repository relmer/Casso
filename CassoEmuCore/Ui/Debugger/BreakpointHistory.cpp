#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/BreakpointHistory.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/UndoText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Record
//
//  Every breakpoint the change added, removed or altered is an entry, in id
//  order, so undoing walks them back in the reverse of that order.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::Record (DebugSession & session, const Change & change)
{
    std::map<int, BreakpointInfo>  before      = ListById (session);
    std::map<int, Saved>           savedBefore = SaveAll (session);
    std::map<int, BreakpointInfo>  after;
    std::map<int, Saved>           savedAfter;
    Step                           step;



    change();
    after      = ListById (session);
    savedAfter = SaveAll (session);

    for (const auto & [id, info] : before)
    {
        auto  now = after.find (id);

        if (now == after.end())
        {
            step.push_back ({ GetHandle (id), info, std::nullopt, savedBefore[id], std::nullopt });
        }
        else if (!IsSame (info, now->second))
        {
            step.push_back ({ GetHandle (id), info, now->second, savedBefore[id], savedAfter[id] });
        }
    }

    for (const auto & [id, info] : after)
    {
        if (!before.contains (id))
        {
            step.push_back ({ GetHandle (id), std::nullopt, info, std::nullopt, savedAfter[id] });
        }
    }

    if (step.empty())
    {
        return;
    }

    m_undo.push_back (std::move (step));
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryUndo
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryUndo (DebugSession & session, std::vector<std::string> & lines)
{
    Step                  step;
    std::vector<Outcome>  outcomes;



    lines.clear();

    if (m_undo.empty())
    {
        return false;
    }

    step = std::move (m_undo.back());
    m_undo.pop_back();

    for (auto entry = step.rbegin(); entry != step.rend(); ++entry)
    {
        outcomes.push_back (Apply (session, *entry, entry->after, entry->before, entry->savedBefore));
    }

    lines = Describe (outcomes);
    m_redo.push_back (std::move (step));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryRedo (DebugSession & session, std::vector<std::string> & lines)
{
    Step                  step;
    std::vector<Outcome>  outcomes;



    lines.clear();

    if (m_redo.empty())
    {
        return false;
    }

    step = std::move (m_redo.back());
    m_redo.pop_back();

    for (const Entry & entry : step)
    {
        outcomes.push_back (Apply (session, entry, entry.before, entry.after, entry.savedAfter));
    }

    lines = Describe (outcomes);
    m_undo.push_back (std::move (step));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::GetFlagsAction
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointHistory::GetFlagsAction (int id, const BreakpointInfo & info, CommandMode mode)
{
    return DebuggerActions::GetChangeBreakpoint (id, info.enabled, info.temporary, info.stops, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Describe
//
//  One console line for each breakpoint an undo or redo changed, saying
//  what happened to it, or one line saying none changed.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> BreakpointHistory::Describe (const std::vector<Outcome> & outcomes)
{
    std::vector<std::string>  lines;



    for (const Outcome & outcome : outcomes)
    {
        if (outcome.kind == Outcome::Kind::None)
        {
            continue;
        }

        lines.push_back (std::format ("{} breakpoint {} ({})",
                                      outcome.kind == Outcome::Kind::Removed ? "Removed" : "Restored",
                                      outcome.id,
                                      BreakpointHandlers::Describe (outcome.info)));
    }

    if (lines.empty())
    {
        lines.push_back ("No breakpoint changed");
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Apply
//
//  Takes one breakpoint from `from` to `to` straight from the table entry
//  saved with the step, with no command built or run. A breakpoint that
//  should exist and no longer does, or is no longer the one recorded, is
//  left alone.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointHistory::Outcome BreakpointHistory::Apply (
    DebugSession                         & session,
    const Entry                          & entry,
    const std::optional<BreakpointInfo>  & from,
    const std::optional<BreakpointInfo>  & to,
    const std::optional<Saved>           & saved)
{
    std::optional<int>  id;



    if (!from.has_value())
    {
        if (!saved.has_value() || !to.has_value())
        {
            return {};
        }

        Add (session, entry.handle, *saved);
        return { Outcome::Kind::Restored, m_ids[entry.handle], *to };
    }

    id = GetLiveId (session, entry.handle, *from);

    if (!id.has_value())
    {
        return {};
    }

    if (!to.has_value())
    {
        BreakpointHandlers::Remove (session, *id);
        return { Outcome::Kind::Removed, *id, *from };
    }

    if (BreakpointHandlers::MakeDefinition (*from) != BreakpointHandlers::MakeDefinition (*to) && saved.has_value())
    {
        BreakpointHandlers::Replace (session, *id, *saved);
    }
    else
    {
        BreakpointHandlers::SetState (session, *id, *to);
    }

    return { Outcome::Kind::Restored, *id, *to };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Add
//
//  The breakpoint handler adds the saved entry back as it was, under a new
//  id that the handle takes.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::Add (DebugSession & session, int handle, const Saved & saved)
{
    auto  previous = m_ids.find (handle);
    int   id       = BreakpointHandlers::Add (session, saved);



    //  The breakpoint comes back under the id it had, unless another took it.
    //  The next id stays past it, since BPC * starts the ids over at 0 and
    //  the next add would otherwise take it too.
    if (previous != m_ids.end() && previous->second != id &&
        (session.GetBreakpoints().TryChangeId (id, previous->second) || session.GetWatchpoints().TryChangeId (id, previous->second)))
    {
        id = previous->second;
        session.SetNextId (std::max (session.GetNextId(), id + 1));
    }

    m_ids[handle] = id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::GetHandle
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointHistory::GetHandle (int id)
{
    int  handle = 0;



    for (const auto & [h, mapped] : m_ids)
    {
        if (mapped == id)
        {
            return h;
        }
    }

    handle        = m_nextHandle++;
    m_ids[handle] = id;
    return handle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::GetLiveId
//
//  The handle's id, while the table still holds a breakpoint there with the
//  definition the step recorded.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> BreakpointHistory::GetLiveId (DebugSession & session, int handle, const BreakpointInfo & expected) const
{
    std::map<int, BreakpointInfo>  now = ListById (session);
    auto                           id  = m_ids.find (handle);
    auto                           at  = now.end();



    if (id == m_ids.end())
    {
        return std::nullopt;
    }

    at = now.find (id->second);

    if (at == now.end() || BreakpointHandlers::MakeDefinition (at->second) != BreakpointHandlers::MakeDefinition (expected))
    {
        return std::nullopt;
    }

    return id->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::ListById
//
////////////////////////////////////////////////////////////////////////////////

std::map<int, BreakpointInfo> BreakpointHistory::ListById (DebugSession & session)
{
    BreakpointListData             list;
    std::map<int, BreakpointInfo>  byId;



    BreakpointHandlers::ListAll (session, list);

    for (const BreakpointInfo & info : list.breakpoints)
    {
        byId[info.id] = info;
    }

    return byId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::SaveAll
//
////////////////////////////////////////////////////////////////////////////////

std::map<int, BreakpointHistory::Saved> BreakpointHistory::SaveAll (DebugSession & session)
{
    std::map<int, Saved>  byId;



    for (const Breakpoint & entry : session.GetBreakpoints().GetAll())
    {
        (void) BreakpointHandlers::TrySave (session, entry.id, byId[entry.id]);
    }

    for (const Watchpoint & entry : session.GetWatchpoints().GetAll())
    {
        (void) BreakpointHandlers::TrySave (session, entry.id, byId[entry.id]);
    }

    return byId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::IsSame
//
//  Hit counts are not a change the pane made.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::IsSame (const BreakpointInfo & left, const BreakpointInfo & right)
{
    return BreakpointHandlers::MakeDefinition (left) == BreakpointHandlers::MakeDefinition (right) && HasSameFlags (left, right);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::HasSameFlags
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::HasSameFlags (const BreakpointInfo & left, const BreakpointInfo & right)
{
    return left.enabled == right.enabled && left.temporary == right.temporary && left.stops == right.stops && left.name == right.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Clear
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::Clear()
{
    m_undo.clear();
    m_redo.clear();
    m_ids.clear();
    m_nextHandle = 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::ToText
//
//  The next handle, the handles' ids, the steps to undo and the steps to
//  redo, apart at bars. Steps are apart at commas, a step's entries at
//  slashes, and an entry is its handle and its two sides apart at dots.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHistory::ToText() const
{
    std::string  text  = std::format ("{:X}|", m_nextHandle);
    bool         first = true;



    for (const auto & [handle, id] : m_ids)
    {
        text += std::format ("{}{:X}:{:X}", first ? "" : ",", handle, id);
        first = false;
    }

    for (const std::vector<Step> * steps : { &m_undo, &m_redo })
    {
        text += "|";

        for (size_t i = 0; i < steps->size(); i++)
        {
            text += (i == 0) ? "" : ",";

            for (size_t j = 0; j < (*steps)[i].size(); j++)
            {
                const Entry  & entry = (*steps)[i][j];

                text += std::format ("{}{:X}.{}.{}", (j == 0) ? "" : "/", entry.handle, FormatSide (entry.before), FormatSide (entry.after));
            }
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::FormatSide
//
//  Empty where the breakpoint did not exist; else its definition, its
//  enabled, temporary and stops flags as 1 or 0, and its name, apart at
//  colons.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHistory::FormatSide (const std::optional<BreakpointInfo> & info)
{
    if (!info.has_value())
    {
        return std::string();
    }

    return std::format ("{}:{}{}{}:{}", UndoText::FormatText (BreakpointHandlers::MakeDefinition (*info)),
                        info->enabled ? 1 : 0, info->temporary ? 1 : 0, info->stops ? 1 : 0, UndoText::FormatText (info->name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::FromText
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::FromText (DebugSession & session, const std::string & text)
{
    std::vector<std::string>  parts  = UndoText::Split (text, '|');
    uint32_t                  next   = 0;
    uint32_t                  handle = 0;
    uint32_t                  id     = 0;
    Step                      step;



    Clear();

    if (parts.size() != 4 || !UndoText::TryParseHex (parts[0], next) || next == 0)
    {
        return;
    }

    for (const std::string & pair : UndoText::Split (parts[1], ','))
    {
        std::vector<std::string>  fields = UndoText::Split (pair, ':');

        if (fields.size() != 2 || !UndoText::TryParseHex (fields[0], handle) || !UndoText::TryParseHex (fields[1], id))
        {
            Clear();
            return;
        }

        m_ids[(int) handle] = (int) id;
    }

    for (size_t list = 2; list <= 3; list++)
    {
        for (const std::string & each : UndoText::Split (parts[list], ','))
        {
            if (!TryParseStep (session, each, step))
            {
                Clear();
                return;
            }

            ((list == 2) ? m_undo : m_redo).push_back (std::move (step));
        }
    }

    m_nextHandle = (int) next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryParseStep
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryParseStep (DebugSession & session, const std::string & text, Step & step)
{
    uint32_t  handle = 0;



    step.clear();

    for (const std::string & each : UndoText::Split (text, '/'))
    {
        std::vector<std::string>  fields = UndoText::Split (each, '.');
        Entry                     entry;

        if (fields.size() != 3 || !UndoText::TryParseHex (fields[0], handle) ||
            !TryParseSide (session, fields[1], entry.before, entry.savedBefore) ||
            !TryParseSide (session, fields[2], entry.after,  entry.savedAfter))
        {
            return false;
        }

        entry.handle = (int) handle;
        step.push_back (std::move (entry));
    }

    return !step.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryParseSide
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryParseSide (DebugSession & session, const std::string & text,
                                      std::optional<BreakpointInfo> & info, std::optional<Saved> & saved)
{
    std::vector<std::string>  fields = UndoText::Split (text, ':');
    std::string               definition;
    std::string               name;
    BreakpointInfo            rebuilt;
    Saved                     entry;



    info.reset();
    saved.reset();

    if (text.empty())
    {
        return true;
    }

    if (fields.size() != 3 || fields[1].size() != 3 || !std::ranges::all_of (fields[1], [] (char ch) { return ch == '0' || ch == '1'; }) ||
        !UndoText::TryParseText (fields[0], definition) || !UndoText::TryParseText (fields[2], name) ||
        !TryRebuild (session, definition, rebuilt, entry))
    {
        return false;
    }

    rebuilt.enabled   = fields[1][0] == '1';
    rebuilt.temporary = fields[1][1] == '1';
    rebuilt.stops     = fields[1][2] == '1';
    rebuilt.name      = name;

    if (entry.isWatchpoint)
    {
        entry.watchpoint.enabled   = rebuilt.enabled;
        entry.watchpoint.temporary = rebuilt.temporary;
        entry.watchpoint.stops     = rebuilt.stops;
        entry.watchpoint.name      = name;
    }
    else
    {
        entry.breakpoint.enabled   = rebuilt.enabled;
        entry.breakpoint.temporary = rebuilt.temporary;
        entry.breakpoint.stops     = rebuilt.stops;
        entry.breakpoint.name      = name;
    }

    info  = rebuilt;
    saved = entry;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryRebuild
//
//  The table entry a definition makes, from adding it to the session and
//  removing it again. A definition that makes other than one entry fails.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryRebuild (DebugSession & session, const std::string & definition, BreakpointInfo & info, Saved & saved)
{
    std::map<int, BreakpointInfo>  before = ListById (session);
    std::map<int, BreakpointInfo>  after;
    std::vector<int>               added;
    int                            nextId = session.GetNextId();
    AppleWinParseResult            parsed = AppleWinParser::Parse (definition, session);
    bool                           isOk   = false;



    if (parsed.status != ParseStatus::Ok)
    {
        return false;
    }

    (void) session.Execute (parsed.command);
    after = ListById (session);

    for (const auto & [id, each] : after)
    {
        if (!before.contains (id))
        {
            added.push_back (id);
        }
    }

    isOk = added.size() == 1 && BreakpointHandlers::TrySave (session, added[0], saved);

    if (isOk)
    {
        info = after[added[0]];
    }

    for (int id : added)
    {
        BreakpointHandlers::Remove (session, id);
    }

    session.SetNextId (nextId);
    return isOk;
}
