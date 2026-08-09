#include "Pch.h"

#include "CreateDiskBodyPanel.h"

#include "Core/DxuiEvents.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Init
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskBodyPanel::Init (const Children & children)
{
    m_kids = children;

    Adopt (*children.pathLabel);
    Adopt (*children.list);
    Adopt (*children.formatLabel);
    Adopt (*children.format);
    Adopt (*children.contentsLabel);
    Adopt (*children.contents);
    Adopt (*children.bootable);
    Adopt (*children.download);
    Adopt (*children.bootHint);
    Adopt (*children.nameLabel);
    Adopt (*children.nameInput);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Path strip on top; the options, bootable, and name strips stacked at the
//  bottom; the listing fills whatever is left between them.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskBodyPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  pathH    = scaler.Px (kPathHeightDip);
    int  pathGap  = scaler.Px (kPathGapDip);
    int  optH     = scaler.Px (kOptionsRowDip);
    int  optGap   = scaler.Px (kOptionsGapDip);
    int  optPad   = scaler.Px (kOptionLabelPadDip);
    int  bootH    = scaler.Px (kBootRowDip);
    int  nameH    = scaler.Px (kNameRowDip);
    int  nameGap  = scaler.Px (kNameGapDip);
    int  labelW   = scaler.Px (kNameLabelDip);
    int  labelPad = scaler.Px (kNameLabelPadDip);
    int  nameTop  = 0;
    int  bootTop  = 0;
    int  optTop   = 0;
    int  x        = 0;



    SetBounds (boundsPx);

    nameTop = boundsPx.bottom - nameH;
    bootTop = nameTop - nameGap - bootH;
    optTop  = bootTop - optGap - optH;

    if (m_kids.pathLabel != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + pathH };

        m_kids.pathLabel->Layout (r, scaler);
    }

    if (m_kids.list != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top + pathH + pathGap,
                    boundsPx.right, optTop - optGap };

        m_kids.list->Layout (r, scaler);
    }

    x = boundsPx.left;

    if (m_kids.formatLabel != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.Px (kFormatLabelDip), optTop + optH };

        m_kids.formatLabel->Layout (r, scaler);
        x = r.right + optPad;
    }

    if (m_kids.format != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.Px (kFormatDropDip), optTop + optH };

        m_kids.format->Layout (r, scaler);
        x = r.right + optGap * 2;
    }

    if (m_kids.contentsLabel != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.Px (kContentsLabelDip), optTop + optH };

        m_kids.contentsLabel->Layout (r, scaler);
        x = r.right + optPad;
    }

    if (m_kids.contents != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.Px (kContentsDropDip), optTop + optH };

        m_kids.contents->Layout (r, scaler);
    }

    x = boundsPx.left;

    if (m_kids.bootable != nullptr)
    {
        RECT  r = { x, bootTop, x + scaler.Px (kBootCheckDip), bootTop + bootH };

        m_kids.bootable->Layout (r, scaler);
        x = r.right + optGap * 2;
    }

    if (m_kids.download != nullptr)
    {
        RECT  r = { x, bootTop, x + scaler.Px (kBootButtonDip), bootTop + bootH };

        m_kids.download->Layout (r, scaler);
        x = r.right + optGap * 2;
    }

    if (m_kids.bootHint != nullptr)
    {
        RECT  r = { x, bootTop, boundsPx.right, bootTop + bootH };

        m_kids.bootHint->Layout (r, scaler);
    }

    if (m_kids.nameLabel != nullptr)
    {
        RECT  r = { boundsPx.left, nameTop, boundsPx.left + labelW, boundsPx.bottom };

        m_kids.nameLabel->Layout (r, scaler);
    }

    if (m_kids.nameInput != nullptr)
    {
        RECT  r = { boundsPx.left + labelW + labelPad, nameTop,
                    boundsPx.right, boundsPx.bottom };

        m_kids.nameInput->Layout (r, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  Dropdowns come first: an open menu must see every click, wherever it
//  lands. The list consumes any press inside itself (scroll / select /
//  column resize) and needs local coordinates; anything left over falls
//  through to the bootable strip and the text input.
//
////////////////////////////////////////////////////////////////////////////////

bool CreateDiskBodyPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  listEv  = ev;
    bool            handled = false;



    if (m_kids.format != nullptr)
    {
        handled = m_kids.format->OnMouse (ev);
    }

    if (!handled && m_kids.contents != nullptr)
    {
        handled = m_kids.contents->OnMouse (ev);
    }

    if (!handled && m_kids.list != nullptr)
    {
        RECT  lb = m_kids.list->Bounds();

        listEv.positionDip = { ev.positionDip.x - lb.left, ev.positionDip.y - lb.top };
        handled            = m_kids.list->OnMouse (listEv);
    }

    if (!handled && m_kids.bootable != nullptr)
    {
        handled = m_kids.bootable->OnMouse (ev);
    }

    if (!handled && m_kids.download != nullptr)
    {
        handled = m_kids.download->OnMouse (ev);
    }

    if (!handled && m_kids.nameInput != nullptr)
    {
        handled = m_kids.nameInput->OnMouse (ev);
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



    if (m_kids.list != nullptr)
    {
        RECT   lb    = m_kids.list->Bounds();
        POINT  local = { clientPx.x - lb.left, clientPx.y - lb.top };

        cursor = m_kids.list->CursorForPoint (local);
    }

    return cursor;
}
