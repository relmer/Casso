#include "Pch.h"

#include "CreateDiskBodyPanel.h"

#include "Core/DxuiEvents.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Init
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskBodyPanel::Init (
    DxuiLabel     * pathLabel,
    DxuiListView  * list,
    DxuiLabel     * nameLabel,
    DxuiTextInput * nameInput)
{
    m_pathLabel = pathLabel;
    m_list      = list;
    m_nameLabel = nameLabel;
    m_nameInput = nameInput;

    Adopt (*pathLabel);
    Adopt (*list);
    Adopt (*nameLabel);
    Adopt (*nameInput);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Path strip on top, name strip on the bottom, the listing fills whatever
//  is left between them.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskBodyPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  pathH    = scaler.Px (kPathHeightDip);
    int  pathGap  = scaler.Px (kPathGapDip);
    int  nameH    = scaler.Px (kNameRowDip);
    int  nameGap  = scaler.Px (kNameGapDip);
    int  labelW   = scaler.Px (kNameLabelDip);
    int  labelPad = scaler.Px (kNameLabelPadDip);
    int  nameTop  = 0;



    SetBounds (boundsPx);

    nameTop = boundsPx.bottom - nameH;

    if (m_pathLabel != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + pathH };

        m_pathLabel->Layout (r, scaler);
    }

    if (m_list != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top + pathH + pathGap,
                    boundsPx.right, nameTop - nameGap };

        m_list->Layout (r, scaler);
    }

    if (m_nameLabel != nullptr)
    {
        RECT  r = { boundsPx.left, nameTop, boundsPx.left + labelW, boundsPx.bottom };

        m_nameLabel->Layout (r, scaler);
    }

    if (m_nameInput != nullptr)
    {
        RECT  r = { boundsPx.left + labelW + labelPad, nameTop,
                    boundsPx.right, boundsPx.bottom };

        m_nameInput->Layout (r, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  The list consumes any press inside itself (scroll / select / column
//  resize) and needs local coordinates; anything it declines falls through
//  to the default panel fan-out, which serves the text input.
//
////////////////////////////////////////////////////////////////////////////////

bool CreateDiskBodyPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  listEv  = ev;
    bool            handled = false;



    if (m_list != nullptr)
    {
        RECT  lb = m_list->Bounds();

        listEv.positionDip = { ev.positionDip.x - lb.left, ev.positionDip.y - lb.top };
        handled            = m_list->OnMouse (listEv);
    }

    if (!handled && m_nameInput != nullptr)
    {
        handled = m_nameInput->OnMouse (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CreateDiskBodyPanel::CursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;



    if (m_list != nullptr)
    {
        RECT   lb    = m_list->Bounds();
        POINT  local = { clientPx.x - lb.left, clientPx.y - lb.top };

        cursor = m_list->CursorForPoint (local);
    }

    return cursor;
}
