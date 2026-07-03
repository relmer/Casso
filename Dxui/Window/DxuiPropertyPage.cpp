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
