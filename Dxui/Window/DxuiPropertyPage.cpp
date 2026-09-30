#include "Pch.h"

#include "DxuiPropertyPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDirty
//
//  Records the dirty state and, on a change, notifies the owning sheet so
//  it can re-evaluate the Apply button.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertyPage::MarkDirty (bool dirty)
{
    bool  changed = (dirty != m_dirty);



    m_dirty = dirty;

    if (changed && m_onDirtyChanged)
    {
        m_onDirtyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetViewport
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertyPage::SetViewport (const RECT * viewportPx)
{
    m_hasViewport = (viewportPx != nullptr);
    m_viewportPx  = m_hasViewport ? *viewportPx : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPointClipped
//
//  Outside the viewport the page's controls are laid out but not drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertyPage::IsPointClipped (POINT clientPx) const
{
    return m_hasViewport && PtInRect (&m_viewportPx, clientPx) == FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetContentHeightPx
//
//  Records the height and, on a change, notifies the owning sheet so it can
//  recompute the scroll range.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertyPage::SetContentHeightPx (int heightPx)
{
    bool  changed = (heightPx != m_contentHeightPx);



    m_contentHeightPx = heightPx;

    if (changed && m_onContentHeightChanged)
    {
        m_onContentHeightChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestReveal
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertyPage::RequestReveal (const RECT & rectPx)
{
    if (m_onRevealRequested)
    {
        m_onRevealRequested (rectPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLowestChildBottomPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertyPage::GetLowestChildBottomPx() const
{
    const IDxuiControl  * child  = nullptr;
    int                   lowest = 0;
    size_t                i      = 0;



    for (i = 0; i < GetChildCount(); ++i)
    {
        child = GetChild (i);

        if (child != nullptr && child->IsVisible())
        {
            lowest = std::max (lowest, (int) child->GetBounds().bottom);
        }
    }

    return lowest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRightmostChildEdgePx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertyPage::GetRightmostChildEdgePx() const
{
    const IDxuiControl  * child = nullptr;
    int                   right = 0;
    size_t                i     = 0;



    for (i = 0; i < GetChildCount(); ++i)
    {
        child = GetChild (i);

        if (child != nullptr && child->IsVisible())
        {
            right = std::max (right, (int) child->GetBounds().right);
        }
    }

    return right;
}
