#include "Pch.h"

#include "Widgets/ListView/DxuiListBand.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::DxuiListBand
//
////////////////////////////////////////////////////////////////////////////////

DxuiListBand::DxuiListBand (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::Begin
//
//  Without Ctrl the band starts from nothing; with it, the band adds to the
//  selection the press found.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListBand::Begin (int lx, int ly, bool ctrl)
{
    POINT  offset = m_list.m_items.GetItemScrollOffsetPx();



    m_active = true;
    m_start         = POINT { lx + offset.x, ly + offset.y };
    m_end           = m_start;
    m_base   = ctrl ? m_list.m_selectedRows : std::vector<int>();

    if (!ctrl)
    {
        m_list.ClearSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::Update
//
//  Every item the band touches is selected, with what Ctrl kept.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListBand::Update (int lx, int ly)
{
    POINT                                  offset  = m_list.m_items.GetItemScrollOffsetPx();
    RECT                                   band    = {};
    std::vector<int>                       rows    = m_base;
    std::vector<int>                       before  = m_list.m_selectedRows;
    int                                    last    = -1;
    bool                                   listGrp = m_list.m_flow.HasListGroups();
    bool                                   itemGrp = !listGrp && m_list.m_items.UsesItemLayout();
    const DxuiListViewTypes::ListLayout  & listLay = listGrp ? m_list.m_flow.GetListLayout() : DxuiListView::s_kNoListLayout;
    const DxuiListViewTypes::ItemLayout  & itemLay = itemGrp ? m_list.m_items.GetItemLayout() : DxuiListItemLayout::s_kNoItemLayout;



    m_end = POINT { lx + offset.x, ly + offset.y };

    band = RECT { (std::min) (m_start.x, m_end.x), (std::min) (m_start.y, m_end.y),
                  (std::max) (m_start.x, m_end.x), (std::max) (m_start.y, m_end.y) };

    for (int item = 0; item < m_list.GetRowCount(); item++)
    {
        RECT  cell   = {};
        RECT  hit    = {};
        int   line   = m_list.IsItemsView() ? -1 : m_list.m_grouping.GetLineOfRow (item);
        bool  placed = false;

        //  In Details a row spans every column, at its line's place.
        if (!m_list.IsItemsView())
        {
            placed = line >= 0;
            cell   = RECT { 0, line * m_list.GetRowHeightPx(), m_list.m_columnSizing.GetContentWidthPx(), (line + 1) * m_list.GetRowHeightPx() };
        }
        else
        {
            placed = listGrp   ? m_list.m_flow.GetListItemRectPx         (listLay, item, cell)
                   : itemGrp ? m_list.m_items.GetGroupedItemRectPx (itemLay, item, cell)
                             : m_list.m_items.GetItemRectPx        (item, cell);

            if (placed)
            {
                OffsetRect (&cell, offset.x, offset.y);
            }
        }

        if (!placed)
        {
            continue;
        }

        if (IntersectRect (&hit, &cell, &band))
        {
            rows.push_back (item);
            last = item;
        }
    }

    m_list.SetSelectedRows (std::move (rows), last);

    //  A band shrunk back off every item clears the selection, and that is a
    //  change too.
    if (m_list.m_onSelectionChanged && (last >= 0 || m_list.m_selectedRows != before))
    {
        m_list.m_onSelectionChanged (last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::OnDrag
//
//  Returns whether a band was being drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListBand::OnDrag (int lx, int ly)
{
    if (!m_active)
    {
        return false;
    }

    Update (lx, ly);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::OnRelease
//
//  The band goes, and what it selected stays selected. Returns whether a
//  band was being drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListBand::OnRelease()
{
    if (!m_active)
    {
        return false;
    }

    m_active = false;
    m_base.clear();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::GetRectPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListBand::GetRectPx() const
{
    POINT  offset = m_list.m_items.GetItemScrollOffsetPx();



    if (!m_active)
    {
        return RECT {};
    }

    return RECT { (std::min) (m_start.x, m_end.x) - offset.x, (std::min) (m_start.y, m_end.y) - offset.y,
                  (std::max) (m_start.x, m_end.x) - offset.x, (std::max) (m_start.y, m_end.y) - offset.y };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand::Paint
//
//  The rubber band, over everything it selects.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListBand::Paint (IDxuiPainter & painter, const DxuiListViewTypes::Palette & pal, float x, float y) const
{
    RECT      band      = GetRectPx();
    COLORREF  highlight = GetSysColor (COLOR_HIGHLIGHT);
    uint32_t  rgb       = ((uint32_t) GetRValue (highlight) << 16) | ((uint32_t) GetGValue (highlight) << 8) | GetBValue (highlight);



    UNREFERENCED_PARAMETER (pal);

    if (!m_active)
    {
        return;
    }

    OffsetRect (&band, (int) x, (int) y);

    //  Explorer's band is the system highlight, #0078D7 by default: a solid
    //  edge over a fill of about a third, measured.
    painter.FillRect    ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                         s_kFillAlpha | rgb);
    painter.OutlineRect ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                         1.0f, 0xFF000000u | rgb);
}
