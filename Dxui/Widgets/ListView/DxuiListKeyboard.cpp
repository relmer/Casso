#include "Pch.h"

#include "Widgets/ListView/DxuiListKeyboard.h"
#include "Core/DxuiClipboard.h"
#include "Theme/DxuiTheme.h"
#include "Core/DxuiTextElide.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiColor.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::DxuiListKeyboard
//
////////////////////////////////////////////////////////////////////////////////

DxuiListKeyboard::DxuiListKeyboard (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleKeyboardBodyRowNav
//
//  Moves the selected row from the list-body keyboard sub-stop. Arrows
//  step one row and Home / End jump to the ends, the target clamped to a
//  valid row; PageUp / PageDown go to HandleKeyboardPage. A page that
//  scrolled slides into view, as Explorer's does; any other key that
//  scrolls ends a slide under way, and the view jumps.
//
//  A page's move, and Home's or End's wherever the view is placed after the
//  move, is reported last, with the view where the key leaves it and any
//  slide begun. A host that draws a frame from the report, as one that fills
//  a preview does, would otherwise show the move with the view still on its
//  way: a frame at the page's end before the slide back from its start.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleKeyboardBodyRowNav (WPARAM vk, bool shift, bool ctrl)
{
    bool                          paging  = (vk == VK_PRIOR || vk == VK_NEXT);
    int                           shownPx = paging ? m_list.GetShownScrollPx() : 0;
    int                           restPx  = paging ? m_list.m_items.GetRestScrollPx()  : 0;
    DxuiListViewTypes::PageSlide  running = m_list.m_pageSlide;
    bool                          handled = false;



    if (m_list.m_multiSelect && vk == VK_SPACE)
    {
        return SelectFocused (ctrl);
    }

    if (m_list.IsItemsView())
    {
        handled = m_list.m_itemKeys.HandleKeyboardItemNav (vk, shift, ctrl);
    }
    else if (paging)
    {
        handled = HandleKeyboardPage (vk, shift, ctrl);
    }
    else if (m_list.HasGroupLines())
    {
        handled = HandleKeyboardGroupedNav (vk, shift, ctrl);
    }
    else
    {
        handled = HandleKeyboardRowNav (vk, shift, ctrl);
    }

    if (paging)
    {
        m_list.m_items.BeginPageSlide (running, shownPx, restPx);
    }

    if (m_keyMovePending)
    {
        m_keyMovePending = false;

        if (m_list.m_onSelectionChanged)
        {
            m_list.m_onSelectionChanged (m_keyMoveRow);
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleKeyboardRowNav
//
//  Details without groups: the arrows, Home and End over the rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleKeyboardRowNav (WPARAM vk, bool shift, bool ctrl)
{
    int   rows  = m_list.GetRowCount();
    int   cur   = m_list.GetSelectedRow();
    int   next  = cur;
    bool  moved = (rows > 0);



    switch (vk)
    {
        case VK_UP:    next = cur - 1;    break;
        case VK_DOWN:  next = cur + 1;    break;
        case VK_HOME:  next = 0;          break;
        case VK_END:   next = rows - 1;   break;
        default:       moved = false;     break;
    }

    // An empty list has nowhere to move, so even a navigation key goes
    // unhandled and the host gets a chance at it.
    if (moved)
    {
        if (next < 0)
        {
            next = 0;
        }

        if (next > rows - 1)
        {
            next = rows - 1;
        }

        //  Ctrl moves the focus alone, leaving the selection for Space.
        if (m_list.m_multiSelect && ctrl && !shift)
        {
            m_list.m_selectedRow = next;
            m_list.EnsureVisible (next);
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

        //  Reported only with multiple selection on, where a keyboard move is
        //  a selection change like any click; a single-select list has always
        //  left keyboard moves unreported.
        if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
        {
            m_list.m_onSelectionChanged (next);
        }
    }

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleKeyboardGroupedNav
//
//  With groups, the arrows step over lines, and a header is a stop like a
//  row: reaching one selects its rows and gives it the focus. On a header,
//  Left collapses the group and Right opens it. Home and End go to the first
//  and last rows, past the headers.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleKeyboardGroupedNav (WPARAM vk, bool shift, bool ctrl)
{
    int   lines = m_list.m_grouping.GetLineCount();
    int   cur   = (m_list.m_focusGroup >= 0) ? m_list.m_grouping.GetLineOfGroup (m_list.m_focusGroup) : m_list.m_grouping.GetLineOfRow (m_list.GetSelectedRow());
    int   next  = cur;
    int   row   = -1;



    if (lines <= 0)
    {
        return false;
    }

    if (m_list.m_focusGroup >= 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        m_list.m_grouping.SetGroupCollapsed (m_list.m_focusGroup, vk == VK_LEFT);
        return true;
    }

    if (vk == VK_HOME || vk == VK_END)
    {
        return HandleKeyboardDetailsJump (vk, shift, ctrl);
    }

    switch (vk)
    {
        case VK_UP:    next = (cur < 0) ? 0 : cur - 1; break;
        case VK_DOWN:  next = (cur < 0) ? 0 : cur + 1; break;
        default:       return false;
    }

    next = std::clamp (next, 0, lines - 1);
    row  = m_list.m_grouping.GetRowOfLine (next);

    //  Ctrl moves the focus alone, over headers and rows alike.
    if (m_list.m_multiSelect && ctrl && !shift)
    {
        m_list.m_focusGroup  = (row < 0) ? m_list.m_grouping.GetGroupOfLine (next) : -1;
        m_list.m_selectedRow = (row < 0) ? m_list.m_selectedRow : row;
        m_list.m_grouping.EnsureLineVisible (next);
        return true;
    }

    if (row < 0)
    {
        m_list.m_grouping.SelectGroup (m_list.m_grouping.GetGroupOfLine (next));
    }
    else
    {
        m_list.m_focusGroup = -1;

        if (m_list.m_multiSelect && shift)
        {
            m_list.SelectRangeFromAnchor (row);
        }
        else
        {
            m_list.SetSelectedRow (row);
        }

        if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
        {
            m_list.m_onSelectionChanged (row);
        }
    }

    m_list.m_grouping.EnsureLineVisible (next);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleKeyboardDetailsJump
//
//  Home and End in grouped Details: the first or the last row, past the
//  headers, with the view at its very top or its very end. With every group
//  collapsed there is no row, and the first or last header takes the focus
//  instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleKeyboardDetailsJump (WPARAM vk, bool shift, bool ctrl)
{
    bool  home   = (vk == VK_HOME);
    int   lines  = m_list.m_grouping.GetLineCount();
    int   target = GetEdgeRowLine (!home);



    if (lines <= 0)
    {
        return false;
    }

    if (target < 0)
    {
        target = home ? 0 : lines - 1;

        if (m_list.m_multiSelect && ctrl && !shift)
        {
            m_list.m_focusGroup = m_list.m_grouping.GetGroupOfLine (target);
        }
        else
        {
            m_list.m_grouping.SelectGroup (m_list.m_grouping.GetGroupOfLine (target));
        }

        m_list.m_grouping.EnsureLineVisible (target);
        return true;
    }

    ApplyKeyboardMove (m_list.m_grouping.GetRowOfLine (target), shift, ctrl);

    if (home)
    {
        m_list.m_scrollbars.SetTopRow (0);
    }
    else
    {
        ScrollLineToBottom (target);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleKeyboardPage
//
//  Page Up and Page Down in Details, as Explorer's, measured at 150%. With
//  the focus short of the edge the key is toward, or on no row, it goes to
//  the whole row at that edge and nothing scrolls. On that edge, or past it
//  out of sight, it goes a page on, headers not counted, and the view puts
//  that row flush with the edge, so the row it left is the first or last
//  whole one at the other side. A focused header pages from its line, as a
//  row under a collapsed header does from its group's.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleKeyboardPage (WPARAM vk, bool shift, bool ctrl)
{
    bool  down       = (vk == VK_NEXT);
    int   rows       = m_list.GetRowCount();
    int   page       = GetPageLines();
    int   cur        = -1;
    int   first      = -1;
    int   last       = -1;
    int   target     = -1;
    int   savedTop   = m_list.m_topRow;
    bool  savedFlush = m_list.m_flushBottom;
    bool  scroll     = false;



    if (!TryGetWholeRowLineSpan (first, last))
    {
        return false;
    }

    if (m_list.m_focusGroup >= 0)
    {
        cur = m_list.m_grouping.GetLineOfGroup (m_list.m_focusGroup);
    }
    else if (m_list.m_selectedRow >= 0 && m_list.m_selectedRow < rows)
    {
        cur = m_list.m_grouping.GetLineOfRow (m_list.m_selectedRow);
        cur = (cur >= 0) ? cur : m_list.m_grouping.GetLineOfGroup (m_list.m_grouping.GetGroupOfRow (m_list.m_selectedRow));
    }

    target = DxuiListView::IsAtPageEdge (cur, down ? last : first, down) ? StepRowLines (cur, down ? page : -page)
                                                            : (down ? last : first);

    //  A header with no row past it in the key's direction.
    if (target < 0)
    {
        return true;
    }

    scroll = !IsLineWhollyShown (target);

    ApplyKeyboardMove (m_list.m_grouping.GetRowOfLine (target), shift, ctrl);

    //  Selecting a group's first row scrolls its header into sight, which a
    //  move that stays in sight undoes.
    if (!scroll)
    {
        m_list.m_scrollbars.SetTopRow (savedTop);
        m_list.m_flushBottom = savedFlush;
    }
    else if (down)
    {
        ScrollLineToBottom (target);
    }
    else
    {
        ScrollLineToTop (target);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::ApplyKeyboardMove
//
//  A key's move to a row, as the arrows make it: alone, the row becomes the
//  selection; Shift selects from the anchor to it; Ctrl moves the focus
//  alone, leaving the selection for Space. Selecting scrolls the row into
//  sight, so a caller that places the view itself does it afterward, and the
//  move is reported once it has: see HandleKeyboardBodyRowNav.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListKeyboard::ApplyKeyboardMove (int row, bool shift, bool ctrl)
{
    m_list.m_focusGroup = -1;

    if (m_list.m_multiSelect && ctrl && !shift)
    {
        m_list.m_selectedRow = row;
        return;
    }

    if (m_list.m_multiSelect && shift)
    {
        m_list.SelectRangeFromAnchor (row);
    }
    else
    {
        m_list.SetSelectedRow (row);
    }

    //  Reported only with multiple selection on, where a keyboard move is a
    //  selection change like any click; a single-select list has always left
    //  keyboard moves unreported.
    if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
    {
        m_keyMoveRow     = row;
        m_keyMovePending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::GetPageLines
//
//  How many lines a page moves: the lines of the base height the view holds
//  whole, less one, and at least one. A row taller than the base, as a
//  wrapped name makes one, still counts as one line, so after a page the row
//  left behind can end partly out of sight, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListKeyboard::GetPageLines() const
{
    int  whole = m_list.m_items.UsesItemLayout() ? m_list.m_items.GetItemGrid().visible : m_list.m_scrollbars.GetVisibleRowCapacity();



    return (std::max) (1, whole - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::TryGetWholeLineSpan
//
//  The lines Details shows whole, headers included: from the top one, or the
//  one under it while the rows are lifted, for as many as the body holds.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::TryGetWholeLineSpan (int & first, int & last) const
{
    int  cap   = m_list.m_scrollbars.GetVisibleRowCapacity();
    int  lines = m_list.m_grouping.GetLineCount();



    first = m_list.m_topRow + ((m_list.m_scrollbars.GetRowShiftPx() > 0) ? 1 : 0);
    last  = (std::min) (first + cap - 1, lines - 1);

    return cap > 0 && first <= last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::TryGetWholeRowLineSpan
//
//  The first and last rows Details shows whole, as lines, past any header at
//  either end. With no row whole in sight, the row nearest the top stands for
//  both. False only when there is no row to show at all.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::TryGetWholeRowLineSpan (int & first, int & last) const
{
    int  lines   = m_list.m_grouping.GetLineCount();
    int  nearest = -1;



    if (lines <= 0)
    {
        return false;
    }

    if (TryGetWholeLineSpan (first, last))
    {
        while (first <= last && m_list.m_grouping.GetRowOfLine (first) < 0)
        {
            first++;
        }

        while (last >= first && m_list.m_grouping.GetRowOfLine (last) < 0)
        {
            last--;
        }

        if (first <= last)
        {
            return true;
        }
    }

    nearest = (std::min) (m_list.m_topRow, lines - 1);
    nearest = (m_list.m_grouping.GetRowOfLine (nearest) >= 0) ? nearest : StepRowLines (nearest, 1);
    nearest = (nearest >= 0) ? nearest : StepRowLines ((std::min) (m_list.m_topRow, lines - 1), -1);
    first   = nearest;
    last    = nearest;

    return nearest >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::IsLineWhollyShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::IsLineWhollyShown (int line) const
{
    int  first = 0;
    int  last  = 0;



    return TryGetWholeLineSpan (first, last) && line >= first && line <= last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::StepRowLines
//
//  The row `steps` rows on from a line, as a line, down for a positive count:
//  only rows are counted, and the count stops at the first or the last. A row
//  with none past it gives itself; a header with none past it gives -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListKeyboard::StepRowLines (int line, int steps) const
{
    int  lines  = m_list.m_grouping.GetLineCount();
    int  dir    = (steps < 0) ? -1 : 1;
    int  left   = (steps < 0) ? -steps : steps;
    int  result = (line >= 0 && line < lines && m_list.m_grouping.GetRowOfLine (line) >= 0) ? line : -1;
    int  at     = line;



    while (left > 0)
    {
        at += dir;

        if (at < 0 || at >= lines)
        {
            break;
        }

        if (m_list.m_grouping.GetRowOfLine (at) >= 0)
        {
            result = at;
            left--;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::GetEdgeRowLine
//
//  The first or the last row's line, past the headers; -1 with no row shown.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListKeyboard::GetEdgeRowLine (bool last) const
{
    int  lines = m_list.m_grouping.GetLineCount();
    int  edge  = last ? lines - 1 : 0;



    if (lines <= 0)
    {
        return -1;
    }

    return (m_list.m_grouping.GetRowOfLine (edge) >= 0) ? edge : StepRowLines (edge, last ? -1 : 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::ScrollLineToBottom
//
//  Scrolls Details so a line ends flush with the body's bottom: when the body
//  ends partway through a row, the rows are lifted for it and the top one is
//  cut off. The last row goes to the very end, so the headers of collapsed
//  groups under it come into sight with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListKeyboard::ScrollLineToBottom (int line)
{
    DxuiListViewTypes::ScrollLayout  layout = m_list.m_scrollbars.ComputeScrollLayout();
    int                              cap    = layout.rowCap;



    if (cap <= 0)
    {
        m_list.m_scrollbars.SetTopRow (line);
        return;
    }

    //  Past the last top row is the very end.
    if (line == GetEdgeRowLine (true))
    {
        m_list.m_scrollbars.SetTopRow (m_list.m_scrollbars.GetMaxTopRow() + 1);

        if (IsLineWhollyShown (line))
        {
            return;
        }
    }

    if (layout.partialPx <= 0 || line < cap)
    {
        m_list.m_scrollbars.SetTopRow (line - cap + 1);
        return;
    }

    m_list.m_scrollbars.SetTopRow (line - cap);
    m_list.m_flushBottom = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::ScrollLineToTop
//
//  Scrolls Details so a line is the top one; for the first row, to the very
//  top, so the header above it shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListKeyboard::ScrollLineToTop (int line)
{
    m_list.m_scrollbars.SetTopRow ((line == GetEdgeRowLine (false)) ? 0 : line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::SelectFocused
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::SelectFocused (bool ctrl)
{
    int  row = m_list.m_selectedRow;



    if (m_list.m_focusGroup >= 0)
    {
        m_list.m_grouping.SelectGroup (m_list.m_focusGroup);
        return true;
    }

    if (row < 0 || row >= m_list.GetRowCount())
    {
        return false;
    }

    if (ctrl)
    {
        m_list.ClickRow (row, true, false);
    }
    else
    {
        m_list.SetSelectedRow (row);
    }

    if (m_list.m_onSelectionChanged)
    {
        m_list.m_onSelectionChanged (row);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard::HandleTypeAhead
//
//  Characters typed close together build a prefix, so "te" finds Temp ahead
//  of Tools. The same character typed again steps to the next row starting
//  with it instead, as Explorer's does. A new search begins below the current
//  row; a growing prefix starts at the current row, so it stays put while the
//  row still matches. Either way the search wraps around the end.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListKeyboard::HandleTypeAhead (wchar_t ch)
{
    int64_t            now    = m_clock ? m_clock() : (int64_t) GetTickCount64();
    int                rows   = m_list.GetRowCount();
    int                cur    = m_list.GetSelectedRow();
    bool               repeat = false;
    int                from   = 0;
    int                step   = 0;
    std::wstring       prefix;



    if (rows <= 0)
    {
        return false;
    }

    if (now - m_typeAheadMs > s_kTypeAheadResetMs)
    {
        m_typeAhead.clear();
    }

    m_typeAheadMs = now;
    m_typeAhead  += ch;

    //  "aaa" steps through the rows starting with a; "abc" looks for abc.
    repeat = m_typeAhead.size() > 1
             && std::all_of (m_typeAhead.begin(), m_typeAhead.end(),
                             [&] (wchar_t c) { return towlower (c) == towlower (m_typeAhead[0]); });
    prefix = repeat ? m_typeAhead.substr (0, 1) : m_typeAhead;
    from   = (m_typeAhead.size() == 1 || repeat) ? cur + 1 : (std::max) (cur, 0);

    for (step = 0; step < rows; step++)
    {
        int                                           row   = ((from + step) % rows + rows) % rows;
        const std::vector<DxuiListViewTypes::Cell>  & cells = m_list.GetRowCells (row);

        if (!cells.empty() && cells[0].text.size() >= prefix.size()
            && _wcsnicmp (cells[0].text.c_str(), prefix.c_str(), prefix.size()) == 0)
        {
            m_list.SetSelectedRow (row);

            if (m_list.m_multiSelect && m_list.m_onSelectionChanged)
            {
                m_list.m_onSelectionChanged (row);
            }

            break;
        }
    }

    //  Taken whether or not a row matched: a character typed at a list is a
    //  search, not something to hand on.
    return true;
}
