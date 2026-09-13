#include "Pch.h"

#include "DxuiStandardCommand.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandRouter::TranslateKey
//
//  The Windows keystrokes for these commands, including the older alternates
//  still in use: Ctrl+Insert for Copy, Shift+Insert for Paste and Shift+Delete
//  for Cut.
//
////////////////////////////////////////////////////////////////////////////////

DxuiStandardCommand DxuiCommandRouter::TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift)
{
    if (alt)
    {
        return DxuiStandardCommand::None;
    }

    if (ctrl && !shift)
    {
        switch (vk)
        {
        case 'X':       return DxuiStandardCommand::Cut;
        case 'C':       return DxuiStandardCommand::Copy;
        case 'V':       return DxuiStandardCommand::Paste;
        case 'A':       return DxuiStandardCommand::SelectAll;
        case 'Z':       return DxuiStandardCommand::Undo;
        case 'Y':       return DxuiStandardCommand::Redo;
        case VK_INSERT: return DxuiStandardCommand::Copy;
        default:        break;
        }
    }

    if (ctrl && shift && (vk == 'Z'))
    {
        return DxuiStandardCommand::Redo;
    }

    if (shift && !ctrl)
    {
        switch (vk)
        {
        case VK_INSERT: return DxuiStandardCommand::Paste;
        case VK_DELETE: return DxuiStandardCommand::Cut;
        default:        break;
        }
    }

    return DxuiStandardCommand::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandRouter::Invoke
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCommandRouter::Invoke (IDxuiControl * from, DxuiStandardCommand command)
{
    IDxuiControl *  at = from;



    while (at != nullptr)
    {
        if (at->InvokeCommand (command))
        {
            return true;
        }

        at = at->GetParent();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandRouter::Query
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiCommandRouter::Query (const IDxuiControl * from, DxuiStandardCommand command, bool & outEnabled)
{
    const IDxuiControl *  at = from;



    outEnabled = false;

    while (at != nullptr)
    {
        if (at->QueryCommand (command, outEnabled))
        {
            return true;
        }

        at = at->GetParent();
    }

    return false;
}
