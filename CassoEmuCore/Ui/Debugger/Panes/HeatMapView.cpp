#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/Reply.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/HeatStrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetLevels
//
//  One level per address for each kind; empty vectors are a cold map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetLevels (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write)
{
    m_execute = execute;
    m_read    = read;
    m_write   = write;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetChannelLevels
//
//  Kept for the next SetLevels, which draws them with the rest.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetChannelLevels (const std::vector<Byte> & unwritten, const std::vector<Byte> & changed, const std::vector<Byte> & edited)
{
    m_unwritten = unwritten;
    m_changed   = changed;
    m_edited    = edited;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetPalette
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetPalette (const Palette & palette)
{
    if (palette == m_palette)
    {
        return;
    }

    m_palette = palette;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMode
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMode (Mode mode)
{
    m_options.view = mode;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetOptions
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetOptions (const HeatMapOptions & options)
{
    m_options = options;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetRanges
//
//  The same ranges again keep the zoom and scroll the user left; new ones
//  start fitted to the pane, from the top.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetRanges (std::vector<Band> ranges)
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
//  HeatMapView::ClearRanges
//
//  All of memory again, fitted to the pane.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ClearRanges()
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
//  HeatMapView::GetSectionStarts
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> HeatMapView::GetSectionStarts (const std::string & machineId)
{
    constexpr uint32_t  kPage = 0x100;
    std::vector<Word>   starts (kSectionStarts.begin(), kSectionStarts.end());
    Word                rom   = kSectionStarts.back();



    for (const HeatMapRangeSets::Preset & preset : HeatMapRangeSets::GetPresets())
    {
        //  "My program" has no fixed span, and I/O and the language card are
        //  cut at kSectionStarts.
        if (preset.last <= preset.first || preset.last >= kSectionStarts.front())
        {
            continue;
        }

        starts.push_back (preset.first);
        starts.push_back ((Word) (preset.last + 1));
    }

    for (uint32_t page = (uint32_t) rom + kPage; page < (uint32_t) kAddressCount; page += kPage)
    {
        if (MemoryRegionKeys::GetRomPartKey (machineId, (Word) page) != MemoryRegionKeys::GetRomPartKey (machineId, (Word) (page - kPage)))
        {
            starts.push_back ((Word) page);
        }
    }

    std::ranges::sort (starts);
    starts.erase (std::unique (starts.begin(), starts.end()), starts.end());
    std::erase (starts, (Word) 0);
    return starts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetRamLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetRamLabel (Word address)
{
    for (const HeatMapRangeSets::Preset & preset : HeatMapRangeSets::GetPresets())
    {
        if (preset.last > preset.first && preset.last < kSectionStarts.front() && address >= preset.first && address <= preset.last)
        {
            return TextEncoding::NarrowToWide (preset.name);
        }
    }

    return L"RAM";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SplitAtSections
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapView::Band> HeatMapView::SplitAtSections (const std::vector<Band> & ranges, bool isAllMemory, const std::string & machineId)
{
    std::vector<Band>  bands;
    std::vector<Word>  starts = GetSectionStarts (machineId);



    for (const Band & range : ranges)
    {
        uint32_t               first = range.first;
        uint32_t               end   = first + (uint32_t) std::max (0, range.count);
        std::vector<uint32_t>  cuts;
        Band                   whole = range;



        for (Word start : starts)
        {
            if (start > first && start < end)
            {
                cuts.push_back (start);
            }
        }

        if (cuts.empty())
        {
            whole.isSection = isAllMemory;
            bands.push_back (whole);
            continue;
        }

        cuts.push_back (end);

        for (uint32_t cut : cuts)
        {
            bands.push_back (Band { range.title, (Word) first, (int) (cut - first), true });
            first = cut;
        }
    }

    return bands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::CanSplit
//
//  Sections require the regions, which only the CPU's bank has.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::CanSplit() const
{
    return m_regions.size() == (size_t) kAddressCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SyncBands
//
//  The ranges cut into sections in a bank that has regions, and whole in one
//  that does not. The cell the mouse frames is found again by its address,
//  since its range and place in the range change.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SyncBands()
{
    bool                 isSplit = CanSplit();
    std::optional<Word>  hover   = GetHover();



    if (isSplit == m_isSplit)
    {
        return;
    }

    m_isSplit = isSplit;
    m_bands   = isSplit ? SplitAtSections (m_ranges, !m_hasRanges, m_machineId) : m_ranges;
    m_hover   = hover.has_value() ? GetPlaceOf (*hover) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetRegions
//
//  Regions arrive or go away as the current bank changes, which cuts the
//  map into sections or joins them, so it is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetRegions (const std::vector<uint16_t> & regions)
{
    bool  wasEmpty = m_regions.empty();



    m_regions = regions;

    if (wasEmpty != m_regions.empty() && m_boundsDip.right > m_boundsDip.left)
    {
        PlaceMap();
        m_isFrameStale = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMachineId
//
//  The sections of the system ROM's parts start where the machine's ROM
//  changes part, so a new machine cuts the map again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMachineId (const std::string & machineId)
{
    if (machineId == m_machineId)
    {
        return;
    }

    m_machineId = machineId;
    m_isSplit   = false;
    m_bands     = m_ranges;
    m_hover.reset();

    if (m_boundsDip.right > m_boundsDip.left)
    {
        PlaceMap();
        m_isFrameStale = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::FitRanges
//
//  The largest cell whose rows all fit the pane, with no scrolling, for the
//  ranges or for all of memory; a pane too small for even one pixel a cell
//  scrolls at one. While the user has not zoomed, a resize fits them again.
//  The fitted size is the zoom's 100%.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::FitRanges()
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
//  HeatMapView::GetColumnsOf
//
//  A range holds no more addresses to a row than the next power of two up
//  from its size, so a small range is one row across the pane rather than
//  a row mostly empty.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::GetColumnsOf (int columns, int count)
{
    int  fitted = 1;



    while (fitted < count && fitted < columns)
    {
        fitted *= 2;
    }

    return std::max (1, std::min (fitted, columns));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PlaceBands
//
//  Each range's header and rows, one after another down the map. All of
//  memory is one range with no header, until sections cut it. A range under
//  a header takes at least a row label's line, so the label of a range of
//  one short row ends before the next header. A section's rows are as wide
//  as the map's, so its cells line up with every other section's, and room
//  after them takes the bottom of its box.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PlaceBands()
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



        placed.columns   = m_bands[i].isSection ? m_columns : GetColumnsOf (m_columns, count);
        placed.headerTop = y;
        placed.rowsTop   = y + (!isHeaded ? 0L : m_bands[i].isSection ? label : header);
        placed.rows      = (count + placed.columns - 1) / placed.columns;

        y = placed.rowsTop + std::max (placed.rows * pitch, isHeaded ? m_labelLinePx : 0L) + (m_bands[i].isSection ? after : 0L);
    }

    m_contentH = y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetRows
//
//  Every range's rows together.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::GetRows() const
{
    int  rows = 0;



    for (const Band & band : m_bands)
    {
        int  count   = std::max (1, band.count);
        int  columns = band.isSection ? m_columns : GetColumnsOf (m_columns, count);



        rows += (count + columns - 1) / columns;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetBandAt
//
//  The range whose rows hold a height in the map, or -1 over a header or
//  past the last range.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapView::GetBandAt (long y) const
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
//  HeatMapView::GetPlaceAt
//
//  The cell under a point in the map, a street counting as the cell before
//  it; none over a header, past a range's last address, or off the map.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetPlaceAt (POINT point) const
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
//  HeatMapView::GetPlaceOf
//
//  The first range that holds an address.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetPlaceOf (Word address) const
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
//  HeatMapView::GetAddressOf
//
////////////////////////////////////////////////////////////////////////////////

Word HeatMapView::GetAddressOf (const Place & place) const
{
    return (Word) (m_bands[place.band].first + place.offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPlaceRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetPlaceRect (const Place & place) const
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
//  HeatMapView::GetHeaderRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetHeaderRect (size_t range) const
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
//  HeatMapView::GetHover
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapView::GetHover() const
{
    return m_hover.has_value() ? std::optional<Word> (GetAddressOf (*m_hover)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColumnsFor
//
//  Powers of two keep every row starting at a round address, so a row's label
//  is the first address in it. The row is then stretched to the width, by
//  less than twice over; past kMaxStretch twice the addresses are narrowed to
//  it instead, by no more than a quarter, so the cells are nearer the zoom's
//  size, as long as each cell is at least a pixel wide beside its street. A
//  width narrower than one cell at the pitch has a row of one address,
//  narrowed to the width, so a row never runs past it.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::GetColumnsFor (long widthPx, long pitchPx)
{
    constexpr long  kNarrowestPitch = 1 + kStreetPx;
    int             columns         = kMinColumns;
    long            fitted          = 0;
    bool            isTooStretched  = false;
    bool            canDouble       = false;



    while (columns * 2 <= kMaxColumns && (long) columns * 2 * pitchPx <= widthPx)
    {
        columns *= 2;
    }

    fitted         = (long) columns * pitchPx;
    isTooStretched = fitted <= widthPx && (double) widthPx > (double) fitted * kMaxStretch;
    canDouble      = columns * 2 <= kMaxColumns && (long) columns * 2 * kNarrowestPitch <= widthPx;

    if (isTooStretched && canDouble)
    {
        columns *= 2;
    }

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColumnLeft
//
//  Where a column starts within a range's row, from its left: the row's
//  width shared among its columns, the leftover pixels spread one to a
//  column, so no column is more than a pixel wider than another. Every row
//  is as wide as the map's area.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapView::GetColumnLeft (const Placed & placed, long column) const
{
    return (long) (((long long) column * m_rowPx) / placed.columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColumnAt
//
//  The column a point a distance into the row falls in, its street counting
//  as the column's; the last column past the row's end.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapView::GetColumnAt (const Placed & placed, long x) const
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
//  HeatMapView::PlaceMap
//
//  The addresses a row holds at the zoom in force, the row's width, and the
//  map's area: right of the row labels and below the bar, as wide as the
//  room and as tall as the rows or the room, less a scrollbar down the right
//  when the rows overflow. A row is stretched or narrowed to the room's
//  width, so the map never scrolls across. Taking the scrollbar's room can
//  change the row, so the fit is found again until it settles.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PlaceMap()
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
        m_columns = GetColumnsFor (width, pitch);

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
//  HeatMapView::ApplyCellPx
//
//  A new cell size, with the row of the address under the point scrolled to
//  the point's height, the same fraction of the way down its cell: the rows
//  may contain a different number of addresses afterward, so the address is
//  found again by its range and its place in the range rather than by its
//  row. Over a header, the scroll is the same fraction of the way down the
//  map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ApplyCellPx (int cellPx, POINT point)
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
//  HeatMapView::ZoomAt
//
//  Each notch up makes a cell kZoomPerNotch times larger, and each down that
//  much smaller, by at least a pixel; the row of the address under the point
//  stays at its height. A point off the map zooms about the map's center.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ZoomAt (POINT point, float notches)
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
//  HeatMapView::ZoomIn
//
//  A notch about the middle of the map, as the bar's button does.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ZoomIn()
{
    ZoomAt ({ (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 }, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ZoomOut
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ZoomOut()
{
    ZoomAt ({ (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 }, -1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ZoomTo
//
//  A cell cellPx tall, about the map's top left, as a zoom the user made:
//  the map is no longer fitted to the pane.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ZoomTo (int cellPx)
{
    ApplyCellPx (cellPx, { m_map.left, m_map.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ScrollBy
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ScrollBy (int dy)
{
    long  from = m_scrollTop;



    //  A map zoomed past an end scrolls back toward it, not farther past.
    m_scrollTop += dy;

    LimitScroll (std::min (0L, from), std::max (GetMaxScroll(), from));
    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ResetZoom
//
//  Back to the size that fits the map to the pane, from its top left.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ResetZoom()
{
    FitRanges();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ClampScroll
//
//  The scroll is clamped so the map's bottom is no higher than the area's; a
//  map that fits its area does not scroll at all. The scrollbar is updated to
//  match.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ClampScroll()
{
    LimitScroll (0, GetMaxScroll());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMaxScroll
//
//  The farthest the map scrolls with its bottom no higher than the area's.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapView::GetMaxScroll() const
{
    return std::max (0L, m_contentH - (m_areaBottom - m_map.top));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::LimitScroll
//
//  The scroll is clamped between lowest and highest. Past either end of
//  memory, the map fills its whole area, so the rows scrolled into it are
//  drawn and the empty part around memory is part of the map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::LimitScroll (long lowest, long highest)
{
    bool  isPast = false;



    m_scrollTop  = std::clamp (m_scrollTop, lowest, std::max (lowest, highest));
    isPast       = m_scrollTop < 0 || m_scrollTop > GetMaxScroll();
    m_map.bottom = m_map.top + (isPast ? m_areaBottom - m_map.top : std::min (m_areaBottom - m_map.top, m_contentH));

    SyncScrollbars();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetVerticalBarRect
//
//  Past the sections' boxes, which are kSectionPadDip out from the cells.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetVerticalBarRect() const
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
//  HeatMapView::SyncScrollbars
//
//  The bar in pixels: the rows' height, the part in view and the scroll.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SyncScrollbars()
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
//  HeatMapView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TickScrollbars (int64_t nowMs)
{
    bool  isBarMoving  = m_vertBar.Tick (nowMs);
    bool  isZoomMoving = m_zoomWidget.Tick (nowMs);



    return isBarMoving || isZoomMoving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  An address takes the color of the kind that touched it most, as the mode
//  lets it be seen, at its level on that kind's scale, the disassembly's
//  heat column's (HeatStrip::GetScaleColor). Where code and data are equally
//  hot, code is drawn. A write is drawn over a read as hot. An address nothing
//  touched is the cold gray.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetColor (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette)
{
    return GetColor (mode, execute, read, write, palette, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  With Blend, an address takes the colors of every kind the mode shows that
//  touched it, each at its level on its scale and weighted by how hot it
//  is; one both run as code and written takes the self-modifying color,
//  mixed toward it from the cold gray by how hot it is.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetColor (
    Mode               mode,
    Byte               execute,
    Byte               read,
    Byte               write,
    const Palette    & palette,
    bool               isBlend)
{
    constexpr float  kTop            = 255.0f;
    constexpr int    kRedShift       = 16;
    constexpr int    kGreenShift     = 8;
    constexpr int    kBlueShift      = 0;
    uint32_t         cold            = palette.cold | 0xFF000000u;
    Byte             ran             = (mode == Mode::Data) ? (Byte) 0 : execute;
    Byte             got             = (mode == Mode::Code) ? (Byte) 0 : read;
    Byte             put             = (mode == Mode::Code) ? (Byte) 0 : write;
    Byte             data            = std::max (read, write);
    Byte             level           = std::max ({ ran, got, put });
    float            total           = std::max (1.0f, (float) ran + (float) got + (float) put);   // at least 1, so C4723 cannot fire; a nonzero total of whole counts is 1 or more already
    uint32_t         code            = HeatStrip::GetScaleColor (HeatStrip::Kind::Run, ran, palette);
    uint32_t         readColor       = HeatStrip::GetScaleColor (HeatStrip::Kind::Read,  got, palette);
    uint32_t         writeColor      = HeatStrip::GetScaleColor (HeatStrip::Kind::Write, put, palette);
    auto             mixChannel      = [&] (int shift)
    {
        float  sum = (float) ((code >> shift) & 0xFF) * ran + (float) ((readColor >> shift) & 0xFF) * got + (float) ((writeColor >> shift) & 0xFF) * put;

        return (uint32_t) std::lround (sum / total) << shift;
    };



    if (isBlend && level == 0)
    {
        return cold;
    }

    if (isBlend && ran > 0 && put > 0)
    {
        return DxuiColor::Mix (cold, palette.selfModifying | 0xFF000000u, kFaintest + (1.0f - kFaintest) * (float) level / kTop) | 0xFF000000u;
    }

    if (isBlend)
    {
        return mixChannel (kRedShift) | mixChannel (kGreenShift) | mixChannel (kBlueShift) | 0xFF000000u;
    }

    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        return (execute == 0) ? cold : HeatStrip::GetScaleColor (HeatStrip::Kind::Run, execute, palette);
    }

    if (data == 0)
    {
        return cold;
    }

    return HeatStrip::GetScaleColor ((write >= read) ? HeatStrip::Kind::Write : HeatStrip::Kind::Read, data, palette);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetChannelColor
//
//  Changed shows the changes alone, mixed toward their color from the cold
//  gray as the others are. All and Data draw a read before written over
//  whatever else the address is, since it is the rare thing worth seeing,
//  from kUnwrittenFaintest rather than kFaintest.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetChannelColor (
    Mode              mode,
    Byte              unwritten,
    Byte              changed,
    uint32_t          color,
    const Palette   & palette)
{
    constexpr float  kTop   = 255.0f;
    uint32_t         hue    = palette.unwritten;
    Byte             level  = unwritten;
    float            floor  = kUnwrittenFaintest;



    if (mode == Mode::Changed)
    {
        hue   = palette.changed;
        level = changed;
        floor = kFaintest;
    }
    else if (mode == Mode::Code || unwritten == 0)
    {
        return color;
    }

    if (level == 0)
    {
        return palette.cold | 0xFF000000u;
    }

    return DxuiColor::Mix (palette.cold | 0xFF000000u, hue | 0xFF000000u,
                           floor + (1.0f - floor) * (float) level / kTop) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetModeLabel (Mode mode)
{
    switch (mode)
    {
    case Mode::Code:    return L"Code";
    case Mode::Data:    return L"Data";
    case Mode::Changed: return L"Changed";
    default:            return L"All";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetShownLevel
//
//  How hot an address is as the mode shows it.
//
////////////////////////////////////////////////////////////////////////////////

Byte HeatMapView::GetShownLevel (Word address) const
{
    constexpr size_t  kCount  = (size_t) kAddressCount;
    Byte              execute = (m_execute.size() == kCount) ? m_execute[address] : (Byte) 0;
    Byte              read    = (m_read.size()    == kCount) ? m_read[address]    : (Byte) 0;
    Byte              write   = (m_write.size()   == kCount) ? m_write[address]   : (Byte) 0;
    Byte              changed = (m_changed.size() == kCount) ? m_changed[address] : (Byte) 0;



    switch (m_options.view)
    {
    case Mode::Code:    return execute;
    case Mode::Data:    return std::max (read, write);
    case Mode::Changed: return changed;
    default:            return std::max ({ execute, read, write });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::BuildCells
//
//  Each address's color; a kind with no levels is cold everywhere. An
//  address with a symbol is tinted toward the symbol color, and the symbols
//  are found again first when the bank shown has changed.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildCells()
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    bool              hasExecute = m_execute.size() == kCount;
    bool              hasRead    = m_read.size()    == kCount;
    bool              hasWrite   = m_write.size()   == kCount;
    bool              hasSymbols = false;
    bool              hasUnread  = m_unwritten.size() == kCount;
    bool              hasChanged = m_changed.size() == kCount;



    if (m_symbols != nullptr && m_symbolsBank != m_shownBank)
    {
        MarkSymbols();
    }

    hasSymbols = m_symbolMarks.size() == kCount;

    m_cells.resize (kCount);

    for (size_t address = 0; address < kCount; address++)
    {
        m_cells[address] = GetColor (m_options.view,
                                     hasExecute ? m_execute[address] : (Byte) 0,
                                     hasRead    ? m_read[address]    : (Byte) 0,
                                     hasWrite   ? m_write[address]   : (Byte) 0,
                                     m_palette,
                                     m_options.blend);

        m_cells[address] = GetChannelColor (m_options.view,
                                            hasUnread  ? m_unwritten[address] : (Byte) 0,
                                            hasChanged ? m_changed[address]   : (Byte) 0,
                                            m_cells[address],
                                            m_palette);

        if (hasSymbols && m_symbolMarks[address] != 0)
        {
            m_cells[address] = DxuiColor::Mix (m_cells[address], m_palette.symbol | 0xFF000000u, kSymbolTint) | 0xFF000000u;
        }
    }

    m_isFrameStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetOpcodes
//
//  Drawn from the next levels set, which come with every snapshot.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetOpcodes (const std::vector<Byte> & opcodes)
{
    m_opcodes = opcodes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsOperand
//
//  Run as code but never fetched as an opcode: only ever an operand. With
//  no opcodes known, no byte is.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsOperand (Word address) const
{
    constexpr size_t  kCount = (size_t) kAddressCount;



    return m_opcodes.size() == kCount && m_execute.size() == kCount && m_execute[address] > 0 && m_opcodes[address] == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetBreakpoints (std::vector<Breakpoint> breakpoints)
{
    m_breakpoints = std::move (breakpoints);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetSymbols
//
//  The same copy again changes nothing; another is marked and drawn.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetSymbols (std::shared_ptr<const HeatMapSymbols> symbols)
{
    if (symbols == m_symbols)
    {
        return;
    }

    m_symbols = std::move (symbols);

    MarkSymbols();
    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::MarkSymbols
//
//  Each address of the bank shown whose CPU address has a symbol.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::MarkSymbols()
{
    constexpr size_t  kCount = (size_t) kAddressCount;
    std::string       name;



    m_symbolsBank = m_shownBank;
    m_symbolMarks.clear();

    if (m_symbols == nullptr)
    {
        return;
    }

    m_symbolMarks.assign (kCount, 0);

    for (size_t address = 0; address < kCount; address++)
    {
        if (HeatMapOptions::IsShown (m_shownBank, (Word) address) &&
            m_symbols->TryGetNameAt (HeatMapOptions::GetCpuAddress (m_shownBank, (Word) address), name))
        {
            m_symbolMarks[address] = 1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetBreakKind
//
//  A breakpoint on an address is run; a watchpoint on memory or I/O is read,
//  written or both, as it is set; one on memory taking a value is written.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::BreakKind> HeatMapView::GetBreakKind (const BreakpointInfo & info)
{
    if (!info.enabled)
    {
        return std::nullopt;
    }

    switch (info.kind)
    {
    case BreakpointKind::Address:
        return BreakKind::Execute;

    case BreakpointKind::Memory:
        switch (info.access)
        {
        case WatchAccess::Read:  return BreakKind::Read;
        case WatchAccess::Write: return BreakKind::Write;
        default:                 return BreakKind::ReadWrite;
        }

    case BreakpointKind::MemoryValue:
        return BreakKind::Write;

    default:
        return std::nullopt;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetOutlineColors
//
//  A watchpoint on both reads and writes is outlined twice, read outside.
//  A breakpoint is on the CPU's address, so in a bank's view it marks the
//  bank's byte the CPU reaches at that address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> HeatMapView::GetOutlineColors (Word address) const
{
    std::vector<uint32_t>  colors;
    Word                   cpu    = HeatMapOptions::GetCpuAddress (m_shownBank, address);



    if (m_pc == address)
    {
        colors.push_back (m_palette.pc);
    }

    if (m_stack == address)
    {
        colors.push_back (m_palette.stack);
    }

    for (const Breakpoint & breakpoint : m_breakpoints)
    {
        if (cpu < breakpoint.first || cpu > breakpoint.last)
        {
            continue;
        }

        switch (breakpoint.kind)
        {
        case BreakKind::Execute:
            colors.push_back (m_palette.breakpoint);
            break;

        case BreakKind::Read:
            colors.push_back (m_palette.readWatch);
            break;

        case BreakKind::Write:
            colors.push_back (m_palette.writeWatch);
            break;

        default:
            colors.push_back (m_palette.readWatch);
            colors.push_back (m_palette.writeWatch);
            break;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetCellColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetCellColor (Word address) const
{
    return m_cells.empty() ? (m_palette.cold | 0xFF000000u) : m_cells[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::BuildFrame
//
//  The map's area a pixel at a time, as scrolled: each cell its color, and
//  the street after it, a range's header and the part of its last row past
//  its last address the page.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildFrame()
{
    long                           width      = m_map.right  - m_map.left;
    long                           height     = m_map.bottom - m_map.top;
    long                           pitch      = GetPitch();
    uint32_t                       background = m_palette.background | 0xFF000000u;
    std::vector<std::vector<int>>  columns (m_bands.size());
    std::vector<long>              bands;
    std::vector<int>               rows;



    //  A pane behind another tab is drawn when it next paints, not on every
    //  snapshot the machine sends meanwhile.
    m_isFrameStale = !m_visible;

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
                line[x] = GetCellColor ((Word) (m_bands[(size_t) band].first + offset));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPixels
//
//  A change marks the frame stale rather than drawing it again: a wheel spun
//  or a snapshot that came in redrew every pixel of the map each time, and
//  input waited behind it. The frame is drawn once, when it is painted or
//  asked for.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> & HeatMapView::GetPixels()
{
    if (m_isFrameStale)
    {
        BuildFrame();
    }

    return m_frame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetCellRect (Word address) const
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
//  HeatMapView::GetAddressAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapView::GetAddressAt (POINT point) const
{
    std::optional<Place>  place = GetPlaceAt (point);



    return place.has_value() ? std::optional<Word> (GetAddressOf (*place)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPickAt
//
//  While cells are smaller than kComfortCellDip, a point over a cold cell
//  takes the hottest cell within kSnapDip of it, the nearer of two as hot;
//  with none, or with cells large enough to land on, the cell under it.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapView::GetPickAt (POINT point) const
{
    std::optional<Place>  pick = GetPickPlaceAt (point);



    return pick.has_value() ? std::optional<Word> (GetAddressOf (*pick)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPickPlaceAt
//
//  The search stays in the row under the mouse, so moving up or down always
//  lands on the next row, however cold; along the row the pick is the
//  nearest hot cell within kSnapDip. Over an instruction drawn as one cell,
//  the pick is its opcode.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetPickPlaceAt (POINT point) const
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

    if (!under.has_value() || m_cellPx >= m_scaler.ToPx (kComfortCellDip) || GetShownLevel (GetAddressOf (*under)) > 0)
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
        Byte   level    = (place.offset < m_bands[under->band].count) ? GetShownLevel (GetAddressOf (place)) : (Byte) 0;
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
//  HeatMapView::IsPressed
//
//  A press on the map, or the scrollbar's thumb or the zoom being dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsPressed() const
{
    return m_press.has_value() || m_vertBar.IsDragging() || m_zoomWidget.IsDragging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsOverMap
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsOverMap (POINT point) const
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
//  HeatMapView::FormatAmount
//
//  The value a level stands for, from the logarithmic scale the levels are
//  made on: within a few percent, so shown to two figures. While fading, a
//  rate a second, with level 255 meaning the top or more; while cumulative,
//  a count.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::FormatAmount (Byte level) const
{
    constexpr double  kTopLevel = 255.0;
    constexpr double  kOneTenth = 10.0;
    constexpr double  kFigures  = 2.0;
    double            value     = std::expm1 ((double) level / kTopLevel * std::log1p (m_top));
    double            scale     = 1.0;
    uint64_t          rounded   = 0;
    std::wstring      grouped;



    if (m_top <= 0.0 || level == 0)
    {
        return {};
    }

    if (!m_options.cumulative && value < 1.0 / kOneTenth)
    {
        return L"under 0.1/s";
    }

    if (!m_options.cumulative && value < kOneTenth)
    {
        return std::format (L"{:.1f}/s", value);
    }

    scale   = std::pow (kOneTenth, std::max (0.0, std::floor (std::log10 (std::max (value, 1.0))) + 1.0 - kFigures));
    rounded = (uint64_t) std::max (1.0, std::round (value / scale) * scale);
    grouped = DebuggerStatusText::FormatCount (rounded);

    if (m_options.cumulative)
    {
        return (rounded == 1) ? std::wstring (L"once") : grouped + L" times";
    }

    return grouped + ((level == (Byte) kTopLevel) ? L"+/s" : L"/s");
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetTipText
//
//  "$C65E  executed 120/s, read 3.5/s" while fading; "$C65E  executed 1,200
//  times, read once" while cumulative; "$C65E  untouched" when nothing did.
//  In a bank's view the address is where in the bank the cell is, "Aux RAM
//  $2000". A byte only ever run as an operand is "executed as an operand".
//  A line follows for each overlay on the cell (DescribeMarks). Once the
//  machine has looked them up for the cell, a line for its last writer and
//  one for its last reader follow.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetTipText (Word address) const
{
    constexpr size_t  kCount   = (size_t) kAddressCount;
    std::wstring      contents = GetContentsLine (address);
    std::wstring      text     = HeatMapOptions::DescribeLocation (m_shownBank, address, m_hasAux);
    std::wstring      amount;
    int               shown    = 0;
    const std::pair<const wchar_t *, const std::vector<Byte> *>  kinds[] =
    {
        { IsOperand (address) ? L"executed as an operand" : L"executed", &m_execute   },
        { L"read",                                                        &m_read      },
        { L"written",                                                     &m_write     },
        { L"changed",                                                     &m_changed   },
        { L"read before written",                                         &m_unwritten },
    };



    for (const auto & [name, levels] : kinds)
    {
        Byte  level = (levels->size() == kCount) ? (*levels)[address] : (Byte) 0;



        if (level == 0)
        {
            continue;
        }

        amount  = FormatAmount (level);
        text   += (shown == 0) ? L"  " : L", ";
        text   += name;
        text   += amount.empty() ? std::wstring() : L" " + amount;
        shown++;
    }

    if (shown == 0)
    {
        text += L"  untouched";
    }

    text += DescribeMarks (address);

    if (m_edited.size() == kCount && m_edited[address] != 0)
    {
        text += L"\nEdited in the debugger";
    }

    if (m_hoverAccess != nullptr && m_hoverAccess->address == address && m_hoverAccess->bank == m_shownBank)
    {
        text += L"\n" + HeatAccessJump::Describe (true,  m_hoverAccess->writer);
        text += L"\n" + HeatAccessJump::Describe (false, m_hoverAccess->reader);
    }

    return contents.empty() ? text : contents + L"\n" + text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetContentsLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetContentsLine (Word address) const
{
    int16_t       stored      = (m_values.size() == (size_t) kAddressCount) ? m_values[address] : (int16_t) -1;
    std::wstring  symbol      = TextEncoding::NarrowToWide (GetSymbolAt (address));
    std::wstring  instruction = DescribeInstruction (address);



    if (stored < 0)
    {
        return symbol;
    }

    if (!instruction.empty())
    {
        instruction.erase (0, 1);
        return symbol.empty() ? instruction : symbol + L": " + instruction;
    }

    return std::format (L"{}: {:02X}", symbol.empty() ? std::wstring (L"Value") : symbol, (Byte) stored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetTipTag
//
//  The PC and the stack pointer move as the machine runs, so they are a tag
//  at the end of the tip's first line rather than separate lines that come
//  and go.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::GetTipTag (Word address, std::wstring & tag, uint32_t & argb) const
{
    bool  isPc    = m_pc    == address;
    bool  isStack = m_stack == address;



    tag  = (isPc && isStack) ? L"PC, SP" : isPc ? L"PC" : isStack ? L"SP" : L"";
    argb = isPc ? m_palette.pc : isStack ? m_palette.stack : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetTipFrame
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::GetTipFrame (Word address, std::wstring & label, uint32_t & argb) const
{
    uint16_t  key = GetRegionAt (address);



    label.clear();
    argb = 0;

    if (m_regions.size() != (size_t) kAddressCount)
    {
        return;
    }

    if (key == 0)
    {
        label = GetRamLabel (address);
        return;
    }

    if (key == MemoryRegionKeys::kRegionAux && GetRamLabel (address) != L"RAM")
    {
        label = L"Aux RAM: " + GetRamLabel (address);
        argb  = MemoryRegionKeys::GetColor (key, m_regionColors);
        return;
    }

    label = MemoryRegionKeys::GetRomLabel (key, address, m_slotCards, m_machineId);
    argb  = MemoryRegionKeys::GetColor (key, m_regionColors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetCellKind
//
//  What GetColor and GetChannelColor draw for the levels, in the order they
//  draw it: Changed draws the changed writes alone; a read before written
//  covers the rest in All and Data; then Blend gives a mix of kinds, or the
//  self-modifying color, and else the hotter of code and data is drawn.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::CellKind HeatMapView::GetCellKind (
    Mode   mode,
    Byte   execute,
    Byte   read,
    Byte   write,
    Byte   unwritten,
    Byte   changed,
    bool   isOperand,
    bool   isBlend)
{
    Byte      ran       = (mode == Mode::Data) ? (Byte) 0 : execute;
    Byte      got       = (mode == Mode::Code) ? (Byte) 0 : read;
    Byte      put       = (mode == Mode::Code) ? (Byte) 0 : write;
    Byte      data      = std::max (read, write);
    int       kindCount = (ran > 0 ? 1 : 0) + (got > 0 ? 1 : 0) + (put > 0 ? 1 : 0);
    CellKind  code      = isOperand ? CellKind::Operand : CellKind::Code;



    if (mode == Mode::Changed)
    {
        return (changed > 0) ? CellKind::Changed : CellKind::Untouched;
    }

    if (mode != Mode::Code && unwritten > 0)
    {
        return CellKind::Unwritten;
    }

    if (isBlend)
    {
        if (kindCount == 0)
        {
            return CellKind::Untouched;
        }

        if (ran > 0 && put > 0)
        {
            return CellKind::SelfModifying;
        }

        if (kindCount > 1)
        {
            return CellKind::Blended;
        }

        return (ran > 0) ? code : (got > 0) ? CellKind::Read : CellKind::Written;
    }

    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        return (execute > 0) ? code : CellKind::Untouched;
    }

    if (data == 0)
    {
        return CellKind::Untouched;
    }

    return (write >= read) ? CellKind::Written : CellKind::Read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetCellKind
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::CellKind HeatMapView::GetCellKind (Word address) const
{
    constexpr size_t  kCount = (size_t) kAddressCount;
    Byte              level[5] =
    {
        (m_execute.size()   == kCount) ? m_execute[address]   : (Byte) 0,
        (m_read.size()      == kCount) ? m_read[address]      : (Byte) 0,
        (m_write.size()     == kCount) ? m_write[address]     : (Byte) 0,
        (m_unwritten.size() == kCount) ? m_unwritten[address] : (Byte) 0,
        (m_changed.size()   == kCount) ? m_changed[address]   : (Byte) 0,
    };



    return GetCellKind (m_options.view, level[0], level[1], level[2], level[3], level[4], IsOperand (address), m_options.blend);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetTipLead
//
//  The label of the kind that drew the cell, which the legend lists as well
//  ("Value changed"), and the cell's color as drawn, so the reader matches
//  the swatch to the map.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::TipLead HeatMapView::GetTipLead (Word address) const
{
    constexpr const wchar_t  * kpszUntouched = L"Untouched";
    constexpr const wchar_t  * kpszBlended   = L"Blend: mixed from the kinds that touched it";
    TipLead                    lead;



    lead.argb = (m_cells.size() == (size_t) kAddressCount) ? m_cells[address] : (m_palette.cold | 0xFF000000u);

    switch (GetCellKind (address))
    {
    case CellKind::Code:
    case CellKind::Operand:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatCode);          break;
    case CellKind::Read:          lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatRead);          break;
    case CellKind::Written:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatWrite);         break;
    case CellKind::SelfModifying: lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatSelfModifying); break;
    case CellKind::Unwritten:     lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatUnwritten);     break;
    case CellKind::Changed:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatChanged);       break;
    case CellKind::Blended:       lead.meaning = kpszBlended;                                                     break;
    case CellKind::Untouched:     lead.meaning = kpszUntouched;                                                   break;
    }

    return lead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::DescribeMarks
//
//  A line each, every one starting a new line of the tip: "Self-modifying:
//  run as code and written", "PC", "Stack pointer", each breakpoint as
//  "Breakpoint #2", "Read watchpoint #3", "Write watchpoint #4" or "Read
//  and write watchpoint #5", and "Symbol COUT".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::DescribeMarks (Word address) const
{
    constexpr size_t  kCount   = (size_t) kAddressCount;
    Word              cpu      = HeatMapOptions::GetCpuAddress (m_shownBank, address);
    bool              hasRun   = m_execute.size() == kCount && m_execute[address] > 0;
    bool              hasWrite = m_write.size()   == kCount && m_write[address]   > 0;
    std::wstring      text;
    std::string       name;



    if (hasRun && hasWrite)
    {
        text += L"\nSelf-modifying: run as code and written";
    }

    for (const Breakpoint & breakpoint : m_breakpoints)
    {
        if (cpu < breakpoint.first || cpu > breakpoint.last)
        {
            continue;
        }

        switch (breakpoint.kind)
        {
        case BreakKind::Execute: text += std::format (L"\nBreakpoint #{}",                breakpoint.id); break;
        case BreakKind::Read:    text += std::format (L"\nRead data breakpoint #{}",           breakpoint.id); break;
        case BreakKind::Write:   text += std::format (L"\nWrite data breakpoint #{}",          breakpoint.id); break;
        default:                 text += std::format (L"\nRead and write data breakpoint #{}", breakpoint.id); break;
        }
    }

    if (m_symbols != nullptr && m_symbols->TryGetNameAt (cpu, name))
    {
        text += L"\nSymbol " + TextEncoding::NarrowToWide (name);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetCellScale
//
//  The cell's color is mixed from the cold gray toward its kind's color by
//  its level, from kFaintest at level 1 to the full color at level 255
//  (GetColor, GetChannelColor); the scale has those two ends and the middle.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetCellScale (Word address, std::vector<uint32_t> & gradient, float & mark) const
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    constexpr float   kTop       = 255.0f;
    constexpr float   kMiddle    = 0.5f;
    uint32_t          cold       = m_palette.cold | 0xFF000000u;
    Byte              execute    = (m_execute.size()   == kCount) ? m_execute[address]   : (Byte) 0;
    Byte              read       = (m_read.size()      == kCount) ? m_read[address]      : (Byte) 0;
    Byte              write      = (m_write.size()     == kCount) ? m_write[address]     : (Byte) 0;
    Byte              unwritten  = (m_unwritten.size() == kCount) ? m_unwritten[address] : (Byte) 0;
    Byte              changed    = (m_changed.size()   == kCount) ? m_changed[address]   : (Byte) 0;
    uint32_t          hue        = 0;
    Byte              level      = 0;
    float             floor      = kFaintest;



    switch (GetCellKind (address))
    {
    case CellKind::Code:
    case CellKind::Operand:       hue = m_palette.execute;                                                           level = execute;                          break;
    case CellKind::Read:          hue = m_palette.read;                                                              level = read;                             break;
    case CellKind::Written:       hue = m_palette.write;                                                             level = write;                            break;
    case CellKind::SelfModifying: hue = m_palette.selfModifying;                                                     level = std::max ({ execute, read, write }); break;
    case CellKind::Unwritten:     hue = m_palette.unwritten;                                                         level = unwritten;                        break;
    case CellKind::Changed:       hue = m_palette.changed;                                                           level = changed;                          break;
    default:                                                                                                                                                   break;
    }

    if (level == 0)
    {
        return false;
    }

    mark = (float) level / kTop;

    gradient = GetScaleStops (GetCellKind (address));

    if (!gradient.empty())
    {
        return true;
    }

    floor     = (GetCellKind (address) == CellKind::Unwritten) ? kUnwrittenFaintest : kFaintest;
    hue      |= 0xFF000000u;
    gradient  = { DxuiColor::Mix (cold, hue, floor) | 0xFF000000u,
                  DxuiColor::Mix (cold, hue, floor + (1.0f - floor) * kMiddle) | 0xFF000000u,
                  hue };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetScaleStops
//
//  A kind's scale as the cells draw it: the heat column's stops for code,
//  an operand, a read or a write; none for the other kinds.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> HeatMapView::GetScaleStops (CellKind kind) const
{
    std::vector<uint32_t>  stops;
    HeatStrip::Kind        strip = HeatStrip::Kind::Run;



    switch (kind)
    {
    case CellKind::Code:
    case CellKind::Operand: strip = HeatStrip::Kind::Run; break;
    case CellKind::Read:    strip = HeatStrip::Kind::Read;    break;
    case CellKind::Written: strip = HeatStrip::Kind::Write;   break;
    default:                return stops;
    }

    for (uint32_t stop : HeatStrip::GetScaleStops (strip))
    {
        stops.push_back (stop);
    }

    return stops;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetTipAt
//
//  The tip is anchored on the cell the pick framed, so it sits clear of the
//  frame and follows the pick from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches) const
{
    std::vector<uint32_t>  gradient;
    float                  mark = -1.0f;



    return TryGetTipAt (point, anchor, text, swatches, gradient, mark);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetTipAt
//
//  With the cell's scale and its level on it, as a code view's heat tip
//  draws them.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches,
                               std::vector<uint32_t> & gradient, float & mark) const
{
    std::optional<Place>  pick;
    TipLead               lead;
    std::wstring          contents;
    std::wstring          rest;



    if (!m_visible || IsPressed() || m_zoomWidget.HitTest (point) != HeatMapZoomWidget::Hit::None)
    {
        return false;
    }

    pick = GetPickPlaceAt (point);

    if (!pick.has_value())
    {
        return false;
    }

    anchor   = GetPlaceRect (*pick);
    InflateRect (&anchor, kStreetPx, kStreetPx);
    lead     = GetTipLead (GetAddressOf (*pick));
    text     = GetTipText (GetAddressOf (*pick));
    contents = GetContentsLine (GetAddressOf (*pick));
    rest     = contents.empty() ? text : text.substr (contents.size() + 1);

    //  The cell's contents come first, with no swatch. An untouched cell's
    //  next line already ends in "untouched", so the swatch goes on that line
    //  in place of a lead line.
    if (GetCellKind (GetAddressOf (*pick)) != CellKind::Untouched)
    {
        rest = lead.meaning + L"\n" + rest;
    }

    text = contents.empty() ? rest : contents + L"\n" + rest;
    swatches.assign (contents.empty() ? 1 : 2, 0);
    swatches.back() = lead.argb | 0xFF000000u;

    gradient.clear();
    mark = -1.0f;

    if (!TryGetCellScale (GetAddressOf (*pick), gradient, mark))
    {
        gradient.clear();
        mark = -1.0f;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsTipComplete
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsTipComplete (POINT point) const
{
    std::optional<Place>  pick    = GetPickPlaceAt (point);
    Word                  address = 0;



    if (!pick.has_value() || !m_onHoverChanged)
    {
        return true;
    }

    address = GetAddressOf (*pick);

    return m_hoverAccess != nullptr && m_hoverAccess->address == address && m_hoverAccess->bank == m_shownBank;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Layout
//
//  A map the user has not zoomed is fitted to the pane again at its new
//  size.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
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
//  HeatMapView::RescaleForDpi
//
//  The zoom's percentage is the user's zoom rather than a size in pixels,
//  so it is the same after a move to a monitor of another scale: the cell
//  becomes the same multiple of the size that fits the pane at the new
//  scale. The cells then scale with the monitor as the text does. The map
//  is scrolled the same fraction of the way down.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::RescaleForDpi (const RECT & boundsPx, const DxuiDpiScaler & scaler)
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
//  HeatMapView::ComputeFitCellPx
//
//  The cell FitRanges would choose for the pane as it is now, leaving the
//  zoom and the scroll the user has as they were.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::ComputeFitCellPx()
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
//  HeatMapView::MeasureGutter
//
//  As wide as the widest row label in the face it is drawn in, so a text
//  zoom widens it, plus the pane's text inset ahead of the label, so the
//  widest label starts where the pane's title does, and the row labels' gap
//  before the map after it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::MeasureGutter (IDxuiTextRenderer & text, const IDxuiTheme & theme)
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
//  HeatMapView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    long     width  = 0;
    long     height = 0;
    HRESULT  hr     = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    MeasureGutter (text, theme);

    if (m_isFrameStale)
    {
        BuildFrame();
    }

    width  = m_map.right  - m_map.left;
    height = m_map.bottom - m_map.top;

    PaintBar       (painter, text, theme);
    PaintRowLabels (text, theme);

    if (width > 0 && height > 0 && m_frame.size() == (size_t) (width * height))
    {
        hr = text.DrawFramebuffer (m_frame.data(), (int) width, (int) height, (float) m_map.left, (float) m_map.top, (float) width, (float) height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    PaintOverlays (text, theme);

    if (m_isSplit)
    {
        PaintSections (text, theme);
    }

    PaintHeaders  (text, theme);
    PaintHover    (text, theme);

    PlaceZoomWidget();
    m_zoomWidget.Paint (text, theme, m_cellPx, GetStartCellPx(), kMaxCellPx);

    if (m_hasVertBar)
    {
        m_vertBar.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintBar
//
//  The row of views holds the window's strip of drop-downs (which view,
//  Blend, which ranges and which bank); while the heat is being rebuilt after
//  a move through history, a note in the room the strip leaves says so.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kGapDip = 10.0f;
    DxuiFontHandle   font    = theme.BodyFont();
    float            gap     = m_scaler.ToPxf (kGapDip);
    HRESULT          hr      = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (!m_isRebuilding || m_noteRect.right - m_noteRect.left <= gap)
    {
        return;
    }

    hr = text.DrawString (kpszRebuildingNote, (float) m_noteRect.left + gap, (float) m_noteRect.top,
                          (float) (m_noteRect.right - m_noteRect.left) - gap, (float) (m_noteRect.bottom - m_noteRect.top),
                          theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintRowLabels
//
//  Each row's first address beside it, every row while the rows are tall
//  enough to tell the labels apart and every second, fourth and so on below
//  that, counted from each range's first row.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintRowLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int    kPadDip     = 4;
    DxuiFontHandle   font        = theme.MonospaceFont();
    float            size        = m_scaler.ToPxf (font.sizeDip);
    long             pitch       = GetPitch();
    float            gutter      = (float) (m_gutterPx - m_scaler.ToPx (kPadDip));
    float            y           = 0.0f;
    std::wstring     label;
    HRESULT          hr          = S_OK;



    if (pitch <= 0 || gutter <= 0.0f)
    {
        return;
    }

    for (size_t band = 0; band < m_placed.size(); band++)
    {
        const Placed  & placed = m_placed[band];
        long            step   = 1;
        long            row    = 0;



        while (step < placed.rows && (float) (step * pitch) < size * kLabelLine)
        {
            step *= 2;
        }

        //  The first labeled row whose top is at or below the area's top, so
        //  a row partly scrolled off has no label over the bar.
        row = (std::max (0L, m_scrollTop - placed.rowsTop) + pitch - 1) / pitch;
        row = ((row + step - 1) / step) * step;

        for (; row < placed.rows; row += step)
        {
            y = (float) (m_map.top + placed.rowsTop + row * pitch - m_scrollTop);

            if (y + size > (float) m_map.bottom)
            {
                break;
            }

            label = std::format (L"${:04X}", (Word) (m_bands[band].first + row * placed.columns));

            hr = text.DrawString (label.c_str(), (float) m_boundsDip.left, y, gutter, size * kLabelLine,
                                  theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintHeaders
//
//  Each range's name and span across the top of its rows, "Hi-res page 1
//  $2000-$3FFF", where the whole header is in view; or, for a set with
//  nothing in it, a note saying so where the map would be. A header starts
//  at the pane's text inset, where the pane's title does.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintHeaders (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           size  = m_scaler.ToPxf (font.sizeDip);
    float           left  = (float) (m_boundsDip.left + DxuiPaneMetrics::GetContentTextInsetPx (m_scaler));
    float           width = std::max (0.0f, (float) m_map.right - left);
    std::wstring    title;
    HRESULT         hr    = S_OK;



    if (!m_hasRanges && !m_isSplit)
    {
        return;
    }

    if (m_bands.empty())
    {
        hr = text.DrawString (kpszNoRangesNote, (float) m_map.left, (float) m_map.top, std::max (0.0f, (float) m_boundsDip.right - (float) m_map.left),
                              m_scaler.ToPxf ((float) kHeaderDip), theme.ForegroundMuted(), size, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    for (size_t band = 0; band < m_bands.size() && band < m_placed.size(); band++)
    {
        RECT  header = GetHeaderRect (band);



        if (m_bands[band].isSection || !HasHeader (band) || header.top < m_map.top || header.bottom > m_map.bottom)
        {
            continue;
        }

        title = std::format (L"{}  ${:04X}-${:04X}", m_bands[band].title, m_bands[band].first,
                             (Word) (m_bands[band].first + std::max (1, m_bands[band].count) - 1));

        hr = text.DrawString (title.c_str(), left, (float) header.top, width, (float) (header.bottom - header.top), theme.Foreground(),
                              size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ForEachVisible
//
//  Every cell with any part in the map's area, as scrolled, with its
//  address.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ForEachVisible (const std::function<void (const Place &, Word)> & visit) const
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
//  HeatMapView::PaintOverlays
//
//  Over the cells in view, clipped to the map: each one's value and second
//  line where the cells are large enough, or its whole instruction, then
//  its outlines, around the whole instruction for its opcode, and last the
//  labeled ranges' outlines. A cell is checked against the marks
//  before its outlines are gathered, so a map with none costs a comparison
//  a cell.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintOverlays (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    float             minFont    = m_scaler.ToPxf (kMinDetailFontDip);
    float             pad        = m_scaler.ToPxf ((float) kDetailPadDip);
    bool              hasDetail  = m_values.size() == kCount && (float) m_cellPx >= minFont * kLinePerEm + pad * 2.0f;
    bool              hasMarks   = m_pc.has_value() || m_stack.has_value() || !m_breakpoints.empty();
    bool              hasRegions = m_regions.size() == kCount && m_cellPx >= kRegionCellPx;
    int               covered    = 0;
    HRESULT           hr         = S_OK;
    auto              isMarked   = [this] (Word address)
    {
        Word  cpu = HeatMapOptions::GetCpuAddress (m_shownBank, address);



        return m_pc == address || m_stack == address ||
               std::ranges::any_of (m_breakpoints, [cpu] (const Breakpoint & each) { return cpu >= each.first && cpu <= each.last; });
    };



    if (!hasDetail && !hasMarks && !hasRegions)
    {
        return;
    }

    //  A street wider on each side than the map, so the lines in the street
    //  left of the first column are drawn too.
    hr = text.PushClipRect ((float) (m_map.left - kStreetPx), (float) m_map.top, (float) (m_map.right - m_map.left + 2 * kStreetPx), (float) (m_map.bottom - m_map.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    ForEachVisible ([&] (const Place & place, Word address)
    {
        std::optional<Merged>  merged;



        //  An instruction's operand cells are drawn as part of its opcode's,
        //  and so is the first one in view when its opcode is above it.
        if (covered > 0)
        {
            covered--;
        }
        else if (hasDetail)
        {
            merged = GetMergedAt (place);

            if (merged.has_value())
            {
                PaintInstruction (text, theme, *merged);
                covered = (int) (merged->first.offset + merged->length - 1 - place.offset);
            }
            else
            {
                PaintDetail (text, theme, place, address);
            }
        }

        if (merged.has_value() && isMarked (merged->opcode))
        {
            PaintOutlines (text, GetMergedRects (*merged), merged->opcode);
        }
        else if ((!merged.has_value() || merged->opcode != address) && isMarked (address))
        {
            PaintOutlines (text, { GetPlaceRect (place) }, address);
        }
    });

    if (hasRegions)
    {
        PaintRegions (text);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetDetail
//
//  Two lines where both fit at kMinDetailFontDip or more, else one, else
//  none; the text as large as fits, up to the theme's monospace size.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::Detail HeatMapView::GetDetail (long widthPx, long heightPx, float maxFontPx, float minFontPx, float padPx)
{
    float  innerW = (float) widthPx  - padPx * 2.0f;
    float  innerH = (float) heightPx - padPx * 2.0f;
    float  one    = std::min ({ maxFontPx, innerH / kLinePerEm,        innerW / ((float) kValueChars  * kAdvancePerEm) });
    float  two    = std::min ({ maxFontPx, innerH / (kLinePerEm * 2.0f), innerW / ((float) kDetailChars * kAdvancePerEm) });



    if (two >= minFontPx)
    {
        return Detail { 2, two };
    }

    if (one >= minFontPx)
    {
        return Detail { 1, one };
    }

    return Detail {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetDetailLine
//
//  The opcode's form for a byte fetched as an opcode whose instruction is
//  known; otherwise the byte's bits, high first.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetDetailLine (Byte value, bool isOpcode, const std::string & form)
{
    constexpr int  kBits = 8;
    std::wstring   bits;



    if (isOpcode && !form.empty())
    {
        return TextEncoding::NarrowToWide (form);
    }

    for (int bit = kBits - 1; bit >= 0; bit--)
    {
        bits += ((value >> bit) & 1) ? L'1' : L'0';
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetDetailTextColor
//
//  The theme's text color or its page color, whichever stands out more from
//  the cell, so a value reads on a dim cell and a bright one alike.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetDetailTextColor (uint32_t cell, uint32_t foreground, uint32_t background)
{
    float  onText = DxuiColor::ComputeContrastRatio (foreground | 0xFF000000u, cell | 0xFF000000u);
    float  onPage = DxuiColor::ComputeContrastRatio (background | 0xFF000000u, cell | 0xFF000000u);



    return ((onText >= onPage) ? foreground : background) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintDetail
//
//  The value in hex, and under it the second line, centered in the cell. A
//  cell with a symbol has the symbol's name over the value, in place of the
//  second line where three lines do not fit, wherever the cell is tall enough
//  for two: the name is made smaller to fit across. A form or a name longer
//  than the cell is made smaller to fit, but no smaller than
//  kMinDetailFontDip, and is clipped to its cell beyond that. The value is
//  in ROM's or I/O's color for a byte in either, and the name in the
//  symbol color, each moved until it reads on the cell.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintDetail (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Place & place, Word address) const
{
    DxuiFontHandle  font     = theme.MonospaceFont();
    RECT            cell     = GetPlaceRect (place);
    long            wide     = cell.right  - cell.left;
    long            tall     = cell.bottom - cell.top;
    float           maxFont  = m_scaler.ToPxf (font.sizeDip);
    float           minFont  = m_scaler.ToPxf (kMinDetailFontDip);
    float           pad      = m_scaler.ToPxf ((float) kDetailPadDip);
    Detail          detail   = GetDetail (wide, tall, maxFont, minFont, pad);
    int16_t         stored   = m_values[address];
    Byte            value    = (Byte) stored;
    bool            isOpcode = m_opcodes.size() == (size_t) kAddressCount && m_opcodes[address] != 0;
    uint32_t        fill     = GetCellColor (address) | 0xFF000000u;
    uint32_t        plain    = DebuggerTextColors::GetReadable (GetDetailTextColor (fill, theme.Foreground(), m_palette.background), fill);
    uint32_t        ink      = GetValueInk (address, fill, plain);
    float           two      = std::min ({ maxFont, ((float) tall - pad * 2.0f) / (kLinePerEm * 2.0f), ((float) wide - pad * 2.0f) / ((float) kValueChars  * kAdvancePerEm) });
    float           three    = std::min ({ maxFont, ((float) tall - pad * 2.0f) / (kLinePerEm * 3.0f), ((float) wide - pad * 2.0f) / ((float) kDetailChars * kAdvancePerEm) });
    std::wstring    name     = (two >= minFont) ? TextEncoding::NarrowToWide (GetSymbolAt (address)) : std::wstring();
    int             lines    = detail.lines;
    float           fontPx   = detail.fontPx;
    float           lineH    = 0.0f;
    float           y        = 0.0f;
    std::wstring    second;
    HRESULT         hr       = S_OK;
    auto            drawFit  = [&] (const std::wstring & line, float lineTop, uint32_t color)
    {
        float  fit = ((float) wide - pad * 2.0f) / ((float) std::max<size_t> (1, line.size()) * kAdvancePerEm);



        hr = text.DrawString (line.c_str(), (float) cell.left, lineTop, (float) wide, lineH, color,
                              std::max (minFont, std::min (fontPx, fit)), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    };



    if (detail.lines == 0 || stored < 0)
    {
        return;
    }

    if (!name.empty() && three >= minFont)
    {
        lines  = 3;
        fontPx = three;
    }
    else if (!name.empty())
    {
        lines  = 2;
        fontPx = two;
    }

    lineH = fontPx * kLinePerEm;
    y     = (float) cell.top + ((float) tall - lineH * (float) lines) / 2.0f;

    hr = text.PushClipRect ((float) cell.left, (float) cell.top, (float) wide, (float) tall);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!name.empty())
    {
        drawFit (name, y, DebuggerTextColors::GetReadable (m_palette.symbol | 0xFF000000u, fill));
        y += lineH;
        lines--;
    }

    hr = text.DrawString (std::format (L"{:02X}", value).c_str(), (float) cell.left, y, (float) wide, lineH, ink,
                          fontPx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (lines >= 2)
    {
        second = GetDetailLine (value, isOpcode, (m_opcodeForms != nullptr && m_opcodeForms->size() > value) ? (*m_opcodeForms)[value] : std::string());
        drawFit (second, y + lineH, plain);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetValueInk
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetValueInk (Word address, uint32_t cell, uint32_t plain) const
{
    uint16_t  key = GetRegionAt (address);



    if (MemoryRegionKeys::IsRom (key) && m_regionColors.rom != 0)
    {
        return DebuggerTextColors::GetReadable (m_regionColors.rom | 0xFF000000u, cell | 0xFF000000u);
    }

    if (MemoryRegionKeys::IsIo (key) && m_regionColors.io != 0)
    {
        return DebuggerTextColors::GetReadable (m_regionColors.io | 0xFF000000u, cell | 0xFF000000u);
    }

    return plain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetSymbolAt
//
//  The marks are checked first, so a cell with no symbol costs a lookup in
//  a table rather than a search of the symbols.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapView::GetSymbolAt (Word address) const
{
    std::string  name;



    if (m_symbols == nullptr || m_symbolMarks.size() != (size_t) kAddressCount || m_symbolMarks[address] == 0)
    {
        return {};
    }

    if (!m_symbols->TryGetNameAt (HeatMapOptions::GetCpuAddress (m_shownBank, address), name))
    {
        name.clear();
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintRegions
//
//  Each cell in a labeled range has a line in the street on each side whose
//  neighbor is in another range, or in none, so the lines outline the whole
//  range: a range that ends partway along a row steps in there. The lines
//  are the range's color. A section above RAM is outlined by PaintSections
//  instead.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintRegions (IDxuiTextRenderer & text) const
{
    constexpr float  kOutlineMix = 0.8f;
    HRESULT          hr          = S_OK;



    ForEachVisible ([&] (const Place & place, Word address)
    {
        uint16_t        key     = GetRegionAt (address);
        const Band    & band    = m_bands[place.band];
        const Placed  & placed  = m_placed[place.band];
        long            column  = place.offset % placed.columns;
        RECT            cell    = {};
        float           street  = (float) kStreetPx;
        float           wide    = 0.0f;
        float           tall    = 0.0f;
        uint32_t        stroke  = 0;
        auto            isSame  = [&] (long offset)
        {
            return offset >= 0 && offset < band.count && GetRegionAt (GetAddressOf (Place { place.band, offset })) == key;
        };



        if (key == 0 || (band.isSection && band.first >= kSectionStarts.front()))
        {
            return;
        }

        cell   = GetPlaceRect (place);
        wide   = (float) (cell.right  - cell.left);
        tall   = (float) (cell.bottom - cell.top);
        stroke = DxuiColor::Mix (m_palette.background | 0xFF000000u, MemoryRegionKeys::GetColor (key, m_regionColors) | 0xFF000000u, kOutlineMix) | 0xFF000000u;
        stroke = DxuiColor::ComputeInkForContrast (stroke, m_palette.background | 0xFF000000u, kMinLineContrast) | 0xFF000000u;

        if (!isSame (place.offset - placed.columns))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.top - street, wide + street * 2.0f, street, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (!isSame (place.offset + placed.columns))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.bottom, wide + street * 2.0f, street, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (column == 0 || !isSame (place.offset - 1))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.top - street, street, tall + street * 2.0f, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (column == placed.columns - 1 || !isSame (place.offset + 1))
        {
            hr = text.FillRect ((float) cell.right, (float) cell.top - street, street, tall + street * 2.0f, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintSections
//
//  Each section in view as a box: a rounded outline in the range's color the
//  width of the map around its rows, at least a row label's line tall,
//  kSectionPadDip from the cells on every side, with its label on the top edge from
//  the left, past the corner's curve, in the header's room above the cells,
//  over a patch of the page that breaks the line there. A section of RAM
//  has its label alone, since a memory window doesn't outline main RAM.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintSections (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kOutlineMix = 0.6f;
    constexpr float  kCornerDip  = 4.0f;
    DxuiFontHandle   font        = theme.MonospaceFont();
    float            fontPx      = m_scaler.ToPxf (kSectionLabelDip);
    float            labelH      = fontPx * kLinePerEm;
    float            pad         = m_scaler.ToPxf ((float) kDetailPadDip);
    float            inside      = (float) m_scaler.ToPx (kSectionPadDip);
    float            radius      = m_scaler.ToPxf (kCornerDip);
    float            thickness   = m_scaler.ToPxf (1.0f);
    float            street      = (float) kStreetPx;
    long             pitch       = GetPitch();
    uint32_t         back        = m_palette.background | 0xFF000000u;
    HRESULT          hr          = S_OK;



    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_map.top, (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_map.bottom - m_map.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (size_t i = 0; i < m_bands.size() && i < m_placed.size(); i++)
    {
        const Band              & band      = m_bands[i];
        const Placed            & placed    = m_placed[i];
        bool                      isRam     = band.first < kSectionStarts.front();
        uint16_t                  key       = isRam ? (uint16_t) 0 : GetRegionAt (band.first);
        std::wstring              label     = isRam ? GetRamLabel (band.first) : MemoryRegionKeys::GetRangeLabel (key, band.first, band.count, m_slotCards, m_machineId);
        uint32_t                  color     = isRam ? theme.ForegroundMuted() : MemoryRegionKeys::GetColor (key, m_regionColors);
        float                     top       = (float) (m_map.top + placed.rowsTop - m_scrollTop);
        float                     left      = (float) m_map.left - street - inside;
        float                     right     = (float) (m_map.left + m_rowPx) + inside;
        float                     edge      = top - street - inside;
        float                     bottom    = top + (float) std::max (placed.rows * pitch, m_labelLinePx) + inside;
        float                     labelW    = (float) label.size() * fontPx * kAdvancePerEm + pad * 2.0f;
        float                     labelX    = left + radius + pad;
        std::vector<DxuiPointF>   points    = { { left, edge }, { right, edge }, { right, bottom }, { left, bottom } };



        if (!band.isSection || bottom < (float) m_map.top || edge - labelH > (float) m_map.bottom)
        {
            continue;
        }

        if (!isRam && key != 0 && color != 0)
        {
            for (const DxuiRoundedOutline::Segment & segment : DxuiRoundedOutline::GetSegments (points, radius))
            {
                hr = text.DrawLine (segment.from.x, segment.from.y, segment.to.x, segment.to.y, thickness,
                                    DxuiColor::ComputeInkForContrast (DxuiColor::Mix (back, color | 0xFF000000u, kOutlineMix) | 0xFF000000u, back, kMinLineContrast) | 0xFF000000u);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        hr = text.FillRect (labelX, edge - labelH / 2.0f, labelW, labelH, back);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (label.c_str(), labelX, edge - labelH / 2.0f, labelW, labelH, DebuggerTextColors::GetReadable (color | 0xFF000000u, back),
                              fontPx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetInstructionLength
//
//  From the opcode's form, its operand's digits dots: a branch has one byte
//  of offset however its target is written, BBR and BBS a zero-page byte
//  and an offset, and any other operand a byte for each two dots.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::GetInstructionLength (const std::string & form)
{
    static constexpr const char *  kBranches[] = { "BCC", "BCS", "BEQ", "BMI", "BNE", "BPL", "BVC", "BVS", "BRA" };
    size_t                         space       = form.find (' ');
    std::string                    mnemonic    = form.substr (0, space);



    if (form.empty() || space == std::string::npos)
    {
        return 1;
    }

    if (mnemonic.starts_with ("BBR") || mnemonic.starts_with ("BBS"))
    {
        return 3;
    }

    if (std::ranges::find (kBranches, mnemonic) != std::end (kBranches))
    {
        return 2;
    }

    return 1 + (int) std::ranges::count (form, '.') / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetInstructionText
//
//  The form with its dots filled in from the operand bytes: an address with
//  a symbol is the symbol's name; any other one, an immediate value or a zero
//  page address in hex; a branch's offset is its target.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetInstructionText (const std::string & form, Word cpu, const std::vector<Byte> & operands) const
{
    int           length   = GetInstructionLength (form);
    std::string   mnemonic = form.substr (0, form.find (' '));
    bool          isBranch = length == 2 && form.find ('#') == std::string::npos && std::ranges::count (form, '.') == 4;
    std::string   text;
    std::string   name;
    size_t        next     = 0;
    int           group    = 0;
    auto          symbolic = [this, &name] (Word address, int digits)
    {
        name.clear();

        if (m_symbols != nullptr && m_symbols->TryGetNameAt (address, name) && !name.empty())
        {
            return name;
        }

        return (digits == 4) ? std::format ("${:04X}", address) : std::format ("${:02X}", address);
    };



    for (size_t i = 0; i < form.size(); i = next)
    {
        size_t  run      = 0;
        bool    isImmed  = i > 0 && form[i - 1] == '#';
        Word    at       = 0;
        size_t  operand  = (size_t) group;



        next = i + 1;

        if (form[i] != '.')
        {
            text += form[i];
            continue;
        }

        while (i + run < form.size() && form[i + run] == '.')
        {
            run++;
        }

        next = i + run;

        if (mnemonic.starts_with ("BBR") || mnemonic.starts_with ("BBS"))
        {
            at    = (group == 0) ? (Word) operands.at (0) : (Word) (cpu + 3 + (int8_t) operands.at (1));
            text += (group == 0) ? symbolic (at, 2) : symbolic (at, 4);
        }
        else if (isBranch)
        {
            text += symbolic ((Word) (cpu + 2 + (int8_t) operands.at (0)), 4);
        }
        else if (run == 4)
        {
            text += symbolic ((Word) (operands.at (operand) | (operands.at (operand + 1) << 8)), 4);
        }
        else if (isImmed)
        {
            text += std::format ("${:02X}", operands.at (operand));
        }
        else
        {
            text += symbolic (operands.at (operand), 2);
        }

        group++;
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::DescribeInstruction
//
//  For a byte run as an opcode, a line with its instruction as the merged
//  cell writes it and the instruction's bytes, "LDA KBD  (AD 00 C0)"; empty
//  for any other byte, or one whose instruction's bytes are not all known.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::DescribeInstruction (Word address) const
{
    constexpr size_t   kCount = (size_t) kAddressCount;
    int16_t            stored = (m_values.size() == kCount) ? m_values[address] : (int16_t) -1;
    std::string        form;
    std::vector<Byte>  operands;
    std::wstring       bytes;
    int                length = 0;



    if (stored < 0 || m_opcodes.size() != kCount || m_opcodes[address] == 0 || m_opcodeForms == nullptr ||
        m_opcodeForms->size() <= (size_t) (Byte) stored || (*m_opcodeForms)[(Byte) stored].empty())
    {
        return {};
    }

    form   = (*m_opcodeForms)[(Byte) stored];
    length = GetInstructionLength (form);
    bytes  = std::format (L"{:02X}", (Byte) stored);

    for (int i = 1; i < length; i++)
    {
        int16_t  value = m_values[(Word) (address + i)];



        if (value < 0)
        {
            return {};
        }

        operands.push_back ((Byte) value);
        bytes += std::format (L" {:02X}", (Byte) value);
    }

    return L"\n" + GetInstructionText (form, HeatMapOptions::GetCpuAddress (m_shownBank, address), operands) + L"  (" + bytes + L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsMergeZoom
//
//  Whether the cells are tall enough for an instruction's two lines, so an
//  instruction is drawn as one cell across its bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsMergeZoom() const
{
    float  minFont = m_scaler.ToPxf (kMinDetailFontDip);
    float  pad     = m_scaler.ToPxf ((float) kDetailPadDip);



    return m_values.size() == (size_t) kAddressCount && m_opcodes.size() == (size_t) kAddressCount && m_opcodeForms != nullptr &&
           ((float) m_cellPx - pad * 2.0f) / (kLinePerEm * 2.0f) >= minFont;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMergeLength
//
//  The length of the instruction whose opcode is at a place, when it is
//  drawn as one cell: a byte run as an opcode, drawn in the code color, with its form
//  and every byte of it known, inside its range. Zero otherwise.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::GetMergeLength (const Place & place, Word address) const
{
    int16_t  stored = m_values[address];
    int      length = 0;



    if (stored < 0 || m_opcodes[address] == 0 || m_opcodeForms->size() <= (size_t) (Byte) stored ||
        (*m_opcodeForms)[(Byte) stored].empty() || GetCellKind (address) != CellKind::Code)
    {
        return 0;
    }

    length = GetInstructionLength ((*m_opcodeForms)[(Byte) stored]);

    if (place.offset + length > m_bands[place.band].count)
    {
        return 0;
    }

    for (int i = 1; i < length; i++)
    {
        if (m_values[(Word) (address + i)] < 0)
        {
            return 0;
        }
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMergedAt
//
//  The instruction drawn as one cell that contains a place: the instruction
//  at the place when the place is an opcode, else the one whose opcode is up
//  to two bytes before the place and whose length covers it. None at a zoom
//  too small to merge.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Merged> HeatMapView::GetMergedAt (const Place & place) const
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
        if (m_opcodes[address] != 0)
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMergedRects
//
//  An instruction's cell, one rectangle for each row it is on: one that
//  passes the end of its row continues at the start of the next.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> HeatMapView::GetMergedRects (const Merged & merged) const
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
//  HeatMapView::SplitInstruction
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::InstructionLines HeatMapView::SplitInstruction (const std::wstring & instruction, const std::wstring & bytes, const std::vector<long> & pieceCells)
{
    InstructionLines  lines (pieceCells.size());
    size_t            space = instruction.find (L' ');
    size_t            gap   = bytes.find (L' ');



    if (lines.empty())
    {
        return lines;
    }

    if (lines.size() > 1 && pieceCells.front() == 1 && space != std::wstring::npos && gap != std::wstring::npos)
    {
        lines[0] = { instruction.substr (0, space), bytes.substr (0, gap) };
        lines[1] = { instruction.substr (space + 1), bytes.substr (gap + 1) };
        return lines;
    }

    lines[0] = { instruction, bytes };
    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintInstruction
//
//  The instruction with its operand as a symbol where there is one, and
//  under it the bytes in hex, smaller and dimmer, as the disassembly draws
//  a line's bytes under its source. An instruction on two rows is split
//  between its pieces by SplitInstruction, each piece's text centered in
//  it, at one font size for all of them.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintInstruction (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Merged & merged) const
{
    constexpr float     kBytesScale = 0.8f;
    constexpr float     kBytesDim   = 0.45f;
    DxuiFontHandle      font        = theme.MonospaceFont();
    float               minFont     = m_scaler.ToPxf (kMinDetailFontDip);
    float               pad         = m_scaler.ToPxf ((float) kDetailPadDip);
    std::vector<RECT>   pieces      = GetMergedRects (merged);
    Byte                opcode      = (Byte) m_values[merged.opcode];
    std::vector<Byte>   operands;
    std::wstring        bytes       = std::format (L"{:02X}", opcode);
    std::wstring        instruction;
    std::vector<long>   cells;
    InstructionLines    lines;
    long                pitch       = GetPitch();
    long                tall        = pieces.front().bottom - pieces.front().top;
    float               fontPx      = 0.0f;
    float               lineH       = 0.0f;
    uint32_t            fill        = GetCellColor (merged.opcode) | 0xFF000000u;
    uint32_t            ink         = GetValueInk (merged.opcode, fill, DebuggerTextColors::GetReadable (GetDetailTextColor (fill, theme.Foreground(), m_palette.background), fill));
    HRESULT             hr          = S_OK;



    for (int i = 1; i < merged.length; i++)
    {
        Byte  value = (Byte) m_values[(Word) (merged.opcode + i)];



        operands.push_back (value);
        bytes += std::format (L" {:02X}", value);
    }

    for (const RECT & piece : pieces)
    {
        cells.push_back (std::max (1L, (piece.right - piece.left + HeatMapView::kStreetPx) / std::max (1L, pitch)));
    }

    instruction = GetInstructionText ((*m_opcodeForms)[opcode], HeatMapOptions::GetCpuAddress (m_shownBank, merged.opcode), operands);
    lines       = SplitInstruction (instruction, bytes, cells);
    fontPx      = std::min (m_scaler.ToPxf (font.sizeDip), ((float) tall - pad * 2.0f) / (kLinePerEm * 2.0f));

    for (size_t i = 0; i < pieces.size(); i++)
    {
        size_t  longest = std::max (lines[i].first.size(), lines[i].second.size());



        if (longest > 0)
        {
            fontPx = std::min (fontPx, ((float) (pieces[i].right - pieces[i].left) - pad * 2.0f) / ((float) longest * kAdvancePerEm));
        }
    }

    fontPx = std::max (minFont, fontPx);
    lineH  = fontPx * kLinePerEm;

    for (size_t i = 0; i < pieces.size(); i++)
    {
        const RECT  & piece = pieces[i];
        float         left  = (float) piece.left;
        float         top   = (float) piece.top + ((float) tall - lineH * 2.0f) / 2.0f;
        float         width = (float) (piece.right - piece.left);



        hr = text.FillRect (left, (float) piece.top, width, (float) tall, fill);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (lines[i].first.empty() && lines[i].second.empty())
        {
            continue;
        }

        hr = text.PushClipRect (left, (float) piece.top, width, (float) tall);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (lines[i].first.c_str(), left, top, width, lineH, ink, fontPx, font.face,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (lines[i].second.c_str(), left, top + lineH, width, lineH, DebuggerTextColors::GetReadable (DxuiColor::Mix (ink, fill, kBytesDim) | 0xFF000000u, fill),
                              std::max (minFont, fontPx * kBytesScale), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintOutlines
//
//  Each outline a ring, the first in the street around the cell like the
//  hover frame and each after it inside the one before; two pixels thick
//  once cells are large enough to keep their color inside them.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintOutlines (IDxuiTextRenderer & text, const std::vector<RECT> & rects, Word address) const
{
    constexpr int          kThickCellPx = 12;
    long                   line         = (m_cellPx >= kThickCellPx) ? 2 : 1;
    std::vector<uint32_t>  colors       = GetOutlineColors (address);
    RECT                   ring         = {};
    float                  left         = 0.0f;
    float                  top          = 0.0f;
    float                  wide         = 0.0f;
    float                  tall         = 0.0f;
    float                  thick        = (float) line;
    HRESULT                hr           = S_OK;



    for (size_t k = 0; k < colors.size() * rects.size(); k++)
    {
        long  inset = (long) (k / rects.size()) * line;



        ring = rects[k % rects.size()];
        InflateRect (&ring, kStreetPx - inset, kStreetPx - inset);

        if (ring.right - ring.left <= line * 2 || ring.bottom - ring.top <= line * 2)
        {
            break;
        }

        left = (float) ring.left;
        top  = (float) ring.top;
        wide = (float) (ring.right  - ring.left);
        tall = (float) (ring.bottom - ring.top);

        hr = text.FillRect (left,                top,                wide,  thick, colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top + tall - thick, wide,  thick, colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top,                thick, tall,  colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left + wide - thick, top,                thick, tall,  colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintHover
//
//  A frame around the cell the mouse picks, drawn in the streets around it,
//  so a snap to a busier cell shows where it went. An instruction drawn as
//  one cell is framed whole, a piece on each row it is on.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintHover (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    std::optional<Merged>  merged;
    std::vector<RECT>      rects;
    float                  tall  = (float) (m_cellPx + 2 * kStreetPx);
    float                  line  = (float) kStreetPx;
    uint32_t               color = theme.Foreground();
    HRESULT                hr    = S_OK;



    if (!m_hover.has_value())
    {
        return;
    }

    merged = GetMergedAt (*m_hover);
    rects  = merged.has_value() ? GetMergedRects (*merged) : std::vector<RECT> { GetPlaceRect (*m_hover) };

    for (const RECT & cell : rects)
    {
        float  left = (float) (cell.left - kStreetPx);
        float  top  = (float) (cell.top  - kStreetPx);
        float  wide = (float) (cell.right - cell.left + 2 * kStreetPx);



        if (left + wide <= (float) m_map.left || left >= (float) m_map.right || top + tall <= (float) m_map.top || top >= (float) m_map.bottom)
        {
            continue;
        }

        hr = text.FillRect (left,               top,               wide, line, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,               top + tall - line, wide, line, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,               top,               line, tall, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left + wide - line, top,               line, tall, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnKey
//
//  The arrow, page, Home and End keys move the frame to another cell, as the
//  mouse does, and the map is scrolled so the cell is in view. Enter opens
//  the cell's address in a memory window, as a click does. With no cell
//  framed, the first key frames the first cell in view.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnKey (const DxuiKeyEvent & ev)
{
    std::optional<Place>  target;



    if (ev.kind != DxuiKeyEventKind::Down || m_placed.empty())
    {
        return false;
    }

    if (ev.vk == VK_RETURN)
    {
        if (m_hover.has_value() && m_onPickAddress)
        {
            m_onPickAddress (GetAddressOf (*m_hover));
        }

        return m_hover.has_value();
    }

    target = GetKeyTarget (ev);

    if (!target.has_value())
    {
        return false;
    }

    ScrollToPlace (*target);
    SetHover (target);
    m_isFrameStale = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetKeyTarget
//
//  The cell a key moves the frame to, within the frame's range: the arrows
//  a cell or a row, an instruction drawn as one cell at a time; Page Up and
//  Page Down a map's height of rows; Home and End the row's ends, or with
//  Ctrl the range's. None for any other key.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetKeyTarget (const DxuiKeyEvent & ev) const
{
    std::optional<Place>   from    = m_hover.has_value() ? m_hover : GetPlaceAt (POINT { m_map.left + 1, m_map.top + 1 });
    std::optional<Merged>  merged;
    long                   columns = 0;
    long                   count   = 0;
    long                   page    = 0;
    long                   offset  = 0;



    if (!from.has_value() || from->band >= m_placed.size())
    {
        return std::nullopt;
    }

    if (!m_hover.has_value())
    {
        return from;
    }

    merged  = GetMergedAt (*from);
    columns = m_placed[from->band].columns;
    count   = m_bands[from->band].count;
    page    = std::max (1L, (m_map.bottom - m_map.top) / std::max (1, GetPitch())) * columns;
    offset  = from->offset;

    switch (ev.vk)
    {
    case VK_LEFT:  offset -= 1;                                                                    break;
    case VK_RIGHT: offset  = merged.has_value() ? merged->first.offset + merged->length : offset + 1; break;
    case VK_UP:    offset -= columns;                                                              break;
    case VK_DOWN:  offset += columns;                                                              break;
    case VK_PRIOR: offset -= page;                                                                 break;
    case VK_NEXT:  offset += page;                                                                 break;
    case VK_HOME:  offset  = ev.ctrl ? 0         : offset - offset % columns;                      break;
    case VK_END:   offset  = ev.ctrl ? count - 1 : offset - offset % columns + columns - 1;        break;
    default:       return std::nullopt;
    }

    offset = std::clamp (offset, 0L, count - 1);
    merged = GetMergedAt (Place { from->band, offset });

    return merged.has_value() ? merged->first : Place { from->band, offset };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ScrollToPlace
//
//  As little as brings the cell's row wholly into the map's area.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ScrollToPlace (const Place & place)
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
//  HeatMapView::OnPress
//
//  A press on the scrollbar moves it, its thumb by a drag; one on the map
//  starts what is a click or a drag, which the release and the moves decide.
//  The view is chosen on the window's strip in the row of views.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnPress (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;



    if (m_hasVertBar && m_vertBar.HitTest (at.x, at.y))
    {
        m_vertBar.OnMouseDown (at.x, at.y);
        ScrollBy (m_vertBar.GetScrollPos() - (int) m_scrollTop);
        return true;
    }

    if (!GetAddressAt (at).has_value())
    {
        return false;
    }

    m_press      = at;
    m_pressTop   = m_scrollTop;
    m_isDragging = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnRelease
//
//  A press that never became a drag is a click on the cell it picked, for
//  what the keys held ask (GetPickAction).
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnRelease (const DxuiMouseEvent & ev)
{
    std::optional<Word>  picked;
    PickAction           action = PickAction::ShowMemory;



    if (m_vertBar.IsDragging())
    {
        m_vertBar.OnMouseUp();
        return true;
    }

    if (!m_press.has_value())
    {
        return false;
    }

    if (!m_isDragging)
    {
        picked = GetPickAt (*m_press);
    }

    m_press.reset();
    m_isDragging = false;

    SetHover (GetPickPlaceAt (ev.positionDip));

    if (!picked.has_value())
    {
        return true;
    }

    action = GetPickAction (ev.ctrl, ev.shift, ev.alt);

    if (action == PickAction::ShowMemory && m_onPickAddress)
    {
        m_onPickAddress (*picked);
    }
    else if (action != PickAction::ShowMemory && m_onPickAccess)
    {
        m_onPickAccess (*picked, action);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnDragOrHover
//
//  The scrollbar's thumb follows the mouse while it is dragged. With the
//  button down on the map, once the mouse has gone kDragDip the map follows
//  it up and down; otherwise the pick follows the mouse, and the scrollbar
//  under it widens.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnDragOrHover (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;
    long   dx = 0;
    long   dy = 0;



    if (m_vertBar.IsDragging())
    {
        m_vertBar.OnMouseMove (at.x, at.y);
        ScrollBy (m_vertBar.GetScrollPos() - (int) m_scrollTop);
        return true;
    }

    if (!m_press.has_value())
    {
        (void) m_vertBar.SetHover (m_hasVertBar && m_vertBar.HitTest (at.x, at.y), at);

        SetHover (GetPickPlaceAt (at));
        return false;
    }

    dx = at.x - m_press->x;
    dy = at.y - m_press->y;

    if (!m_isDragging && std::max (std::abs (dx), std::abs (dy)) >= m_scaler.ToPx (kDragDip))
    {
        m_isDragging = true;
        SetHover (std::nullopt);
    }

    if (!m_isDragging)
    {
        return true;
    }

    m_scrollTop = m_pressTop - dy;

    LimitScroll (std::min (0L, m_pressTop), std::max (GetMaxScroll(), m_pressTop));
    m_isFrameStale = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnWheel
//
//  Over the map or its scrollbar: Ctrl with the wheel zooms about the mouse;
//  the wheel alone scrolls up and down, a few rows a notch. The rows always
//  fit across, so Shift with the wheel, or a sideways wheel, is left for the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnWheel (const DxuiMouseEvent & ev)
{
    long  pitch = GetPitch();
    long  rows  = std::max (1L, (long) std::lround ((double) m_scaler.ToPx (kWheelStepDip) / (double) pitch));
    int   step  = (int) std::lround (ev.wheelDelta * (float) (rows * pitch));



    //  Over the zoom's button or its panel, the wheel zooms about the map's
    //  middle, as the slider does.
    if (m_zoomWidget.HitTest (ev.positionDip) != HeatMapZoomWidget::Hit::None && ev.wheelDelta != 0.0f && !ev.wheelHorizontal)
    {
        ZoomAt ({ (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 }, ev.wheelDelta);
        return true;
    }

    if (!IsOverMap (ev.positionDip) || ev.wheelDelta == 0.0f || ev.wheelHorizontal)
    {
        return false;
    }

    if (ev.ctrl)
    {
        ZoomAt (ev.positionDip, ev.wheelDelta);
    }
    else if (ev.shift)
    {
        return false;
    }
    else
    {
        ScrollBy (-step);
    }

    SetHover (GetPickPlaceAt (ev.positionDip));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnZoomWidget
//
//  A press on the zoom's button opens its panel or closes it; one on the
//  track zooms to the cell size there and drags the zoom along it until the
//  button comes up; one on Reset goes back to the starting zoom. A press
//  anywhere else closes the panel and goes on to the map. The mouse over the
//  widget frames no cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnZoomWidget (const DxuiMouseEvent & ev)
{
    using Hit = HeatMapZoomWidget::Hit;

    POINT  center = { (m_map.left + m_map.right) / 2, (m_map.top + m_map.bottom) / 2 };
    Hit    hit    = Hit::None;



    PlaceZoomWidget();
    hit = m_zoomWidget.HitTest (ev.positionDip);

    if (m_zoomWidget.IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move)
        {
            ApplyCellPx (m_zoomWidget.GetCellPxAt (ev.positionDip, kMaxCellPx), center);
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_zoomWidget.SetDragging (false);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up)
    {
        return hit != Hit::None;
    }

    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    switch (hit)
    {
    case Hit::Button:
        m_zoomWidget.Toggle();
        return true;

    case Hit::Track:
        m_zoomWidget.SetDragging (true);
        ApplyCellPx (m_zoomWidget.GetCellPxAt (ev.positionDip, kMaxCellPx), center);
        return true;

    case Hit::Reset:
        ResetZoom();
        return true;

    case Hit::Panel:
        return true;

    default:
        m_zoomWidget.SetOpen (false);
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetViewRowFreeRect
//
//  From the map's left, past the row labels, to the pane's inset, as tall
//  as the row.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetViewRowFreeRect() const
{
    long  left  = m_boundsDip.left + m_gutterPx;
    long  right = m_boundsDip.right - m_scaler.ToPx (kInsetDip);



    if (!m_visible || right <= left)
    {
        return RECT {};
    }

    return RECT { left, m_boundsDip.top, right, m_boundsDip.top + m_scaler.ToPx (kBarDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PlaceZoomWidget
//
//  In the corner, except while its slider is open in a pane the same size:
//  zooming there brings the scrollbar and takes it away, and the slider
//  stays under the pointer rather than moving with the corner.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PlaceZoomWidget()
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
//  HeatMapView::GetZoomCorner
//
//  The pane's bottom right, inside the scrollbar when it is visible, so the
//  zoom widget is placed its margin in from the corner. The map starts
//  fitted, with no scrollbar, so that is where it usually is.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetZoomCorner() const
{
    long  right = m_boundsDip.right - (m_hasVertBar ? m_scaler.ToPx (kScrollbarDip) : 0);



    return { m_boundsDip.left + m_gutterPx, m_boundsDip.top + m_scaler.ToPx (kBarDip), right, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetZoomTipAt
//
//  Over the zoom's button, how to zoom: the tip anchored to the button.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetZoomTipAt (POINT point, RECT & anchor, std::wstring & text) const
{
    if (!m_visible || m_zoomWidget.HitTest (point) != HeatMapZoomWidget::Hit::Button)
    {
        return false;
    }

    anchor = m_zoomWidget.GetButtonRect();
    text   = HeatMapZoomWidget::kpszTip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnFocusChanged
//
//  The zoom slider closes when the focus moves to another pane, as a menu
//  does, since a click there isn't routed to the map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::OnFocusChanged (bool focused)
{
    if (!focused)
    {
        m_zoomWidget.SetOpen (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
    //  The zoom slider opens with the pointer over its button, and closes
    //  once the pointer moves away from it or off the map, except while its
    //  thumb is being dragged.
    if (ev.kind == DxuiMouseEventKind::Move && !m_zoomWidget.IsDragging())
    {
        PlaceZoomWidget();
        m_zoomWidget.OnHover (ev.positionDip);
    }
    else if (ev.kind == DxuiMouseEventKind::Leave && m_zoomWidget.IsOpen() && !m_zoomWidget.IsDragging())
    {
        m_zoomWidget.SetOpen (false);
    }

    //  The pointer over the zoom frames no cell, and the move is left for the
    //  window, which shows the zoom's tip.
    if (ev.kind == DxuiMouseEventKind::Move && !IsPressed() && m_zoomWidget.HitTest (ev.positionDip) != HeatMapZoomWidget::Hit::None)
    {
        SetHover (std::nullopt);
        return false;
    }

    if (OnZoomWidget (ev))
    {
        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Leave:
        if (!IsPressed())
        {
            SetHover (std::nullopt);
            (void) m_vertBar.SetHover (false);
        }

        return false;

    case DxuiMouseEventKind::Move:
        return OnDragOrHover (ev);

    case DxuiMouseEventKind::Down:
        return ev.button == DxuiMouseButton::Left && OnPress (ev);

    case DxuiMouseEventKind::Up:
        return ev.button == DxuiMouseButton::Left && OnRelease (ev);

    case DxuiMouseEventKind::Wheel:
        return OnWheel (ev);

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPickAction
//
//  A plain click shows the address in memory. Ctrl shows the last writer in
//  the disassembly, and Shift with it the last reader; Alt with Ctrl goes
//  back through history to that write or read instead.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::PickAction HeatMapView::GetPickAction (bool ctrl, bool shift, bool alt)
{
    if (!ctrl)
    {
        return PickAction::ShowMemory;
    }

    if (alt)
    {
        return shift ? PickAction::RewindToRead : PickAction::RewindToWrite;
    }

    return shift ? PickAction::ShowReader : PickAction::ShowWriter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetHover
//
//  The cell framed, and whoever follows it told when it changes.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetHover (const std::optional<Place> & hover)
{
    std::optional<Word>  before = GetHover();



    m_hover = hover;

    if (GetHover() != before && m_onHoverChanged)
    {
        m_onHoverChanged (GetHover());
    }
}





