#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiThread.h"
#include "Core/DxuiTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetBounds
//
//  Stores the new bounds rect. Notification of bounds changes is the
//  responsibility of subclasses that care (panels relayout via their
//  own Layout() override).
//
////////////////////////////////////////////////////////////////////////////////

void IDxuiControl::SetBounds (RECT boundsDip)
{
    DXUI_ASSERT_UI_THREAD();

    m_boundsDip = boundsDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetVisible
//
//  Toggle visibility with Collapsed semantics: hidden controls take
//  zero layout space. When the visible state changes, give subclasses
//  (and parent panels) a chance to react via OnVisibilityChanged().
//
////////////////////////////////////////////////////////////////////////////////

void IDxuiControl::SetVisible (bool visible)
{
    bool         prior      = false;
    DxuiPanel *  parentPnl  = nullptr;



    DXUI_ASSERT_UI_THREAD();

    prior     = m_visible;
    m_visible = visible;

    if (prior != visible)
    {
        OnVisibilityChanged();
        parentPnl = dynamic_cast<DxuiPanel *> (m_parent);
        if (parentPnl != nullptr)
        {
            parentPnl->OnChildVisibilityChanged (this);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void IDxuiControl::SetEnabled (bool enabled)
{
    DXUI_ASSERT_UI_THREAD();

    m_enabled = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocusable
//
////////////////////////////////////////////////////////////////////////////////

void IDxuiControl::SetFocusable (bool focusable)
{
    DXUI_ASSERT_UI_THREAD();

    m_focusable = focusable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTabIndex
//
////////////////////////////////////////////////////////////////////////////////

void IDxuiControl::SetTabIndex (int tabIndex)
{
    DXUI_ASSERT_UI_THREAD();

    m_tabIndex = tabIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHitAt
//
//  Every point of the bounds of a visible control, relative to its top-left
//  for a control that takes local points.
//
////////////////////////////////////////////////////////////////////////////////

bool IDxuiControl::IsHitAt (POINT pointPx) const
{
    RECT  bounds = m_boundsDip;



    if (!m_visible)
    {
        return false;
    }

    if (UsesLocalPoints())
    {
        OffsetRect (&bounds, -bounds.left, -bounds.top);
    }

    return PtInRect (&bounds, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetTip
//
//  The tip function's tip, or else the control's tip text over its bounds.
//
////////////////////////////////////////////////////////////////////////////////

bool IDxuiControl::TryGetTip (POINT pointPx, DxuiTip & tip) const
{
    RECT  bounds = m_boundsDip;



    if (!IsHitAt (pointPx))
    {
        return false;
    }

    if (TryGetTipFromFn (pointPx, tip))
    {
        return true;
    }

    if (m_tip.empty())
    {
        return false;
    }

    if (UsesLocalPoints())
    {
        OffsetRect (&bounds, -bounds.left, -bounds.top);
    }

    tip        = DxuiTip();
    tip.text   = m_tip;
    tip.anchor = bounds;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetTipFromFn
//
//  The function takes and returns client pixels, so a control that takes
//  local points has the point moved out to the client area and the anchor
//  moved back.
//
////////////////////////////////////////////////////////////////////////////////

bool IDxuiControl::TryGetTipFromFn (POINT pointPx, DxuiTip & tip) const
{
    POINT    clientPx = pointPx;
    POINT    origin   = {};
    DxuiTip  found;



    if (m_tipFn == nullptr)
    {
        return false;
    }

    if (UsesLocalPoints())
    {
        origin = POINT { m_boundsDip.left, m_boundsDip.top };
    }

    clientPx.x += origin.x;
    clientPx.y += origin.y;

    if (!m_tipFn (clientPx, found))
    {
        return false;
    }

    OffsetRect (&found.anchor, -origin.x, -origin.y);
    tip = std::move (found);
    return true;
}
