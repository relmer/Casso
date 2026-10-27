#include "Pch.h"

#include "Widgets/ListView/DxuiListItemKeys.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::DxuiListItemKeys
//
////////////////////////////////////////////////////////////////////////////////

DxuiListItemKeys::DxuiListItemKeys (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardGroupedItemNav
//
//  As the grid lies on screen, with each group's header a stop between its
//  rows and the group above, as in Details. Left and Right step along the
//  items in order, or close and open a focused header. The page keys, Home
//  and End go to HandleKeyboardItemPage and HandleKeyboardItemJump.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardGroupedItemNav (WPARAM vk, bool shift, bool ctrl)
{
    DxuiListViewTypes::ItemLayout  layout = m_list.m_items.GetItemLayout();
    int                            lines  = (int) layout.lines.size();
    int                            cur    = m_list.GetSelectedRow();
    int                            line   = -1;
    int                            slot   = 0;
    int                            next   = -1;
    int                            item   = -1;



    if (lines <= 0)
    {
        return false;
    }

    if (m_list.m_focusGroup >= 0)
    {
        for (int l = 0; l < lines; l++)
        {
            if (layout.lines[(size_t) l].group == m_list.m_focusGroup)
            {
                line = l;
            }
        }
    }
    else if (cur >= 0 && cur < (int) layout.rowLine.size())
    {
        line = layout.rowLine[(size_t) cur];
        slot = (line >= 0) ? cur - layout.lines[(size_t) line].first : 0;
    }

    if (m_list.m_focusGroup >= 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        m_list.m_grouping.SetGroupCollapsed (m_list.m_focusGroup, vk == VK_LEFT);
        return true;
    }

    if (vk == VK_PRIOR || vk == VK_NEXT)
    {
        return HandleKeyboardItemPage (layout, vk, line, slot, shift, ctrl);
    }

    if (vk == VK_HOME || vk == VK_END)
    {
        return HandleKeyboardItemJump (layout, vk, shift, ctrl);
    }

    switch (vk)
    {
        case VK_UP:    next = (line < 0) ? 0 : line - 1; break;
        case VK_DOWN:  next = (line < 0) ? 0 : line + 1; break;

        case VK_LEFT:
        case VK_RIGHT:
            //  Along the items in order, past a collapsed group's.
            item = cur;

            do
            {
                item += (vk == VK_RIGHT) ? 1 : -1;
            }
            while (item >= 0 && item < (int) layout.rowLine.size() && layout.rowLine[(size_t) item] < 0);

            if (item < 0 || item >= (int) layout.rowLine.size())
            {
                return true;
            }

            next = layout.rowLine[(size_t) item];
            slot = item - layout.lines[(size_t) next].first;
            break;

        default:
            return false;
    }

    next = std::clamp (next, 0, lines - 1);

    if (layout.lines[(size_t) next].group < 0)
    {
        item = layout.lines[(size_t) next].first + (std::min) (slot, layout.lines[(size_t) next].count - 1);
    }

    m_list.m_items.EnsureItemLineVisible (layout, next);

    //  Ctrl moves the focus alone, leaving the selection for Space.
    if (m_list.m_multiSelect && ctrl && !shift)
    {
        m_list.m_focusGroup  = (item < 0) ? layout.lines[(size_t) next].group : -1;
        m_list.m_selectedRow = (item < 0) ? m_list.m_selectedRow : item;
        return true;
    }

    if (item < 0)
    {
        m_list.m_grouping.SelectGroup (layout.lines[(size_t) next].group);
        return true;
    }

    m_list.m_focusGroup = -1;

    if (m_list.m_multiSelect && shift)
    {
        m_list.SelectRangeFromAnchor (item);
    }
    else
    {
        m_list.SetSelectedRow (item);
    }

    if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
    {
        m_list.m_onSelectionChanged (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardItemPage
//
//  Page Up and Page Down in a view that scrolls down, as Explorer's, measured
//  at 150%: as Details pages its rows, over the lines of items, the focus
//  keeping its place along the line, or the last place a shorter line has. A
//  focused header pages from its line at the first place, as an item under a
//  collapsed header does from that header's line.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardItemPage (const DxuiListViewTypes::ItemLayout & layout, WPARAM vk, int line, int slot, bool shift, bool ctrl)
{
    bool  down        = (vk == VK_NEXT);
    int   rows        = m_list.GetRowCount();
    int   page        = m_list.m_keyboard.GetPageLines();
    int   cur         = line;
    int   group       = -1;
    int   first       = -1;
    int   last        = -1;
    int   target      = -1;
    int   item        = -1;
    int   savedTop    = m_list.m_topRow;
    int   savedAnchor = m_list.m_itemAnchorLine;
    bool  savedBottom = m_list.m_itemAnchorBottom;
    bool  scroll      = false;



    if (!TryGetWholeItemLineSpan (layout, first, last))
    {
        return false;
    }

    if (cur < 0 && m_list.m_focusGroup < 0 && m_list.m_selectedRow >= 0 && m_list.m_selectedRow < rows)
    {
        group = m_list.m_grouping.GetGroupOfRow (m_list.m_selectedRow);

        for (size_t at = 0; at < layout.lines.size() && group >= 0 && cur < 0; at++)
        {
            cur = (layout.lines[at].group == group) ? (int) at : -1;
        }
    }

    target = DxuiListView::IsAtPageEdge (cur, down ? last : first, down) ? StepItemLines (layout, cur, down ? page : -page)
                                                            : (down ? last : first);

    //  A header with no items past it in the key's direction.
    if (target < 0)
    {
        return true;
    }

    item   = layout.lines[(size_t) target].first + (std::min) (slot, layout.lines[(size_t) target].count - 1);
    scroll = !IsItemLineWhollyShown (layout, target);

    m_list.m_keyboard.ApplyKeyboardMove (item, shift, ctrl);

    if (!scroll)
    {
        m_list.m_scrollbars.SetTopRow (savedTop);
        m_list.m_itemAnchorLine   = savedAnchor;
        m_list.m_itemAnchorBottom = savedBottom;
    }
    else if (down)
    {
        ScrollItemLineToBottom (target);
    }
    else
    {
        ScrollItemLineToTop (target);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardItemJump
//
//  Home selects the first item, past any header over it, with the view at
//  its very top; End selects the last, with the view at its very end. With
//  every group collapsed there is no item, and the first or last header
//  takes the focus instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardItemJump (const DxuiListViewTypes::ItemLayout & layout, WPARAM vk, bool shift, bool ctrl)
{
    bool  home   = (vk == VK_HOME);
    int   lines  = (int) layout.lines.size();
    int   last   = GetLastItemLine (layout);
    int   target = home ? DxuiListItemLayout::GetFirstItemLine (layout) : last;
    int   group  = -1;
    int   item   = -1;



    if (last < 0)
    {
        group = layout.lines[(size_t) (home ? 0 : lines - 1)].group;

        if (m_list.m_multiSelect && ctrl && !shift)
        {
            m_list.m_focusGroup = group;
        }
        else
        {
            m_list.m_grouping.SelectGroup (group);
        }

        m_list.m_scrollbars.SetTopRow (home ? 0 : m_list.m_scrollbars.GetMaxTopRow());
        return true;
    }

    item = layout.lines[(size_t) target].first + (home ? 0 : layout.lines[(size_t) target].count - 1);

    m_list.m_keyboard.ApplyKeyboardMove (item, shift, ctrl);

    if (home)
    {
        m_list.m_scrollbars.SetTopRow (0);
    }
    else
    {
        ScrollItemLineToBottom (target);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::GetLastItemLine
//
//  The last line of items in a layout, past the headers under it; -1 when
//  there are no items.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemKeys::GetLastItemLine (const DxuiListViewTypes::ItemLayout & layout)
{
    for (int at = (int) layout.lines.size() - 1; at >= 0; at--)
    {
        if (DxuiListView::IsItemLine (layout.lines[(size_t) at]))
        {
            return at;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::StepItemLines
//
//  The line of items `steps` such lines on from a line, down for a positive
//  count: headers are not counted, and the count stops at the first or the
//  last. A line of items with none past it gives itself; a header with none
//  past it gives -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemKeys::StepItemLines (const DxuiListViewTypes::ItemLayout & layout, int line, int steps)
{
    int  lines  = (int) layout.lines.size();
    int  dir    = (steps < 0) ? -1 : 1;
    int  left   = (steps < 0) ? -steps : steps;
    int  result = (line >= 0 && line < lines && DxuiListView::IsItemLine (layout.lines[(size_t) line])) ? line : -1;
    int  at     = line;



    while (left > 0)
    {
        at += dir;

        if (at < 0 || at >= lines)
        {
            break;
        }

        if (DxuiListView::IsItemLine (layout.lines[(size_t) at]))
        {
            result = at;
            left--;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::TryGetWholeItemLineSpan
//
//  The first and last lines of items shown whole. With none whole in sight,
//  as when a line is taller than the view, the line of items nearest the top
//  stands for both. False only when there are no items to show.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::TryGetWholeItemLineSpan (const DxuiListViewTypes::ItemLayout & layout, int & first, int & last) const
{
    int  topPx   = m_list.m_items.GetItemTopPx (layout);
    int  viewH   = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int  lines   = (int) layout.lines.size();
    int  start   = -1;
    int  nearest = -1;



    first = -1;
    last  = -1;

    for (int at = 0; at < lines && layout.lines[(size_t) at].top < topPx + viewH; at++)
    {
        const DxuiListViewTypes::ItemLine & line = layout.lines[(size_t) at];

        if (DxuiListView::IsItemLine (line) && line.top >= topPx && line.top + line.height <= topPx + viewH)
        {
            first = (first < 0) ? at : first;
            last  = at;
        }
    }

    if (first >= 0 || lines <= 0)
    {
        return first >= 0;
    }

    start   = DxuiListItemLayout::FindItemLine (layout, topPx);
    start   = (start >= 0) ? start : ((topPx < layout.lines[0].top) ? 0 : lines - 1);
    nearest = DxuiListView::IsItemLine (layout.lines[(size_t) start]) ? start : StepItemLines (layout, start, 1);
    nearest = (nearest >= 0) ? nearest : StepItemLines (layout, start, -1);
    first   = nearest;
    last    = nearest;

    return nearest >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::IsItemLineWhollyShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::IsItemLineWhollyShown (const DxuiListViewTypes::ItemLayout & layout, int line) const
{
    int  topPx = m_list.m_items.GetItemTopPx (layout);
    int  viewH = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;



    if (line < 0 || line >= (int) layout.lines.size())
    {
        return false;
    }

    return layout.lines[(size_t) line].top >= topPx && layout.lines[(size_t) line].top + layout.lines[(size_t) line].height <= topPx + viewH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::AnchorItemLine
//
//  Holds the view with a line's top at the view's top, or its bottom at the
//  view's bottom. The top row follows to the line at the margin's height, so
//  the scrollbar, the wheel and the arrows continue from about here, and a
//  wheel notch still moves the way it turns.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemKeys::AnchorItemLine (int line, bool bottom)
{
    const DxuiListViewTypes::ItemLayout  & layout = m_list.m_items.GetItemLayout();
    int                                    px     = 0;
    int                                    top    = 0;



    if (line < 0 || line >= (int) layout.lines.size())
    {
        return;
    }

    //  A group mid-slide is laid out with it open, where the line is another.
    m_list.m_groupSlide       = {};
    m_list.m_itemAnchorLine   = line;
    m_list.m_itemAnchorBottom = bottom;
    px                        = m_list.m_items.GetItemTopPx (layout);

    if (px <= 0)
    {
        m_list.m_scrollbars.SetTopRow (0);
        return;
    }

    top = DxuiListItemLayout::FindItemLine (layout, px + layout.lines[0].top);

    //  The top row is set first, since setting it lets go of the line.
    m_list.m_scrollbars.SetTopRow ((top >= 0) ? top : (int) layout.lines.size() - 1);

    m_list.m_itemAnchorLine   = line;
    m_list.m_itemAnchorBottom = bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::ScrollItemLineToBottom
//
//  Holds a line flush with the view's bottom; the last line of items holds
//  the very end instead, so the headers of collapsed groups under it show.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemKeys::ScrollItemLineToBottom (int line)
{
    const DxuiListViewTypes::ItemLayout & layout = m_list.m_items.GetItemLayout();



    if (line == GetLastItemLine (layout))
    {
        AnchorItemLine ((int) layout.lines.size() - 1, true);

        if (IsItemLineWhollyShown (m_list.m_items.GetItemLayout(), line))
        {
            return;
        }
    }

    AnchorItemLine (line, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::ScrollItemLineToTop
//
//  Holds a line flush with the view's top; the first line of items scrolls
//  to the very top instead, so its margin and any header over it show.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemKeys::ScrollItemLineToTop (int line)
{
    if (line == DxuiListItemLayout::GetFirstItemLine (m_list.m_items.GetItemLayout()))
    {
        m_list.m_scrollbars.SetTopRow (0);
        return;
    }

    AnchorItemLine (line, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardListGroupNav
//
//  Up and Down step along the items in order, with each group's header a stop
//  ahead of its first; Left and Right move a column, keeping the row, or
//  close and open a focused header.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardListGroupNav (WPARAM vk, bool shift, bool ctrl)
{
    DxuiListViewTypes::ListLayout  layout = m_list.m_flow.GetListLayout();
    int                            rows   = m_list.GetRowCount();
    int                            cur    = m_list.GetSelectedRow();
    int                            item   = -1;
    int                            group  = -1;
    int                            at     = (cur >= 0 && cur < rows) ? layout.blockOf[(size_t) cur] : -1;
    int                            k      = (at >= 0) ? cur - layout.blocks[(size_t) at].first : 0;



    if (rows <= 0 || layout.blocks.empty())
    {
        return false;
    }

    if (m_list.m_focusGroup >= 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        return true;
    }

    switch (vk)
    {
        case VK_UP:
            if (m_list.m_focusGroup >= 0)
            {
                //  From a header to the last item shown above it.
                for (int b = m_list.m_focusGroup - 1; b >= 0 && item < 0; b--)
                {
                    item = (layout.blocks[(size_t) b].count > 0) ? layout.blocks[(size_t) b].first + layout.blocks[(size_t) b].count - 1 : -1;
                    group = (item < 0) ? b : -1;

                    if (group >= 0)
                    {
                        break;
                    }
                }
            }
            else if (at >= 0 && k == 0)
            {
                group = layout.blocks[(size_t) at].group;
            }
            else
            {
                item = cur - 1;
            }

            break;

        case VK_DOWN:
            if (m_list.m_focusGroup >= 0)
            {
                item  = (layout.blocks[(size_t) m_list.m_focusGroup].count > 0) ? layout.blocks[(size_t) m_list.m_focusGroup].first : -1;
                group = (item < 0 && m_list.m_focusGroup + 1 < (int) layout.blocks.size()) ? m_list.m_focusGroup + 1 : -1;
            }
            else if (at >= 0 && k + 1 >= layout.blocks[(size_t) at].count)
            {
                group = (at + 1 < (int) layout.blocks.size()) ? at + 1 : -1;
                item  = (group < 0) ? cur : -1;
            }
            else
            {
                item = cur + 1;
            }

            break;

        case VK_LEFT:
        case VK_RIGHT:
            if (at < 0)
            {
                return true;
            }

            k += (vk == VK_RIGHT) ? layout.perCol : -layout.perCol;
            item = (k >= 0 && k < layout.blocks[(size_t) at].count) ? layout.blocks[(size_t) at].first + k : cur;
            break;

        default:
            return false;
    }

    if (group >= 0)
    {
        if (m_list.m_multiSelect && ctrl && !shift)
        {
            m_list.m_focusGroup = group;
        }
        else
        {
            m_list.m_grouping.SelectGroup (group);
        }

        m_list.m_scrollbars.SetLeftPx ((std::min) (m_list.m_leftPx, layout.blocks[(size_t) group].left));
        return true;
    }

    if (item < 0 || item >= rows || layout.blockOf[(size_t) item] < 0)
    {
        return true;
    }

    m_list.m_focusGroup = -1;

    if (m_list.m_multiSelect && ctrl && !shift)
    {
        m_list.m_selectedRow = item;
    }
    else if (m_list.m_multiSelect && shift)
    {
        m_list.SelectRangeFromAnchor (item);
    }
    else
    {
        m_list.SetSelectedRow (item);
    }

    m_list.m_items.EnsureItemVisible (item);

    if (m_list.m_multiSelect && m_list.m_onSelectionChanged && !(ctrl && !shift))
    {
        m_list.m_onSelectionChanged (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::TryGetWholeListColumnSpan
//
//  The first and last columns List shows whole. With none whole in sight, as
//  when a name is wider than the view, the column at the view's left stands
//  for both. False only when there are no columns.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::TryGetWholeListColumnSpan (const std::vector<DxuiListViewTypes::ListColumn> & cols, int & first, int & last) const
{
    first = -1;
    last  = -1;

    for (size_t c = 0; c < cols.size(); c++)
    {
        if (IsListColumnWhollyShown (cols[c]))
        {
            first = (first < 0) ? (int) c : first;
            last  = (int) c;
        }
    }

    for (size_t c = 0; c < cols.size() && first < 0; c++)
    {
        first = (cols[c].right > m_list.m_leftPx) ? (int) c : -1;
    }

    first = (first >= 0) ? first : (int) cols.size() - 1;
    last  = (last >= 0) ? last : first;

    return !cols.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::IsListColumnWhollyShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::IsListColumnWhollyShown (const DxuiListViewTypes::ListColumn & col) const
{
    return col.left >= m_list.m_leftPx && col.right <= m_list.m_leftPx + (m_list.m_boundsDip.right - m_list.m_boundsDip.left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::GetListPageStartColumn
//
//  The column a page starts from when the focus is on a group's header, or on
//  an item under a collapsed one: the first column at or past the group's
//  block going right, or the last before it going left; -1 for none.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemKeys::GetListPageStartColumn (const std::vector<DxuiListViewTypes::ListColumn> & cols, bool down) const
{
    int  rows  = m_list.GetRowCount();
    int  group = (m_list.m_focusGroup >= 0) ? m_list.m_focusGroup : ((m_list.m_selectedRow >= 0 && m_list.m_selectedRow < rows) ? m_list.m_grouping.GetGroupOfRow (m_list.m_selectedRow) : -1);
    int  col   = -1;
    int  x     = 0;



    if (group < 0 || !m_list.m_flow.HasListGroups() || group >= (int) m_list.m_flow.GetListLayout().blocks.size())
    {
        return -1;
    }

    x = m_list.m_flow.GetListLayout().blocks[(size_t) group].left;

    for (size_t c = 0; c < cols.size(); c++)
    {
        if (down ? (col < 0 && cols[c].left >= x) : (cols[c].left < x))
        {
            col = (int) c;
        }
    }

    return col;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardListJump
//
//  List's Home and End, and its page keys, which go on to
//  HandleKeyboardListPage. Home selects the first item, past any header, with
//  the view at its left end; End selects the last, with the view at its right
//  end. With every group collapsed there is no item, and the first or last
//  header takes the focus instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardListJump (WPARAM vk, bool shift, bool ctrl)
{
    std::vector<DxuiListViewTypes::ListColumn>  cols   = m_list.m_flow.GetListColumns();
    bool                                        home   = (vk == VK_HOME);
    bool                                        paging = (vk == VK_PRIOR || vk == VK_NEXT);
    int                                         group  = home ? 0 : (int) m_list.m_groups.size() - 1;



    if (cols.empty() && (paging || !m_list.m_flow.HasListGroups()))
    {
        return false;
    }

    if (paging)
    {
        return HandleKeyboardListPage (cols, vk == VK_NEXT, shift, ctrl);
    }

    if (cols.empty())
    {
        if (m_list.m_multiSelect && ctrl && !shift)
        {
            m_list.m_focusGroup = group;
        }
        else
        {
            m_list.m_grouping.SelectGroup (group);
        }
    }
    else
    {
        m_list.m_keyboard.ApplyKeyboardMove (home ? cols.front().first : cols.back().first + cols.back().count - 1, shift, ctrl);
    }

    m_list.m_scrollbars.SetLeftPx (home ? 0 : m_list.m_scrollbars.GetMaxLeftPx());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardListPage
//
//  Page Up and Page Down in List, which pages sideways, as Explorer's does,
//  measured at 150%: from a column short of the view's edge, to the column
//  whole at that edge, in the same row, without scrolling. From the edge
//  column, the view scrolls to put that column at the other edge, and the
//  focus goes to the same row of the column then whole at the edge paged
//  toward.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardListPage (const std::vector<DxuiListViewTypes::ListColumn> & cols, bool down, bool shift, bool ctrl)
{
    int   n         = (int) cols.size();
    int   rows      = m_list.GetRowCount();
    int   viewW     = m_list.m_boundsDip.right - m_list.m_boundsDip.left;
    int   own       = (m_list.m_focusGroup < 0) ? DxuiListFlowLayout::FindColumnOfItem (cols, m_list.m_selectedRow) : -1;
    int   slot      = (own >= 0) ? m_list.m_selectedRow - cols[(size_t) own].first : 0;
    int   col       = (own >= 0) ? own : GetListPageStartColumn (cols, down);
    bool  focused   = m_list.m_focusGroup >= 0 || (m_list.m_selectedRow >= 0 && m_list.m_selectedRow < rows);
    int   first     = -1;
    int   last      = -1;
    int   target    = -1;
    int   item      = -1;
    int   leftAfter = 0;



    //  A header with no column past it in the key's direction.
    if (focused && col < 0)
    {
        return true;
    }

    TryGetWholeListColumnSpan (cols, first, last);

    if (!DxuiListView::IsAtPageEdge (col, down ? last : first, down))
    {
        target = down ? last : first;
    }
    else if (down ? (col >= n - 1) : (col <= 0))
    {
        target = col;
    }
    else if (down)
    {
        m_list.m_scrollbars.SetLeftPx (cols[(size_t) col].left);
        TryGetWholeListColumnSpan (cols, first, last);
        target = (last > col) ? last : col + 1;
    }
    else
    {
        m_list.m_scrollbars.SetLeftPx (cols[(size_t) col].right - viewW);
        TryGetWholeListColumnSpan (cols, first, last);
        target = (first < col) ? first : col - 1;
        m_list.m_scrollbars.SetLeftPx ((target == 0) ? 0 : m_list.m_leftPx);
    }

    item      = cols[(size_t) target].first + (std::min) (slot, cols[(size_t) target].count - 1);
    leftAfter = m_list.m_leftPx;

    m_list.m_keyboard.ApplyKeyboardMove (item, shift, ctrl);

    //  Selecting brings an item's column into sight with the gap after it,
    //  which would nudge a column already whole at the edge.
    m_list.m_scrollbars.SetLeftPx (leftAfter);

    if (!IsListColumnWhollyShown (cols[(size_t) target]))
    {
        m_list.m_items.EnsureItemVisible (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys::HandleKeyboardItemNav
//
//  Arrows move through the grid as it lies on screen: along a row with Left
//  and Right and between rows with Up and Down, or down a column and across
//  columns in List. List's page keys, Home and End go to
//  HandleKeyboardListJump.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemKeys::HandleKeyboardItemNav (WPARAM vk, bool shift, bool ctrl)
{
    if (m_list.m_items.UsesItemLayout())
    {
        return HandleKeyboardGroupedItemNav (vk, shift, ctrl);
    }

    if (vk == VK_PRIOR || vk == VK_NEXT || vk == VK_HOME || vk == VK_END)
    {
        return HandleKeyboardListJump (vk, shift, ctrl);
    }

    if (m_list.m_flow.HasListGroups())
    {
        return HandleKeyboardListGroupNav (vk, shift, ctrl);
    }

    DxuiListViewTypes::ItemGrid  grid    = m_list.m_items.GetItemGrid();
    bool                         columns = DxuiListItemLayout::GetItemMetrics (m_list.m_view).columns;
    int                          rows    = m_list.GetRowCount();
    int                          cur     = (std::max) (0, m_list.GetSelectedRow());
    int                          across  = columns ? grid.perLine : 1;
    int                          down    = columns ? 1 : grid.perLine;
    int                          next    = cur;
    bool                         moved   = rows > 0;



    switch (vk)
    {
        case VK_LEFT:  next = cur - across; break;
        case VK_RIGHT: next = cur + across; break;
        case VK_UP:    next = cur - down;   break;
        case VK_DOWN:  next = cur + down;   break;
        default:       moved = false;       break;
    }

    if (!moved)
    {
        return false;
    }

    //  A move off the grid's edge stays where it is, as Explorer's does,
    //  rather than wrapping to the far side.
    if (next < 0 || next >= rows)
    {
        next = cur;
    }

    //  Ctrl moves the focus alone, leaving the selection for Space.
    if (m_list.m_multiSelect && ctrl && !shift)
    {
        m_list.m_selectedRow = next;
        m_list.m_items.EnsureItemVisible (next);
        return true;
    }

    if (m_list.m_multiSelect && shift)
    {
        m_list.SelectRangeFromAnchor (next);
    }
    else
    {
        m_list.SetSelectedRow (next);
    }

    m_list.m_items.EnsureItemVisible (next);

    if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
    {
        m_list.m_onSelectionChanged (next);
    }

    return true;
}
