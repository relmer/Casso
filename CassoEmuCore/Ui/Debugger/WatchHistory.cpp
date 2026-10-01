#include "Pch.h"

#include "Ui/Debugger/WatchHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void WatchHistory::Record (DebuggerViewState::WatchUndo undo, std::vector<DebuggerAction> edit)
{
    m_undo.push_back ({ std::move (undo), std::move (edit) });
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::OnSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void WatchHistory::OnSnapshot (const DebuggerViewSnapshot & now)
{
    for (Step & step : m_undo)
    {
        DebuggerViewState::NoteMovedWatch (now, step.undo);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::TryUndo
//
//  A move no snapshot has shown yet stays on the stack for the next try. A
//  move, once undone, has nothing a redo could act on, so it is not kept.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<DebuggerAction>> WatchHistory::TryUndo (const DebuggerViewSnapshot & now, CommandMode mode)
{
    std::optional<std::vector<DebuggerAction>>  actions;



    if (m_undo.empty())
    {
        return std::nullopt;
    }

    actions = DebuggerViewState::GetWatchUndoActions (now, m_undo.back().undo, mode);

    if (!actions.has_value())
    {
        return std::nullopt;
    }

    if (!m_undo.back().undo.restoreAddress.has_value())
    {
        m_redo.push_back (std::move (m_undo.back()));
    }

    m_undo.pop_back();
    return actions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<DebuggerAction>> WatchHistory::TryRedo()
{
    std::vector<DebuggerAction>  actions;



    if (m_redo.empty())
    {
        return std::nullopt;
    }

    actions = m_redo.back().edit;
    m_undo.push_back (std::move (m_redo.back()));
    m_redo.pop_back();
    return actions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::Clear
//
////////////////////////////////////////////////////////////////////////////////

void WatchHistory::Clear()
{
    m_undo.clear();
    m_redo.clear();
}
