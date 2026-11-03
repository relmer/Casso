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
//  TryGetTip
//
//  The control's tip text over its bounds, relative to its top-left for a
//  control that takes local points.
//
////////////////////////////////////////////////////////////////////////////////

bool IDxuiControl::TryGetTip (POINT pointPx, DxuiTip & tip) const
{
    RECT  bounds = m_boundsDip;



    if (m_tip.empty() || !m_visible)
    {
        return false;
    }

    if (UsesLocalPoints())
    {
        OffsetRect (&bounds, -bounds.left, -bounds.top);
    }

    if (PtInRect (&bounds, pointPx) == FALSE)
    {
        return false;
    }

    tip        = DxuiTip();
    tip.text   = m_tip;
    tip.anchor = bounds;
    return true;
}
