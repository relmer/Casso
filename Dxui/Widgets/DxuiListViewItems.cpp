#include "Pch.h"

#include "Theme/DxuiTheme.h"
#include "Theme/DxuiRowLook.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiTextElide.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemMetrics
//
//  Every view but Details lays its entries out as items of one size. The big
//  icon views put the name under the icon; the rest put it beside. List runs
//  its items down columns and scrolls sideways; the others run them along
//  rows and scroll down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemMetrics DxuiListView::GetItemMetrics (View view)
{
    switch (view)
    {
        //  Explorer's pitches, measured at 150%. Its icon views size a row to
        //  the longest wrapped name in it, from one line, each name line 17
        //  dip below the last.
        case View::ExtraLargeIcons: return ItemMetrics { 191, 193, 256, false, true,  1, 17 };
        case View::LargeIcons:      return ItemMetrics { 115, 118,  96, false, true,  1, 17 };
        case View::MediumIcons:     return ItemMetrics {  76,  70,  48, false, true,  1, 17 };
        case View::SmallIcons:      return ItemMetrics { 240,  34,  16, false, false, 1 };
        case View::List:            return ItemMetrics { 240,  33,  16, true,  false, 1 };
        case View::Tiles:           return ItemMetrics { 254,  56,  48, false, false, 3, 17 };
        case View::Content:         return ItemMetrics {   0,  53,  32, false, false, 2 };
        default:                    return ItemMetrics {   0,  30,  16, false, false, 1 };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemIconPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemIconPx (View view, UINT dpi)
{
    return (std::min) (MulDiv (GetItemMetrics (view).iconDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi), s_kMaxItemIconPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SetView
//
//  The header belongs to Details alone, so it is put away while another view
//  shows and comes back with Details. The view opens at its start, with the
//  selected entry scrolled into sight.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetView (View view)
{
    if (view == m_view)
    {
        return;
    }

    if (m_view == View::Details)
    {
        m_detailsHeader = m_showHeader;
        m_showHeader    = false;
    }
    else if (view == View::Details)
    {
        m_showHeader = m_detailsHeader;
    }

    m_view           = view;
    m_topRow         = 0;
    m_leftPx         = 0;
    m_pageSlide      = {};
    m_flushBottom    = false;
    m_itemAnchorLine = -1;

    EnsureVisible (m_selectedRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemGrid
//
//  How the items of the current view fit the list: the cell in pixels, how
//  many share a line (a row, or a column for List), and how many lines show.
//  The vertical bar's width is always kept back, so the count per row does not
//  change as the bar comes and goes.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemGrid DxuiListView::GetItemGrid() const
{
    ItemGrid     grid;
    ItemMetrics  metrics = GetItemMetrics (m_view);
    int          fullW   = m_boundsDip.right  - m_boundsDip.left;
    int          fullH   = m_boundsDip.bottom - m_boundsDip.top;
    int          barW    = GetScrollbarWidthPx();
    int          rows    = GetRowCount();



    //  A big icon's row is its fixed part and one caption line, which is the
    //  font's height in GDI's measure, as Explorer's rows are.
    grid.cellH = metrics.labelBelow              ? GetItemIconPx (m_view, m_scaler.GetDpi()) + (int) std::lround (m_scaler.ToPxf (s_kItemBoxPadDip))
                                                   + (int) std::lround (m_scaler.ToPxf (s_kItemRowGapDip)) + GetCaptionLinePx()
               : (m_view == View::Tiles)      ? GetTileRowPx (0)
               : (m_view == View::Content)    ? (int) std::lround (m_scaler.ToPxf ((m_scaler.GetDpi() >= 144) ? s_kContentRowDip : s_kContentRowLowDpiDip))
               : (m_view == View::SmallIcons) ? GetSmallRowPx()
               : (m_view == View::List)       ? (std::max) (GetSmallRowPx(), (int) std::lround (m_scaler.ToPxf ((float) metrics.cellHDip)))
                                              : m_scaler.ToPx (metrics.cellHDip);

    //  Once the names are measured, Small icons and List are as wide as they
    //  need: List's widest column here, its others in GetListColumnLefts.
    if ((m_view == View::SmallIcons || m_view == View::List) && !m_itemNameWPx.empty() && m_itemNameWPx.size() == (size_t) rows)
    {
        int  widest = GetNameCellWPx (m_itemNameWidestPx);

        metrics.cellWDip = 0;
        grid.cellW       = (m_view == View::SmallIcons) ? (std::min) (widest, m_scaler.ToPx (s_kSmallIconsMaxWDip)) : widest;
        grid.boxW        = grid.cellW - (int) std::lround (m_scaler.ToPxf ((grid.cellW < widest) ? s_kSmallIconsGapDip : s_kNameColumnGapDip));
    }

    if (metrics.columns)
    {
        grid.cellW   = (grid.cellW > 0) ? grid.cellW : m_scaler.ToPx (metrics.cellWDip);
        grid.boxW    = (grid.boxW > 0) ? grid.boxW : grid.cellW;
        //  Below the top margin, and above the horizontal scrollbar's row,
        //  kept back as Explorer keeps it.
        grid.perLine = (grid.cellH > 0) ? (std::max) (1, (fullH - m_scaler.ToPx (s_kItemsTopDip) - (int) std::lround (m_scaler.ToPxf (s_kItemsBarDip))) / grid.cellH) : 1;
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
    grid.cellW   = (grid.cellW > 0) ? grid.cellW : (metrics.cellWDip > 0) ? m_scaler.ToPx (metrics.cellWDip)
                 : (std::max) (1, fullW - GetItemsLeftPx() - (int) std::lround (m_scaler.ToPxf (s_kItemsBarDip + s_kContentEndGapDip)));
    grid.boxW    = (m_view == View::Tiles) ? grid.cellW - (int) std::lround (m_scaler.ToPxf (s_kTileGapDip)) : grid.boxW;
    grid.boxW    = (grid.boxW > 0) ? grid.boxW : grid.cellW;
    grid.perLine = (grid.cellW > 0) ? (std::max) (1, (fullW - barW - GetItemsLeftPx()) / grid.cellW) : 1;
    grid.lines   = (rows + grid.perLine - 1) / grid.perLine;

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::ComputeItemScrollLayout
//
//  List scrolls sideways through its columns and never down; every other
//  item view scrolls down through its rows and never sideways. The row
//  capacity is counted in items, so the scrollbar and paging arithmetic that
//  Details uses reads the same way here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ScrollLayout DxuiListView::ComputeItemScrollLayout() const
{
    ScrollLayout  layout;
    ItemGrid      grid  = GetItemGrid();
    int           fullW = m_boundsDip.right - m_boundsDip.left;
    int           barW  = GetScrollbarWidthPx();



    if (GetItemMetrics (m_view).columns)
    {
        layout.contentW  = HasListGroups() ? GetListLayout().totalW : GetListColumnLefts().back();
        layout.hBar      = layout.contentW > fullW;
        layout.vBar      = false;
        layout.rowCap    = GetRowCount();
        layout.viewportW = fullW;

        return layout;
    }

    //  Grouped, the view scrolls by lines, a header or a row of items each.
    if (UsesItemLayout())
    {
        const ItemLayout & items = GetItemLayout();

        layout.vBar      = items.totalH > m_boundsDip.bottom - m_boundsDip.top;
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
//  DxuiListView::HasItemGroups
//
//  Groups in an item view that scrolls down; List lays its groups out as
//  columns of their own instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HasItemGroups() const
{
    return !m_groups.empty() && IsItemsView() && !GetItemMetrics (m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::UsesItemLayout
//
//  Every item view that scrolls down lays its items out as lines, grouped or
//  not, so each row can be as tall as its tallest name.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::UsesItemLayout() const
{
    return IsItemsView() && !GetItemMetrics (m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SyncItemTextLines
//
//  The measurements belong to one row set, view, width and font; any change
//  drops them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SyncItemTextLines() const
{
    //  The box, not the pitch: names wrap to the box, and the pitch moves
    //  with the scrollbar, which the measured lines decide.
    ItemTextKey  key = { (int) m_view, GetItemGrid().boxW, GetRowCount(), m_rowsVersion, (int) (m_fontDip * 100.0f) };



    if (key != m_itemTextKey)
    {
        m_itemTextKey = key;
        m_itemTextLines.assign ((size_t) (std::max) (GetRowCount(), 0), 0);
        m_itemTextVersion++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::MeasureItemTextLines
//
//  The lines each item on screen not yet measured wraps its name to. Whether
//  any of them needs more than the two lines a cell has room for, which
//  changes the layout.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::MeasureItemTextLines (IDxuiTextRenderer & text, const std::vector<std::pair<int, RECT>> & onScreen) const
{
    ItemMetrics  metrics = GetItemMetrics (m_view);
    float        fontPx  = m_scaler.ToPxf (m_fontDip);
    bool         taller  = false;



    if (!metrics.labelBelow && m_view != View::Tiles)
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

        const std::vector<Cell> & cells = GetRowCells (place.first);

        if (cells.empty())
        {
            continue;
        }

        lines = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace, (float) (label.right - label.left),
                                            (m_view == View::Tiles) ? 2 : s_kItemNameMaxLines, true, true).size();
        lines = std::clamp (lines, (size_t) 1, (size_t) s_kItemNameMaxLines);

        //  A tile keeps its name's lines in the low bits and the details shown
        //  under it in the high ones.
        if (m_view == View::Tiles)
        {
            details = cells[0].tileNameOnly ? 0 : (std::min) (cells[0].tileLines.size(), (size_t) s_kTileMaxLines - lines);
            lines   = lines | (details << 4);
        }

        m_itemTextLines[(size_t) place.first] = (uint8_t) lines;
        taller = taller || (m_view == View::Tiles) || (int) lines > metrics.textLines;
    }

    if (taller)
    {
        m_itemTextVersion++;
    }

    return taller;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemExtraPx
//
//  How much taller than the grid's cell an item is, for the name lines past
//  the two every cell has room for.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemExtraPx (int item) const
{
    ItemMetrics  metrics = GetItemMetrics (m_view);
    int          lines   = (item >= 0 && (size_t) item < m_itemTextLines.size()) ? m_itemTextLines[(size_t) item] : 0;



    if (m_view == View::Tiles)
    {
        return (lines == 0) ? 0 : GetTileRowPx ((lines & 0x0F) + (lines >> 4)) - GetTileRowPx (0);
    }

    return (metrics.labelBelow && lines > metrics.textLines) ? (lines - metrics.textLines) * GetCaptionLinePx() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetSmallRowPx
//
//  A Small icons row: the captions' pitch and the icon, and two pixels more
//  from 150% up, as Explorer's measure at 100, 125 and 150%. List's row is
//  this or 33 dip, whichever is taller.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetSmallRowPx() const
{
    int  icon = GetItemIconPx (View::SmallIcons, m_scaler.GetDpi());



    return GetCaptionLinePx() + icon + ((m_scaler.GetDpi() >= 144) ? 2 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetTileLinePx
//
//  A tile's text line, name or detail: one pixel more than the captions'.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetTileLinePx() const
{
    return GetCaptionLinePx() + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetTileRowPx
//
//  A tile row with this many text lines: its icon or its text, whichever is
//  taller, and the padding.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetTileRowPx (int lines) const
{
    int  icon = GetItemIconPx (View::Tiles, m_scaler.GetDpi());
    int  pad  = (int) std::floor (m_scaler.ToPxf (s_kTilePadDip)) - s_kTilePadTrimPx;



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

int DxuiListView::GetCaptionLinePx() const
{
    return (m_captionLinePx > 0) ? m_captionLinePx : m_scaler.ToPx (GetItemMetrics (m_view).lineDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::MeasureItemNames
//
//  Every item's name width, once for each row set and font, for the views
//  whose columns follow the names.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::MeasureItemNames (IDxuiTextRenderer & text) const
{
    HRESULT      hr     = S_OK;
    int          rows   = GetRowCount();
    float        fontPx = m_scaler.ToPxf (m_fontDip);
    ItemTextKey  key    = { -1, 0, rows, m_rowsVersion, (int) (m_fontDip * 100.0f) };



    if (key == m_itemNameKey && m_itemNameWPx.size() == (size_t) (std::max) (rows, 0))
    {
        return;
    }

    m_itemNameKey      = key;
    m_itemNameWidestPx = 0;
    m_itemNameWPx.assign ((size_t) (std::max) (rows, 0), 0);

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
            const std::vector<Cell> & cells = GetRowCells (row);

            name = cells.empty() ? std::wstring() : cells[0].text;
        }

        //  By GDI's advances, as Explorer sizes its columns and draws its names.
        hr = name.empty() ? S_OK : text.MeasureStringGdi (name.c_str(), fontPx, DxuiTheme::kBodyFace, w);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_itemNameWPx[(size_t) row] = (int) std::ceil (w);
        m_itemNameWidestPx          = (std::max) (m_itemNameWidestPx, m_itemNameWPx[(size_t) row]);
    }

    m_itemTextVersion++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetNameCellWPx
//
//  The width of a cell whose name is this wide: the icon, the name, and
//  Explorer's gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetNameCellWPx (int textPx) const
{
    return GetItemIconLeftPx() + GetItemIconPx (m_view, m_scaler.GetDpi()) + GetNameTextGapPx() + textPx
         + (int) std::lround (m_scaler.ToPxf (s_kNameRightPadDip)) + (int) std::lround (m_scaler.ToPxf (s_kNameColumnGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetListColumnLefts
//
//  Where each of List's columns starts, each as wide as the widest name in
//  it, with one more entry for where the last ends. Uniform columns before
//  the names are measured.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<int> & DxuiListView::GetListColumnLefts() const
{
    ItemGrid             grid    = GetItemGrid();
    int                  rows    = GetRowCount();
    bool                 sized   = m_itemNameWPx.size() == (size_t) rows && rows > 0;
    std::pair<int, int>  key     = { grid.perLine, sized ? m_itemTextVersion : -1 };
    int                  x       = 0;



    if (key == m_listColumnsKey && !m_listColumnLefts.empty())
    {
        return m_listColumnLefts;
    }

    m_listColumnsKey = key;
    x                = GetItemsLeftPx();
    m_listColumnLefts.assign (1, x);

    for (int first = 0; first < rows; first += grid.perLine)
    {
        int  widest = 0;

        for (int item = first; sized && item < (std::min) (rows, first + grid.perLine); item++)
        {
            widest = (std::max) (widest, m_itemNameWPx[(size_t) item]);
        }

        x += sized ? GetNameCellWPx (widest) : grid.cellW;
        m_listColumnLefts.push_back (x);
    }

    return m_listColumnLefts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::FindListColumn
//
//  List's column at a point in its content's own pixels, or -1 past them.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::FindListColumn (int x) const
{
    const std::vector<int> & lefts = GetListColumnLefts();
    auto                     after = std::upper_bound (lefts.begin(), lefts.end(), x);



    if (x < 0 || after == lefts.begin() || after == lefts.end())
    {
        return -1;
    }

    return (int) (after - lefts.begin()) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetLayoutKey
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::LayoutKey DxuiListView::GetLayoutKey() const
{
    LayoutKey  key;
    ItemGrid   grid = GetItemGrid();



    SyncItemTextLines();

    key.names   = m_itemTextVersion;
    key.caption = GetCaptionLinePx();
    key.cellW   = grid.cellW;
    key.cellH   = grid.cellH;
    key.perLine = grid.perLine;
    key.fullH   = m_boundsDip.bottom - m_boundsDip.top;
    key.barW    = GetScrollbarWidthPx();
    key.headerH = GetRowHeightPx();
    key.indent  = m_scaler.ToPx (s_kGroupLabelDip);
    key.rows    = GetRowCount();

    key.groups.reserve (m_groups.size());

    for (const Group & group : m_groups)
    {
        key.groups.emplace_back (group.firstRow, group.collapsed);
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemLayout
//
//  Built again only when what it is built from changes.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiListView::ItemLayout & DxuiListView::GetItemLayout() const
{
    LayoutKey  key = GetLayoutKey();



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
//  DxuiListView::BuildItemLayout
//
//  A header line for each group, a row's height, then its items in rows of
//  the grid's width, each group starting a row of its own; none under a
//  collapsed header, other than openGroup's, which a slide draws open.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemLayout DxuiListView::BuildItemLayout (int openGroup) const
{
    ItemLayout  layout;
    ItemGrid    grid    = GetItemGrid();
    int         rows    = GetRowCount();
    int         headerH = GetRowHeightPx();
    int         y       = m_scaler.ToPx (s_kItemsTopDip);
    bool        grouped = !m_groups.empty();
    size_t      runs    = grouped ? m_groups.size() : 1;



    layout.rowLine.assign ((size_t) (std::max) (rows, 0), -1);

    //  Without groups the items are one run, with no header over it.
    for (size_t g = 0; g < runs; g++)
    {
        int   start     = grouped ? std::clamp (m_groups[g].firstRow, 0, rows) : 0;
        int   end       = (grouped && g + 1 < m_groups.size()) ? std::clamp (m_groups[g + 1].firstRow, start, rows) : rows;
        bool  collapsed = grouped && m_groups[g].collapsed && (int) g != openGroup;

        if (grouped)
        {
            layout.lines.push_back (ItemLine { (int) g, start, 0, y, headerH });
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

            layout.lines.push_back (ItemLine { -1, first, count, y, height });
            y += height;
        }
    }

    layout.totalH = y;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::FindItemLine
//
//  The line at a height in the layout's own space, or -1 past its end.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::FindItemLine (const ItemLayout & layout, int y)
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
//  DxuiListView::BeginPageSlide
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

void DxuiListView::BeginPageSlide (const PageSlide & running, int fromPx, int restBeforePx)
{
    constexpr int64_t    kPageSlideLeadMs = 33;   // two frames at 60 Hz
    const ItemLayout   * layout           = UsesItemLayout() ? &GetItemLayout() : nullptr;
    int                  restPx           = GetRestScrollPx();
    int                  viewPx           = GetScrollViewPx();
    int                  line             = -1;



    if (!UsesPageSlide())
    {
        return;
    }

    if (restPx == restBeforePx)
    {
        m_pageSlide = running;
        return;
    }

    fromPx = std::clamp (fromPx, restPx - viewPx, restPx + viewPx);

    if (fromPx == restPx)
    {
        m_pageSlide = {};
        return;
    }

    //  The first line starting at or below where the view's top was drawn,
    //  which keeps its place when the line it cuts through grows.
    for (size_t at = 0; layout != nullptr && at < layout->lines.size() && line < 0; at++)
    {
        line = (layout->lines[at].top >= fromPx) ? (int) at : -1;
    }

    fromPx = (layout != nullptr && line >= 0) ? fromPx - layout->lines[(size_t) line].top : fromPx;

    m_pageSlide = { true, fromPx, line, GetClockMs() - kPageSlideLeadMs };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetPageSlidePx
//
//  How far from where it rests a sliding page is drawn, down positive: the
//  whole of the way at the start, none at the end, and in a straight line
//  between, as Explorer's is. An item view's start is found from where its
//  line lies now. A slide starts no more than a view away, and names measured
//  as it comes into sight can add most of another, but whatever else moved
//  the rest, it is never drawn more than two views away.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetPageSlidePx() const
{
    constexpr int       kMaxSlideViews = 2;
    bool                inLine         = m_pageSlide.active && m_pageSlide.fromLine >= 0 && UsesItemLayout();
    const ItemLayout  * layout         = inLine ? &GetItemLayout() : nullptr;
    int                 fromPx         = m_pageSlide.fromPx;
    int                 boundPx        = 0;
    float               t              = 0.0f;



    if (!m_pageSlide.active || !UsesPageSlide())
    {
        return 0;
    }

    if (layout != nullptr)
    {
        if (m_pageSlide.fromLine >= (int) layout->lines.size())
        {
            return 0;
        }

        fromPx += layout->lines[(size_t) m_pageSlide.fromLine].top;
    }

    boundPx = GetScrollViewPx() * kMaxSlideViews;
    t       = std::clamp ((float) (GetClockMs() - m_pageSlide.startMs) / (float) s_kPageSlideMs, 0.0f, 1.0f);

    return std::clamp ((int) std::lround ((float) (GetRestScrollPx() - fromPx) * (1.0f - t)), -boundPx, boundPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::UsesPageSlide
//
//  Details and the views that scroll down slide a page into view; List, which
//  scrolls sideways, does not.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::UsesPageSlide() const
{
    return m_pageSlideEnabled && (m_view == View::Details || UsesItemLayout());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetRestScrollPx
//
//  How far down the view rests scrolled, in pixels: Details' top row and the
//  lift a page down gives its rows, or an item view's own measure. List does
//  not scroll down.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetRestScrollPx() const
{
    if (UsesItemLayout())
    {
        return GetItemTopPx (GetItemLayout());
    }

    return IsItemsView() ? 0 : m_topRow * GetRowHeightPx() + GetRowShiftPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetScrollViewPx
//
//  The height the rows scroll through: Details' body under its header and
//  over any horizontal scrollbar, or an item view's whole height.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetScrollViewPx() const
{
    ScrollLayout  layout;



    if (IsItemsView())
    {
        return m_boundsDip.bottom - m_boundsDip.top;
    }

    layout = ComputeScrollLayout();

    return layout.rowCap * GetRowHeightPx() + layout.partialPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemTopPx
//
//  How far the layout is scrolled: the top of the first line shown. After a
//  page, the line it landed on holds the view, flush with the top or the
//  bottom edge, worked out again from the layout as it stands now, so the
//  line stays flush when a row above it grows as its names are measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemTopPx (const ItemLayout & layout) const
{
    int  viewH = m_boundsDip.bottom - m_boundsDip.top;
    int  px    = 0;



    if (m_itemAnchorLine >= 0 && m_itemAnchorLine < (int) layout.lines.size())
    {
        const ItemLine & line = layout.lines[(size_t) m_itemAnchorLine];

        //  A line taller than the view shows from its top either way.
        px = (m_itemAnchorBottom && line.height <= viewH) ? line.top + line.height - viewH : line.top;

        return std::clamp (px, 0, (std::max) (0, layout.totalH - viewH));
    }

    //  From the first line, so the margin above it stays when the list is at its top.
    return (m_topRow >= 0 && m_topRow < (int) layout.lines.size()) ? layout.lines[(size_t) m_topRow].top - layout.lines[0].top : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetMaxItemTopLine
//
//  The first line that, at the top, still shows the end whole. The top line
//  is drawn at the margin, so the view starts that far above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetMaxItemTopLine (const ItemLayout & layout) const
{
    int  viewH = m_boundsDip.bottom - m_boundsDip.top;



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
//  DxuiListView::EnsureItemLineVisible
//
//  A line whole in sight leaves the view alone, which keeps a page's line
//  flush with its edge. After a page, a line past the bottom comes in flush
//  with the bottom too; otherwise the line scrolled to is the top one, at the
//  margin, or the view moves down by lines until it ends under the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::EnsureItemLineVisible (const ItemLayout & layout, int line)
{
    int  viewH  = m_boundsDip.bottom - m_boundsDip.top;
    int  bottom = 0;
    int  top    = m_topRow;



    if (line < 0 || line >= (int) layout.lines.size() || IsItemLineWhollyShown (layout, line))
    {
        return;
    }

    if (line < m_topRow)
    {
        SetTopRow (line);
        return;
    }

    bottom = layout.lines[(size_t) line].top + layout.lines[(size_t) line].height;

    if (m_itemAnchorLine >= 0 && bottom > GetItemTopPx (layout) + viewH)
    {
        AnchorItemLine (line, true);
        return;
    }

    //  The view's top is the top line's less the margin above the first.
    while (top < line && bottom - (layout.lines[(size_t) top].top - layout.lines[0].top) > viewH)
    {
        top++;
    }

    SetTopRow (top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardGroupedItemNav
//
//  As the grid lies on screen, with each group's header a stop between its
//  rows and the group above, as in Details. Left and Right step along the
//  items in order, or close and open a focused header. The page keys, Home
//  and End go to HandleKeyboardItemPage and HandleKeyboardItemJump.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardGroupedItemNav (WPARAM vk, bool shift, bool ctrl)
{
    ItemLayout  layout = GetItemLayout();
    int         lines  = (int) layout.lines.size();
    int         cur    = GetSelectedRow();
    int         line   = -1;
    int         slot   = 0;
    int         next   = -1;
    int         item   = -1;



    if (lines <= 0)
    {
        return false;
    }

    if (m_focusGroup >= 0)
    {
        for (int l = 0; l < lines; l++)
        {
            if (layout.lines[(size_t) l].group == m_focusGroup)
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

    if (m_focusGroup >= 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        SetGroupCollapsed (m_focusGroup, vk == VK_LEFT);
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

    EnsureItemLineVisible (layout, next);

    //  Ctrl moves the focus alone, leaving the selection for Space.
    if (m_multiSelect && ctrl && !shift)
    {
        m_focusGroup  = (item < 0) ? layout.lines[(size_t) next].group : -1;
        m_selectedRow = (item < 0) ? m_selectedRow : item;
        return true;
    }

    if (item < 0)
    {
        SelectGroup (layout.lines[(size_t) next].group);
        return true;
    }

    m_focusGroup = -1;

    if (m_multiSelect && shift)
    {
        SelectRangeFromAnchor (item);
    }
    else
    {
        SetSelectedRow (item);
    }

    if (m_multiSelect && m_onSelectionChanged)
    {
        m_onSelectionChanged (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardItemPage
//
//  Page Up and Page Down in a view that scrolls down, as Explorer's, measured
//  at 150%: as Details pages its rows, over the lines of items, the focus
//  keeping its place along the line, or the last place a shorter line has. A
//  focused header pages from its line at the first place, as an item under a
//  collapsed header does from that header's line.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardItemPage (const ItemLayout & layout, WPARAM vk, int line, int slot, bool shift, bool ctrl)
{
    bool  down        = (vk == VK_NEXT);
    int   rows        = GetRowCount();
    int   page        = GetPageLines();
    int   cur         = line;
    int   group       = -1;
    int   first       = -1;
    int   last        = -1;
    int   target      = -1;
    int   item        = -1;
    int   savedTop    = m_topRow;
    int   savedAnchor = m_itemAnchorLine;
    bool  savedBottom = m_itemAnchorBottom;
    bool  scroll      = false;



    if (!TryGetWholeItemLineSpan (layout, first, last))
    {
        return false;
    }

    if (cur < 0 && m_focusGroup < 0 && m_selectedRow >= 0 && m_selectedRow < rows)
    {
        group = GetGroupOfRow (m_selectedRow);

        for (size_t at = 0; at < layout.lines.size() && group >= 0 && cur < 0; at++)
        {
            cur = (layout.lines[at].group == group) ? (int) at : -1;
        }
    }

    target = IsAtPageEdge (cur, down ? last : first, down) ? StepItemLines (layout, cur, down ? page : -page)
                                                            : (down ? last : first);

    //  A header with no items past it in the key's direction.
    if (target < 0)
    {
        return true;
    }

    item   = layout.lines[(size_t) target].first + (std::min) (slot, layout.lines[(size_t) target].count - 1);
    scroll = !IsItemLineWhollyShown (layout, target);

    ApplyKeyboardMove (item, shift, ctrl);

    if (!scroll)
    {
        SetTopRow (savedTop);
        m_itemAnchorLine   = savedAnchor;
        m_itemAnchorBottom = savedBottom;
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
//  DxuiListView::HandleKeyboardItemJump
//
//  Home selects the first item, past any header over it, with the view at
//  its very top; End selects the last, with the view at its very end. With
//  every group collapsed there is no item, and the first or last header
//  takes the focus instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardItemJump (const ItemLayout & layout, WPARAM vk, bool shift, bool ctrl)
{
    bool  home   = (vk == VK_HOME);
    int   lines  = (int) layout.lines.size();
    int   last   = GetLastItemLine (layout);
    int   target = home ? GetFirstItemLine (layout) : last;
    int   group  = -1;
    int   item   = -1;



    if (last < 0)
    {
        group = layout.lines[(size_t) (home ? 0 : lines - 1)].group;

        if (m_multiSelect && ctrl && !shift)
        {
            m_focusGroup = group;
        }
        else
        {
            SelectGroup (group);
        }

        SetTopRow (home ? 0 : GetMaxTopRow());
        return true;
    }

    item = layout.lines[(size_t) target].first + (home ? 0 : layout.lines[(size_t) target].count - 1);

    ApplyKeyboardMove (item, shift, ctrl);

    if (home)
    {
        SetTopRow (0);
    }
    else
    {
        ScrollItemLineToBottom (target);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetLastItemLine
//
//  The last line of items in a layout, past the headers under it; -1 when
//  there are no items.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetLastItemLine (const ItemLayout & layout)
{
    for (int at = (int) layout.lines.size() - 1; at >= 0; at--)
    {
        if (IsItemLine (layout.lines[(size_t) at]))
        {
            return at;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::StepItemLines
//
//  The line of items `steps` such lines on from a line, down for a positive
//  count: headers are not counted, and the count stops at the first or the
//  last. A line of items with none past it gives itself; a header with none
//  past it gives -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::StepItemLines (const ItemLayout & layout, int line, int steps)
{
    int  lines  = (int) layout.lines.size();
    int  dir    = (steps < 0) ? -1 : 1;
    int  left   = (steps < 0) ? -steps : steps;
    int  result = (line >= 0 && line < lines && IsItemLine (layout.lines[(size_t) line])) ? line : -1;
    int  at     = line;



    while (left > 0)
    {
        at += dir;

        if (at < 0 || at >= lines)
        {
            break;
        }

        if (IsItemLine (layout.lines[(size_t) at]))
        {
            result = at;
            left--;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::TryGetWholeItemLineSpan
//
//  The first and last lines of items shown whole. With none whole in sight,
//  as when a line is taller than the view, the line of items nearest the top
//  stands for both. False only when there are no items to show.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::TryGetWholeItemLineSpan (const ItemLayout & layout, int & first, int & last) const
{
    int  topPx   = GetItemTopPx (layout);
    int  viewH   = m_boundsDip.bottom - m_boundsDip.top;
    int  lines   = (int) layout.lines.size();
    int  start   = -1;
    int  nearest = -1;



    first = -1;
    last  = -1;

    for (int at = 0; at < lines && layout.lines[(size_t) at].top < topPx + viewH; at++)
    {
        const ItemLine & line = layout.lines[(size_t) at];

        if (IsItemLine (line) && line.top >= topPx && line.top + line.height <= topPx + viewH)
        {
            first = (first < 0) ? at : first;
            last  = at;
        }
    }

    if (first >= 0 || lines <= 0)
    {
        return first >= 0;
    }

    start   = FindItemLine (layout, topPx);
    start   = (start >= 0) ? start : ((topPx < layout.lines[0].top) ? 0 : lines - 1);
    nearest = IsItemLine (layout.lines[(size_t) start]) ? start : StepItemLines (layout, start, 1);
    nearest = (nearest >= 0) ? nearest : StepItemLines (layout, start, -1);
    first   = nearest;
    last    = nearest;

    return nearest >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::IsItemLineWhollyShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsItemLineWhollyShown (const ItemLayout & layout, int line) const
{
    int  topPx = GetItemTopPx (layout);
    int  viewH = m_boundsDip.bottom - m_boundsDip.top;



    if (line < 0 || line >= (int) layout.lines.size())
    {
        return false;
    }

    return layout.lines[(size_t) line].top >= topPx && layout.lines[(size_t) line].top + layout.lines[(size_t) line].height <= topPx + viewH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::AnchorItemLine
//
//  Holds the view with a line's top at the view's top, or its bottom at the
//  view's bottom. The top row follows to the line at the margin's height, so
//  the scrollbar, the wheel and the arrows continue from about here, and a
//  wheel notch still moves the way it turns.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::AnchorItemLine (int line, bool bottom)
{
    const ItemLayout & layout = GetItemLayout();
    int                px     = 0;
    int                top    = 0;



    if (line < 0 || line >= (int) layout.lines.size())
    {
        return;
    }

    //  A group mid-slide is laid out with it open, where the line is another.
    m_groupSlide       = {};
    m_itemAnchorLine   = line;
    m_itemAnchorBottom = bottom;
    px                 = GetItemTopPx (layout);

    if (px <= 0)
    {
        SetTopRow (0);
        return;
    }

    top = FindItemLine (layout, px + layout.lines[0].top);

    //  The top row is set first, since setting it lets go of the line.
    SetTopRow ((top >= 0) ? top : (int) layout.lines.size() - 1);

    m_itemAnchorLine   = line;
    m_itemAnchorBottom = bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::ScrollItemLineToBottom
//
//  Holds a line flush with the view's bottom; the last line of items holds
//  the very end instead, so the headers of collapsed groups under it show.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ScrollItemLineToBottom (int line)
{
    const ItemLayout & layout = GetItemLayout();



    if (line == GetLastItemLine (layout))
    {
        AnchorItemLine ((int) layout.lines.size() - 1, true);

        if (IsItemLineWhollyShown (GetItemLayout(), line))
        {
            return;
        }
    }

    AnchorItemLine (line, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::ScrollItemLineToTop
//
//  Holds a line flush with the view's top; the first line of items scrolls
//  to the very top instead, so its margin and any header over it show.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ScrollItemLineToTop (int line)
{
    if (line == GetFirstItemLine (GetItemLayout()))
    {
        SetTopRow (0);
        return;
    }

    AnchorItemLine (line, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HasListGroups
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HasListGroups() const
{
    return !m_groups.empty() && IsItemsView() && GetItemMetrics (m_view).columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetListLayout
//
//  Built again only when what it is built from changes.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiListView::ListLayout & DxuiListView::GetListLayout() const
{
    LayoutKey  key = GetLayoutKey();



    if (!m_listLayoutBuilt || !(key == m_listLayoutKey))
    {
        m_listLayout      = BuildListLayout();
        m_listLayoutKey   = std::move (key);
        m_listLayoutBuilt = true;
    }

    return m_listLayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::BuildListLayout
//
//  Explorer's grouped List: each group a block of columns side by side, its
//  label across the top and its items under it, indented from the label and
//  running down each column before the next. A collapsed group keeps a
//  column's width for its label.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ListLayout DxuiListView::BuildListLayout() const
{
    ListLayout  layout;
    ItemGrid    grid   = GetItemGrid();
    int         rows   = GetRowCount();
    int         fullH  = m_boundsDip.bottom - m_boundsDip.top;
    int         x      = 0;



    layout.headerH = GetRowHeightPx();
    layout.indent  = m_scaler.ToPx (s_kGroupLabelDip);
    layout.perCol  = (grid.cellH > 0) ? (std::max) (1, (fullH - GetScrollbarWidthPx() - layout.headerH) / grid.cellH) : 1;
    layout.blockOf.assign ((size_t) (std::max) (rows, 0), -1);

    for (size_t g = 0; g < m_groups.size(); g++)
    {
        int        start = std::clamp (m_groups[g].firstRow, 0, rows);
        int        end   = (g + 1 < m_groups.size()) ? std::clamp (m_groups[g + 1].firstRow, start, rows) : rows;
        ListBlock  block;

        block.group = (int) g;
        block.first = start;
        block.count = m_groups[g].collapsed ? 0 : end - start;
        block.left  = x;
        block.width = layout.indent + (std::max) (1, (block.count + layout.perCol - 1) / layout.perCol) * grid.cellW;

        for (int item = start; item < start + block.count; item++)
        {
            layout.blockOf[(size_t) item] = (int) layout.blocks.size();
        }

        layout.blocks.push_back (block);
        x += block.width;
    }

    layout.totalW = x;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetListItemRectPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::GetListItemRectPx (const ListLayout & layout, int item, RECT & outRect) const
{
    ItemGrid  grid = GetItemGrid();
    int       at   = (item >= 0 && item < (int) layout.blockOf.size()) ? layout.blockOf[(size_t) item] : -1;
    int       k    = 0;



    if (at < 0)
    {
        return false;
    }

    k              = item - layout.blocks[(size_t) at].first;
    outRect.left   = layout.blocks[(size_t) at].left + layout.indent + (k / layout.perCol) * grid.cellW - m_leftPx;
    outRect.top    = layout.headerH + (k % layout.perCol) * grid.cellH;
    outRect.right  = outRect.left + grid.boxW;
    outRect.bottom = outRect.top  + grid.cellH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::FindListBlock
//
//  The block at a distance from the content's left, or -1 past the last.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::FindListBlock (const ListLayout & layout, int x)
{
    for (size_t b = 0; b < layout.blocks.size(); b++)
    {
        if (x >= layout.blocks[b].left && x < layout.blocks[b].left + layout.blocks[b].width)
        {
            return (int) b;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HitTestListItem
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestListItem (const ListLayout & layout, int xPx, int yPx) const
{
    ItemGrid  grid = GetItemGrid();
    int       at   = FindListBlock (layout, xPx + m_leftPx);
    int       col  = 0;
    int       row  = 0;
    int       k    = 0;



    if (at < 0 || yPx < layout.headerH || grid.cellW <= 0 || grid.cellH <= 0)
    {
        return -1;
    }

    col = (xPx + m_leftPx - layout.blocks[(size_t) at].left - layout.indent);
    row = (yPx - layout.headerH) / grid.cellH;

    if (col < 0 || row >= layout.perCol)
    {
        return -1;
    }

    k = (col / grid.cellW) * layout.perCol + row;

    return (k < layout.blocks[(size_t) at].count) ? layout.blocks[(size_t) at].first + k : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardListGroupNav
//
//  Up and Down step along the items in order, with each group's header a stop
//  ahead of its first; Left and Right move a column, keeping the row, or
//  close and open a focused header.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardListGroupNav (WPARAM vk, bool shift, bool ctrl)
{
    ListLayout  layout = GetListLayout();
    int         rows   = GetRowCount();
    int         cur    = GetSelectedRow();
    int         item   = -1;
    int         group  = -1;
    int         at     = (cur >= 0 && cur < rows) ? layout.blockOf[(size_t) cur] : -1;
    int         k      = (at >= 0) ? cur - layout.blocks[(size_t) at].first : 0;



    if (rows <= 0 || layout.blocks.empty())
    {
        return false;
    }

    if (m_focusGroup >= 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        return true;
    }

    switch (vk)
    {
        case VK_UP:
            if (m_focusGroup >= 0)
            {
                //  From a header to the last item shown above it.
                for (int b = m_focusGroup - 1; b >= 0 && item < 0; b--)
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
            if (m_focusGroup >= 0)
            {
                item  = (layout.blocks[(size_t) m_focusGroup].count > 0) ? layout.blocks[(size_t) m_focusGroup].first : -1;
                group = (item < 0 && m_focusGroup + 1 < (int) layout.blocks.size()) ? m_focusGroup + 1 : -1;
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
        if (m_multiSelect && ctrl && !shift)
        {
            m_focusGroup = group;
        }
        else
        {
            SelectGroup (group);
        }

        SetLeftPx ((std::min) (m_leftPx, layout.blocks[(size_t) group].left));
        return true;
    }

    if (item < 0 || item >= rows || layout.blockOf[(size_t) item] < 0)
    {
        return true;
    }

    m_focusGroup = -1;

    if (m_multiSelect && ctrl && !shift)
    {
        m_selectedRow = item;
    }
    else if (m_multiSelect && shift)
    {
        SelectRangeFromAnchor (item);
    }
    else
    {
        SetSelectedRow (item);
    }

    EnsureItemVisible (item);

    if (m_multiSelect && m_onSelectionChanged && !(ctrl && !shift))
    {
        m_onSelectionChanged (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetListColumns
//
//  List's columns left to right, grouped or not. A collapsed group has none,
//  since its header holds no items.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::ListColumn> DxuiListView::GetListColumns() const
{
    std::vector<ListColumn>    cols;
    ItemGrid                   grid  = GetItemGrid();
    int                        rows  = GetRowCount();
    int                        gapPx = (int) std::lround (m_scaler.ToPxf (s_kNameColumnGapDip));
    const std::vector<int>   & lefts = GetListColumnLefts();



    if (HasListGroups())
    {
        const ListLayout & layout = GetListLayout();

        for (const ListBlock & block : layout.blocks)
        {
            for (int k = 0; k < block.count; k += layout.perCol)
            {
                int  left = block.left + layout.indent + (k / layout.perCol) * grid.cellW;

                cols.push_back (ListColumn { left, left + grid.boxW, block.first + k, (std::min) (layout.perCol, block.count - k) });
            }
        }

        return cols;
    }

    //  Each column's box ends short of the next by the gap after its names.
    for (size_t c = 0; c + 1 < lefts.size(); c++)
    {
        int  first = (int) c * grid.perLine;

        cols.push_back (ListColumn { lefts[c], lefts[c + 1] - gapPx, first, (std::min) (grid.perLine, rows - first) });
    }

    return cols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::FindColumnOfItem
//
//  The column holding an item, or -1 for one under a collapsed header.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::FindColumnOfItem (const std::vector<ListColumn> & cols, int item)
{
    for (size_t c = 0; c < cols.size(); c++)
    {
        if (item >= cols[c].first && item < cols[c].first + cols[c].count)
        {
            return (int) c;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::TryGetWholeListColumnSpan
//
//  The first and last columns List shows whole. With none whole in sight, as
//  when a name is wider than the view, the column at the view's left stands
//  for both. False only when there are no columns.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::TryGetWholeListColumnSpan (const std::vector<ListColumn> & cols, int & first, int & last) const
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
        first = (cols[c].right > m_leftPx) ? (int) c : -1;
    }

    first = (first >= 0) ? first : (int) cols.size() - 1;
    last  = (last >= 0) ? last : first;

    return !cols.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::IsListColumnWhollyShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsListColumnWhollyShown (const ListColumn & col) const
{
    return col.left >= m_leftPx && col.right <= m_leftPx + (m_boundsDip.right - m_boundsDip.left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetListPageStartColumn
//
//  The column a page starts from when the focus is on a group's header, or on
//  an item under a collapsed one: the first column at or past the group's
//  block going right, or the last before it going left; -1 for none.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetListPageStartColumn (const std::vector<ListColumn> & cols, bool down) const
{
    int  rows  = GetRowCount();
    int  group = (m_focusGroup >= 0) ? m_focusGroup : ((m_selectedRow >= 0 && m_selectedRow < rows) ? GetGroupOfRow (m_selectedRow) : -1);
    int  col   = -1;
    int  x     = 0;



    if (group < 0 || !HasListGroups() || group >= (int) GetListLayout().blocks.size())
    {
        return -1;
    }

    x = GetListLayout().blocks[(size_t) group].left;

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
//  DxuiListView::HandleKeyboardListJump
//
//  List's Home and End, and its page keys, which go on to
//  HandleKeyboardListPage. Home selects the first item, past any header, with
//  the view at its left end; End selects the last, with the view at its right
//  end. With every group collapsed there is no item, and the first or last
//  header takes the focus instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardListJump (WPARAM vk, bool shift, bool ctrl)
{
    std::vector<ListColumn>  cols   = GetListColumns();
    bool                     home   = (vk == VK_HOME);
    bool                     paging = (vk == VK_PRIOR || vk == VK_NEXT);
    int                      group  = home ? 0 : (int) m_groups.size() - 1;



    if (cols.empty() && (paging || !HasListGroups()))
    {
        return false;
    }

    if (paging)
    {
        return HandleKeyboardListPage (cols, vk == VK_NEXT, shift, ctrl);
    }

    if (cols.empty())
    {
        if (m_multiSelect && ctrl && !shift)
        {
            m_focusGroup = group;
        }
        else
        {
            SelectGroup (group);
        }
    }
    else
    {
        ApplyKeyboardMove (home ? cols.front().first : cols.back().first + cols.back().count - 1, shift, ctrl);
    }

    SetLeftPx (home ? 0 : GetMaxLeftPx());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardListPage
//
//  Page Up and Page Down in List, which pages sideways, as Explorer's does,
//  measured at 150%: from a column short of the view's edge, to the column
//  whole at that edge, in the same row, without scrolling. From the edge
//  column, the view scrolls to put that column at the other edge, and the
//  focus goes to the same row of the column then whole at the edge paged
//  toward.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardListPage (const std::vector<ListColumn> & cols, bool down, bool shift, bool ctrl)
{
    int   n         = (int) cols.size();
    int   rows      = GetRowCount();
    int   viewW     = m_boundsDip.right - m_boundsDip.left;
    int   own       = (m_focusGroup < 0) ? FindColumnOfItem (cols, m_selectedRow) : -1;
    int   slot      = (own >= 0) ? m_selectedRow - cols[(size_t) own].first : 0;
    int   col       = (own >= 0) ? own : GetListPageStartColumn (cols, down);
    bool  focused   = m_focusGroup >= 0 || (m_selectedRow >= 0 && m_selectedRow < rows);
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

    if (!IsAtPageEdge (col, down ? last : first, down))
    {
        target = down ? last : first;
    }
    else if (down ? (col >= n - 1) : (col <= 0))
    {
        target = col;
    }
    else if (down)
    {
        SetLeftPx (cols[(size_t) col].left);
        TryGetWholeListColumnSpan (cols, first, last);
        target = (last > col) ? last : col + 1;
    }
    else
    {
        SetLeftPx (cols[(size_t) col].right - viewW);
        TryGetWholeListColumnSpan (cols, first, last);
        target = (first < col) ? first : col - 1;
        SetLeftPx ((target == 0) ? 0 : m_leftPx);
    }

    item      = cols[(size_t) target].first + (std::min) (slot, cols[(size_t) target].count - 1);
    leftAfter = m_leftPx;

    ApplyKeyboardMove (item, shift, ctrl);

    //  Selecting brings an item's column into sight with the gap after it,
    //  which would nudge a column already whole at the edge.
    SetLeftPx (leftAfter);

    if (!IsListColumnWhollyShown (cols[(size_t) target]))
    {
        EnsureItemVisible (item);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::IsFirstOfGroup
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsFirstOfGroup (int item) const
{
    return std::any_of (m_groups.begin(), m_groups.end(), [item] (const Group & group) { return group.firstRow == item; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemBoxPx
//
//  How wide a big icon's box is.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemBoxPx() const
{
    if (m_view == View::ExtraLargeIcons)
    {
        return GetItemIconPx (m_view, m_scaler.GetDpi()) + s_kXLargeBoxPadPx + (int) std::lround (m_scaler.ToPxf (s_kXLargeBoxPadDip));
    }

    //  A part pixel is dropped, as Explorer drops it.
    return (int) std::floor (m_scaler.ToPxf ((m_view == View::MediumIcons) ? s_kMediumBoxDip : (m_scaler.GetDpi() >= 144) ? s_kLargeBoxDip : s_kLargeBoxLowDpiDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SpreadItemBoxes
//
//  Explorer's big icon views, measured at 100, 125 and 150%: as many item
//  boxes as fit the width past the left margin and the scrollbar's column,
//  whether or not the rows overflow, and the width shared between them,
//  each box at the start of its share. The scrollbar's column comes out of
//  the shared width only while the rows overflow the pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SpreadItemBoxes (ItemGrid & grid, int fullW, int fullH, int rows) const
{
    int  bar   = (int) std::lround (m_scaler.ToPxf (s_kItemsBarDip));
    int  top   = m_scaler.ToPx (s_kItemsTopDip);
    int  space = fullW - GetItemsLeftPx();



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
//  DxuiListView::PaintSmallArtFrame
//
//  Explorer's frame around a big icon box whose art is smaller than it,
//  measured at 150% in the dark theme: two pixels a shade darker than the
//  background at the box's edge, then three pixels inside them that fade from
//  well lighter than the background back to it. The light theme takes the
//  same steps the other way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintSmallArtFrame (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg)
{
    static constexpr int  kRing[] = { 69, 46, 23 };
    int                   r       = (int) ((bg >> 16) & 0xFF);
    int                   g       = (int) ((bg >>  8) & 0xFF);
    int                   b       = (int) ( bg        & 0xFF);
    bool                  dark    = (r * 299 + g * 587 + b * 114) / 1000 < 128;
    int                   sign    = dark ? 1 : -1;
    int                   inset   = 2;



    painter.OutlineRect (x, y, sizePx, sizePx, 2.0f, ShiftGray (bg, -4 * sign));

    for (int step : kRing)
    {
        painter.OutlineRect (x + (float) inset, y + (float) inset, sizePx - (float) (2 * inset), sizePx - (float) (2 * inset), 1.0f, ShiftGray (bg, step * sign));
        inset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::ShiftGray
//
//  An opaque color this much lighter (or, negative, darker) in each channel.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiListView::ShiftGray (uint32_t argb, int delta)
{
    int  r = std::clamp ((int) ((argb >> 16) & 0xFF) + delta, 0, 255);
    int  g = std::clamp ((int) ((argb >>  8) & 0xFF) + delta, 0, 255);
    int  b = std::clamp ((int) ( argb        & 0xFF) + delta, 0, 255);



    return 0xFF000000u | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetFirstItemLine
//
//  The first line of items in a grouped layout, past the headers above it;
//  the first line when there are no items.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetFirstItemLine (const ItemLayout & layout)
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
//  DxuiListView::GetItemLinesHPx
//
//  How tall the rows are at this many a line, each as tall as the longest
//  name measured in it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemLinesHPx (const ItemGrid & grid, int rows) const
{
    std::array<int, 5>  key   = { grid.perLine, grid.cellH, rows, m_rowsVersion, m_itemTextVersion };
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
//  DxuiListView::GetGroupedItemRectPx
//
//  An item's cell in a grouped item view, from a layout the caller built once.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::GetGroupedItemRectPx (const ItemLayout & layout, int item, RECT & outRect) const
{
    ItemGrid  grid = GetItemGrid();
    int       at   = (item >= 0 && item < (int) layout.rowLine.size()) ? layout.rowLine[(size_t) item] : -1;



    if (at < 0)
    {
        return false;
    }

    outRect.left   = GetItemsLeftPx() + (item - layout.lines[(size_t) at].first) * grid.cellW;
    outRect.top    = layout.lines[(size_t) at].top - GetItemTopPx (layout);
    outRect.right  = outRect.left + grid.boxW;
    outRect.bottom = outRect.top  + grid.cellH + GetItemExtraPx (item);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemRectPx
//
//  An item's cell relative to the list's top-left, wherever it is scrolled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::GetItemRectPx (int item, RECT & outRect) const
{
    ItemGrid  grid = GetItemGrid();
    int       line = 0;
    int       slot = 0;



    if (m_view == View::Details || item < 0 || item >= GetRowCount() || grid.perLine <= 0)
    {
        return false;
    }

    if (HasListGroups())
    {
        return GetListItemRectPx (GetListLayout(), item, outRect);
    }

    if (UsesItemLayout())
    {
        return GetGroupedItemRectPx (GetItemLayout(), item, outRect);
    }

    line = item / grid.perLine;
    slot = item % grid.perLine;

    if (GetItemMetrics (m_view).columns)
    {
        const std::vector<int> & lefts = GetListColumnLefts();

        outRect.left   = lefts[(size_t) line] - m_leftPx;
        outRect.top    = m_scaler.ToPx (s_kItemsTopDip) + slot * grid.cellH;
        outRect.right  = lefts[(size_t) line + 1] - (int) std::lround (m_scaler.ToPxf (s_kNameColumnGapDip)) - m_leftPx;
        outRect.bottom = outRect.top + grid.cellH;

        return true;
    }

    outRect.left = slot * grid.cellW;
    outRect.top  = (line - m_topRow / grid.perLine) * grid.cellH;

    outRect.right  = outRect.left + grid.boxW;
    outRect.bottom = outRect.top  + grid.cellH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HitTestItem
//
//  The item whose cell holds a point relative to the list, or -1 between
//  items, past the last one, or over a scrollbar. The cells are where they
//  are drawn, a sliding page's offset included.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestItem (int xPx, int yPx) const
{
    ItemGrid      grid   = GetItemGrid();
    ScrollLayout  layout = ComputeItemScrollLayout();
    int           fullH  = m_boundsDip.bottom - m_boundsDip.top;
    int           barW   = GetScrollbarWidthPx();
    int           item   = -1;



    if (xPx < 0 || yPx < 0 || xPx >= layout.viewportW || grid.cellW <= 0 || grid.cellH <= 0)
    {
        return -1;
    }

    if (UsesItemLayout())
    {
        const ItemLayout  & items = GetItemLayout();
        int                 at    = FindItemLine (items, yPx + GetShownScrollPx());
        int                 slot  = (xPx >= GetItemsLeftPx() && (xPx - GetItemsLeftPx()) % grid.cellW < grid.boxW) ? (xPx - GetItemsLeftPx()) / grid.cellW : -1;

        return (at >= 0 && slot >= 0 && items.lines[(size_t) at].group < 0 && slot < items.lines[(size_t) at].count) ? items.lines[(size_t) at].first + slot : -1;
    }

    if (GetItemMetrics (m_view).columns)
    {
        if (layout.hBar && yPx >= fullH - barW)
        {
            return -1;
        }

        if (HasListGroups())
        {
            return HitTestListItem (GetListLayout(), xPx, yPx);
        }

        yPx -= m_scaler.ToPx (s_kItemsTopDip);

        if (yPx < 0 || yPx / grid.cellH >= grid.perLine)
        {
            return -1;
        }

        item = FindListColumn (xPx + m_leftPx);
        item = (item < 0) ? GetRowCount() : item * grid.perLine + yPx / grid.cellH;
    }
    else
    {
        if (xPx / grid.cellW >= grid.perLine)
        {
            return -1;
        }

        item = (m_topRow / grid.perLine + yPx / grid.cellH) * grid.perLine + xPx / grid.cellW;
    }

    return (item < GetRowCount()) ? item : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::EnsureItemVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::EnsureItemVisible (int item)
{
    ItemGrid  grid = GetItemGrid();
    int       line = 0;
    int       top  = 0;



    if (item < 0 || item >= GetRowCount() || grid.perLine <= 0)
    {
        return;
    }

    if (UsesItemLayout())
    {
        const ItemLayout & layout = GetItemLayout();

        EnsureItemLineVisible (layout, layout.rowLine[(size_t) item]);
        return;
    }

    line = item / grid.perLine;

    if (HasListGroups())
    {
        RECT  cell  = {};
        int   viewW = m_boundsDip.right - m_boundsDip.left;

        if (GetListItemRectPx (GetListLayout(), item, cell))
        {
            if (cell.left < 0)
            {
                SetLeftPx (m_leftPx + cell.left);
            }
            else if (cell.right > viewW)
            {
                SetLeftPx (m_leftPx + cell.right - viewW);
            }
        }

        return;
    }

    if (GetItemMetrics (m_view).columns)
    {
        const std::vector<int> & lefts = GetListColumnLefts();
        int                      viewW = m_boundsDip.right - m_boundsDip.left;
        int                      left  = lefts[(size_t) line];
        int                      right = lefts[(size_t) line + 1];

        if (left < m_leftPx)
        {
            SetLeftPx (left);
        }
        else if (right > m_leftPx + viewW)
        {
            SetLeftPx (right - viewW);
        }

        return;
    }

    top = m_topRow / grid.perLine;

    if (line < top)
    {
        SetTopRow (line * grid.perLine);
    }
    else if (line >= top + grid.visible)
    {
        SetTopRow ((line - grid.visible + 1) * grid.perLine);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::HandleKeyboardItemNav
//
//  Arrows move through the grid as it lies on screen: along a row with Left
//  and Right and between rows with Up and Down, or down a column and across
//  columns in List. List's page keys, Home and End go to
//  HandleKeyboardListJump.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HandleKeyboardItemNav (WPARAM vk, bool shift, bool ctrl)
{
    if (UsesItemLayout())
    {
        return HandleKeyboardGroupedItemNav (vk, shift, ctrl);
    }

    if (vk == VK_PRIOR || vk == VK_NEXT || vk == VK_HOME || vk == VK_END)
    {
        return HandleKeyboardListJump (vk, shift, ctrl);
    }

    if (HasListGroups())
    {
        return HandleKeyboardListGroupNav (vk, shift, ctrl);
    }

    ItemGrid  grid    = GetItemGrid();
    bool      columns = GetItemMetrics (m_view).columns;
    int       rows    = GetRowCount();
    int       cur     = (std::max) (0, GetSelectedRow());
    int       across  = columns ? grid.perLine : 1;
    int       down    = columns ? 1 : grid.perLine;
    int       next    = cur;
    bool      moved   = rows > 0;



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
    if (m_multiSelect && ctrl && !shift)
    {
        m_selectedRow = next;
        EnsureItemVisible (next);
        return true;
    }

    if (m_multiSelect && shift)
    {
        SelectRangeFromAnchor (next);
    }
    else
    {
        SetSelectedRow (next);
    }

    EnsureItemVisible (next);

    if (m_multiSelect && m_onSelectionChanged)
    {
        m_onSelectionChanged (next);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemLabelRectPx
//
//  Where an item's name is drawn in its cell: under the icon for the big icon
//  views, beside it otherwise.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListView::GetItemLabelRectPx (const RECT & cell) const
{
    ItemMetrics  metrics = GetItemMetrics (m_view);
    int          pad     = m_scaler.ToPx (s_kItemPadDip);
    int          iconPx  = GetItemIconPx (m_view, m_scaler.GetDpi());
    int          lineH   = metrics.labelBelow ? GetCaptionLinePx() : m_scaler.ToPx (metrics.lineDip);
    int          gap     = pad;
    int          right   = pad;



    //  Explorer's names under a big icon are narrower than the cell: measured
    //  from where its names wrap.
    if (metrics.labelBelow)
    {
        int  inset = m_scaler.ToPx (s_kLargeLabelInsetDip);
        int  width = (cell.right - cell.left) - inset * 2;

        //  Medium's and Large's are widths in dip rather than insets, 91 and
        //  138 px at 150% as measured where Explorer's names break; Extra
        //  large's is its icon's width less 2 px at any scale. Centered.
        if (m_view == View::MediumIcons || m_view == View::LargeIcons || m_view == View::ExtraLargeIcons)
        {
            width = (m_view == View::ExtraLargeIcons) ? iconPx - s_kXLargeLabelInsetPx
                  : (int) std::lround (m_scaler.ToPxf ((m_view == View::MediumIcons) ? s_kMediumLabelWDip : s_kLargeLabelWDip));
            inset = ((cell.right - cell.left) - width) / 2;
        }

        int  top   = cell.top + m_scaler.ToPx (GetIconTopDip (m_view)) + iconPx + m_scaler.ToPx (GetLabelGapDip (m_view));

        return RECT { cell.left + inset, top, cell.left + inset + width, top + lineH * metrics.textLines };
    }

    //  Beside a small icon the name starts just past it and keeps clear of
    //  the box's right edge; a tile's text starts further out.
    if (m_view == View::SmallIcons || m_view == View::List)
    {
        gap   = GetNameTextGapPx();
        right = (int) std::lround (m_scaler.ToPxf (s_kNameRightPadDip));
    }
    else if (m_view == View::Tiles)
    {
        gap = (int) std::lround (m_scaler.ToPxf (s_kTileTextGapDip));
    }
    else if (m_view == View::Content)
    {
        gap = (int) std::lround (m_scaler.ToPxf (s_kContentNameGapDip));
    }

    return RECT { cell.left + GetItemIconLeftPx() + iconPx + gap, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2,
                  cell.right - right, cell.top + ((cell.bottom - cell.top) - lineH * metrics.textLines) / 2 + lineH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemIconLeftPx
//
//  How far into its cell an item's icon starts, in a view with the name
//  beside it: Content's sits well in, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetItemIconLeftPx() const
{
    if (m_view == View::SmallIcons || m_view == View::List)
    {
        return (int) std::lround (m_scaler.ToPxf (s_kNameIconLeftDip));
    }

    if (m_view == View::Tiles)
    {
        return (int) std::lround (m_scaler.ToPxf (s_kTileIconLeftDip));
    }

    return (m_view == View::Content) ? (int) std::lround (m_scaler.ToPxf (s_kContentIconLeftDip)) : m_scaler.ToPx (s_kItemPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::PlaceItemSlide
//
//  Where a grouped item view's lines go while a group slides open or shut:
//  the group's items come out from under its header, or go back under it,
//  and the lines below follow their edge. Inactive when nothing slides.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ItemSlide DxuiListView::PlaceItemSlide() const
{
    ItemSlide  slide;
    float      shown    = (UsesItemLayout() && !HasListGroups()) ? GetGroupSlideShown() : -1.0f;
    int        topPx    = 0;
    int        bodyTop  = 0;
    int        bodyEnd  = 0;



    if (shown < 0.0f)
    {
        return slide;
    }

    slide.layout = BuildItemLayout (m_groupSlide.group);
    topPx        = GetItemTopPx (GetItemLayout());

    for (size_t line = 0; line < slide.layout.lines.size(); line++)
    {
        if (slide.layout.lines[line].group == m_groupSlide.group)
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

    GetGroupRowSpan (m_groupSlide.group, slide.start, slide.end);

    return slide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::CollectItemsToMeasure
//
//  The items whose names a paint measures, with their cells where they rest:
//  those on the lines in sight, or every item in a short folder, so whether
//  its rows overflow, which sets the columns, counts the ones a wrapped name
//  above has pushed out of sight.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::CollectItemsToMeasure (std::vector<std::pair<int, RECT>> & out) const
{
    const ItemLayout  & layout = GetItemLayout();
    int                 topPx  = GetItemTopPx (layout);
    int                 viewH  = m_boundsDip.bottom - m_boundsDip.top;
    bool                every  = GetRowCount() <= s_kMeasureAllItemsMax;



    for (const ItemLine & line : layout.lines)
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
//  DxuiListView::CollectLineItemsInSight
//
//  A line's items that show, with their cells where they are drawn: moved by
//  a sliding page, and placed from a sliding group's layout, lifted under its
//  header.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::CollectLineItemsInSight (
    const ItemLayout                   & layout,
    const ItemLine                     & line,
    const ItemSlide                    & slide,
    int                                  pageY,
    std::vector<std::pair<int, RECT>>  & out) const
{
    int  viewW = m_boundsDip.right  - m_boundsDip.left;
    int  viewH = m_boundsDip.bottom - m_boundsDip.top;



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
//  DxuiListView::PaintItems
//
//  Each item in sight: the selection or hover card, the icon at the view's
//  size, the name, and for Tiles and Content the other columns in the muted
//  color on the lines below.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintItems (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal, float x, float y) const
{
    HRESULT            hr      = S_OK;
    ItemMetrics        metrics = GetItemMetrics (m_view);
    ItemGrid           grid    = GetItemGrid();
    int                rows    = GetRowCount();
    int                first   = 0;
    int                last    = rows;
    int                pad     = m_scaler.ToPx (s_kItemPadDip);
    int                iconPx  = GetItemIconPx (m_view, m_scaler.GetDpi());
    int                lineH   = m_scaler.ToPx (metrics.lineDip);
    float              fontPx  = m_scaler.ToPxf (m_fontDip);
    bool               listGrp = HasListGroups();
    bool               itemGrp = !listGrp && UsesItemLayout();
    //  Built once here: building either per item made a frame cost the square
    //  of the item count.
    const ListLayout & listLayout = listGrp ? GetListLayout() : s_kNoListLayout;
    //  A group sliding open or shut paints from a layout that holds it open.
    ItemSlide          slide      = itemGrp ? PlaceItemSlide() : ItemSlide {};
    int                pageY      = 0;
    bool               clipped    = false;
    //  The items on screen and their cells, found before any row's cells are
    //  asked for: a host that builds rows on demand builds only these.
    std::vector<std::pair<int, RECT>>  onScreen;



    m_nameCut.clear();

    //  The captions' line pitch, which the layout's row heights are built from;
    //  a change moves every row, so the layout is laid out again.
    {
        float  linePx   = 0.0f;
        int    previous = m_captionLinePx;

        hr = text.GetLineHeightGdi (fontPx, DxuiTheme::kBodyFace, linePx);
        m_captionLinePx = (SUCCEEDED (hr) && linePx > 0.0f) ? (int) std::lround (linePx) : 0;

        if (m_captionLinePx != previous)
        {
            m_itemTextVersion++;
        }
    }

    //  Measured before anything is drawn: a name that wraps past two lines
    //  makes its row taller and moves everything under it, headers included.
    if (itemGrp)
    {
        CollectItemsToMeasure (onScreen);
        MeasureItemTextLines  (text, onScreen);

        //  A page sliding into view draws everything this far from where it
        //  rests, found once the names in sight are measured, since a row they
        //  make taller moves where the view rests.
        pageY = GetPageSlidePx();
    }

    onScreen.clear();

    if (listGrp)
    {
        const ListLayout & layout = listLayout;

        for (const ListBlock & block : layout.blocks)
        {
            float  left = x + (float) (block.left - m_leftPx);

            if (left + (float) block.width > x && left < x + (float) (m_boundsDip.right - m_boundsDip.left))
            {
                PaintGroupHeader (painter, text, pal, block.group, left, left, y, (float) block.width);
            }
        }

        first = 0;
        last  = rows;
    }
    else if (metrics.columns)
    {
        int  firstColumn = (std::max) (0, FindListColumn (m_leftPx));
        int  lastColumn  = FindListColumn (m_leftPx + (m_boundsDip.right - m_boundsDip.left));

        first = firstColumn * grid.perLine;
        last  = (lastColumn < 0) ? rows : (std::min) (rows, (lastColumn + 1) * grid.perLine);
    }
    else if (itemGrp)
    {
        const ItemLayout & layout = slide.active ? slide.layout : GetItemLayout();
        int                topPx  = GetItemTopPx (GetItemLayout());
        float              viewW  = (float) ComputeItemScrollLayout().viewportW;

        for (size_t at = 0; at < layout.lines.size(); at++)
        {
            const ItemLine & line  = layout.lines[at];
            int              lineY = line.top - topPx + pageY - ((slide.active && (int) at > slide.header) ? slide.lift : 0);

            if (lineY + line.height <= 0 || lineY >= m_boundsDip.bottom - m_boundsDip.top)
            {
                continue;
            }

            if (line.group >= 0)
            {
                PaintGroupHeader (painter, text, pal, line.group, x, x, y + (float) lineY, viewW);
            }

            //  Only the items of lines in sight are placed: placing every item
            //  made each frame cost the whole folder.
            CollectLineItemsInSight (layout, line, slide, pageY, onScreen);
        }

        first = 0;
        last  = 0;
    }
    else
    {
        first = m_topRow;
        last  = (std::min) (rows, first + (grid.visible + 1) * grid.perLine);
    }

    for (int item = first; item < last; item++)
    {
        RECT  cell   = {};
        bool  placed = listGrp ? GetListItemRectPx (listLayout, item, cell)
                               : GetItemRectPx     (item, cell);

        if (placed && cell.bottom > 0 && cell.top < m_boundsDip.bottom - m_boundsDip.top &&
            cell.right > 0 && cell.left < m_boundsDip.right - m_boundsDip.left)
        {
            onScreen.emplace_back (item, cell);
        }
    }

    //  The sliding group's items last, so its clip is pushed once.
    if (slide.active)
    {
        std::stable_partition (onScreen.begin(), onScreen.end(), [&slide] (const std::pair<int, RECT> & place)
        {
            return place.first < slide.start || place.first >= slide.end;
        });
    }

    for (const std::pair<int, RECT> & place : onScreen)
    {
        int                         item   = place.first;
        RECT                        cell   = place.second;
        RECT                        label  = {};
        const std::vector<Cell> &   cells  = GetRowCells (item);
        bool                        isSel  = (m_listFocused || m_alwaysShowSelection) &&
                                             (m_multiSelect ? IsRowSelected (item) : item == m_selectedRow);
        float                       iconX  = 0.0f;
        float                       iconY  = 0.0f;

        if (slide.active && !clipped && item >= slide.start && item < slide.end)
        {
            hr = text.PushClipRect (x, y + (float) slide.clipTop, (float) (m_boundsDip.right - m_boundsDip.left), (float) slide.clipH);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClipRect (x, y + (float) slide.clipTop, (float) (m_boundsDip.right - m_boundsDip.left), (float) slide.clipH);
            clipped = true;
        }

        if (cells.empty())
        {
            continue;
        }

        OffsetRect (&cell, (int) x, (int) y);

        //  A tile's box, and a big icon's, stops short of its row.
        if (m_view == View::Tiles)
        {
            cell.bottom -= (int) std::lround (m_scaler.ToPxf (s_kTileBoxShortDip));
        }
        else if (metrics.labelBelow)
        {
            cell.bottom -= (int) std::lround (m_scaler.ToPxf (s_kItemRowGapDip));
        }

        //  Square, and drawn as Details draws a row: the fill, and the outline
        //  a focused or multiple selection takes.
        if (m_theme != nullptr)
        {
            DxuiRowLook  look = DxuiRowLook::Resolve (*m_theme, isSel, item == m_selectedRow && m_focusGroup < 0, item == m_hovered, m_listFocused);

            float  fillW = (float) (cell.right - cell.left);

            if (look.fill != 0)
            {
                painter.FillRect ((float) cell.left, (float) cell.top, fillW, (float) (cell.bottom - cell.top), look.fill);
            }

            if (look.edge != 0)
            {
                painter.OutlineRect ((float) cell.left, (float) cell.top, fillW, (float) (cell.bottom - cell.top),
                                     DxuiRowLook::GetOutlinePx (m_scaler.ToPxf (1.0f)), look.edge);
            }
        }

        //  Content's rule between items, though not under a group's header.
        if (m_view == View::Content && m_theme != nullptr && item > 0 && (!HasItemGroups() || !IsFirstOfGroup (item)))
        {
            float  ruleLeft  = (float) cell.left  + m_scaler.ToPxf (s_kContentRuleInsetDip);
            float  ruleRight = (float) cell.right - m_scaler.ToPxf (s_kContentRuleEndDip);

            painter.FillRect (std::round (ruleLeft), (float) cell.top, std::round (ruleRight) - std::round (ruleLeft), 1.0f, m_theme->Divider());
        }

        if (item == m_dropRow && m_theme != nullptr)
        {
            painter.OutlineRect ((float) cell.left, (float) cell.top, (float) (cell.right - cell.left), (float) (cell.bottom - cell.top),
                                 DxuiRowLook::GetOutlinePx (m_scaler.ToPxf (1.0f)), m_theme->ContentSelectionMultiEdge());
        }

        if (metrics.labelBelow)
        {
            iconX = (float) (cell.left + ((cell.right - cell.left) - iconPx) / 2);
            iconY = (float) (cell.top + m_scaler.ToPx (GetIconTopDip (m_view)));
        }
        else
        {
            iconX = (float) (cell.left + GetItemIconLeftPx());
            iconY = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) iconPx) * 0.5f;
        }

        //  Explorer frames a big icon whose art is smaller than its box.
        if (GetItemMetrics (m_view).labelBelow && cells[0].icon && !cells[0].icon->bgraPremul.empty() && cells[0].icon->width < iconPx)
        {
            PaintSmallArtFrame (painter, iconX, iconY, (float) iconPx, pal.bgRow);
        }

        if (cells[0].icon && !cells[0].icon->bgraPremul.empty())
        {
            float  alpha = text.GetGlobalAlpha();

            if (cells[0].iconGhosted)
            {
                text.SetGlobalAlpha (alpha * s_kGhostedIconAlpha);
            }

            //  An image smaller than the icon box, a type with no art at this
            //  size, is drawn at its own size in the middle, as Explorer draws it.
            float  drawPx = (float) (std::min) (iconPx, (std::max) (cells[0].icon->width, 1));
            float  inset  = ((float) iconPx - drawPx) * 0.5f;

            hr = text.DrawIconBitmap (cells[0].icon->bgraPremul.data(), cells[0].icon->width, cells[0].icon->height,
                                      iconX + inset, iconY + inset, drawPx, drawPx);
            IGNORE_RETURN_VALUE (hr, S_OK);

            text.SetGlobalAlpha (alpha);
        }

        if (cells[0].iconBroken)
        {
            PaintBrokenBadge (text, iconX, iconY, (float) iconPx, m_scaler.ToPxf (s_kBrokenBadgeMinDip));
        }

        label = GetItemLabelRectPx (cell);

        //  Explorer's Content: the name over its type, and to the right the
        //  date over the size.
        if (m_view == View::Content)
        {
            float         left   = (float) (cell.left - GetItemsLeftPx());
            float         edge   = (float) cell.right + m_scaler.ToPxf (s_kContentEdgeDip) - left;
            float         rightX = left + std::round ((std::min) (m_scaler.ToPxf (s_kContentRightMaxDip),
                                                                  (std::max) (edge * s_kContentRightShare + m_scaler.ToPxf (s_kContentRightNudgeDip),
                                                                              edge - m_scaler.ToPxf (s_kContentRightRoomDip))));
            float         top    = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) (2 * lineH)) * 0.5f;
            std::wstring  name   = DxuiTextElide::ToWidth (text, cells[0].text, fontPx * s_kContentNameScale, DxuiTheme::kBodyFace,
                                                           rightX - (float) label.left, DxuiElide::Tail, 0, true);

            //  The name in Explorer's larger face, 11 points to the body's 9,
            //  sitting on the same baseline as it would in the body face.
            hr = text.DrawString (name.c_str(), (float) label.left, top - fontPx * (s_kContentNameScale - 1.0f), rightX - (float) label.left,
                                  (float) lineH * s_kContentNameScale,
                                  cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx * s_kContentNameScale, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (size_t at = 0; at < cells[0].contentLeft.size(); at++)
            {
                hr = text.DrawString (cells[0].contentLeft[at].text.c_str(), (float) label.left, top + (float) ((at + 1) * lineH), rightX - (float) label.left, (float) lineH,
                                      pal.fgDim, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            for (size_t at = 0; at < cells[0].contentRight.size(); at++)
            {
                hr = text.DrawString (cells[0].contentRight[at].text.c_str(), rightX, top + (float) (at * lineH), (float) cell.right - rightX, (float) lineH,
                                      pal.fgDim, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            continue;
        }

        //  Explorer's tiles: the name on up to two lines, the second ending in
        //  an ellipsis when it runs on, then the row's own lines (type and
        //  size) while three lines last, the whole block centered in the box.
        if (m_view == View::Tiles)
        {
            std::vector<std::wstring>  name     = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace,
                                                                               (float) (label.right - label.left), 2, true, true);
            std::vector<const Cell *>  details;
            float                      top      = 0.0f;
            int                        lineStep = GetTileLinePx();

            for (size_t at = 0; !cells[0].tileNameOnly && at < cells[0].tileLines.size() && name.size() + details.size() < (size_t) s_kTileMaxLines; at++)
            {
                details.push_back (&cells[0].tileLines[at]);
            }

            //  The block is centered as at least two lines, as a name alone is.
            top = (float) cell.top + (float) ((cell.bottom - cell.top) - (std::max) (2, (int) (name.size() + details.size())) * lineStep) * 0.5f;

            hr = text.PushClipRect ((float) label.left, (float) cell.top, (float) (label.right - label.left), (float) (cell.bottom - cell.top));
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (const std::wstring & line : name)
            {
                hr = text.DrawString (line.c_str(), (float) label.left, top, (float) (cell.right - label.left), (float) lineH,
                                      cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx, DxuiTheme::kBodyFace,
                                      DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
                top += (float) lineStep;
            }

            for (const Cell * detail : details)
            {
                if (detail->meter >= 0.0f)
                {
                    PaintMeter (painter, (float) label.left, top, (float) (label.right - label.left), (float) lineH, detail->meter);
                }
                else
                {
                    hr = text.DrawString (detail->text.c_str(), (float) label.left, top, (float) (cell.right - label.left), (float) lineH,
                                          pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                                          DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                    IGNORE_RETURN_VALUE (hr, S_OK);
                }

                top += (float) lineStep;
            }

            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
            continue;
        }

        //  Explorer's captions: under a big icon, wrapped to two lines, or one
        //  for Extra large; beside a small one, one line. Either way a name
        //  that runs on ends in an ellipsis inside its cell.
        {
            int                        lines = metrics.labelBelow ? s_kItemNameMaxLines : 1;
            std::vector<std::wstring>  shown = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace,
                                                                            (float) (label.right - label.left), lines, true, metrics.labelBelow);

            if (!shown.empty() && shown.back().ends_with (L"\x2026") && !cells[0].text.ends_with (L"\x2026"))
            {
                m_nameCut.push_back (item);
            }

            for (size_t at = 0; at < shown.size(); at++)
            {
                hr = text.DrawString (shown[at].c_str(), (float) label.left, (float) label.top + (float) (at * GetCaptionLinePx()),
                                      (float) (label.right - label.left), (float) lineH,
                                      cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx, DxuiTheme::kBodyFace,
                                      metrics.labelBelow ? DxuiTextHAlign::Center : DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                                      DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        //  Tiles and Content show the other columns underneath, one a line,
        //  or the row's own tile lines when it has them.
        for (int line = 1; !metrics.labelBelow && !(m_view == View::Tiles && cells[0].tileNameOnly) && line < metrics.textLines; line++)
        {
            bool                        own    = !cells[0].tileLines.empty();
            const std::vector<Cell> &   source = own ? cells[0].tileLines : cells;
            size_t                      index  = own ? (size_t) (line - 1) : (size_t) line;
            float                       top    = (float) (label.top + line * lineH);

            if (index >= source.size())
            {
                break;
            }

            if (source[index].meter >= 0.0f)
            {
                PaintMeter (painter, (float) label.left, top, (float) (label.right - label.left), (float) lineH, source[index].meter);
                continue;
            }

            hr = text.DrawString (source[index].text.c_str(), (float) label.left, top,
                                  (float) (label.right - label.left), (float) lineH,
                                  pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }

    //  The rubber band, over everything it selects.
    if (m_bandActive)
    {
        RECT  band = GetSelectionBandPx();

        OffsetRect (&band, (int) x, (int) y);

        painter.FillRect    ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                             (pal.bgSel & 0x00FFFFFFu) | 0x60000000u);
        painter.OutlineRect ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                             1.0f, pal.edgeSel != 0 ? pal.edgeSel : pal.fg);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::PaintBrokenBadge
//
//  A red disc with a white cross over the icon's lower-left corner, about
//  half the icon across and never under the size given, so it reads on a
//  16 dip icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintBrokenBadge (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx)
{
    HRESULT  hr    = S_OK;
    float    d     = (std::max) (minPx, iconPx * 0.45f);
    float    cx    = iconX + d * 0.5f;
    float    cy    = iconY + iconPx - d * 0.5f;
    float    arm   = d * 0.2f;
    float    thick = (std::max) (1.0f, d * 0.12f);



    hr = text.FillEllipse (cx, cy, d * 0.5f, d * 0.5f, s_kBrokenBadgeArgb);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy - arm, cx + arm, cy + arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy + arm, cx + arm, cy - arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::PaintMeter
//
//  Explorer's usage bar: an edged track, filled from the left to the
//  fraction, centered in its line and no wider than the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintMeter (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const
{
    float     edge   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));
    float     width  = (std::min) (w, m_scaler.ToPxf ((float) s_kMeterWidthDip));
    float     height = m_scaler.ToPxf ((float) s_kMeterHeightDip);
    float     top    = y + (lineH - height) * 0.5f;
    float     fill   = std::clamp (fraction, 0.0f, 1.0f);
    uint32_t  argb   = (fill >= s_kMeterFullAt) ? s_kMeterFullArgb : s_kMeterFillArgb;



    if (width <= edge * 2.0f)
    {
        return;
    }

    painter.FillRect (x, top, width, height, s_kMeterEdgeArgb);
    painter.FillRect (x + edge, top + edge, width - edge * 2.0f, height - edge * 2.0f, s_kMeterTrackArgb);
    painter.FillRect (x + edge, top + edge, (width - edge * 2.0f) * fill, height - edge * 2.0f, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetItemScrollOffsetPx
//
//  How far the item view is scrolled, so a point in the list's pixels can be
//  turned into one in the items' own space: as drawn, so a sliding page's
//  offset counts.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiListView::GetItemScrollOffsetPx() const
{
    ItemGrid  grid = GetItemGrid();



    if (GetItemMetrics (m_view).columns)
    {
        return POINT { m_leftPx, 0 };
    }

    if (UsesItemLayout())
    {
        return POINT { 0, GetShownScrollPx() };
    }

    return POINT { 0, (grid.perLine > 0) ? (m_topRow / grid.perLine) * grid.cellH : 0 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::BeginSelectionBand
//
//  Without Ctrl the band starts from nothing; with it, the band adds to the
//  selection the press found.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::BeginSelectionBand (int lx, int ly, bool ctrl)
{
    POINT  offset = GetItemScrollOffsetPx();



    m_bandActive = true;
    m_bandStart  = POINT { lx + offset.x, ly + offset.y };
    m_bandEnd    = m_bandStart;
    m_bandBase   = ctrl ? m_selectedRows : std::vector<int>();

    if (!ctrl)
    {
        ClearSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::UpdateSelectionBand
//
//  Every item the band touches is selected, with what Ctrl kept.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::UpdateSelectionBand (int lx, int ly)
{
    POINT               offset  = GetItemScrollOffsetPx();
    RECT                band    = {};
    std::vector<int>    rows    = m_bandBase;
    int                 last    = -1;
    bool                listGrp = HasListGroups();
    bool                itemGrp = !listGrp && UsesItemLayout();
    const ListLayout  & listLay = listGrp ? GetListLayout() : s_kNoListLayout;
    const ItemLayout  & itemLay = itemGrp ? GetItemLayout() : s_kNoItemLayout;



    m_bandEnd = POINT { lx + offset.x, ly + offset.y };

    band = RECT { (std::min) (m_bandStart.x, m_bandEnd.x), (std::min) (m_bandStart.y, m_bandEnd.y),
                  (std::max) (m_bandStart.x, m_bandEnd.x), (std::max) (m_bandStart.y, m_bandEnd.y) };

    for (int item = 0; item < GetRowCount(); item++)
    {
        RECT  cell   = {};
        RECT  hit    = {};
        bool  placed = listGrp ? GetListItemRectPx    (listLay, item, cell)
                               : itemGrp ? GetGroupedItemRectPx (itemLay, item, cell)
                                         : GetItemRectPx        (item, cell);

        if (!placed)
        {
            continue;
        }

        OffsetRect (&cell, offset.x, offset.y);

        if (IntersectRect (&hit, &cell, &band))
        {
            rows.push_back (item);
            last = item;
        }
    }

    SetSelectedRows (std::move (rows), last);

    if (m_onSelectionChanged && last >= 0)
    {
        m_onSelectionChanged (last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetSelectionBandPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiListView::GetSelectionBandPx() const
{
    POINT  offset = GetItemScrollOffsetPx();



    if (!m_bandActive)
    {
        return RECT {};
    }

    return RECT { (std::min) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::min) (m_bandStart.y, m_bandEnd.y) - offset.y,
                  (std::max) (m_bandStart.x, m_bandEnd.x) - offset.x, (std::max) (m_bandStart.y, m_bandEnd.y) - offset.y };
}
