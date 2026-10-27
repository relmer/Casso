#include "Pch.h"

#include "Widgets/ListView/DxuiListItemLayout.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::DxuiListItemLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiListItemLayout::DxuiListItemLayout (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemMetrics
//
//  Every view but Details lays its entries out as items of one size. The big
//  icon views put the name under the icon; the rest put it beside. List runs
//  its items down columns and scrolls sideways; the others run them along
//  rows and scroll down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ItemMetrics DxuiListItemLayout::GetItemMetrics (DxuiListViewTypes::View view)
{
    switch (view)
    {
        //  Explorer's pitches, measured at 150%. Its icon views size a row to
        //  the longest wrapped name in it, from one line, each name line 17
        //  dip below the last.
        case DxuiListViewTypes::View::ExtraLargeIcons: return DxuiListViewTypes::ItemMetrics { 191, 193, 256, false, true,  1, 17 };
        case DxuiListViewTypes::View::LargeIcons:      return DxuiListViewTypes::ItemMetrics { 115, 118,  96, false, true,  1, 17 };
        case DxuiListViewTypes::View::MediumIcons:     return DxuiListViewTypes::ItemMetrics {  76,  70,  48, false, true,  1, 17 };
        case DxuiListViewTypes::View::SmallIcons:      return DxuiListViewTypes::ItemMetrics { 240,  34,  16, false, false, 1 };
        case DxuiListViewTypes::View::List:            return DxuiListViewTypes::ItemMetrics { 240,  33,  16, true,  false, 1 };
        case DxuiListViewTypes::View::Tiles:           return DxuiListViewTypes::ItemMetrics { 254,  56,  48, false, false, 3, 17 };
        case DxuiListViewTypes::View::Content:         return DxuiListViewTypes::ItemMetrics {   0,  53,  32, false, false, 2 };
        default:                    return DxuiListViewTypes::ItemMetrics {   0,  30,  16, false, false, 1 };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemIconPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemIconPx (DxuiListViewTypes::View view, UINT dpi)
{
    return (std::min) (MulDiv (GetItemMetrics (view).iconDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi), s_kMaxItemIconPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemGrid
//
//  How the items of the current view fit the list: the cell in pixels, how
//  many share a line (a row, or a column for List), and how many lines show.
//  The vertical bar's width is always kept back, so the count per row does not
//  change as the bar comes and goes.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ItemGrid DxuiListItemLayout::GetItemGrid() const
{
    DxuiListViewTypes::ItemGrid     grid;
    DxuiListViewTypes::ItemMetrics  metrics = GetItemMetrics (m_list.m_view);
    int                             fullW   = m_list.m_boundsDip.right  - m_list.m_boundsDip.left;
    int                             fullH   = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int                             barW    = m_list.GetScrollbarWidthPx();
    int                             rows    = m_list.GetRowCount();



    //  A big icon's row is its fixed part and one caption line, which is the
    //  font's height in GDI's measure, as Explorer's rows are.
    grid.cellH = metrics.labelBelow              ? GetItemIconPx (m_list.m_view, m_list.m_scaler.GetDpi()) + (int) std::lround (m_list.m_scaler.ToPxf (s_kItemBoxPadDip))
                                                   + (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kItemRowGapDip)) + GetCaptionLinePx()
               : (m_list.m_view == DxuiListViewTypes::View::Tiles)      ? GetTileRowPx (0)
               : (m_list.m_view == DxuiListViewTypes::View::Content)    ? (int) std::lround (m_list.m_scaler.ToPxf ((m_list.m_scaler.GetDpi() >= 144) ? s_kContentRowDip : s_kContentRowLowDpiDip))
               : (m_list.m_view == DxuiListViewTypes::View::SmallIcons) ? GetSmallRowPx()
               : (m_list.m_view == DxuiListViewTypes::View::List)       ? (std::max) (GetSmallRowPx(), (int) std::lround (m_list.m_scaler.ToPxf ((float) metrics.cellHDip)))
                                              : m_list.m_scaler.ToPx (metrics.cellHDip);

    //  Once the names are measured, Small icons and List are as wide as they
    //  need: List's widest column here, its others in GetListColumnLefts.
    if ((m_list.m_view == DxuiListViewTypes::View::SmallIcons || m_list.m_view == DxuiListViewTypes::View::List) && !m_list.m_itemNameWPx.empty() && m_list.m_itemNameWPx.size() == (size_t) rows)
    {
        int  widest = GetNameCellWPx (m_itemNameWidestPx);

        metrics.cellWDip = 0;
        grid.cellW       = (m_list.m_view == DxuiListViewTypes::View::SmallIcons) ? (std::min) (widest, m_list.m_scaler.ToPx (s_kSmallIconsMaxWDip)) : widest;
        grid.boxW        = grid.cellW - (int) std::lround (m_list.m_scaler.ToPxf ((grid.cellW < widest) ? s_kSmallIconsGapDip : DxuiListView::s_kNameColumnGapDip));
    }

    if (metrics.columns)
    {
        grid.cellW = (grid.cellW > 0) ? grid.cellW : m_list.m_scaler.ToPx (metrics.cellWDip);
        grid.boxW  = (grid.boxW > 0) ? grid.boxW : grid.cellW;
        //  Below the top margin, and above the horizontal scrollbar's row,
        //  kept back as Explorer keeps it.
        grid.perLine = (grid.cellH > 0) ? (std::max) (1, (fullH - m_list.m_scaler.ToPx (s_kItemsTopDip) - (int) std::lround (m_list.m_scaler.ToPxf (s_kItemsBarDip))) / grid.cellH) : 1;
        grid.lines   = (rows + grid.perLine - 1) / grid.perLine;
        grid.visible = (grid.cellW > 0) ? (std::max) (1, fullW / grid.cellW) : 1;

        return grid;
    }

    grid.visible = (grid.cellH > 0) ? (std::max) (1, fullH / grid.cellH) : 1;

    if (metrics.labelBelow)
    {
        SpreadItemBoxes (grid, fullW, fullH, rows);
        return grid;
    }

    //  A row as wide as the pane allows: past the left margin, short of the
    //  scrollbar's column and a little more.
    grid.cellW   = (grid.cellW > 0) ? grid.cellW : (metrics.cellWDip > 0) ? m_list.m_scaler.ToPx (metrics.cellWDip)
                 : (std::max) (1, fullW - m_list.GetItemsLeftPx() - (int) std::lround (m_list.m_scaler.ToPxf (s_kItemsBarDip + s_kContentEndGapDip)));
    grid.boxW    = (m_list.m_view == DxuiListViewTypes::View::Tiles) ? grid.cellW - (int) std::lround (m_list.m_scaler.ToPxf (s_kTileGapDip)) : grid.boxW;
    grid.boxW    = (grid.boxW > 0) ? grid.boxW : grid.cellW;
    grid.perLine = (grid.cellW > 0) ? (std::max) (1, (fullW - barW - m_list.GetItemsLeftPx()) / grid.cellW) : 1;
    grid.lines   = (rows + grid.perLine - 1) / grid.perLine;

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::ComputeItemScrollLayout
//
//  List scrolls sideways through its columns and never down; every other
//  item view scrolls down through its rows and never sideways. The row
//  capacity is counted in items, so the scrollbar and paging arithmetic that
//  Details uses reads the same way here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ScrollLayout DxuiListItemLayout::ComputeItemScrollLayout() const
{
    DxuiListViewTypes::ScrollLayout  layout;
    DxuiListViewTypes::ItemGrid      grid   = GetItemGrid();
    int                              fullW  = m_list.m_boundsDip.right - m_list.m_boundsDip.left;
    int                              barW   = m_list.GetScrollbarWidthPx();



    if (GetItemMetrics (m_list.m_view).columns)
    {
        layout.contentW  = m_list.m_flow.HasListGroups() ? m_list.m_flow.GetListLayout().totalW : m_list.m_flow.GetListColumnLefts().back();
        layout.hBar      = layout.contentW > fullW;
        layout.vBar      = false;
        layout.rowCap    = m_list.GetRowCount();
        layout.viewportW = fullW;

        return layout;
    }

    //  Grouped, the view scrolls by lines, a header or a row of items each.
    if (UsesItemLayout())
    {
        const DxuiListViewTypes::ItemLayout & items = GetItemLayout();

        layout.vBar      = items.totalH > m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
        layout.hBar      = false;
        layout.rowCap    = grid.visible;
        layout.viewportW = fullW - (layout.vBar ? barW : 0);
        layout.contentW  = layout.viewportW;

        return layout;
    }

    layout.vBar      = grid.lines > grid.visible;
    layout.hBar      = false;
    layout.rowCap    = grid.visible * grid.perLine;
    layout.viewportW = fullW - (layout.vBar ? barW : 0);
    layout.contentW  = layout.viewportW;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::HasItemGroups
//
//  Groups in an item view that scrolls down; List lays its groups out as
//  columns of their own instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::HasItemGroups() const
{
    return !m_list.m_groups.empty() && m_list.IsItemsView() && !GetItemMetrics (m_list.m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::UsesItemLayout
//
//  Every item view that scrolls down lays its items out as lines, grouped or
//  not, so each row can be as tall as its tallest name.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::UsesItemLayout() const
{
    return m_list.IsItemsView() && !GetItemMetrics (m_list.m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::SyncItemTextLines
//
//  The measurements belong to one row set, view, width and font; any change
//  drops them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::SyncItemTextLines() const
{
    //  The box, not the pitch: names wrap to the box, and the pitch moves
    //  with the scrollbar, which the measured lines decide.
    ItemTextKey  key = { (int) m_list.m_view, GetItemGrid().boxW, m_list.GetRowCount(), m_list.m_rowsVersion, (int) (m_list.m_fontDip * 100.0f), (int) m_list.m_scaler.GetDpi() };



    if (key != m_itemTextKey)
    {
        m_itemTextKey = key;
        m_itemTextLines.assign ((size_t) (std::max) (m_list.GetRowCount(), 0), 0);
        m_list.m_itemTextVersion++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::MeasureItemTextLines
//
//  The lines each item on screen not yet measured wraps its name to. Whether
//  any of them needs more than the two lines a cell has room for, which
//  changes the layout.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::MeasureItemTextLines (IDxuiTextRenderer & text, const std::vector<std::pair<int, RECT>> & onScreen) const
{
    DxuiListViewTypes::ItemMetrics  metrics = GetItemMetrics (m_list.m_view);
    float                           fontPx  = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    bool                            taller  = false;



    if (!metrics.labelBelow && m_list.m_view != DxuiListViewTypes::View::Tiles)
    {
        return false;
    }

    SyncItemTextLines();

    for (const std::pair<int, RECT> & place : onScreen)
    {
        RECT    label   = GetItemLabelRectPx (place.second);
        size_t  lines   = 0;
        size_t  details = 0;

        if (place.first < 0 || (size_t) place.first >= m_itemTextLines.size() || m_itemTextLines[(size_t) place.first] != 0)
        {
            continue;
        }

        const std::vector<DxuiListViewTypes::Cell> & cells = m_list.GetRowCells (place.first);

        if (cells.empty())
        {
            continue;
        }

        lines = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace, (float) (label.right - label.left),
                                            (m_list.m_view == DxuiListViewTypes::View::Tiles) ? 2 : DxuiListView::s_kItemNameMaxLines, true, true).size();
        lines = std::clamp (lines, (size_t) 1, (size_t) DxuiListView::s_kItemNameMaxLines);

        //  A tile keeps its name's lines in the low bits and the details shown
        //  under it in the high ones.
        if (m_list.m_view == DxuiListViewTypes::View::Tiles)
        {
            details = cells[0].tileNameOnly ? 0 : (std::min) (cells[0].tileLines.size(), (size_t) DxuiListView::s_kTileMaxLines - lines);
            lines   = lines | (details << 4);
        }

        m_itemTextLines[(size_t) place.first] = (uint8_t) lines;
        taller = taller || (m_list.m_view == DxuiListViewTypes::View::Tiles) || (int) lines > metrics.textLines;
    }

    if (taller)
    {
        m_list.m_itemTextVersion++;
    }

    return taller;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemExtraPx
//
//  How much taller than the grid's cell an item is, for the name lines past
//  the two every cell has room for.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemExtraPx (int item) const
{
    DxuiListViewTypes::ItemMetrics  metrics = GetItemMetrics (m_list.m_view);
    int                             lines   = (item >= 0 && (size_t) item < m_itemTextLines.size()) ? m_itemTextLines[(size_t) item] : 0;



    if (m_list.m_view == DxuiListViewTypes::View::Tiles)
    {
        return (lines == 0) ? 0 : GetTileRowPx ((lines & 0x0F) + (lines >> 4)) - GetTileRowPx (0);
    }

    return (metrics.labelBelow && lines > metrics.textLines) ? (lines - metrics.textLines) * GetCaptionLinePx() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetSmallRowPx
//
//  A Small icons row: the captions' pitch and the icon, and two pixels more
//  from 150% up, as Explorer's measure at 100, 125 and 150%. List's row is
//  this or 33 dip, whichever is taller.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetSmallRowPx() const
{
    int  icon = GetItemIconPx (DxuiListViewTypes::View::SmallIcons, m_list.m_scaler.GetDpi());



    return GetCaptionLinePx() + icon + ((m_list.m_scaler.GetDpi() >= 144) ? 2 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetTileLinePx
//
//  A tile's text line, name or detail: one pixel more than the captions'.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetTileLinePx() const
{
    return GetCaptionLinePx() + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetTileRowPx
//
//  A tile row with this many text lines: its icon or its text, whichever is
//  taller, and the padding.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetTileRowPx (int lines) const
{
    int  icon = GetItemIconPx (DxuiListViewTypes::View::Tiles, m_list.m_scaler.GetDpi());
    int  pad  = (int) std::floor (m_list.m_scaler.ToPxf (s_kTilePadDip)) - s_kTilePadTrimPx;



    return (std::max) (icon, lines * GetTileLinePx()) + pad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCaptionLinePx
//
//  How far apart a big icon's caption lines are: the font's height in GDI's
//  measure, as Explorer stacks them, once a paint has measured it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetCaptionLinePx() const
{
    return (m_list.m_captionLinePx > 0) ? m_list.m_captionLinePx : m_list.m_scaler.ToPx (GetItemMetrics (m_list.m_view).lineDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::MeasureItemNames
//
//  Every item's name width, once for each row set and font, for the views
//  whose columns follow the names.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::MeasureItemNames (IDxuiTextRenderer & text) const
{
    HRESULT      hr     = S_OK;
    int          rows   = m_list.GetRowCount();
    float        fontPx = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    ItemTextKey  key    = { -1, 0, rows, m_list.m_rowsVersion, (int) (m_list.m_fontDip * 100.0f), (int) m_list.m_scaler.GetDpi() };



    if (key == m_list.m_itemNameKey && m_list.m_itemNameWPx.size() == (size_t) (std::max) (rows, 0))
    {
        return;
    }

    m_list.m_itemNameKey = key;
    m_itemNameWidestPx   = 0;
    m_list.m_itemNameWPx.assign ((size_t) (std::max) (rows, 0), 0);

    for (int row = 0; row < rows; row++)
    {
        std::wstring  name;
        float         w    = 0.0f;
        float         h    = 0.0f;

        if (m_rowNames)
        {
            name = m_rowNames (row);
        }
        else
        {
            const std::vector<DxuiListViewTypes::Cell> & cells = m_list.GetRowCells (row);

            name = cells.empty() ? std::wstring() : cells[0].text;
        }

        //  By GDI's advances, as Explorer sizes its columns and draws its names.
        hr = name.empty() ? S_OK : text.MeasureStringGdi (name.c_str(), fontPx, DxuiTheme::kBodyFace, w);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_list.m_itemNameWPx[(size_t) row] = (int) std::ceil (w);
        m_itemNameWidestPx                 = (std::max) (m_itemNameWidestPx, m_list.m_itemNameWPx[(size_t) row]);
    }

    m_list.m_itemTextVersion++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetNameCellWPx
//
//  The width of a cell whose name is this wide: the icon, the name, and
//  Explorer's gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetNameCellWPx (int textPx) const
{
    return GetItemIconLeftPx() + GetItemIconPx (m_list.m_view, m_list.m_scaler.GetDpi()) + m_list.GetNameTextGapPx() + textPx
         + (int) std::lround (m_list.m_scaler.ToPxf (s_kNameRightPadDip)) + (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kNameColumnGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetLayoutKey
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::LayoutKey DxuiListItemLayout::GetLayoutKey() const
{
    DxuiListViewTypes::LayoutKey  key;
    DxuiListViewTypes::ItemGrid   grid = GetItemGrid();



    SyncItemTextLines();

    key.names   = m_list.m_itemTextVersion;
    key.caption = GetCaptionLinePx();
    key.cellW   = grid.cellW;
    key.cellH   = grid.cellH;
    key.perLine = grid.perLine;
    key.fullH   = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    key.barW    = m_list.GetScrollbarWidthPx();
    key.headerH = m_list.GetRowHeightPx();
    key.indent  = m_list.m_scaler.ToPx (DxuiListView::s_kGroupLabelDip) - (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kListGroupItemsPullDip));
    key.rows    = m_list.GetRowCount();

    key.groups.reserve (m_list.m_groups.size());

    for (const DxuiListViewTypes::Group & group : m_list.m_groups)
    {
        key.groups.emplace_back (group.firstRow, group.collapsed);
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemLayout
//
//  Built again only when what it is built from changes.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiListViewTypes::ItemLayout & DxuiListItemLayout::GetItemLayout() const
{
    DxuiListViewTypes::LayoutKey  key = GetLayoutKey();



    if (!m_itemLayoutBuilt || !(key == m_itemLayoutKey))
    {
        m_itemLayout      = BuildItemLayout();
        m_itemLayoutKey   = std::move (key);
        m_itemLayoutBuilt = true;
    }

    return m_itemLayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::BuildItemLayout
//
//  A header line for each group, a row's height, then its items in rows of
//  the grid's width, each group starting a row of its own; none under a
//  collapsed header, other than openGroup's, which a slide draws open.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ItemLayout DxuiListItemLayout::BuildItemLayout (int openGroup) const
{
    DxuiListViewTypes::ItemLayout  layout;
    DxuiListViewTypes::ItemGrid    grid    = GetItemGrid();
    int                            rows    = m_list.GetRowCount();
    int                            headerH = m_list.GetRowHeightPx();
    int                            y       = m_list.m_scaler.ToPx (s_kItemsTopDip);
    bool                           grouped = !m_list.m_groups.empty();
    size_t                         runs    = grouped ? m_list.m_groups.size() : 1;



    layout.rowLine.assign ((size_t) (std::max) (rows, 0), -1);

    //  Without groups the items are one run, with no header over it.
    for (size_t g = 0; g < runs; g++)
    {
        int   start     = grouped ? std::clamp (m_list.m_groups[g].firstRow, 0, rows) : 0;
        int   end       = (grouped && g + 1 < m_list.m_groups.size()) ? std::clamp (m_list.m_groups[g + 1].firstRow, start, rows) : rows;
        bool  collapsed = grouped && m_list.m_groups[g].collapsed && (int) g != openGroup;

        if (grouped)
        {
            layout.lines.push_back (DxuiListViewTypes::ItemLine { (int) g, start, 0, y, headerH });
            y += headerH;
        }

        for (int first = start; first < end && !collapsed; first += grid.perLine)
        {
            int  count  = (std::min) (grid.perLine, end - first);
            int  height = grid.cellH;

            for (int item = first; item < first + count; item++)
            {
                layout.rowLine[(size_t) item] = (int) layout.lines.size();
                height = (std::max) (height, grid.cellH + GetItemExtraPx (item));
            }

            layout.lines.push_back (DxuiListViewTypes::ItemLine { -1, first, count, y, height });
            y += height;
        }
    }

    layout.totalH = y;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::FindItemLine
//
//  The line at a height in the layout's own space, or -1 past its end.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::FindItemLine (const DxuiListViewTypes::ItemLayout & layout, int y)
{
    for (size_t line = 0; line < layout.lines.size(); line++)
    {
        if (y >= layout.lines[line].top && y < layout.lines[line].top + layout.lines[line].height)
        {
            return (int) line;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::BeginPageSlide
//
//  Starts the slide from where the view was drawn to where it now rests,
//  when a page moved where it rests. Any scroll ends a slide under way, a
//  page's own included, so a page that moved nothing puts back `running`,
//  the slide that was under way, to run on.
//
//  Explorer's slide, measured at 150%, moves at one steady speed over 175 ms
//  for its whole length, and its first frame on screen is already about a
//  fifth of the way along. A frame here takes the time it is drawn, and a
//  window that paces its frames shows each one two vsyncs later, so the slide
//  starts two frames early: the first frame on screen is that far along, the
//  rows move in the same frame as the scrollbar's thumb, and the last frame
//  reaches the screen as the 175 ms run out. A restart keeps going at the new
//  length's speed rather than easing in again. A page pressed mid-slide
//  starts from where the view is drawn, but never more than the view's height
//  away, so a held key cannot leave the view pages behind. In an item view
//  the start is kept against a line, so it moves with what was drawn there
//  when names above it are measured.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::BeginPageSlide (const DxuiListViewTypes::PageSlide & running, int fromPx, int restBeforePx)
{
    constexpr int64_t                      kPageSlideLeadMs = 33;   // two frames at 60 Hz
    const DxuiListViewTypes::ItemLayout  * layout           = UsesItemLayout() ? &GetItemLayout() : nullptr;
    int                                    restPx           = GetRestScrollPx();
    int                                    viewPx           = GetScrollViewPx();
    int                                    line             = -1;



    if (!UsesPageSlide())
    {
        return;
    }

    if (restPx == restBeforePx)
    {
        m_list.m_pageSlide = running;
        return;
    }

    fromPx = std::clamp (fromPx, restPx - viewPx, restPx + viewPx);

    if (fromPx == restPx)
    {
        m_list.m_pageSlide = {};
        return;
    }

    //  The first line starting at or below where the view's top was drawn,
    //  which keeps its place when the line it cuts through grows.
    for (size_t at = 0; layout != nullptr && at < layout->lines.size() && line < 0; at++)
    {
        line = (layout->lines[at].top >= fromPx) ? (int) at : -1;
    }

    fromPx = (layout != nullptr && line >= 0) ? fromPx - layout->lines[(size_t) line].top : fromPx;

    m_list.m_pageSlide = { true, fromPx, line, m_list.GetClockMs() - kPageSlideLeadMs };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetPageSlidePx
//
//  How far from where it rests a sliding page is drawn, down positive: the
//  whole of the way at the start, none at the end, and in a straight line
//  between, as Explorer's is. An item view's start is found from where its
//  line lies now. A slide starts no more than a view away, and names measured
//  as it comes into sight can add most of another, but whatever else moved
//  the rest, it is never drawn more than two views away.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetPageSlidePx() const
{
    constexpr int                          kMaxSlideViews = 2;
    bool                                   inLine         = m_list.m_pageSlide.active && m_list.m_pageSlide.fromLine >= 0 && UsesItemLayout();
    const DxuiListViewTypes::ItemLayout  * layout         = inLine ? &GetItemLayout() : nullptr;
    int                                    fromPx         = m_list.m_pageSlide.fromPx;
    int                                    boundPx        = 0;
    float                                  t              = 0.0f;



    if (!m_list.m_pageSlide.active || !UsesPageSlide())
    {
        return 0;
    }

    if (layout != nullptr)
    {
        if (m_list.m_pageSlide.fromLine >= (int) layout->lines.size())
        {
            return 0;
        }

        fromPx += layout->lines[(size_t) m_list.m_pageSlide.fromLine].top;
    }

    boundPx = GetScrollViewPx() * kMaxSlideViews;
    t       = std::clamp ((float) (m_list.GetClockMs() - m_list.m_pageSlide.startMs) / (float) DxuiListView::s_kPageSlideMs, 0.0f, 1.0f);

    return std::clamp ((int) std::lround ((float) (GetRestScrollPx() - fromPx) * (1.0f - t)), -boundPx, boundPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::UsesPageSlide
//
//  Details and the views that scroll down slide a page into view; List, which
//  scrolls sideways, does not.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::UsesPageSlide() const
{
    return m_pageSlideEnabled && (m_list.m_view == DxuiListViewTypes::View::Details || UsesItemLayout());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetRestScrollPx
//
//  How far down the view rests scrolled, in pixels: Details' top row and the
//  lift a page down gives its rows, or an item view's own measure. List does
//  not scroll down.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetRestScrollPx() const
{
    if (UsesItemLayout())
    {
        return GetItemTopPx (GetItemLayout());
    }

    return m_list.IsItemsView() ? 0 : m_list.m_topRow * m_list.GetRowHeightPx() + m_list.m_scrollbars.GetRowShiftPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetScrollViewPx
//
//  The height the rows scroll through: Details' body under its header and
//  over any horizontal scrollbar, or an item view's whole height.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetScrollViewPx() const
{
    DxuiListViewTypes::ScrollLayout  layout;



    if (m_list.IsItemsView())
    {
        return m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    }

    layout = m_list.m_scrollbars.ComputeScrollLayout();

    return layout.rowCap * m_list.GetRowHeightPx() + layout.partialPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemTopPx
//
//  How far the layout is scrolled: the top of the first line shown. After a
//  page, the line it landed on holds the view, flush with the top or the
//  bottom edge, worked out again from the layout as it stands now, so the
//  line stays flush when a row above it grows as its names are measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemTopPx (const DxuiListViewTypes::ItemLayout & layout) const
{
    int  viewH = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int  px    = 0;



    if (m_list.m_itemAnchorLine >= 0 && m_list.m_itemAnchorLine < (int) layout.lines.size())
    {
        const DxuiListViewTypes::ItemLine & line = layout.lines[(size_t) m_list.m_itemAnchorLine];

        //  A line taller than the view shows from its top either way.
        px = (m_list.m_itemAnchorBottom && line.height <= viewH) ? line.top + line.height - viewH : line.top;

        return std::clamp (px, 0, (std::max) (0, layout.totalH - viewH));
    }

    //  From the first line, so the margin above it stays when the list is at its top.
    return (m_list.m_topRow >= 0 && m_list.m_topRow < (int) layout.lines.size()) ? layout.lines[(size_t) m_list.m_topRow].top - layout.lines[0].top : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetMaxItemTopLine
//
//  The first line that, at the top, still shows the end whole. The top line
//  is drawn at the margin, so the view starts that far above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetMaxItemTopLine (const DxuiListViewTypes::ItemLayout & layout) const
{
    int  viewH = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;



    for (size_t line = 0; line < layout.lines.size(); line++)
    {
        if (layout.totalH - (layout.lines[line].top - layout.lines[0].top) <= viewH)
        {
            return (int) line;
        }
    }

    return (std::max) (0, (int) layout.lines.size() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::EnsureItemLineVisible
//
//  A line whole in sight leaves the view alone, which keeps a page's line
//  flush with its edge. After a page, a line past the bottom comes in flush
//  with the bottom too; otherwise the line scrolled to is the top one, at the
//  margin, or the view moves down by lines until it ends under the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::EnsureItemLineVisible (const DxuiListViewTypes::ItemLayout & layout, int line)
{
    int  viewH  = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int  bottom = 0;
    int  top    = m_list.m_topRow;



    if (line < 0 || line >= (int) layout.lines.size() || m_list.m_itemKeys.IsItemLineWhollyShown (layout, line))
    {
        return;
    }

    if (line < m_list.m_topRow)
    {
        m_list.m_scrollbars.SetTopRow (line);
        return;
    }

    bottom = layout.lines[(size_t) line].top + layout.lines[(size_t) line].height;

    if (m_list.m_itemAnchorLine >= 0 && bottom > GetItemTopPx (layout) + viewH)
    {
        m_list.m_itemKeys.AnchorItemLine (line, true);
        return;
    }

    //  The view's top is the top line's less the margin above the first.
    while (top < line && bottom - (layout.lines[(size_t) top].top - layout.lines[0].top) > viewH)
    {
        top++;
    }

    m_list.m_scrollbars.SetTopRow (top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::IsFirstOfGroup
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::IsFirstOfGroup (int item) const
{
    return std::any_of (m_list.m_groups.begin(), m_list.m_groups.end(), [item] (const DxuiListViewTypes::Group & group) { return group.firstRow == item; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemBoxPx
//
//  How wide a big icon's box is.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemBoxPx() const
{
    if (m_list.m_view == DxuiListViewTypes::View::ExtraLargeIcons)
    {
        return GetItemIconPx (m_list.m_view, m_list.m_scaler.GetDpi()) + s_kXLargeBoxPadPx + (int) std::lround (m_list.m_scaler.ToPxf (s_kXLargeBoxPadDip));
    }

    //  A part pixel is dropped, as Explorer drops it.
    return (int) std::floor (m_list.m_scaler.ToPxf ((m_list.m_view == DxuiListViewTypes::View::MediumIcons) ? s_kMediumBoxDip : (m_list.m_scaler.GetDpi() >= 144) ? s_kLargeBoxDip : s_kLargeBoxLowDpiDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::SpreadItemBoxes
//
//  Explorer's big icon views, measured at 100, 125 and 150%: as many item
//  boxes as fit the width past the left margin and the scrollbar's column,
//  whether or not the rows overflow, and the width shared between them,
//  each box at the start of its share. The scrollbar's column comes out of
//  the shared width only while the rows overflow the pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::SpreadItemBoxes (DxuiListViewTypes::ItemGrid & grid, int fullW, int fullH, int rows) const
{
    int  bar   = (int) std::lround (m_list.m_scaler.ToPxf (s_kItemsBarDip));
    int  top   = m_list.m_scaler.ToPx (s_kItemsTopDip);
    int  space = fullW - m_list.GetItemsLeftPx();



    grid.boxW    = (std::max) (1, GetItemBoxPx());
    grid.perLine = (std::max) (1, (space - bar) / grid.boxW);

    if (GetItemLinesHPx (grid, rows) + top > fullH)
    {
        space -= bar;
    }

    grid.cellW = (std::max) (grid.boxW, space / grid.perLine);
    grid.lines = (rows + grid.perLine - 1) / grid.perLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetFirstItemLine
//
//  The first line of items in a grouped layout, past the headers above it;
//  the first line when there are no items.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetFirstItemLine (const DxuiListViewTypes::ItemLayout & layout)
{
    for (size_t at = 0; at < layout.lines.size(); at++)
    {
        if (layout.lines[at].group < 0 && layout.lines[at].count > 0)
        {
            return (int) at;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemLinesHPx
//
//  How tall the rows are at this many a line, each as tall as the longest
//  name measured in it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemLinesHPx (const DxuiListViewTypes::ItemGrid & grid, int rows) const
{
    std::array<int, 5>  key   = { grid.perLine, grid.cellH, rows, m_list.m_rowsVersion, m_list.m_itemTextVersion };
    int                 total = 0;



    //  The grid is asked for many times a frame; the walk is over every item.
    if (key == m_linesHKey)
    {
        return m_linesHPx;
    }

    for (int first = 0; first < rows; first += grid.perLine)
    {
        int  extra = 0;

        for (int item = first; item < rows && item < first + grid.perLine; item++)
        {
            extra = (std::max) (extra, GetItemExtraPx (item));
        }

        total += grid.cellH + extra;
    }

    m_linesHKey = key;
    m_linesHPx  = total;

    return total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetGroupedItemRectPx
//
//  An item's cell in a grouped item view, from a layout the caller built once.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::GetGroupedItemRectPx (const DxuiListViewTypes::ItemLayout & layout, int item, RECT & outRect) const
{
    DxuiListViewTypes::ItemGrid  grid = GetItemGrid();
    int                          at   = (item >= 0 && item < (int) layout.rowLine.size()) ? layout.rowLine[(size_t) item] : -1;



    if (at < 0)
    {
        return false;
    }

    outRect.left   = m_list.GetItemsLeftPx() + (item - layout.lines[(size_t) at].first) * grid.cellW;
    outRect.top    = layout.lines[(size_t) at].top - GetItemTopPx (layout);
    outRect.right  = outRect.left + grid.boxW;
    outRect.bottom = outRect.top  + grid.cellH + GetItemExtraPx (item);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemRectPx
//
//  An item's cell relative to the list's top-left, wherever it is scrolled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListItemLayout::GetItemRectPx (int item, RECT & outRect) const
{
    DxuiListViewTypes::ItemGrid  grid = GetItemGrid();
    int                          line = 0;
    int                          slot = 0;



    if (m_list.m_view == DxuiListViewTypes::View::Details || item < 0 || item >= m_list.GetRowCount() || grid.perLine <= 0)
    {
        return false;
    }

    if (m_list.m_flow.HasListGroups())
    {
        return m_list.m_flow.GetListItemRectPx (m_list.m_flow.GetListLayout(), item, outRect);
    }

    if (UsesItemLayout())
    {
        return GetGroupedItemRectPx (GetItemLayout(), item, outRect);
    }

    line = item / grid.perLine;
    slot = item % grid.perLine;

    if (GetItemMetrics (m_list.m_view).columns)
    {
        const std::vector<int> & lefts = m_list.m_flow.GetListColumnLefts();

        outRect.left   = lefts[(size_t) line] - m_list.m_leftPx;
        outRect.top    = m_list.m_scaler.ToPx (s_kItemsTopDip) + slot * grid.cellH;
        outRect.right  = lefts[(size_t) line + 1] - (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kNameColumnGapDip)) - m_list.m_leftPx;
        outRect.bottom = outRect.top + grid.cellH;

        return true;
    }

    outRect.left = slot * grid.cellW;
    outRect.top  = (line - m_list.m_topRow / grid.perLine) * grid.cellH;

    outRect.right  = outRect.left + grid.boxW;
    outRect.bottom = outRect.top  + grid.cellH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::HitTestItem
//
//  The item whose cell holds a point relative to the list, or -1 between
//  items, past the last one, or over a scrollbar. The cells are where they
//  are drawn, a sliding page's offset included.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::HitTestItem (int xPx, int yPx) const
{
    DxuiListViewTypes::ItemGrid      grid   = GetItemGrid();
    DxuiListViewTypes::ScrollLayout  layout = ComputeItemScrollLayout();
    int                              fullH  = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int                              barW   = m_list.GetScrollbarWidthPx();
    int                              item   = -1;



    if (xPx < 0 || yPx < 0 || xPx >= layout.viewportW || grid.cellW <= 0 || grid.cellH <= 0)
    {
        return -1;
    }

    if (UsesItemLayout())
    {
        const DxuiListViewTypes::ItemLayout  & items = GetItemLayout();
        int                                    at    = FindItemLine (items, yPx + m_list.GetShownScrollPx());
        int                                    slot  = (xPx >= m_list.GetItemsLeftPx() && (xPx - m_list.GetItemsLeftPx()) % grid.cellW < grid.boxW) ? (xPx - m_list.GetItemsLeftPx()) / grid.cellW : -1;

        return (at >= 0 && slot >= 0 && items.lines[(size_t) at].group < 0 && slot < items.lines[(size_t) at].count) ? items.lines[(size_t) at].first + slot : -1;
    }

    if (GetItemMetrics (m_list.m_view).columns)
    {
        if (layout.hBar && yPx >= fullH - barW)
        {
            return -1;
        }

        if (m_list.m_flow.HasListGroups())
        {
            return m_list.m_flow.HitTestListItem (m_list.m_flow.GetListLayout(), xPx, yPx);
        }

        yPx -= m_list.m_scaler.ToPx (s_kItemsTopDip);

        if (yPx < 0 || yPx / grid.cellH >= grid.perLine)
        {
            return -1;
        }

        item = m_list.m_flow.FindListColumn (xPx + m_list.m_leftPx);
        item = (item < 0) ? m_list.GetRowCount() : item * grid.perLine + yPx / grid.cellH;
    }
    else
    {
        if (xPx / grid.cellW >= grid.perLine)
        {
            return -1;
        }

        item = (m_list.m_topRow / grid.perLine + yPx / grid.cellH) * grid.perLine + xPx / grid.cellW;
    }

    return (item < m_list.GetRowCount()) ? item : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::EnsureItemVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::EnsureItemVisible (int item)
{
    DxuiListViewTypes::ItemGrid  grid = GetItemGrid();
    int                          line = 0;
    int                          top  = 0;



    if (item < 0 || item >= m_list.GetRowCount() || grid.perLine <= 0)
    {
        return;
    }

    if (UsesItemLayout())
    {
        const DxuiListViewTypes::ItemLayout & layout = GetItemLayout();

        EnsureItemLineVisible (layout, layout.rowLine[(size_t) item]);
        return;
    }

    line = item / grid.perLine;

    if (m_list.m_flow.HasListGroups())
    {
        RECT  cell  = {};
        int   viewW = m_list.m_boundsDip.right - m_list.m_boundsDip.left;

        if (m_list.m_flow.GetListItemRectPx (m_list.m_flow.GetListLayout(), item, cell))
        {
            if (cell.left < 0)
            {
                m_list.m_scrollbars.SetLeftPx (m_list.m_leftPx + cell.left);
            }
            else if (cell.right > viewW)
            {
                m_list.m_scrollbars.SetLeftPx (m_list.m_leftPx + cell.right - viewW);
            }
        }

        return;
    }

    if (GetItemMetrics (m_list.m_view).columns)
    {
        const std::vector<int> & lefts = m_list.m_flow.GetListColumnLefts();
        int                      viewW = m_list.m_boundsDip.right - m_list.m_boundsDip.left;
        int                      left  = lefts[(size_t) line];
        int                      right = lefts[(size_t) line + 1];

        if (left < m_list.m_leftPx)
        {
            m_list.m_scrollbars.SetLeftPx (left);
        }
        else if (right > m_list.m_leftPx + viewW)
        {
            m_list.m_scrollbars.SetLeftPx (right - viewW);
        }

        return;
    }

    top = m_list.m_topRow / grid.perLine;

    if (line < top)
    {
        m_list.m_scrollbars.SetTopRow (line * grid.perLine);
    }
    else if (line >= top + grid.visible)
    {
        m_list.m_scrollbars.SetTopRow ((line - grid.visible + 1) * grid.perLine);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemLabelRectPx
//
//  Where an item's name is drawn in its cell: under the icon for the big icon
//  views, beside it otherwise.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListItemLayout::GetItemLabelRectPx (const RECT & cell) const
{
    DxuiListViewTypes::ItemMetrics  metrics = GetItemMetrics (m_list.m_view);
    int                             pad     = m_list.m_scaler.ToPx (DxuiListView::s_kItemPadDip);
    int                             iconPx  = GetItemIconPx (m_list.m_view, m_list.m_scaler.GetDpi());
    int                             lineH   = metrics.labelBelow ? GetCaptionLinePx() : m_list.m_scaler.ToPx (metrics.lineDip);
    int                             gap     = pad;
    int                             right   = pad;



    //  Explorer's names under a big icon are narrower than the cell: measured
    //  from where its names wrap.
    if (metrics.labelBelow)
    {
        int  inset = m_list.m_scaler.ToPx (s_kLargeLabelInsetDip);
        int  width = (cell.right - cell.left) - inset * 2;

        //  Medium's and Large's are widths in dip rather than insets, 91 and
        //  138 px at 150% as measured where Explorer's names break; Extra
        //  large's is its icon's width less 2 px at any scale. Centered.
        if (m_list.m_view == DxuiListViewTypes::View::MediumIcons || m_list.m_view == DxuiListViewTypes::View::LargeIcons || m_list.m_view == DxuiListViewTypes::View::ExtraLargeIcons)
        {
            width = (m_list.m_view == DxuiListViewTypes::View::ExtraLargeIcons) ? iconPx - s_kXLargeLabelInsetPx
                  : (int) std::lround (m_list.m_scaler.ToPxf ((m_list.m_view == DxuiListViewTypes::View::MediumIcons) ? s_kMediumLabelWDip : s_kLargeLabelWDip));
            inset = ((cell.right - cell.left) - width) / 2;
        }

        int  top   = cell.top + m_list.m_scaler.ToPx (DxuiListView::GetIconTopDip (m_list.m_view)) + iconPx + m_list.m_scaler.ToPx (DxuiListView::GetLabelGapDip (m_list.m_view));

        return RECT { cell.left + inset, top, cell.left + inset + width, top + lineH * metrics.textLines };
    }

    //  Beside a small icon the name starts just past it and keeps clear of
    //  the box's right edge; a tile's text starts further out.
    if (m_list.m_view == DxuiListViewTypes::View::SmallIcons || m_list.m_view == DxuiListViewTypes::View::List)
    {
        gap   = m_list.GetNameTextGapPx();
        right = (int) std::lround (m_list.m_scaler.ToPxf (s_kNameRightPadDip));
    }
    else if (m_list.m_view == DxuiListViewTypes::View::Tiles)
    {
        gap = (int) std::lround (m_list.m_scaler.ToPxf (s_kTileTextGapDip));
    }
    else if (m_list.m_view == DxuiListViewTypes::View::Content)
    {
        gap = (int) std::lround (m_list.m_scaler.ToPxf (s_kContentNameGapDip));
    }

    return RECT { cell.left + GetItemIconLeftPx() + iconPx + gap, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2,
                  cell.right - right, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2 + lineH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemIconLeftPx
//
//  How far into its cell an item's icon starts, in a view with the name
//  beside it: Content's sits well in, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListItemLayout::GetItemIconLeftPx() const
{
    if (m_list.m_view == DxuiListViewTypes::View::SmallIcons || m_list.m_view == DxuiListViewTypes::View::List)
    {
        return (int) std::lround (m_list.m_scaler.ToPxf (s_kNameIconLeftDip));
    }

    if (m_list.m_view == DxuiListViewTypes::View::Tiles)
    {
        return (int) std::lround (m_list.m_scaler.ToPxf (s_kTileIconLeftDip));
    }

    return (m_list.m_view == DxuiListViewTypes::View::Content) ? (int) std::lround (m_list.m_scaler.ToPxf (s_kContentIconLeftDip)) : m_list.m_scaler.ToPx (DxuiListView::s_kItemPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::PlaceItemSlide
//
//  Where a grouped item view's lines go while a group slides open or shut:
//  the group's items come out from under its header, or go back under it,
//  and the lines below follow their edge. Inactive when nothing slides.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ItemSlide DxuiListItemLayout::PlaceItemSlide() const
{
    DxuiListViewTypes::ItemSlide  slide;
    float                         shown   = (UsesItemLayout() && !m_list.m_flow.HasListGroups()) ? m_list.m_grouping.GetGroupSlideShown() : -1.0f;
    int                           topPx   = 0;
    int                           bodyTop = 0;
    int                           bodyEnd = 0;



    if (shown < 0.0f)
    {
        return slide;
    }

    slide.layout = BuildItemLayout (m_list.m_groupSlide.group);
    topPx        = GetItemTopPx (GetItemLayout());

    for (size_t line = 0; line < slide.layout.lines.size(); line++)
    {
        if (slide.layout.lines[line].group == m_list.m_groupSlide.group)
        {
            slide.header = (int) line;
        }
        else if (slide.header >= 0 && slide.layout.lines[line].group >= 0)
        {
            bodyEnd = slide.layout.lines[line].top;
            break;
        }
    }

    if (slide.header < 0)
    {
        return slide;
    }

    bodyTop = slide.layout.lines[(size_t) slide.header].top + slide.layout.lines[(size_t) slide.header].height;
    bodyEnd = (bodyEnd > 0) ? bodyEnd : slide.layout.totalH;

    slide.clipH   = (int) ((float) (bodyEnd - bodyTop) * shown);
    slide.lift    = (bodyEnd - bodyTop) - slide.clipH;
    slide.clipTop = bodyTop - topPx;
    slide.shift   = GetItemTopPx (slide.layout) - topPx;
    slide.active  = true;

    m_list.m_grouping.GetGroupRowSpan (m_list.m_groupSlide.group, slide.start, slide.end);

    return slide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::CollectItemsToMeasure
//
//  The items whose names a paint measures, with their cells where they rest:
//  those on the lines in sight, or every item in a short folder, so whether
//  its rows overflow, which sets the columns, counts the ones a wrapped name
//  above has pushed out of sight.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::CollectItemsToMeasure (std::vector<std::pair<int, RECT>> & out) const
{
    const DxuiListViewTypes::ItemLayout  & layout = GetItemLayout();
    int                                    topPx  = GetItemTopPx (layout);
    int                                    viewH  = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    bool                                   every  = m_list.GetRowCount() <= s_kMeasureAllItemsMax;



    for (const DxuiListViewTypes::ItemLine & line : layout.lines)
    {
        if (!every && (line.top - topPx + line.height <= 0 || line.top - topPx >= viewH))
        {
            continue;
        }

        for (int item = line.first; item < line.first + line.count; item++)
        {
            RECT  cell = {};

            if (GetGroupedItemRectPx (layout, item, cell) && (every || (cell.bottom > 0 && cell.top < viewH)))
            {
                out.emplace_back (item, cell);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::CollectLineItemsInSight
//
//  A line's items that show, with their cells where they are drawn: moved by
//  a sliding page, and placed from a sliding group's layout, lifted under its
//  header.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::CollectLineItemsInSight (
    const DxuiListViewTypes::ItemLayout                   & layout,
    const DxuiListViewTypes::ItemLine                     & line,
    const DxuiListViewTypes::ItemSlide                    & slide,
    int                                  pageY,
    std::vector<std::pair<int, RECT>>  & out) const
{
    int  viewW = m_list.m_boundsDip.right  - m_list.m_boundsDip.left;
    int  viewH = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;



    for (int item = line.first; item < line.first + line.count; item++)
    {
        RECT  cell = {};

        if (!GetGroupedItemRectPx (layout, item, cell))
        {
            continue;
        }

        if (slide.active)
        {
            OffsetRect (&cell, 0, slide.shift - ((slide.layout.rowLine[(size_t) item] > slide.header) ? slide.lift : 0));
        }

        OffsetRect (&cell, 0, pageY);

        if (cell.bottom > 0 && cell.top < viewH && cell.right > 0 && cell.left < viewW)
        {
            out.emplace_back (item, cell);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::GetItemScrollOffsetPx
//
//  How far the item view is scrolled, so a point in the list's pixels can be
//  turned into one in the items' own space: as drawn, so a sliding page's
//  offset counts.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiListItemLayout::GetItemScrollOffsetPx() const
{
    DxuiListViewTypes::ItemGrid  grid = GetItemGrid();



    if (!m_list.IsItemsView())
    {
        return POINT { m_list.m_leftPx, m_list.m_topRow * m_list.GetRowHeightPx() - m_list.GetHeaderHeightPx() - (m_list.m_showHeader ? m_list.GetHeaderGapPx() : 0) };
    }

    if (GetItemMetrics (m_list.m_view).columns)
    {
        return POINT { m_list.m_leftPx, 0 };
    }

    if (UsesItemLayout())
    {
        return POINT { 0, m_list.GetShownScrollPx() };
    }

    return POINT { 0, (grid.perLine > 0) ? (m_list.m_topRow / grid.perLine) * grid.cellH : 0 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::BeginSelectionBand
//
//  Without Ctrl the band starts from nothing; with it, the band adds to the
//  selection the press found.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::BeginSelectionBand (int lx, int ly, bool ctrl)
{
    POINT  offset = GetItemScrollOffsetPx();



    m_list.m_bandActive = true;
    m_bandStart         = POINT { lx + offset.x, ly + offset.y };
    m_bandEnd           = m_bandStart;
    m_list.m_bandBase   = ctrl ? m_list.m_selectedRows : std::vector<int>();

    if (!ctrl)
    {
        m_list.ClearSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout::UpdateSelectionBand
//
//  Every item the band touches is selected, with what Ctrl kept.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemLayout::UpdateSelectionBand (int lx, int ly)
{
    POINT                                  offset  = GetItemScrollOffsetPx();
    RECT                                   band    = {};
    std::vector<int>                       rows    = m_list.m_bandBase;
    std::vector<int>                       before  = m_list.m_selectedRows;
    int                                    last    = -1;
    bool                                   listGrp = m_list.m_flow.HasListGroups();
    bool                                   itemGrp = !listGrp && UsesItemLayout();
    const DxuiListViewTypes::ListLayout  & listLay = listGrp ? m_list.m_flow.GetListLayout() : DxuiListView::s_kNoListLayout;
    const DxuiListViewTypes::ItemLayout  & itemLay = itemGrp ? GetItemLayout() : s_kNoItemLayout;



    m_bandEnd = POINT { lx + offset.x, ly + offset.y };

    band = RECT { (std::min) (m_bandStart.x, m_bandEnd.x), (std::min) (m_bandStart.y, m_bandEnd.y),
                  (std::max) (m_bandStart.x, m_bandEnd.x), (std::max) (m_bandStart.y, m_bandEnd.y) };

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
            placed = listGrp ? m_list.m_flow.GetListItemRectPx    (listLay, item, cell)
                             : itemGrp ? GetGroupedItemRectPx (itemLay, item, cell)
                                       : GetItemRectPx        (item, cell);

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
//  DxuiListItemLayout::GetSelectionBandPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListItemLayout::GetSelectionBandPx() const
{
    POINT  offset = GetItemScrollOffsetPx();



    if (!m_list.m_bandActive)
    {
        return RECT {};
    }

    return RECT { (std::min) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::min) (m_bandStart.y, m_bandEnd.y) - offset.y,
                  (std::max) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::max) (m_bandStart.y, m_bandEnd.y) - offset.y };
}
