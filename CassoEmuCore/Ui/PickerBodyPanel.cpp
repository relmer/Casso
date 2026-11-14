#include "Pch.h"

#include "PickerBodyPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Init
//
////////////////////////////////////////////////////////////////////////////////

void PickerBodyPanel::Init (DxuiSearchBox * search, DxuiListView * list, int searchHeightDip, int gapDip)
{
    m_search          = search;
    m_list            = list;
    m_searchHeightDip = searchHeightDip;
    m_gapDip          = gapDip;

    Adopt (*search);
    Adopt (*list);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void PickerBodyPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  sh  = scaler.ToPx (m_searchHeightDip);
    int  gap = scaler.ToPx (m_gapDip);



    SetBounds (boundsPx);

    if (m_search != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + sh };

        m_search->Layout (r, scaler);
    }

    if (m_list != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top + sh + gap, boundsPx.right, boundsPx.bottom };

        m_list->Layout (r, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool PickerBodyPanel::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    if (m_list != nullptr)
    {
        handled = m_list->AsControl().OnMouse (ev);
    }

    if (!handled && m_search != nullptr)
    {
        handled = m_search->OnMouse (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PickerBodyPanel::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;



    if (m_list != nullptr)
    {
        cursor = m_list->AsControl().GetCursorForPoint (clientPx);
    }

    return cursor;
}
