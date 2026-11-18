#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapGrid.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapCellText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::SetRanges
//
//  The same ranges again keep the zoom and scroll the user left; new ones
//  start fitted to the pane, from the top.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::SetRanges (std::vector<Band> ranges)
{
    if (m_hasRanges && ranges == m_ranges)
    {
        return;
    }

    m_ranges    = std::move (ranges);
    m_bands     = m_ranges;
    m_isSplit   = false;
    m_hasRanges = true;
    m_scrollTop = 0;

    m_hover.reset();
    FitRanges();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ClearRanges
//
//  All of memory again, fitted to the pane.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ClearRanges()
{
    if (!m_hasRanges)
    {
        return;
    }

    m_ranges    = { Band { {}, 0, kAddressCount } };
    m_bands     = m_ranges;
    m_isSplit   = false;
    m_hasRanges = false;

    m_hover.reset();
    ResetZoom();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::CanSplit
//
//  Sections require the regions, which only the CPU's bank has.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapGrid::CanSplit() const
{
    return m_model.GetRegions().size() == (size_t) kAddressCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::SyncBands
//
//  The ranges cut into sections in a bank that has regions, and whole in one
//  that does not. The cell the mouse frames is found again by its address,
//  since its range and place in the range change.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::SyncBands()
{
    bool                 isSplit = CanSplit();
    std::optional<Word>  hover   = GetHover();



    if (isSplit == m_isSplit)
    {
        return;
    }

    m_isSplit = isSplit;
    m_bands   = isSplit ? HeatMapSections::SplitAtSections (m_ranges, !m_hasRanges, m_model.GetMachineId()) : m_ranges;
    m_hover   = hover.has_value() ? GetPlaceOf (*hover) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::FitRanges
//
//  The largest cell whose rows all fit the pane, with no scrolling, for the
//  ranges or for all of memory; a pane too small for even one pixel a cell
//  scrolls at one. While the user has not zoomed, a resize fits them again.
//  The fitted size is the zoom's 100%.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::FitRanges()
{
    m_isFitted  = true;
    m_scrollTop = 0;
    m_fittedFor = m_boundsDip;

    for (int cell = kMaxCellPx; cell >= 1; cell--)
    {
        m_cellPx = cell;
        PlaceMap();

        if (!m_hasVertBar)
        {
            break;
        }
    }

    m_fitCellPx    = m_cellPx;
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::PlaceBands
//
//  Each range's header and rows, one after another down the map. All of
//  memory is one range with no header, until sections cut it. A range under
//  a header takes at least a row label's line, so the label of a range of
//  one short row ends before the next header. A section's rows are as wide
//  as the map's, so its cells line up with every other section's, and room
//  after them takes the bottom of its box.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::PlaceBands()
{
    long  pitch  = GetPitch();
    long  header = m_scaler.ToPx (kHeaderDip);
    long  label  = m_scaler.ToPx (kSectionHeaderDip);
    long  after  = m_scaler.ToPx (kSectionAfterDip);
    long  y      = 0;



    m_placed.assign (m_bands.size(), Placed {});

    for (size_t i = 0; i < m_bands.size(); i++)
    {
        Placed  & placed   = m_placed[i];
        int       count    = std::max (1, m_bands[i].count);
        bool      isHeaded = HasHeader (i);



        placed.columns   = m_bands[i].isSection ? m_columns : HeatMapSections::GetColumnsOf (m_columns, count);
        placed.headerTop = y;
        placed.rowsTop   = y + (!isHeaded ? 0L : m_bands[i].isSection ? label : header);
        placed.rows      = (count + placed.columns - 1) / placed.columns;

        y = placed.rowsTop + std::max (placed.rows * pitch, isHeaded ? m_labelLinePx : 0L) + (m_bands[i].isSection ? after : 0L);
    }

    m_contentH = y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetRows
//
//  Every range's rows together.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapGrid::GetRows() const
{
    int  rows = 0;



    for (const Band & band : m_bands)
    {
        int  count   = std::max (1, band.count);
        int  columns = band.isSection ? m_columns : HeatMapSections::GetColumnsOf (m_columns, count);



        rows += (count + columns - 1) / columns;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetBandAt
//
//  The range whose rows hold a height in the map, or -1 over a header or
//  past the last range.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapGrid::GetBandAt (long y) const
{
    long  pitch = GetPitch();



    for (size_t i = 0; i < m_placed.size(); i++)
    {
        if (y >= m_placed[i].rowsTop && y < m_placed[i].rowsTop + m_placed[i].rows * pitch)
        {
            return (long) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPlaceAt
//
//  The cell under a point in the map, a street counting as the cell before
//  it; none over a header, past a range's last address, or off the map.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapGrid::Place> HeatMapGrid::GetPlaceAt (POINT point) const
{
    const Placed  * placed = nullptr;
    long            pitch  = GetPitch();
    long            atX    = point.x - m_map.left;
    long            atY    = point.y - m_map.top + m_scrollTop;
    long            band   = -1;
    long            offset = 0;



    if (m_cellPx <= 0 || point.x < m_map.left || point.x >= m_map.right || point.y < m_map.top || point.y >= m_map.bottom || atX >= m_rowPx)
    {
        return std::nullopt;
    }

    band = GetBandAt (atY);

    if (band < 0)
    {
        return std::nullopt;
    }

    placed = &m_placed[(size_t) band];
    offset = ((atY - placed->rowsTop) / pitch) * placed->columns + GetColumnAt (*placed, atX);

    if (offset >= m_bands[(size_t) band].count)
    {
        return std::nullopt;
    }

    return Place { (size_t) band, offset };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPlaceOf
//
//  The first range that holds an address.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapGrid::Place> HeatMapGrid::GetPlaceOf (Word address) const
{
    for (size_t i = 0; i < m_bands.size(); i++)
    {
        long  offset = (long) (Word) (address - m_bands[i].first);



        if (offset < m_bands[i].count)
        {
            return Place { i, offset };
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetAddressOf
//
////////////////////////////////////////////////////////////////////////////////

Word HeatMapGrid::GetAddressOf (const Place & place) const
{
    return (Word) (m_bands[place.band].first + place.offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPlaceRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetPlaceRect (const Place & place) const
{
    const Placed  & placed = m_placed[place.band];
    long            column = place.offset % placed.columns;
    long            start  = GetColumnLeft (placed, column);
    long            wide   = GetColumnLeft (placed, column + 1) - start - kStreetPx;
    long            left   = m_map.left + start;
    long            top    = m_map.top  + placed.rowsTop + (place.offset / placed.columns) * GetPitch() - m_scrollTop;



    return { left, top, left + std::max (1L, wide), top + m_cellPx };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetHeaderRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetHeaderRect (size_t range) const
{
    long  top = 0;



    if (range >= m_placed.size() || !HasHeader (range))
    {
        return {};
    }

    top = m_map.top + m_placed[range].headerTop - m_scrollTop;

    return { m_boundsDip.left, top, m_map.right, top + m_scaler.ToPx (kHeaderDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetHover
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapGrid::GetHover() const
{
    return m_hover.has_value() ? std::optional<Word> (GetAddressOf (*m_hover)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetColumnLeft
//
//  Where a column starts within a range's row, from its left: the row's
//  width shared among its columns, the leftover pixels spread one to a
//  column, so no column is more than a pixel wider than another. Every row
//  is as wide as the map's area.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapGrid::GetColumnLeft (const Placed & placed, long column) const
{
    return (long) (((long long) column * m_rowPx) / placed.columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetColumnAt
//
//  The column a point a distance into the row falls in, its street counting
//  as the column's; the last column past the row's end.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapGrid::GetColumnAt (const Placed & placed, long x) const
{
    long  column = 0;



    if (m_rowPx <= 0 || x <= 0)
    {
        return 0;
    }

    column = std::min ((long) (((long long) x * placed.columns) / m_rowPx), (long) placed.columns - 1);

    if (column + 1 < placed.columns && GetColumnLeft (placed, column + 1) <= x)
    {
        column++;
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::PlaceMap
//
//  The addresses a row holds at the zoom in force, the row's width, and the
//  map's area: right of the row labels and below the bar, as wide as the
//  room and as tall as the rows or the room, less a scrollbar down the right
//  when the rows overflow. A row is stretched or narrowed to the room's
//  width, so the map never scrolls across. Taking the scrollbar's room can
//  change the row, so the fit is found again until it settles.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::PlaceMap()
{
    constexpr int  kPasses  = 2;
    long           pitch    = GetPitch();
    long           bar      = m_scaler.ToPx (kScrollbarDip);
    long           left     = m_boundsDip.left + m_gutterPx;
    long           top      = m_boundsDip.top  + m_scaler.ToPx (kBarDip);
    long           right    = std::max (left, m_boundsDip.right  - m_scaler.ToPx (kInsetDip));
    long           bottom   = std::max (top,  m_boundsDip.bottom - m_scaler.ToPx (kInsetDip));
    long           width    = 0;
    long           height   = std::max (0L, bottom - top);
    bool           hasVert  = false;
    bool           needVert = false;



    SyncBands();

    //  Sections' boxes are kSectionPadDip out from the cells on each side.
    if (m_isSplit)
    {
        left  += m_scaler.ToPx (kSectionPadDip);
        right  = std::max (left, right - m_scaler.ToPx (kSectionPadDip));
    }

    for (int pass = 0; pass <= kPasses; pass++)
    {
        width     = std::max (0L, right - left - (hasVert ? bar + (m_isSplit ? m_scaler.ToPx (kSectionPadDip) : 0L) : 0L));
        m_columns = HeatMapSections::GetColumnsFor (width, pitch);

        PlaceBands();

        needVert = m_contentH > height;

        if (needVert == hasVert)
        {
            break;
        }

        hasVert = needVert;
    }

    m_hasVertBar = hasVert;
    m_rowPx      = width;
    m_areaBottom = top + height;
    m_map        = { left, top, left + width, top + std::min (height, m_contentH) };

    PlaceZoomWidget();
    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ApplyCellPx
//
//  A new cell size, with the row of the address under the point scrolled to
//  the point's height, the same fraction of the way down its cell: the rows
//  may contain a different number of addresses afterward, so the address is
//  found again by its range and its place in the range rather than by its
//  row. Over a header, the scroll is the same fraction of the way down the
//  map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ApplyCellPx (int cellPx, POINT point)
{
    std::optional<Place>  place  = GetPlaceAt (point);
    std::optional<Word>   anchor = place.has_value() ? std::optional<Word> (GetAddressOf (*place)) : std::nullopt;
    const Placed        * placed = place.has_value() ? &m_placed[place->band] : nullptr;
    long                  pitch  = GetPitch();
    long                  atY    = point.y - m_map.top + m_scrollTop;
    long                  row    = (placed != nullptr) ? place->offset / placed->columns : 0;
    long                  rowTop = (placed != nullptr) ? placed->rowsTop + row * pitch : 0;
    double                fracY  = (double) std::clamp (atY - rowTop, 0L, pitch) / (double) pitch;
    double                down   = (double) atY / (double) std::max (1L, m_contentH);
    bool                  isIn   = cellPx > m_cellPx;



    m_cellPx   = std::clamp (cellPx, 1, kMaxCellPx);
    m_isFitted = false;

    PlaceMap();

    pitch = GetPitch();
    place = anchor.has_value() ? GetPlaceOf (*anchor) : std::nullopt;

    if (!place.has_value())
    {
        m_scrollTop = (long) std::lround (down * (double) m_contentH) - (point.y - m_map.top);
    }
    else
    {
        placed      = &m_placed[place->band];
        row         = place->offset / placed->columns;
        m_scrollTop = placed->rowsTop + (long) std::lround (((double) row + fracY) * (double) pitch) - (point.y - m_map.top);
    }

    //  Zoomed in, the row stays under the pointer even where that scrolls the
    //  map past either end of memory, as long as a row of it stays in view.
    //  Zoomed out, the map stops at the ends, so memory that fits the pane is
    //  at its top.
    if (isIn)
    {
        LimitScroll (std::min (0L, pitch - (m_areaBottom - m_map.top)), std::max (GetMaxScroll(), m_contentH - pitch));
    }
    else
    {
        ClampScroll();
    }

    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ZoomAt
//
//  Each notch up makes a cell kZoomPerNotch times larger, and each down that
//  much smaller, by at least a pixel; the row of the address under the point
//  stays at its height. A point off the map zooms about the map's center.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ZoomAt (POINT point, float notches)
{
    int  cell = 0;



    if (notches == 0.0f || m_map.right <= m_map.left || m_map.bottom <= m_map.top || m_cellPx <= 0)
    {
        return;
    }

    if (point.x < m_map.left || point.x >= m_map.right || point.y < m_map.top || point.y >= m_map.bottom)
    {
        point = { (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 };
    }

    cell = (int) std::lround ((double) m_cellPx * std::pow ((double) kZoomPerNotch, (double) notches));

    if (cell == m_cellPx)
    {
        cell += (notches > 0.0f) ? 1 : -1;
    }

    ApplyCellPx (cell, point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ZoomIn
//
//  A notch about the middle of the map, as the bar's button does.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ZoomIn()
{
    ZoomAt ({ (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 }, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ZoomOut
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ZoomOut()
{
    ZoomAt ({ (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 }, -1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ZoomTo
//
//  A cell cellPx tall, about the map's top left, as a zoom the user made:
//  the map is no longer fitted to the pane.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ZoomTo (int cellPx)
{
    ApplyCellPx (cellPx, { m_map.left, m_map.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ShowAddress
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ShowAddress (Word address)
{
    std::optional<Place>  place;
    long                  pitch = 0;



    m_cellPx   = kMaxCellPx;
    m_isFitted = false;

    PlaceMap();

    place = GetPlaceOf (address);
    pitch = GetPitch();

    if (place.has_value())
    {
        const Placed  & placed = m_placed[place->band];
        long            row    = place->offset / placed.columns;



        m_scrollTop = placed.rowsTop + row * pitch + pitch / 2 - (m_map.bottom - m_map.top) / 2;
        ClampScroll();
    }

    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ScrollBy
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ScrollBy (int dy)
{
    long  from = m_scrollTop;



    //  A map zoomed past an end scrolls back toward it, not farther past.
    m_scrollTop += dy;

    LimitScroll (std::min (0L, from), std::max (GetMaxScroll(), from));
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ResetZoom
//
//  Back to the size that fits the map to the pane, from its top left.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ResetZoom()
{
    FitRanges();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ClampScroll
//
//  The scroll is clamped so the map's bottom is no higher than the area's; a
//  map that fits its area does not scroll at all. The scrollbar is updated to
//  match.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ClampScroll()
{
    LimitScroll (0, GetMaxScroll());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetMaxScroll
//
//  The farthest the map scrolls with its bottom no higher than the area's.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapGrid::GetMaxScroll() const
{
    return std::max (0L, m_contentH - (m_areaBottom - m_map.top));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::LimitScroll
//
//  The scroll is clamped between lowest and highest. Past either end of
//  memory, the map fills its whole area, so the rows scrolled into it are
//  drawn and the empty part around memory is part of the map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::LimitScroll (long lowest, long highest)
{
    bool  isPast = false;



    m_scrollTop  = std::clamp (m_scrollTop, lowest, std::max (lowest, highest));
    isPast       = m_scrollTop < 0 || m_scrollTop > GetMaxScroll();
    m_map.bottom = m_map.top + (isPast ? m_areaBottom - m_map.top : std::min (m_areaBottom - m_map.top, m_contentH));

    SyncScrollbars();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetVerticalBarRect
//
//  Past the sections' boxes, which are kSectionPadDip out from the cells.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetVerticalBarRect() const
{
    long  left = m_map.right + (m_isSplit ? m_scaler.ToPx (kSectionPadDip) : 0L);



    if (!m_hasVertBar)
    {
        return {};
    }

    return { left, m_map.top, left + m_scaler.ToPx (kScrollbarDip), m_map.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::SyncScrollbars
//
//  The bar in pixels: the rows' height, the part in view and the scroll.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::SyncScrollbars()
{
    int             bar   = m_scaler.ToPx (kScrollbarDip);
    int             pitch = GetPitch();
    DxuiScrollInfo  info;



    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;

    m_vertBar.Configure (DxuiScrollbar::Orientation::Vertical, bar, bar, pitch);
    m_vertBar.SetTrack  (GetVerticalBarRect());

    info.nMax  = (int) m_contentH;
    info.nPage = (UINT) std::max (0L, m_map.bottom - m_map.top);
    info.nPos  = (int) m_scrollTop;
    m_vertBar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapGrid::TickScrollbars (int64_t nowMs)
{
    bool  isBarMoving  = m_vertBar.Tick (nowMs);
    bool  isZoomMoving = m_zoomWidget.Tick (nowMs);



    return isBarMoving || isZoomMoving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::BuildFrame
//
//  The map's area a pixel at a time, as scrolled: each cell its color, and
//  the street after it, a range's header and the part of its last row past
//  its last address the page.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::BuildFrame (bool isVisible)
{
    long                           width      = m_map.right  - m_map.left;
    long                           height     = m_map.bottom - m_map.top;
    long                           pitch      = GetPitch();
    uint32_t                       background = m_model.GetPalette().background | 0xFF000000u;
    std::vector<std::vector<int>>  columns (m_bands.size());
    std::vector<long>              bands;
    std::vector<int>               rows;



    //  A pane behind another tab is drawn when it next paints, not on every
    //  snapshot the machine sends meanwhile.
    m_isFrameStale = !isVisible;

    if (m_isFrameStale)
    {
        return;
    }

    if (width <= 0 || height <= 0 || m_cellPx <= 0)
    {
        m_frame.clear();
        return;
    }

    //  The range and the row each pixel row falls in, or -1 for a street or
    //  a header.
    bands.resize ((size_t) height);
    rows.resize  ((size_t) height);

    for (long y = 0; y < height; y++)
    {
        long  at   = y + m_scrollTop;
        long  band = GetBandAt (at);
        long  into = (band < 0) ? 0 : at - m_placed[(size_t) band].rowsTop;



        bands[(size_t) y] = band;
        rows[(size_t) y]  = (band >= 0 && into % pitch < m_cellPx) ? (int) (into / pitch) : -1;
    }

    m_frame.assign ((size_t) (width * height), background);

    for (long y = 0; y < height; y++)
    {
        uint32_t          * line   = m_frame.data() + (size_t) (y * width);
        int                 row    = rows[(size_t) y];
        long                band   = bands[(size_t) y];
        std::vector<int>  * across = (row < 0) ? nullptr : &columns[(size_t) band];



        if (across == nullptr)
        {
            continue;
        }

        //  The cell each pixel column falls in, or -1 for a street, for each
        //  range as its first row comes into view.
        if (across->empty())
        {
            const Placed  & placed = m_placed[(size_t) band];



            across->resize ((size_t) width);

            for (long x = 0; x < width; x++)
            {
                long  column = GetColumnAt (placed, x);



                (*across)[(size_t) x] = (x < m_rowPx && x < GetColumnLeft (placed, column + 1) - kStreetPx) ? (int) column : -1;
            }
        }

        for (long x = 0; x < width; x++)
        {
            int   column = (*across)[(size_t) x];
            long  offset = (long) row * m_placed[(size_t) band].columns + column;



            if (column >= 0 && offset < m_bands[(size_t) band].count)
            {
                line[x] = m_model.GetCellColor ((Word) (m_bands[(size_t) band].first + offset));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPixels
//
//  A change marks the frame stale rather than drawing it again: a wheel spun
//  or a snapshot that came in redrew every pixel of the map each time, and
//  input waited behind it. The frame is drawn once, when it is painted or
//  asked for.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> & HeatMapGrid::GetPixels (bool isVisible)
{
    if (m_isFrameStale)
    {
        BuildFrame (isVisible);
    }

    return m_frame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetCellRect (Word address) const
{
    std::optional<Place>  place = GetPlaceOf (address);



    if (!place.has_value() || place->band >= m_placed.size())
    {
        return {};
    }

    return GetPlaceRect (*place);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetAddressAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapGrid::GetAddressAt (POINT point) const
{
    std::optional<Place>  place = GetPlaceAt (point);



    return place.has_value() ? std::optional<Word> (GetAddressOf (*place)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPickAt
//
//  While cells are smaller than kComfortCellDip, a point over a cold cell
//  takes the hottest cell within kSnapDip of it, the nearer of two as hot;
//  with none, or with cells large enough to land on, the cell under it.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapGrid::GetPickAt (POINT point) const
{
    std::optional<Place>  pick = GetPickPlaceAt (point);



    return pick.has_value() ? std::optional<Word> (GetAddressOf (*pick)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPickPlaceAt
//
//  The search stays in the row under the mouse, so moving up or down always
//  lands on the next row, however cold; along the row the pick is the
//  nearest hot cell within kSnapDip. Over an instruction drawn as one cell,
//  the pick is its opcode.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapGrid::Place> HeatMapGrid::GetPickPlaceAt (POINT point) const
{
    std::optional<Place>  under   = GetPlaceAt (point);
    long                  reach   = m_scaler.ToPx (kSnapDip);
    long                  atX     = point.x - m_map.left;
    const Placed        * placed  = nullptr;
    long                  row     = 0;
    long                  lastCol = 0;
    Place                 best;
    long                  nearest = reach + 1;



    if (under.has_value() && GetMergedAt (*under).has_value())
    {
        return GetMergedAt (*under)->first;
    }

    if (!under.has_value() || m_cellPx >= m_scaler.ToPx (kComfortCellDip) || m_model.GetShownLevel (GetAddressOf (*under)) > 0)
    {
        return under;
    }

    best    = *under;
    placed  = &m_placed[under->band];
    row     = under->offset / placed->columns;
    lastCol = std::min ((long) placed->columns - 1, (long) GetColumnAt (*placed, atX + reach));

    for (long column = GetColumnAt (*placed, atX - reach); column <= lastCol; column++)
    {
        Place  place    = { under->band, row * placed->columns + column };
        Byte   level    = (place.offset < m_bands[under->band].count) ? m_model.GetShownLevel (GetAddressOf (place)) : (Byte) 0;
        long   left     = GetColumnLeft (*placed, column);
        long   right    = GetColumnLeft (*placed, column + 1) - kStreetPx;
        long   distance = std::max ({ left - atX, 0L, atX - right });



        if (level > 0 && distance < nearest)
        {
            best    = place;
            nearest = distance;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::IsOverMap
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapGrid::IsOverMap (POINT point) const
{
    for (const RECT & area : { m_map, GetVerticalBarRect() })
    {
        if (point.x >= area.left && point.x < area.right && point.y >= area.top && point.y < area.bottom)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::Layout
//
//  A map the user has not zoomed is fitted to the pane again at its new
//  size.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    if (m_cellPx > 0 && scaler.GetDpi() != m_scaler.GetDpi())
    {
        RescaleForDpi (boundsPx, scaler);
        return;
    }

    m_boundsDip = boundsPx;
    m_scaler    = scaler;

    if (m_cellPx <= 0)
    {
        m_cellPx = std::max (1, m_scaler.ToPx (kDefaultCellDip));
    }

    if (m_gutterPx <= 0)
    {
        m_gutterPx = m_scaler.ToPx (kGutterDip);
    }

    //  Fitting tries every cell size, so a map already fitted to these
    //  bounds is left as it is.
    if (m_isFitted && EqualRect (&m_fittedFor, &m_boundsDip))
    {
        return;
    }

    if (m_isFitted)
    {
        FitRanges();
        return;
    }

    //  A map the user zoomed keeps its zoom, but the size that fits the
    //  pane, which is 100% and what Reset zoom returns to, is the new
    //  pane's.
    if (!EqualRect (&m_fittedFor, &m_boundsDip))
    {
        m_fitCellPx = ComputeFitCellPx();
        m_fittedFor = m_boundsDip;
    }

    PlaceMap();
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::RescaleForDpi
//
//  The zoom's percentage is the user's zoom rather than a size in pixels,
//  so it is the same after a move to a monitor of another scale: the cell
//  becomes the same multiple of the size that fits the pane at the new
//  scale. The cells then scale with the monitor as the text does. The map
//  is scrolled the same fraction of the way down.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::RescaleForDpi (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    double  zoom  = (double) m_cellPx / (double) GetStartCellPx();
    double  down  = (double) m_scrollTop / (double) std::max (1L, m_contentH);
    double  scale = (double) scaler.GetDpi() / (double) m_scaler.GetDpi();



    m_boundsDip   = boundsPx;
    m_scaler      = scaler;
    m_gutterPx    = (int) std::lround ((double) m_gutterPx * scale);
    m_labelLinePx = (long) std::lround ((double) m_labelLinePx * scale);

    if (m_gutterPx <= 0)
    {
        m_gutterPx = m_scaler.ToPx (kGutterDip);
    }

    if (m_isFitted)
    {
        FitRanges();
        return;
    }

    m_fitCellPx = ComputeFitCellPx();
    m_fittedFor = m_boundsDip;
    m_cellPx    = std::clamp ((int) std::lround (zoom * (double) m_fitCellPx), 1, kMaxCellPx);

    PlaceMap();

    m_scrollTop = (long) std::lround (down * (double) m_contentH);

    ClampScroll();
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ComputeFitCellPx
//
//  The cell FitRanges would choose for the pane as it is now, leaving the
//  zoom and the scroll the user has as they were.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapGrid::ComputeFitCellPx()
{
    int   zoomed = m_cellPx;
    long  scroll = m_scrollTop;
    int   fit    = 1;



    for (int cell = kMaxCellPx; cell >= 1; cell--)
    {
        m_cellPx = cell;
        PlaceMap();

        if (!m_hasVertBar)
        {
            fit = cell;
            break;
        }
    }

    m_cellPx    = zoomed;
    m_scrollTop = scroll;
    PlaceMap();

    return fit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::MeasureGutter
//
//  As wide as the widest row label in the face it is drawn in, so a text
//  zoom widens it, plus the pane's text inset ahead of the label, so the
//  widest label starts where the pane's title does, and the row labels' gap
//  before the map after it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::MeasureGutter (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr int   kPadDip = 4;
    DxuiFontHandle  font    = theme.MonospaceFont();
    float           width   = 0.0f;
    float           height  = 0.0f;
    int             gutter  = 0;
    long            line    = 0;
    HRESULT         hr      = S_OK;



    hr = text.MeasureString (L"$FFFF", m_scaler.ToPxf (font.sizeDip), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f)
    {
        return;
    }

    gutter = (int) std::ceil (width) + DxuiPaneMetrics::GetContentTextInsetPx (m_scaler) + m_scaler.ToPx (kPadDip);
    line   = (long) std::ceil (m_scaler.ToPxf (font.sizeDip) * kLabelLine);

    if (gutter == m_gutterPx && line == m_labelLinePx)
    {
        return;
    }

    m_gutterPx    = gutter;
    m_labelLinePx = line;

    if (m_isFitted)
    {
        FitRanges();
        return;
    }

    PlaceMap();
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ForEachVisible
//
//  Every cell with any part in the map's area, as scrolled, with its
//  address.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ForEachVisible (const std::function<void (const Place &, Word)> & visit) const
{
    long  pitch  = GetPitch();
    long  width  = m_map.right  - m_map.left;
    long  height = m_map.bottom - m_map.top;



    if (pitch <= 0 || width <= 0 || height <= 0)
    {
        return;
    }

    for (size_t band = 0; band < m_placed.size() && band < m_bands.size(); band++)
    {
        const Placed  & placed   = m_placed[band];
        long            top      = m_scrollTop - placed.rowsTop;
        long            bottom   = top + height;
        long            firstRow = std::max (0L, top / pitch);
        long            lastRow  = std::min (placed.rows - 1, bottom / pitch);



        if (bottom < 0)
        {
            continue;
        }

        for (long row = firstRow; row <= lastRow; row++)
        {
            for (long column = 0; column < placed.columns; column++)
            {
                Place  place = { band, row * placed.columns + column };



                if (place.offset >= m_bands[band].count)
                {
                    continue;
                }

                visit (place, GetAddressOf (place));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::IsMergeZoom
//
//  Whether the cells are tall enough for an instruction's two lines, so an
//  instruction is drawn as one cell across its bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapGrid::IsMergeZoom() const
{
    float  minFont = m_scaler.ToPxf (HeatMapCellText::kMinDetailFontDip);
    float  pad     = m_scaler.ToPxf ((float) HeatMapCellText::kDetailPadDip);



    return m_model.CanMerge() &&
           ((float) m_cellPx - pad * 2.0f) / (HeatMapCellText::kLinePerEm * 2.0f) >= minFont;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetMergeLength
//
//  The length of the instruction whose opcode is at a place, when it is
//  drawn as one cell: a byte run as an opcode, drawn in the code color, with its form
//  and every byte of it known, inside its range. Zero otherwise.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapGrid::GetMergeLength (const Place & place, Word address) const
{
    int  length = m_model.GetMergeLength (address);



    if (length == 0 || place.offset + length > m_bands[place.band].count)
    {
        return 0;
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetMergedAt
//
//  The instruction drawn as one cell that contains a place: the instruction
//  at the place when the place is an opcode, else the one whose opcode is up
//  to two bytes before the place and whose length covers it. None at a zoom
//  too small to merge.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapGrid::Merged> HeatMapGrid::GetMergedAt (const Place & place) const
{
    constexpr long  kMaxOperands = 2;



    if (!IsMergeZoom())
    {
        return std::nullopt;
    }

    for (long back = 0; back <= kMaxOperands && back <= place.offset; back++)
    {
        Place  start   = { place.band, place.offset - back };
        Word   address = GetAddressOf (start);
        int    length  = GetMergeLength (start, address);



        if (length > back)
        {
            return Merged { start, address, length };
        }

        //  An opcode is never part of the instruction before it.
        if (m_model.IsOpcode (address))
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetMergedRects
//
//  An instruction's cell, one rectangle for each row it is on: one that
//  passes the end of its row continues at the start of the next.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> HeatMapGrid::GetMergedRects (const Merged & merged) const
{
    const Placed       & placed = m_placed[merged.first.band];
    std::vector<RECT>    rects;
    long                 offset = merged.first.offset;
    long                 end    = merged.first.offset + merged.length;



    while (offset < end)
    {
        long  rowEnd = std::min (end, (offset / placed.columns + 1) * placed.columns);
        RECT  first  = GetPlaceRect (Place { merged.first.band, offset });
        RECT  last   = GetPlaceRect (Place { merged.first.band, rowEnd - 1 });



        rects.push_back (RECT { first.left, first.top, last.right, last.bottom });
        offset = rowEnd;
    }

    return rects;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetViewRowFreeRect
//
//  From the map's left, past the row labels, to the pane's inset, as tall
//  as the row.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetViewRowFreeRect (bool isVisible) const
{
    long  left  = m_boundsDip.left + m_gutterPx;
    long  right = m_boundsDip.right - m_scaler.ToPx (kInsetDip);



    if (!isVisible || right <= left)
    {
        return RECT {};
    }

    return RECT { left, m_boundsDip.top, right, m_boundsDip.top + m_scaler.ToPx (kBarDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::PlaceZoomWidget
//
//  In the corner, except while its slider is open in a pane the same size:
//  zooming there brings the scrollbar and takes it away, and the slider
//  stays under the pointer rather than moving with the corner.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::PlaceZoomWidget()
{
    if (m_zoomWidget.IsOpen() && EqualRect (&m_zoomPlacedFor, &m_boundsDip))
    {
        return;
    }

    m_zoomPlacedFor = m_boundsDip;
    m_zoomWidget.Place (GetZoomCorner(), m_boundsDip, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetZoomCorner
//
//  The pane's bottom right, inside the scrollbar when it is visible, so the
//  zoom widget is placed its margin in from the corner. The map starts
//  fitted, with no scrollbar, so that is where it usually is.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetZoomCorner() const
{
    long  right = m_boundsDip.right - (m_hasVertBar ? m_scaler.ToPx (kScrollbarDip) : 0);



    return { m_boundsDip.left + m_gutterPx, m_boundsDip.top + m_scaler.ToPx (kBarDip), right, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::TryGetZoomTipAt
//
//  Over the zoom's button, how to zoom: the tip anchored to the button.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapGrid::TryGetZoomTipAt (POINT point, bool isVisible, RECT & anchor, std::wstring & text) const
{
    if (!isVisible || m_zoomWidget.HitTest (point) != HeatMapZoomWidget::Hit::Button)
    {
        return false;
    }

    anchor = m_zoomWidget.GetButtonRect();
    text   = HeatMapZoomWidget::kpszTip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ScrollToPlace
//
//  As little as brings the cell's row wholly into the map's area.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ScrollToPlace (const Place & place)
{
    RECT  cell = GetPlaceRect (place);



    if (cell.top < m_map.top)
    {
        ScrollBy ((int) (cell.top - m_map.top - kStreetPx));
    }
    else if (cell.bottom > m_map.bottom)
    {
        ScrollBy ((int) (cell.bottom - m_map.bottom + kStreetPx));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::PlaceAgain
//
//  Regions arrive or go away as the current bank changes, which cuts the
//  map into sections or joins them, so a map already laid out is laid out
//  again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::PlaceAgain()
{
    if (m_boundsDip.right > m_boundsDip.left)
    {
        PlaceMap();
        m_isFrameStale = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::OnMachineChanged
//
//  The sections of the system ROM's parts start where the machine's ROM
//  changes part, so a new machine cuts the map again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::OnMachineChanged()
{
    m_isSplit = false;
    m_bands   = m_ranges;
    m_hover.reset();

    PlaceAgain();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::DragTo
//
//  The scroll limits are the drag's: a map zoomed past an end keeps the
//  scroll the drag began at in reach.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::DragTo (long top, long from)
{
    m_scrollTop = top;

    LimitScroll (std::min (0L, from), std::max (GetMaxScroll(), from));
    m_isFrameStale = true;
}





