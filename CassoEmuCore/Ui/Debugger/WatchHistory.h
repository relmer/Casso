#pragma once

#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHistory
//
//  Undo and redo for the watch pane. Each edit is one step: what puts it
//  back, from the snapshot as it stood before the edit, and the actions the
//  edit itself ran, which make it again.
//
//  A moved watch is undone but not redone: undoing it adds a watch the
//  engine numbers only once the undo runs, so there is no id for a redo to
//  remove.
//
////////////////////////////////////////////////////////////////////////////////

class WatchHistory
{
public:
    //  An edit the pane made: what undoes it, and the actions it ran. Clears
    //  what could be redone.
    void  Record (DebuggerViewState::WatchUndo undo, std::vector<DebuggerAction> edit);

    //  Notes the watch each pending move made, from a new snapshot.
    void  OnSnapshot (const DebuggerViewSnapshot & now);

    //  The actions that take the newest step back, or nothing when there is
    //  none or a move is not yet shown by any snapshot.
    std::optional<std::vector<DebuggerAction>>  TryUndo (const DebuggerViewSnapshot & now, CommandMode mode);

    //  The actions that make the newest undone step again, or nothing.
    std::optional<std::vector<DebuggerAction>>  TryRedo ();

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    void  Clear ();

private:
    struct Step
    {
        DebuggerViewState::WatchUndo  undo;
        std::vector<DebuggerAction>   edit;
    };

    std::vector<Step>  m_undo;
    std::vector<Step>  m_redo;
};
