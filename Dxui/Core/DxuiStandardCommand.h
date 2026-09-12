#pragma once

#include "Pch.h"

class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStandardCommand
//
//  The commands whose meaning belongs to whatever has focus rather than to the
//  application: what Copy copies is decided by the control the caret is in, not
//  by the window, and the same goes for the rest of them.
//
//  THESE ARE THE ONLY COMMANDS THE LIBRARY NAMES. Everything else an app can do
//  is the app's own, declared in its own table with its own id. A command
//  arrives here only when several unrelated controls would each answer it
//  differently and the answer has to follow the focus.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiStandardCommand
{
    None,
    Cut,
    Copy,
    Paste,
    SelectAll,
    Delete,
    Undo,
    Redo,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandRouter
//
//  Carries a standard command to the control that answers it: the focused one
//  first, then out through its parents, so a control that does not know the
//  command lets the one containing it answer.
//
//  ONE KEY TABLE FOR THE WHOLE LIBRARY. Ctrl+C means Copy in a text box, a list
//  and a hex view, and it is spelled out here once instead of in each of them.
//  A widget translates the key and asks itself; a menu asks the focused control
//  what the row should say and does the same thing when the row is picked. The
//  two paths meet at InvokeCommand, so a menu and its accelerator cannot come
//  to different conclusions.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCommandRouter
{
public:
    //  The standard command a keystroke means, or None.
    static DxuiStandardCommand  TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift);

    //  Asks `from` and then each control containing it, stopping at the first
    //  that answers. Query reports whether anything claims the command at all,
    //  which is what decides between a grayed row and no row.
    static bool  Invoke (IDxuiControl * from, DxuiStandardCommand command);
    static bool  Query  (const IDxuiControl * from, DxuiStandardCommand command, bool & outEnabled);
};
