#include "Pch.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/BreakpointHistory.h"





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
    std::map<int, BreakpointInfo>  before = ListById (session);
    std::map<int, BreakpointInfo>  after;
    Step                           step;



    change();
    after = ListById (session);

    for (const auto & [id, info] : before)
    {
        auto  now = after.find (id);

        if (now == after.end())
        {
            step.push_back ({ GetHandle (id), info, std::nullopt });
        }
        else if (!IsSame (info, now->second))
        {
            step.push_back ({ GetHandle (id), info, now->second });
        }
    }

    for (const auto & [id, info] : after)
    {
        if (!before.contains (id))
        {
            step.push_back ({ GetHandle (id), std::nullopt, info });
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

bool BreakpointHistory::TryUndo (DebugSession & session, const LineRunner & run)
{
    Step  step;



    if (m_undo.empty())
    {
        return false;
    }

    step = std::move (m_undo.back());
    m_undo.pop_back();

    for (auto entry = step.rbegin(); entry != step.rend(); ++entry)
    {
        Apply (session, run, *entry, entry->after, entry->before);
    }

    m_redo.push_back (std::move (step));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHistory::TryRedo (DebugSession & session, const LineRunner & run)
{
    Step  step;



    if (m_redo.empty())
    {
        return false;
    }

    step = std::move (m_redo.back());
    m_redo.pop_back();

    for (const Entry & entry : step)
    {
        Apply (session, run, entry, entry.before, entry.after);
    }

    m_undo.push_back (std::move (step));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::GetFlagsLine
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHistory::GetFlagsLine (int id, const BreakpointInfo & info)
{
    return std::format ("BPCHANGE {} {}{}{}", id, info.enabled ? 'E' : 'e', info.temporary ? 'T' : 't', info.stops ? 'S' : 's');
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Apply
//
//  Takes one breakpoint from `from` to `to`. A breakpoint that should exist
//  and no longer does, or is no longer the one recorded, is left alone.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::Apply (
    DebugSession                         & session,
    const LineRunner                     & run,
    const Entry                          & entry,
    const std::optional<BreakpointInfo>  & from,
    const std::optional<BreakpointInfo>  & to)
{
    std::optional<int>  id;



    if (!from.has_value())
    {
        Add (session, run, entry.handle, *to);
        return;
    }

    id = GetLiveId (session, entry.handle, *from);

    if (!id.has_value())
    {
        return;
    }

    if (!to.has_value())
    {
        run (std::format ("BPC {}", *id));
        return;
    }

    if (BreakpointHandlers::MakeDefinition (*from) != BreakpointHandlers::MakeDefinition (*to))
    {
        run (std::format ("BPEDIT {} {}", *id, BreakpointHandlers::MakeDefinition (*to)));
    }

    if (!HasSameFlags (*from, *to))
    {
        run (GetFlagsLine (*id, *to));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory::Add
//
//  The definition adds the breakpoint with a new id, found as the one id
//  that was not there before; the handle takes it.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHistory::Add (DebugSession & session, const LineRunner & run, int handle, const BreakpointInfo & to)
{
    std::map<int, BreakpointInfo>  before = ListById (session);
    std::map<int, BreakpointInfo>  after;
    BreakpointInfo                 fresh;



    run (BreakpointHandlers::MakeDefinition (to));
    after = ListById (session);

    for (const auto & [id, info] : after)
    {
        if (before.contains (id))
        {
            continue;
        }

        m_ids[handle] = id;

        if (!HasSameFlags (info, to))
        {
            run (GetFlagsLine (id, to));
        }

        return;
    }
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





