#include "Pch.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/BreakpointHistory.h"
#include "Ui/Debugger/DebuggerActions.h"





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

bool BreakpointHistory::TryUndo (DebugSession & session, std::string & line)
{
    Step                  step;
    std::vector<Outcome>  outcomes;



    line.clear();

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

    line = Describe (outcomes);
    m_redo.push_back (std::move (step));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryRedo (DebugSession & session, std::string & line)
{
    Step                  step;
    std::vector<Outcome>  outcomes;



    line.clear();

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

    line = Describe (outcomes);
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
//  One console line for a whole undo or redo: the breakpoint and what it is
//  when the step touched one, or counts when it touched several.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHistory::Describe (const std::vector<Outcome> & outcomes)
{
    const Outcome  * only     = nullptr;
    int              restored = 0;
    int              removed  = 0;
    std::string      text;



    for (const Outcome & outcome : outcomes)
    {
        if (outcome.kind == Outcome::Kind::None)
        {
            continue;
        }

        only = &outcome;
        (outcome.kind == Outcome::Kind::Removed ? removed : restored)++;
    }

    if (restored + removed == 0)
    {
        return "No breakpoint changed";
    }

    if (restored + removed == 1)
    {
        return std::format ("{} breakpoint {} ({})",
                            only->kind == Outcome::Kind::Removed ? "Removed" : "Restored",
                            only->id,
                            BreakpointHandlers::Describe (only->info));
    }

    if (restored > 0)
    {
        text = std::format ("Restored {} breakpoint{}", restored, restored == 1 ? "" : "s");
    }

    if (removed > 0)
    {
        text += text.empty() ? std::format ("Removed {} breakpoint{}", removed, removed == 1 ? "" : "s")
                             : std::format (", removed {}", removed);
    }

    return text;
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
    m_ids[handle] = BreakpointHandlers::Add (session, saved);
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
    return left.enabled == right.enabled && left.temporary == right.temporary && left.stops == right.stops;
}





