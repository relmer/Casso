#include "Pch.h"

#include "Ui/Debugger/FocusAccentOwner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAccentOwner::OnMainFocusChanged
//
//  The main window taking the focus takes the accent back from a float.
//
////////////////////////////////////////////////////////////////////////////////

void FocusAccentOwner::OnMainFocusChanged (bool focused)
{
    if (focused)
    {
        m_focusedFloat.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAccentOwner::OnFloatFocusChanged
//
//  A float taking the focus takes the accent. Losing the focus leaves the
//  accent where it is.
//
////////////////////////////////////////////////////////////////////////////////

void FocusAccentOwner::OnFloatFocusChanged (const std::wstring & pane, bool focused)
{
    if (focused)
    {
        m_focusedFloat = pane;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAccentOwner::OnFloatDocked
//
//  A float that docks has no window left to show the accent.
//
////////////////////////////////////////////////////////////////////////////////

void FocusAccentOwner::OnFloatDocked (const std::wstring & pane)
{
    if (m_focusedFloat == pane)
    {
        m_focusedFloat.clear();
    }
}





