#include "Pch.h"

#include "Ui/Debugger/WatchHistory.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void WatchHistory::Record (DebuggerViewState::WatchUndo undo, std::vector<DebuggerAction> edit, std::wstring text)
{
    m_undo.push_back ({ std::move (undo), std::move (edit), std::move (text) });
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





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::GetEditText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WatchHistory::GetEditText (const DebuggerViewSnapshot & snapshot, std::optional<int> watchId, std::optional<int> autoIndex, int column)
{
    std::wstring  text;



    if (watchId.has_value())
    {
        text = std::format (L"{} watch {}", (column == 0) ? L"moved" : L"changed", *watchId);
    }
    else if (autoIndex.has_value() && *autoIndex >= 0 && *autoIndex < (int) snapshot.autoWatches.size())
    {
        text = L"changed " + TextEncoding::NarrowToWide (snapshot.autoWatches[(size_t) *autoIndex].label);
    }

    return text;
}
