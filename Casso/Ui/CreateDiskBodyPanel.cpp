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
    Adopt (*children.imageTypeLabel);
    Adopt (*children.imageType);
    Adopt (*children.bootable);
    Adopt (*children.download);
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
    int  pathH    = scaler.ToPx (kPathHeightDip);
    int  pathGap  = scaler.ToPx (kPathGapDip);
    int  optH     = scaler.ToPx (kOptionsRowDip);
    int  optGap   = scaler.ToPx (kOptionsGapDip);
    int  optPad   = scaler.ToPx (kOptionLabelPadDip);
    int  bootH    = scaler.ToPx (kBootRowDip);
    int  nameH    = scaler.ToPx (kNameRowDip);
    int  nameGap  = scaler.ToPx (kNameGapDip);
    int  labelW   = scaler.ToPx (kNameLabelDip);
    int  labelPad = scaler.ToPx (kNameLabelPadDip);
    int  nameTop  = 0;
    int  bootTop  = 0;
    int  optTop   = 0;
    int  x        = 0;



    SetBounds (boundsPx);
    m_lastScaler = scaler;

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
        RECT  r = { x, optTop, x + scaler.ToPx (kFormatLabelDip), optTop + optH };

        m_kids.formatLabel->Layout (r, scaler);
        x = r.right + optPad;
    }

    if (m_kids.format != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.ToPx (kFormatDropDip), optTop + optH };

        m_kids.format->Layout (r, scaler);
        x = r.right + optGap * 2;
    }

    if (m_kids.imageTypeLabel != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.ToPx (kImageTypeLabelDip), optTop + optH };

        m_kids.imageTypeLabel->Layout (r, scaler);
        x = r.right + optPad;
    }

    if (m_kids.imageType != nullptr)
    {
        RECT  r = { x, optTop, x + scaler.ToPx (kImageTypeDropDip), optTop + optH };

        m_kids.imageType->Layout (r, scaler);
    }

    x = boundsPx.left;

    if (m_kids.bootable != nullptr)
    {
        // With the download button hidden the checkbox owns the whole strip
        // (its label carries the explanation); with the button visible the
        // checkbox keeps its short-label width and the button follows.
        bool  haveButton = (m_kids.download != nullptr && m_kids.download->IsVisible());
        int   checkW     = haveButton ? scaler.ToPx (kBootCheckDip)
                                      : (boundsPx.right - x);
        RECT  r          = { x, bootTop, x + checkW, bootTop + bootH };

        m_kids.bootable->Layout (r, scaler);

        if (haveButton)
        {
            x = r.right + optGap * 2;
        }
    }

    if (m_kids.download != nullptr)
    {
        int   buttonW = m_kids.download->IsVisible() ? scaler.ToPx (kBootButtonDip) : 0;
        RECT  r       = { x, bootTop, x + buttonW, bootTop + bootH };

        m_kids.download->Layout (r, scaler);
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
//  Relayout
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskBodyPanel::Relayout()
{
    Layout (GetBounds(), m_lastScaler);
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
    IDxuiControl *  pressed = nullptr;
    bool            handled = false;



    if (m_kids.format != nullptr)
    {
        handled = m_kids.format->OnMouse (ev);
        pressed = handled ? m_kids.format : nullptr;
    }

    if (!handled && m_kids.imageType != nullptr)
    {
        handled = m_kids.imageType->OnMouse (ev);
        pressed = handled ? m_kids.imageType : nullptr;
    }

    if (!handled && m_kids.list != nullptr)
    {
        RECT  lb = m_kids.list->GetBounds();

        listEv.positionDip = { ev.positionDip.x - lb.left, ev.positionDip.y - lb.top };
        handled            = m_kids.list->OnMouse (listEv);
        pressed            = handled ? m_kids.list : nullptr;
    }

    if (!handled && m_kids.bootable != nullptr)
    {
        handled = m_kids.bootable->OnMouse (ev);
        pressed = handled ? m_kids.bootable : nullptr;
    }

    if (!handled && m_kids.download != nullptr)
    {
        handled = m_kids.download->OnMouse (ev);
        pressed = handled ? m_kids.download : nullptr;
    }

    if (!handled && m_kids.nameInput != nullptr)
    {
        handled = m_kids.nameInput->OnMouse (ev);
        pressed = handled ? m_kids.nameInput : nullptr;
    }

    // Click-to-focus: a press that a child consumed moves keyboard focus
    // there, so the name field drops its caret when the list (or any other
    // control) is clicked and the list picks up arrow-key navigation.
    if (ev.kind == DxuiMouseEventKind::Down && pressed != nullptr && m_onChildPressed)
    {
        m_onChildPressed (pressed);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CreateDiskBodyPanel::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;



    if (m_kids.list != nullptr)
    {
        RECT   lb    = m_kids.list->GetBounds();
        POINT  local = { clientPx.x - lb.left, clientPx.y - lb.top };

        cursor = m_kids.list->GetCursorForPoint (local);
    }

    return cursor;
}
