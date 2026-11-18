#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapGrid.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapControls.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::SetRanges
//
//  The same ranges again keep the zoom and scroll the user left; new ones
//  start fitted to the pane, from the top.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::SetRanges (std::vector<HeatMapBands::Band> ranges)
{
    if (!m_bands.SetRanges (std::move (ranges)))
    {
        return;
    }

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
    if (!m_bands.Clear())
    {
        return;
    }

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
    return m_model.GetRegions().size() == (size_t) HeatMapSections::kAddressCount;
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
    std::optional<Word>  hover = GetHover();



    if (!m_bands.Sync (CanSplit(), m_model.GetMachineId()))
    {
        return;
    }

    m_hover = hover.has_value() ? m_bands.GetPlaceOf (*hover) : std::nullopt;
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
    m_revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPlaceAt
//
//  The cell under a point in the map, a street counting as the cell before
//  it; none over a header, past a range's last address, or off the map.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapBands::Place> HeatMapGrid::GetPlaceAt (POINT point) const
{
    const HeatMapBands::Placed  * placed = nullptr;
    long                          pitch  = GetPitch();
    long                          atX    = point.x - m_map.left;
    long                          atY    = point.y - m_map.top + m_scrollTop;
    long                          band   = -1;
    long                          offset = 0;



    if (m_cellPx <= 0 || point.x < m_map.left || point.x >= m_map.right || point.y < m_map.top || point.y >= m_map.bottom || atX >= m_rowPx)
    {
        return std::nullopt;
    }

    band = m_bands.GetBandAt (atY, GetPitch());

    if (band < 0)
    {
        return std::nullopt;
    }

    placed = &m_bands.GetPlaced()[(size_t) band];
    offset = ((atY - placed->rowsTop) / pitch) * placed->columns + GetColumnAt (*placed, atX);

    if (offset >= m_bands.GetAll()[(size_t) band].count)
    {
        return std::nullopt;
    }

    return HeatMapBands::Place { (size_t) band, offset };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetPlaceRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetPlaceRect (const HeatMapBands::Place & place) const
{
    const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[place.band];
    long                          column = place.offset % placed.columns;
    long                          start  = GetColumnLeft (placed, column);
    long                          wide   = GetColumnLeft (placed, column + 1) - start - HeatMapSections::kStreetPx;
    long                          left   = m_map.left + start;
    long                          top    = m_map.top  + placed.rowsTop + (place.offset / placed.columns) * GetPitch() - m_scrollTop;



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



    if (range >= m_bands.GetPlaced().size() || !m_bands.HasHeader (range))
    {
        return {};
    }

    top = m_map.top + m_bands.GetPlaced()[range].headerTop - m_scrollTop;

    return { m_boundsDip.left, top, m_map.right, top + m_scaler.ToPx (kHeaderDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetHover
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapGrid::GetHover() const
{
    return m_hover.has_value() ? std::optional<Word> (m_bands.GetAddressOf (*m_hover)) : std::nullopt;
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

long HeatMapGrid::GetColumnLeft (const HeatMapBands::Placed & placed, long column) const
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

long HeatMapGrid::GetColumnAt (const HeatMapBands::Placed & placed, long x) const
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
    constexpr int  kPasses           = 2;
    constexpr int  kSectionAfterDip  = 10;     // the room after a section's rows
    constexpr int  kSectionHeaderDip = 16;     // a section's header
    long           pitch             = GetPitch();
    long           bar               = m_scaler.ToPx (kScrollbarDip);
    long           left              = m_boundsDip.left + m_gutterPx;
    long           top               = m_boundsDip.top  + m_scaler.ToPx (kBarDip);
    long           right             = std::max (left, m_boundsDip.right  - m_scaler.ToPx (kInsetDip));
    long           bottom            = std::max (top,  m_boundsDip.bottom - m_scaler.ToPx (kInsetDip));
    long           width             = 0;
    long           height            = std::max (0L, bottom - top);
    bool           hasVert           = false;
    bool           needVert          = false;



    SyncBands();

    //  Sections' boxes are kSectionPadDip out from the cells on each side.
    if (m_bands.IsSplit())
    {
        left  += m_scaler.ToPx (kSectionPadDip);
        right  = std::max (left, right - m_scaler.ToPx (kSectionPadDip));
    }

    for (int pass = 0; pass <= kPasses; pass++)
    {
        width     = std::max (0L, right - left - (hasVert ? bar + (m_bands.IsSplit() ? m_scaler.ToPx (kSectionPadDip) : 0L) : 0L));
        m_columns = HeatMapSections::GetColumnsFor (width, pitch);

        m_bands.Arrange (m_columns, pitch, m_scaler.ToPx (kHeaderDip), m_scaler.ToPx (kSectionHeaderDip), m_scaler.ToPx (kSectionAfterDip), m_labelLinePx);

        needVert = m_bands.GetContentHeight() > height;

        if (needVert == hasVert)
        {
            break;
        }

        hasVert = needVert;
    }

    m_hasVertBar = hasVert;
    m_rowPx      = width;
    m_areaBottom = top + height;
    m_map        = { left, top, left + width, top + std::min (height, m_bands.GetContentHeight()) };

    m_controls.PlaceZoomWidget (*this);
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
    std::optional<HeatMapBands::Place>    place  = GetPlaceAt (point);
    std::optional<Word>                   anchor = place.has_value() ? std::optional<Word> (m_bands.GetAddressOf (*place)) : std::nullopt;
    const HeatMapBands::Placed          * placed = place.has_value() ? &m_bands.GetPlaced()[place->band] : nullptr;
    long                                  pitch  = GetPitch();
    long                                  atY    = point.y - m_map.top + m_scrollTop;
    long                                  row    = (placed != nullptr) ? place->offset / placed->columns : 0;
    long                                  rowTop = (placed != nullptr) ? placed->rowsTop + row * pitch : 0;
    double                                fracY  = (double) std::clamp (atY - rowTop, 0L, pitch) / (double) pitch;
    double                                down   = (double) atY / (double) std::max (1L, m_bands.GetContentHeight());
    bool                                  isIn   = cellPx > m_cellPx;



    m_cellPx   = std::clamp (cellPx, 1, kMaxCellPx);
    m_isFitted = false;

    PlaceMap();

    pitch = GetPitch();
    place = anchor.has_value() ? m_bands.GetPlaceOf (*anchor) : std::nullopt;

    if (!place.has_value())
    {
        m_scrollTop = (long) std::lround (down * (double) m_bands.GetContentHeight()) - (point.y - m_map.top);
    }
    else
    {
        placed      = &m_bands.GetPlaced()[place->band];
        row         = place->offset / placed->columns;
        m_scrollTop = placed->rowsTop + (long) std::lround (((double) row + fracY) * (double) pitch) - (point.y - m_map.top);
    }

    //  Zoomed in, the row stays under the pointer even where that scrolls the
    //  map past either end of memory, as long as a row of it stays in view.
    //  Zoomed out, the map stops at the ends, so memory that fits the pane is
    //  at its top.
    if (isIn)
    {
        LimitScroll (std::min (0L, pitch - (m_areaBottom - m_map.top)), std::max (GetMaxScroll(), m_bands.GetContentHeight() - pitch));
    }
    else
    {
        ClampScroll();
    }

    m_revision++;
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
    constexpr float  kZoomPerNotch = 1.5f;
    int              cell          = 0;



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
    std::optional<HeatMapBands::Place>  place;
    long                                pitch = 0;



    m_cellPx   = kMaxCellPx;
    m_isFitted = false;

    PlaceMap();

    place = m_bands.GetPlaceOf (address);
    pitch = GetPitch();

    if (place.has_value())
    {
        const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[place->band];
        long                          row    = place->offset / placed.columns;



        m_scrollTop = placed.rowsTop + row * pitch + pitch / 2 - (m_map.bottom - m_map.top) / 2;
        ClampScroll();
    }

    m_revision++;
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
    m_revision++;
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
    return std::max (0L, m_bands.GetContentHeight() - (m_areaBottom - m_map.top));
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
    m_map.bottom = m_map.top + (isPast ? m_areaBottom - m_map.top : std::min (m_areaBottom - m_map.top, m_bands.GetContentHeight()));

    m_controls.SyncScrollbar (*this, m_bands.GetContentHeight());
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
    long  left = m_map.right + (m_bands.IsSplit() ? m_scaler.ToPx (kSectionPadDip) : 0L);



    if (!m_hasVertBar)
    {
        return {};
    }

    return { left, m_map.top, left + m_scaler.ToPx (kScrollbarDip), m_map.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapGrid::GetCellRect (Word address) const
{
    std::optional<HeatMapBands::Place>  place = m_bands.GetPlaceOf (address);



    if (!place.has_value() || place->band >= m_bands.GetPlaced().size())
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
    std::optional<HeatMapBands::Place>  place = GetPlaceAt (point);



    return place.has_value() ? std::optional<Word> (m_bands.GetAddressOf (*place)) : std::nullopt;
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
    m_revision++;
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
    double  down  = (double) m_scrollTop / (double) std::max (1L, m_bands.GetContentHeight());
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

    m_scrollTop = (long) std::lround (down * (double) m_bands.GetContentHeight());

    ClampScroll();
    m_revision++;
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
    m_revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid::ForEachVisible
//
//  Every cell with any part in the map's area, as scrolled, with its
//  address.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ForEachVisible (const std::function<void (const HeatMapBands::Place &, Word)> & visit) const
{
    long  pitch  = GetPitch();
    long  width  = m_map.right  - m_map.left;
    long  height = m_map.bottom - m_map.top;



    if (pitch <= 0 || width <= 0 || height <= 0)
    {
        return;
    }

    for (size_t band = 0; band < m_bands.GetPlaced().size() && band < m_bands.GetAll().size(); band++)
    {
        const HeatMapBands::Placed  & placed   = m_bands.GetPlaced()[band];
        long                          top      = m_scrollTop - placed.rowsTop;
        long                          bottom   = top + height;
        long                          firstRow = std::max (0L, top / pitch);
        long                          lastRow  = std::min (placed.rows - 1, bottom / pitch);



        if (bottom < 0)
        {
            continue;
        }

        for (long row = firstRow; row <= lastRow; row++)
        {
            for (long column = 0; column < placed.columns; column++)
            {
                HeatMapBands::Place  place = { band, row * placed.columns + column };



                if (place.offset >= m_bands.GetAll()[band].count)
                {
                    continue;
                }

                visit (place, m_bands.GetAddressOf (place));
            }
        }
    }
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
//  HeatMapGrid::ScrollToPlace
//
//  As little as brings the cell's row wholly into the map's area.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapGrid::ScrollToPlace (const HeatMapBands::Place & place)
{
    RECT  cell = GetPlaceRect (place);



    if (cell.top < m_map.top)
    {
        ScrollBy ((int) (cell.top - m_map.top - HeatMapSections::kStreetPx));
    }
    else if (cell.bottom > m_map.bottom)
    {
        ScrollBy ((int) (cell.bottom - m_map.bottom + HeatMapSections::kStreetPx));
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
        m_revision++;
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
    m_bands.Reset();
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
    m_revision++;
}
