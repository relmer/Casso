#include "Pch.h"

#include "Widgets/ListView/DxuiListGroups.h"
#include "Theme/DxuiRowLook.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListGroups::DxuiListGroups
//
////////////////////////////////////////////////////////////////////////////////

DxuiListGroups::DxuiListGroups (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestGroupHeader
//
//  The group whose header is drawn under a point, a sliding page's offset
//  included, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::HitTestGroupHeader (int xPx, int yPx) const
{
    if (m_list.m_flow.HasListGroups())
    {
        const DxuiListViewTypes::ListLayout  & layout = m_list.m_flow.GetListLayout();
        int                                    at     = (yPx >= 0 && yPx < layout.headerH) ? DxuiListFlowLayout::FindListBlock (layout, xPx + m_list.m_leftPx) : -1;

        return (at >= 0) ? layout.blocks[(size_t) at].group : -1;
    }

    if (m_list.m_items.UsesItemLayout())
    {
        const DxuiListViewTypes::ItemLayout  & layout = m_list.m_items.GetItemLayout();
        int                                    at     = (xPx >= 0 && yPx >= 0) ? DxuiListItemLayout::FindItemLine (layout, yPx + m_list.GetShownScrollPx()) : -1;

        return (at >= 0) ? layout.lines[(size_t) at].group : -1;
    }

    int  line = m_list.HasGroupLines() ? HitTestLine (xPx, yPx) : -1;



    return (line < 0) ? -1 : GetGroupOfLine (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestLine
//
//  The line under a point relative to the list's rect, or -1 outside every
//  shown line. The line is the one drawn there: past the scroll, the rows'
//  lift, and a sliding page's offset, which can put a line above the top
//  row under the point.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::HitTestLine (int xPx, int yPx) const
{
    HRESULT  hr      = S_OK;
    int      result  = -1;
    int      rowH    = m_list.GetRowHeightPx();
    int      headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int      hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int      body    = yPx - headerH - hdrGap;
    int      shifted = body + m_list.m_scrollbars.GetRowShiftPx();
    int      visIdx  = (body < 0 || rowH <= 0) ? -1 : (shifted / rowH);
    int      drawnPx = shifted - m_list.m_items.GetPageSlidePx();
    int      drawn   = (rowH <= 0) ? 0 : (drawnPx >= 0) ? drawnPx / rowH : -((rowH - 1 - drawnPx) / rowH);
    int      cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    int      abs     = (visIdx < 0) ? -1 : (m_list.m_topRow + drawn);
    int      rowW    = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (GetLineCount() > cap ? m_list.GetScrollbarWidthPx() : 0);



    BAIL_OUT_IF (xPx < 0 || xPx >= rowW, S_OK);
    BAIL_OUT_IF (visIdx < 0 || visIdx >= m_list.m_scrollbars.GetShownRowCount(), S_OK);
    BAIL_OUT_IF (abs < 0 || abs >= GetLineCount(), S_OK);

    result = abs;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildLines
//
//  A header for each group, then its rows unless it is collapsed. Rows ahead
//  of the first group, which a host should not leave, still get lines.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::BuildLines() const
{
    int  rows  = m_list.GetRowCount();
    int  first = m_groups.empty() ? rows : (std::min) (m_groups[0].firstRow, rows);



    if (!m_linesDirty && m_linesRowCount == rows)
    {
        return;
    }

    m_lines.clear();
    m_rowLines.assign ((size_t) (std::max) (rows, 0), -1);

    for (int r = 0; r < first; r++)
    {
        m_rowLines[(size_t) r] = (int) m_lines.size();
        m_lines.push_back (r);
    }

    for (size_t g = 0; g < m_groups.size(); g++)
    {
        int  start = std::clamp (m_groups[g].firstRow, 0, rows);
        int  end   = (g + 1 < m_groups.size()) ? std::clamp (m_groups[g + 1].firstRow, start, rows) : rows;

        m_lines.push_back (-((int) g + 1));

        for (int r = start; r < end && !m_groups[g].collapsed; r++)
        {
            m_rowLines[(size_t) r] = (int) m_lines.size();
            m_lines.push_back (r);
        }
    }

    m_linesDirty    = false;
    m_linesRowCount = rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetLineCount() const
{
    if (m_list.m_items.UsesItemLayout())
    {
        return (int) m_list.m_items.GetItemLayout().lines.size();
    }

    if (!m_list.HasGroupLines())
    {
        return m_list.GetRowCount();
    }

    BuildLines();

    return (int) m_lines.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineOfRow
//
//  -1 for a row under a collapsed header, which has no line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetLineOfRow (int row) const
{
    if (!m_list.HasGroupLines())
    {
        return row;
    }

    BuildLines();

    return (row >= 0 && row < (int) m_rowLines.size()) ? m_rowLines[(size_t) row] : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowOfLine
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetRowOfLine (int line) const
{
    if (!m_list.HasGroupLines())
    {
        return line;
    }

    BuildLines();

    return (line >= 0 && line < (int) m_lines.size() && m_lines[(size_t) line] >= 0) ? m_lines[(size_t) line] : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGroupOfLine
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetGroupOfLine (int line) const
{
    if (!m_list.HasGroupLines())
    {
        return -1;
    }

    BuildLines();

    return (line >= 0 && line < (int) m_lines.size() && m_lines[(size_t) line] < 0) ? -m_lines[(size_t) line] - 1 : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineOfGroup
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetLineOfGroup (int group) const
{
    if (!m_list.HasGroupLines() || group < 0 || group >= (int) m_groups.size())
    {
        return -1;
    }

    BuildLines();

    for (size_t line = 0; line < m_lines.size(); line++)
    {
        if (m_lines[line] == -(group + 1))
        {
            return (int) line;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetGroups
//
//  Groups that change move every line, so a page's hold on a line's edge is
//  let go; the same groups set again, as a refresh does, keep it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::SetGroups (std::vector<DxuiListViewTypes::Group> groups)
{
    bool  same = false;



    for (DxuiListViewTypes::Group & group : groups)
    {
        for (const DxuiListViewTypes::Group & old : m_groups)
        {
            if (old.label == group.label)
            {
                group.collapsed = old.collapsed;
                break;
            }
        }
    }

    same = std::equal (groups.begin(), groups.end(), m_groups.begin(), m_groups.end(), [] (const DxuiListViewTypes::Group & a, const DxuiListViewTypes::Group & b)
    {
        return a.label == b.label && a.firstRow == b.firstRow && a.collapsed == b.collapsed;
    });

    if (!same)
    {
        m_list.m_flushBottom    = false;
        m_list.m_items.ReleaseAnchor();
    }

    m_groups     = std::move (groups);
    m_linesDirty        = true;
    m_hoverGroup = -1;
    m_groupSlide = {};

    if (m_list.m_focusGroup >= (int) m_groups.size())
    {
        m_list.m_focusGroup = -1;
    }

    //  Fewer lines than before can put the top past the end.
    if (m_list.m_topRow > m_list.m_scrollbars.GetMaxTopRow())
    {
        m_list.m_scrollbars.SetTopRow (m_list.m_topRow);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetGroupCollapsed
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::SetGroupCollapsed (int group, bool collapsed)
{
    if (group < 0 || group >= (int) m_groups.size() || m_groups[(size_t) group].collapsed == collapsed)
    {
        return;
    }

    m_groups[(size_t) group].collapsed = collapsed;
    m_linesDirty                              = true;
    m_groupSlide                       = { group, m_list.GetClockMs() };

    m_list.m_scrollbars.SetTopRow (m_list.m_topRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAllGroupsCollapsed
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::SetAllGroupsCollapsed (bool collapsed)
{
    for (DxuiListViewTypes::Group & group : m_groups)
    {
        group.collapsed = collapsed;
    }

    m_linesDirty        = true;
    m_groupSlide = {};

    m_list.m_scrollbars.SetTopRow (m_list.m_topRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGroupCollapsed
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListGroups::IsGroupCollapsed (int group) const
{
    return group >= 0 && group < (int) m_groups.size() && m_groups[(size_t) group].collapsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGroupSliding
//
//  Whether a group opened or closed one at a time, or a page, is still
//  sliding, so the host keeps painting. The slide ends itself here, and this
//  returns true once more after it settles, so the frame after its last one
//  is drawn with the group, or the page, where it rests.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListGroups::IsGroupSliding()
{
    if (m_list.m_items.IsPageSliding())
    {
        return true;
    }

    if (m_groupSlide.group < 0)
    {
        return false;
    }

    if (m_list.GetClockMs() - m_groupSlide.startMs >= s_kGroupSlideMs)
    {
        m_groupSlide = {};
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EaseGroupSlide
//
//  The CSS ease-in-out curve, cubic Bezier (0.42, 0) (0.58, 1), which
//  Explorer's group slide follows: x solved for the time by halving, then
//  its y.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListGroups::EaseGroupSlide (float t)
{
    constexpr float  kX1    = 0.42f;
    constexpr float  kX2    = 0.58f;
    constexpr int    kSteps = 24;
    float            lo     = 0.0f;
    float            hi     = 1.0f;
    float            s      = 0.0f;
    float            bx     = 0.0f;



    t = std::clamp (t, 0.0f, 1.0f);

    for (int step = 0; step < kSteps; step++)
    {
        s  = (lo + hi) * 0.5f;
        bx = 3.0f * (1.0f - s) * (1.0f - s) * s * kX1 + 3.0f * (1.0f - s) * s * s * kX2 + s * s * s;

        if (bx < t)
        {
            lo = s;
        }
        else
        {
            hi = s;
        }
    }

    s = (lo + hi) * 0.5f;

    return 3.0f * (1.0f - s) * s * s + s * s * s;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGroupSlideShown
//
//  How much of the sliding group's rows is in sight, from 0 to 1; -1 when
//  no group slides.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListGroups::GetGroupSlideShown() const
{
    int64_t  elapsed = m_list.GetClockMs() - m_groupSlide.startMs;
    float    eased   = 0.0f;



    if (m_groupSlide.group < 0 || m_groupSlide.group >= (int) m_groups.size() || elapsed >= s_kGroupSlideMs)
    {
        return -1.0f;
    }

    eased = EaseGroupSlide ((float) elapsed / (float) s_kGroupSlideMs);

    return m_groups[(size_t) m_groupSlide.group].collapsed ? 1.0f - eased : eased;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGroupRowSpan
//
//  The rows a group holds, [start, end).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::GetGroupRowSpan (int group, int & start, int & end) const
{
    int  rows = m_list.GetRowCount();



    start = std::clamp (m_groups[(size_t) group].firstRow, 0, rows);
    end   = (group + 1 < (int) m_groups.size()) ? std::clamp (m_groups[(size_t) group + 1].firstRow, start, rows) : rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlaceDataLines
//
//  Where Details draws each line in sight. While a group slides, its rows
//  are drawn from just under its header, lifted by however much of them is
//  still out of sight and clipped to [clipTop, clipTop + clipH); the lines
//  under the group follow the slide's edge. The slid rows come last, so the
//  clip is pushed once.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListViewTypes::LineSpot> DxuiListGroups::PlaceDataLines (
    float    top,
    int      firstLine,
    int      lastLine,
    float  & clipTop,
    float  & clipH) const
{
    std::vector<DxuiListViewTypes::LineSpot>  spots;
    std::vector<DxuiListViewTypes::LineSpot>  slid;
    float                                     rowH   = (float) m_list.GetRowHeightPx();
    float                                     bottom = (float) m_list.m_boundsDip.bottom;
    float                                     shown  = m_list.HasGroupLines() ? GetGroupSlideShown() : -1.0f;
    int                                       group  = m_groupSlide.group;
    int                                       start  = 0;
    int                                       end    = 0;
    float                                     fullH  = 0.0f;
    float                                     extra  = 0.0f;
    int                                       count  = GetLineCount();



    clipTop = 0.0f;
    clipH   = 0.0f;

    if (shown >= 0.0f)
    {
        GetGroupRowSpan (group, start, end);
        fullH    = (float) (end - start) * rowH;
        lastLine = count;
    }

    for (int line = (std::max) (firstLine, 0); line < (std::min) (lastLine, count); ++line)
    {
        DxuiListViewTypes::LineSpot  spot;

        spot.row   = GetRowOfLine (line);
        spot.group = spot.row < 0 ? GetGroupOfLine (line) : -1;
        spot.y     = top + (float) (line - firstLine) * rowH + extra;

        //  The slide draws the group's own rows.
        if (shown >= 0.0f && spot.row >= start && spot.row < end)
        {
            continue;
        }

        if (spot.y >= bottom)
        {
            break;
        }

        spots.push_back (spot);

        if (shown < 0.0f || spot.group != group)
        {
            continue;
        }

        clipTop = spot.y + rowH;
        clipH   = fullH * shown;
        extra   = clipH - (m_groups[(size_t) group].collapsed ? 0.0f : fullH);

        for (int r = start; r < end; r++)
        {
            float  ry = clipTop + (float) (r - start) * rowH - (fullH - clipH);

            if (ry + rowH > clipTop && ry < clipTop + clipH && ry < bottom)
            {
                slid.push_back (DxuiListViewTypes::LineSpot { r, -1, ry, true });
            }
        }
    }

    spots.insert (spots.end(), slid.begin(), slid.end());

    return spots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureLineVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::EnsureLineVisible (int line)
{
    int  cap   = m_list.m_scrollbars.GetVisibleRowCapacity();
    int  first = 0;
    int  last  = 0;



    if (line < 0 || cap <= 0)
    {
        return;
    }

    //  As EnsureVisible: a line whole in sight leaves a page down's hold alone.
    if (m_list.m_keyboard.TryGetWholeLineSpan (first, last) && line >= first && line <= last)
    {
        return;
    }

    if (line < m_list.m_topRow || (line == m_list.m_topRow && m_list.m_scrollbars.GetRowShiftPx() > 0))
    {
        m_list.m_scrollbars.SetTopRow (line);
    }
    else if (m_list.m_flushBottom && line > last)
    {
        m_list.m_keyboard.ScrollLineToBottom (line);
    }
    else if (line >= m_list.m_topRow + cap)
    {
        m_list.m_scrollbars.SetTopRow (line - cap + 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestGroupChevron
//
//  The chevron's box at the header's start, where a click toggles the group.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListGroups::HitTestGroupChevron (int xPx, int group) const
{
    int  colOff = m_list.m_hScrollEnabled ? -m_list.m_leftPx : 0;



    //  In List, the chevron starts its own group's block.
    if (m_list.m_flow.HasListGroups() && group >= 0)
    {
        const DxuiListViewTypes::ListLayout & layout = m_list.m_flow.GetListLayout();

        colOff = (group < (int) layout.blocks.size()) ? layout.blocks[(size_t) group].left - m_list.m_leftPx : colOff;
    }

    return group >= 0 && xPx >= colOff && xPx < colOff + m_list.m_scaler.ToPx (DxuiListView::s_kGroupLabelDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListGroups::OnPress
//
//  The chevron, or a second click on the header, opens or closes the group;
//  a click on the header otherwise selects its rows. List's headers are
//  labels only, as Explorer's are: they select. Returns whether the press
//  was on a group's header.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListGroups::OnPress (int lx, int ly)
{
    int      group = HitTestGroupHeader (lx, ly);
    int64_t  nowMs = 0;
    bool     twice = false;



    if (group < 0)
    {
        return false;
    }

    nowMs = (int64_t) GetTickCount64();
    twice = group == m_lastHeaderGroup && (nowMs - m_lastHeaderMs) <= (int64_t) GetDoubleClickTime();

    m_lastHeaderGroup = twice ? -1 : group;
    m_lastHeaderMs    = nowMs;

    //  Home's headers open and close, and the second has its tabs.
    if (m_list.m_view == DxuiListViewTypes::View::Home)
    {
        for (size_t tab = 0; group == 1 && tab < m_homeTabs.size() && tab < m_list.m_homeHeader.tabs.size(); tab++)
        {
            POINT  point { lx, ly };

            if (PtInRect (&m_homeTabs[tab], point) && m_list.m_homeHeader.onTab)
            {
                m_lastHeaderGroup = -1;
                m_list.m_homeHeader.onTab ((int) tab);

                return true;
            }
        }

        if (HitTestGroupChevron (lx, group))
        {
            SetGroupCollapsed (group, !IsGroupCollapsed (group));
        }

        return true;
    }

    if (!m_list.m_flow.HasListGroups() && (twice || HitTestGroupChevron (lx, group)))
    {
        SetGroupCollapsed (group, !IsGroupCollapsed (group));
        m_list.m_focusGroup = group;
    }
    else
    {
        SelectGroup (group);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectGroup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::SelectGroup (int group)
{
    std::vector<int>  rows;
    int               first = 0;
    int               end   = 0;



    if (group < 0 || group >= (int) m_groups.size())
    {
        return;
    }

    first = m_groups[(size_t) group].firstRow;
    end   = (group + 1 < (int) m_groups.size()) ? m_groups[(size_t) group + 1].firstRow : m_list.GetRowCount();

    for (int r = first; r < end; r++)
    {
        rows.push_back (r);
    }

    m_list.SetSelectedRows (std::move (rows), first);
    m_list.m_focusGroup = group;

    if (m_list.m_onSelectionChanged)
    {
        m_list.m_onSelectionChanged (first);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintGroupHeader
//
//  The group's label where a row's name starts, as Explorer draws it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::PaintGroupHeader (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, int group, float x, float rowX, float ry, float layoutW) const
{
    HRESULT      hr      = S_OK;
    bool         home    = m_list.m_view == DxuiListViewTypes::View::Home;
    float        rowH    = home ? m_list.m_scaler.ToPxf ((float) DxuiListItemLayout::s_kHomeHeaderDip) : (float) m_list.GetRowHeightPx();
    float        labelX  = x + m_list.m_scaler.ToPxf ((float) (m_list.m_flow.HasListGroups() ? s_kListGroupLabelDip : DxuiListView::s_kGroupLabelDip));
    float        armHalf = m_list.m_scaler.ToPxf (s_kGroupChevronArmDip);
    float        thick   = (std::max) (std::floor (m_list.m_scaler.ToPxf (1.0f)), 1.0f);
    float        cx      = x + m_list.m_scaler.ToPxf ((float) s_kGroupChevronCenterDip);
    float        cy      = ry + rowH * 0.5f;
    int          steps   = (int) (armHalf - thick * 0.5f);
    uint32_t     ink     = m_list.m_theme->ForegroundMuted();
    bool         focused = m_list.m_listFocused && group == m_list.m_focusGroup;
    DxuiRowLook  look    = DxuiRowLook::Resolve (*m_list.m_theme, false, focused, group == m_hoverGroup, m_list.m_listFocused);



    if (group < 0 || group >= (int) m_groups.size())
    {
        return;
    }

    //  Home's are boxes: the heading's, and the chevron's beside the tabs.
    if (home)
    {
        float  boxX  = x + m_list.m_scaler.ToPxf ((float) DxuiListItemLayout::s_kHomeTileLeftDip);
        float  boxH  = m_list.m_scaler.ToPxf (34.0f);
        float  boxW  = m_list.m_scaler.ToPxf (36.0f);
        float  textW = 0.0f;
        float  textH = 0.0f;
        float  size  = m_list.m_scaler.ToPxf (m_list.m_fontDip) * s_kHomeHeadingScale;

        if (group == 0)
        {
            hr = text.MeasureString (m_groups[0].label.c_str(), size, m_list.GetBodyFace(), textW, textH);
            IGNORE_RETURN_VALUE (hr, S_OK);

            boxW = m_list.m_scaler.ToPxf (44.0f) + textW + m_list.m_scaler.ToPxf (12.0f);
        }

        if (group == m_hoverGroup && (float) m_hoverX + (float) m_list.m_boundsDip.left < boxX + boxW)
        {
            painter.FillRoundedRect (boxX, ry + (rowH - boxH) * 0.5f, boxW, boxH, m_list.m_scaler.ToPxf (4.0f), m_list.m_theme->HoverBackground());
        }

        cx      = x + m_list.m_scaler.ToPxf (34.0f);
        labelX  = x + m_list.m_scaler.ToPxf (60.0f);
        armHalf = m_list.m_scaler.ToPxf (6.0f);
        steps   = (int) (armHalf - thick * 0.5f);
        ink     = pal.fg;
    }

    //  The whole pane's width, not the columns': a header is not a row.
    if (!home && look.fill != 0)
    {
        painter.FillRect (rowX, ry, layoutW, rowH, look.fill);
    }

    if (focused)
    {
        painter.OutlineRect (rowX, ry, layoutW, rowH, DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), m_list.m_theme->Foreground());
    }

    //  Down while the group is open, right while it is collapsed, as a tree
    //  node's; two strokes stepped toward each other, as the tree draws them.
    //  List's headers have none: its groups do not collapse.
    for (int s = 0; s <= steps && !m_list.m_flow.HasListGroups(); ++s)
    {
        if (m_groups[(size_t) group].collapsed)
        {
            float  dx = cx - armHalf * 0.5f + (float) s;

            painter.FillRect (dx, cy - armHalf + (float) s,         thick, thick, ink);
            painter.FillRect (dx, cy + armHalf - (float) s - thick, thick, thick, ink);
        }
        else
        {
            float  dy = cy - armHalf * 0.5f + (float) s;

            painter.FillRect (cx - armHalf + (float) s,         dy, thick, thick, ink);
            painter.FillRect (cx + armHalf - (float) s - thick, dy, thick, thick, ink);
        }
    }

    if (home && group == 1)
    {
        PaintHomeTabs (painter, text, pal, x, ry, layoutW);
        return;
    }

    if (home)
    {
        hr = text.DrawString (m_groups[(size_t) group].label.c_str(), labelX, ry, layoutW - (labelX - x), rowH,
                              pal.fg, m_list.m_scaler.ToPxf (m_list.m_fontDip) * s_kHomeHeadingScale, m_list.GetBodyFace(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    hr = text.DrawString (m_groups[(size_t) group].label.c_str(), labelX, ry, layoutW - (labelX - x), rowH,
                          pal.fg, m_list.m_scaler.ToPxf (m_list.m_fontDip), m_list.GetBodyFace(),
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListGroups::PaintHomeTabs
//
//  The tabs as pills, each with its glyph, the active one in the accent color,
//  then the column titles under them. Each pill's place is kept for a press.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListGroups::PaintHomeTabs (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, float x, float ry, float layoutW) const
{
    static constexpr const wchar_t *  s_kGlyphFace = L"Segoe MDL2 Assets";



    const DxuiListViewTypes::HomeHeader  & header = m_list.m_homeHeader;
    float                                  fontPx = m_list.m_scaler.ToPxf (m_list.m_fontDip) * s_kHomeHeadingScale;
    float                                  pillH  = m_list.m_scaler.ToPxf (31.0f);
    float                                  pad    = m_list.m_scaler.ToPxf (11.0f);
    float                                  glyphW = m_list.m_scaler.ToPxf (24.0f);
    float                                  left   = x + m_list.m_scaler.ToPxf (60.0f);
    float                                  top    = ry + (m_list.m_scaler.ToPxf ((float) DxuiListItemLayout::s_kHomeHeaderDip) - pillH) * 0.5f;
    float                                  width  = layoutW - 2.0f * m_list.m_scaler.ToPxf ((float) DxuiListItemLayout::s_kHomeTileLeftDip);
    float                                  rowX   = x + m_list.m_scaler.ToPxf ((float) DxuiListItemLayout::s_kHomeTileLeftDip);
    float                                  titleY = ry + m_list.m_scaler.ToPxf (64.0f);
    float                                  titleH = m_list.m_scaler.ToPxf (24.0f);
    HRESULT                                hr     = S_OK;



    m_homeTabs.assign (header.tabs.size(), RECT {});

    for (size_t tab = 0; tab < header.tabs.size(); tab++)
    {
        float     textW  = 0.0f;
        float     textH  = 0.0f;
        bool      active = (int) tab == header.active;
        uint32_t  ink    = active ? m_list.m_theme->ContentBackground() : pal.fg;
        float     pillW  = 0.0f;

        hr = text.MeasureString (header.tabs[tab].c_str(), fontPx, DxuiTheme::kBodyFace, textW, textH);
        IGNORE_RETURN_VALUE (hr, S_OK);

        pillW = pad + glyphW + textW + pad;

        painter.FillRoundedRect (left, top, pillW, pillH, pillH * 0.5f, active ? m_list.m_theme->Accent() : m_list.m_theme->ControlBackground());

        if (tab < header.glyphs.size())
        {
            hr = text.DrawString (header.glyphs[tab].c_str(), left + pad, top, glyphW, pillH, ink, fontPx, s_kGlyphFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        hr = text.DrawString (header.tabs[tab].c_str(), left + pad + glyphW, top, textW + pad, pillH, ink, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_homeTabs[tab] = RECT { (LONG) left - m_list.m_boundsDip.left, (LONG) top - m_list.m_boundsDip.top, (LONG) (left + pillW) - m_list.m_boundsDip.left, (LONG) (top + pillH) - m_list.m_boundsDip.top };
        left           += pillW + m_list.m_scaler.ToPxf (8.0f);
    }

    hr = text.DrawString (L"Name", rowX + m_list.m_scaler.ToPxf (76.0f), titleY, width, titleH, pal.fg, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (size_t column = 0; column < header.columns.size() && column < header.columnAt.size(); column++)
    {
        hr = text.DrawString (header.columns[column].c_str(), rowX + width * header.columnAt[column], titleY, width, titleH, pal.fg, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListGroups::GetGroupOfRow
//
//  The group a row belongs to, or -1 for a row ahead of the first group.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListGroups::GetGroupOfRow (int row) const
{
    int  group = -1;



    for (size_t g = 0; g < m_groups.size() && m_groups[g].firstRow <= row; g++)
    {
        group = (int) g;
    }

    return group;
}
