#include "Pch.h"

#include "Ui/Debugger/WatchHistory.h"
#include "Ui/Debugger/UndoText.h"
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

    if (!m_undo.back().undo.restoreText.has_value())
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
        text = std::format (L"{} watch {}", (column == 0) ? L"move" : L"change", *watchId);
    }
    else if (autoIndex.has_value() && *autoIndex >= 0 && *autoIndex < (int) snapshot.autoWatches.size())
    {
        text = L"change " + TextEncoding::NarrowToWide (snapshot.autoWatches[(size_t) *autoIndex].label);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::ToText
//
//  Steps are apart at commas.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchHistory::ToText() const
{
    std::string  text;



    for (const std::vector<Step> * steps : { &m_undo, &m_redo })
    {
        text += (steps == &m_redo) ? "|" : "";

        for (size_t i = 0; i < steps->size(); i++)
        {
            text += ((i == 0) ? "" : ",") + FormatStep ((*steps)[i]);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::FormatStep
//
//  The undo's actions, the text of what a move watches again, the watch ids before
//  the move, the id of the watch it made, the edit's actions, and the text
//  for the menu, apart at colons; an absent value is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchHistory::FormatStep (const Step & step)
{
    std::string  ids;



    for (size_t i = 0; i < step.undo.movedFromIds.size(); i++)
    {
        ids += std::format ("{}{:X}", (i == 0) ? "" : "+", step.undo.movedFromIds[i]);
    }

    return std::format ("{}:{}:{}:{}:{}:{}", FormatActions (step.undo.actions),
                        step.undo.restoreText.has_value() ? UndoText::FormatText (*step.undo.restoreText) : std::string(),
                        ids,
                        step.undo.movedToId.has_value() ? std::format ("{:X}", *step.undo.movedToId) : std::string(),
                        FormatActions (step.edit), UndoText::FormatText (TextEncoding::WideToUtf8 (step.text)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::FormatActions
//
//  Each action's text, apart at plus signs.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchHistory::FormatActions (const std::vector<DebuggerAction> & actions)
{
    std::string  text;



    for (size_t i = 0; i < actions.size(); i++)
    {
        text += ((i == 0) ? "" : "+") + DebuggerActions::ToText (actions[i]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::FromText
//
////////////////////////////////////////////////////////////////////////////////

void WatchHistory::FromText (const std::string & text)
{
    std::vector<std::string>  parts = UndoText::Split (text, '|');
    Step                      step;



    Clear();

    if (parts.size() != 2)
    {
        return;
    }

    for (size_t list = 0; list < 2; list++)
    {
        for (const std::string & each : UndoText::Split (parts[list], ','))
        {
            if (!TryParseStep (each, step))
            {
                Clear();
                return;
            }

            ((list == 0) ? m_undo : m_redo).push_back (std::move (step));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::TryParseStep
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHistory::TryParseStep (const std::string & text, Step & step)
{
    std::vector<std::string>  fields = UndoText::Split (text, ':');
    std::string               utf8;
    uint32_t                  value  = 0;



    step = {};

    if (fields.size() != 6 || !TryParseActions (fields[0], step.undo.actions) || !TryParseActions (fields[4], step.edit) ||
        !UndoText::TryParseText (fields[5], utf8))
    {
        return false;
    }

    if (!fields[1].empty())
    {
        step.undo.restoreText = std::string();

        if (!UndoText::TryParseText (fields[1], *step.undo.restoreText))
        {
            return false;
        }
    }

    for (const std::string & id : UndoText::Split (fields[2], '+'))
    {
        if (!UndoText::TryParseHex (id, value))
        {
            return false;
        }

        step.undo.movedFromIds.push_back ((int) value);
    }

    if (!fields[3].empty())
    {
        if (!UndoText::TryParseHex (fields[3], value))
        {
            return false;
        }

        step.undo.movedToId = (int) value;
    }

    step.text = TextEncoding::Utf8ToWide (utf8);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory::TryParseActions
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHistory::TryParseActions (const std::string & text, std::vector<DebuggerAction> & actions)
{
    DebuggerAction  action;



    actions.clear();

    for (const std::string & each : UndoText::Split (text, '+'))
    {
        if (!DebuggerActions::TryFromText (each, action))
        {
            return false;
        }

        actions.push_back (std::move (action));
    }

    return true;
}
