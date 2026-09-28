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
