#pragma once

#include "Pch.h"

class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStandardCommand
//
//  Commands whose behavior depends on the focused control rather than the
//  application: what Copy copies depends on the control with the caret, not
//  on the window, and the same applies to the others.
//
//  THESE ARE THE ONLY COMMANDS THE LIBRARY DEFINES. All other commands belong
//  to the application, in its own table with its own ids. A command belongs
//  here only when several unrelated controls would each handle it differently
//  and the handling has to follow focus.
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
//  Routes a standard command to the control that handles it: the focused
//  control first, then each parent in turn, so a control that does not handle
//  the command passes it to its container.
//
//  ONE KEY TABLE FOR THE WHOLE LIBRARY. Ctrl+C is Copy in a text box, a list
//  and a hex view, and the mapping is defined here once instead of in each
//  widget. A widget translates the key and invokes the command on itself; a
//  menu queries the focused control for a row's state and invokes the same
//  command when the row is picked. Both paths call InvokeCommand, so a menu
//  row and its accelerator always behave the same.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCommandRouter
{
public:
    //  The standard command a keystroke means, or None.
    static DxuiStandardCommand  TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift);

    //  Tries `from` and then each control containing it, stopping at the first
    //  that handles the command. Query returns whether any control handles the
    //  command at all, which determines whether a menu row is grayed or absent.
    static bool  Invoke (IDxuiControl * from, DxuiStandardCommand command);
    static bool  Query  (const IDxuiControl * from, DxuiStandardCommand command, bool & outEnabled);
};
