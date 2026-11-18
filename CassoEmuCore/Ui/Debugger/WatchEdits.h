#pragma once

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits
//
//  What an edit in the Watches pane does and what puts it back: the actions a
//  finished edit takes, why typed text cannot be a watch, and the undo of an
//  edit, all from a snapshot.
//
////////////////////////////////////////////////////////////////////////////////

class WatchEdits
{
public:
    //  What puts a watch edit back (FR-097), from the snapshot as it stood
    //  BEFORE the edit. Moving a manual watch cannot be undone by an action
    //  alone: the watch the edit made gets its id from the engine only once
    //  the edit runs, so the undo has the watch it replaced and the caller
    //  finds the new one when the time comes.
    struct WatchUndo
    {
        std::vector<DebuggerAction>  actions;

        //  For a moved watch: the id before the edit, and what to watch again,
        //  an address in hex or BASIC and a variable's name. `movedFromIds` is
        //  every watch id there was, so the one the edit made is the one not
        //  among them.
        std::optional<std::string>   restoreText;
        std::vector<int>             movedFromIds;

        //  The watch the move made, once a snapshot shows it.
        std::optional<int>           movedToId;
    };

    //  The actions a finished watch edit takes (FR-096), echoed in `mode`.
    //  `watchId` picks a manual watch and `autoIndex` an automatic one;
    //  exactly one is set. Column 0 is the expression, 1 the value. Empty for
    //  text with nothing writable, or an edit that is not allowed -- an
    //  automatic watch's expression is what the instruction touches, not the
    //  user's. What was typed is evaluated in the session's mode when the
    //  action runs.
    static std::vector<DebuggerAction>  GetWatchEditActions (const DebuggerViewSnapshot & snapshot,
                                                             std::optional<int> watchId, std::optional<int> autoIndex,
                                                             int column, const std::string & typed, CommandMode mode);

    //  Why text typed as a watch cannot be one, or empty when it can. The
    //  session checks it again when the watch is added; with this check, a
    //  typing mistake stays in the edit box.
    static std::string  GetWatchError (const DebuggerViewSnapshot & snapshot, const std::string & typed);

    static std::optional<WatchUndo>  GetWatchUndo (const DebuggerViewSnapshot & before,
                                                   std::optional<int> watchId, std::optional<int> autoIndex, int column,
                                                   CommandMode mode);

    //  Notes the watch a move made from a snapshot taken after it ran, and
    //  returns the actions that put the edit back: none yet while no snapshot
    //  shows the moved watch, and empty once it has been removed.
    static void                                        NoteMovedWatch      (const DebuggerViewSnapshot & now, WatchUndo & undo);
    static std::optional<std::vector<DebuggerAction>>  GetWatchUndoActions (const DebuggerViewSnapshot & now, WatchUndo & undo, CommandMode mode);

    //  A manual watch of a register, A, X, Y, S, P or PC, in any case.
    static bool         IsRegisterWatch (const DebuggerViewSnapshot::WatchLine & watch);

    //  A manual watch of a byte, its text ending in `.b`.
    static bool         IsByteWatch     (const DebuggerViewSnapshot::WatchLine & watch);
    static std::string  GetUpper        (const std::string & text);

private:
    static bool  TryParseHexWord (const std::string & text, Word & value);
};
