#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchActions
//
//  The actions on watches, zero-page pointers and bookmarks.
//
////////////////////////////////////////////////////////////////////////////////

class WatchActions
{
public:
    static DebuggerAction  GetClearWatch       (int id, CommandMode mode);

    //  WE and WD, by the watch's id; ZP, ZPC, ZPE and ZPD for the zero-page
    //  pointers, by slot.
    static DebuggerAction  GetEnableWatch           (int id, bool enable, CommandMode mode);
    static DebuggerAction  GetAddZeroPagePointer    (Byte address, CommandMode mode);
    static DebuggerAction  GetAddZeroPagePointerAt  (int slot, Byte address, CommandMode mode);
    static DebuggerAction  GetClearZeroPagePointer  (int slot, CommandMode mode);
    static DebuggerAction  GetEnableZeroPagePointer (int slot, bool enable, CommandMode mode);
    static DebuggerAction  GetAddBookmark           (Word address, CommandMode mode);
    static DebuggerAction  GetClearBookmark         (int id, CommandMode mode);
    static DebuggerAction  GetAddWatch              (Word address, CommandMode mode);

    //  The watches pane's edits. A string value is what the user typed, which
    //  the session evaluates in its mode when the action runs.
    static DebuggerAction  GetAddWatch             (const std::string & expression, CommandMode mode);

    //  A watch added back under the id it had, as an undone removal puts it,
    //  so it returns to its place in the list; under a new id if that one is
    //  taken.
    static DebuggerAction  GetAddWatchAt           (int id, const std::string & expression, CommandMode mode);
};
