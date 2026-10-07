#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"





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
    if (m_hasRanges && ranges == m_bands)
    {
        return;
    }

    m_bands     = std::move (ranges);
    m_hasRanges = true;
    m_scroll    = {};

    m_hover.reset();
    FitRanges();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ClearRanges
//
//  All of memory again, at the starting zoom.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ClearRanges()
{
    if (!m_hasRanges)
    {
        return;
    }

    m_bands     = { Band { {}, 0, kAddressCount } };
    m_hasRanges = false;
    m_isFitted  = false;

    m_hover.reset();
    ResetZoom();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::FitRanges
//
//  The largest cell whose rows all fit the pane, with no scrolling either
//  way; a pane too small for even one pixel a cell scrolls at one. While the
//  user has not zoomed, a resize fits them again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::FitRanges()
{
    m_isFitted = true;
    m_scroll   = {};

    for (int cell = kMaxCellPx; cell >= 1; cell--)
    {
        m_cellPx = cell;
        PlaceMap();

        if (!m_hasVertBar && !m_hasHorzBar)
        {
            break;
        }
    }

    BuildFrame();
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
//  memory is one range with no header.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PlaceBands()
{
    long  pitch  = GetPitch();
    long  header = m_hasRanges ? m_scaler.ToPx (kHeaderDip) : 0;
    long  y      = 0;



    m_placed.assign (m_bands.size(), Placed {});

    for (size_t i = 0; i < m_bands.size(); i++)
    {
        Placed  & placed = m_placed[i];
        int       count  = std::max (1, m_bands[i].count);



        placed.columns   = GetColumnsOf (m_columns, count);
        placed.headerTop = y;
        placed.rowsTop   = y + header;
        placed.rows      = (count + placed.columns - 1) / placed.columns;

        y = placed.rowsTop + placed.rows * pitch;
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
        int  columns = GetColumnsOf (m_columns, count);



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
    long            atX    = point.x - m_map.left + m_scroll.x;
    long            atY    = point.y - m_map.top  + m_scroll.y;
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
    long            left   = m_map.left + start - m_scroll.x;
    long            top    = m_map.top  + placed.rowsTop + (place.offset / placed.columns) * GetPitch() - m_scroll.y;



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



    if (!m_hasRanges || range >= m_placed.size())
    {
        return {};
    }

    top = m_map.top + m_placed[range].headerTop - m_scroll.y;

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
//  reads as the addresses it holds. Even kMinColumns that do not fit are
//  kept, and the map scrolls across instead. The row is then stretched to
//  the width, by less than twice over; past kMaxStretch twice the addresses
//  are narrowed to it instead, by no more than a quarter, so the cells stay
//  nearer the zoom's size, as long as each keeps a pixel beside its street.
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
//  column, so no column is more than a pixel wider than another. Rows that
//  fit the pane at the zoom's size are as wide as the pane; kMinColumns that
//  do not are a pitch a column, and scroll.
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
//  room and as tall as the rows or the room, less a scrollbar along each side
//  the rows overflow. A row is stretched to the room's width unless even
//  kMinColumns at the zoom's size are wider. Taking a scrollbar's room can
//  change the row, so the fit is found again until it settles, which takes
//  at most a pass for each side.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PlaceMap()
{
    constexpr int  kPasses  = 3;
    long           pitch    = GetPitch();
    long           bar      = m_scaler.ToPx (kScrollbarDip);
    long           left     = m_boundsDip.left + m_gutterPx;
    long           top      = m_boundsDip.top  + m_scaler.ToPx (kBarDip);
    long           right    = std::max (left, m_boundsDip.right  - m_scaler.ToPx (kInsetDip));
    long           bottom   = std::max (top,  m_boundsDip.bottom - m_scaler.ToPx (kInsetDip));
    long           width    = 0;
    long           height   = 0;
    long           contentW = 0;
    long           contentH = 0;
    bool           hasHorz  = false;
    bool           hasVert  = false;
    bool           needHorz = false;
    bool           needVert = false;



    for (int pass = 0; pass <= kPasses; pass++)
    {
        width     = std::max (0L, right  - left - (hasVert ? bar : 0));
        height    = std::max (0L, bottom - top  - (hasHorz ? bar : 0));
        m_columns = GetColumnsFor (width, pitch);
        contentW  = std::max (width, (m_columns == kMinColumns) ? (long) m_columns * pitch : 0L);

        PlaceBands();

        contentH  = m_contentH;
        needHorz  = contentW > width;
        needVert  = contentH > height;

        if (needHorz == hasHorz && needVert == hasVert)
        {
            break;
        }

        hasHorz = needHorz;
        hasVert = needVert;
    }

    m_hasHorzBar = hasHorz;
    m_hasVertBar = hasVert;
    m_rowPx      = contentW;
    m_map        = { left, top, left + std::min (width, contentW), top + std::min (height, contentH) };

    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ApplyCellPx
//
//  A new cell size, with the address under the point kept under it, the
//  same fraction of the way across its cell: the rows may hold a different
//  number of addresses afterward, so the address is found again by its
//  range and its place in the range rather than by its row and column. A
//  point over a header keeps the same fraction of the way down the map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ApplyCellPx (int cellPx, POINT point)
{
    std::optional<Place>  place  = GetPlaceAt (point);
    const Placed        * placed = place.has_value() ? &m_placed[place->band] : nullptr;
    long                  pitch  = GetPitch();
    long                  atX    = point.x - m_map.left + m_scroll.x;
    long                  atY    = point.y - m_map.top  + m_scroll.y;
    long                  column = (placed != nullptr) ? place->offset % placed->columns : 0;
    long                  row    = (placed != nullptr) ? place->offset / placed->columns : 0;
    long                  left   = (placed != nullptr) ? GetColumnLeft (*placed, column) : 0;
    long                  wide   = (placed != nullptr) ? std::max (1L, GetColumnLeft (*placed, column + 1) - left) : 1;
    long                  rowTop = (placed != nullptr) ? placed->rowsTop + row * pitch : 0;
    double                fracX  = (double) std::clamp (atX - left, 0L, wide) / (double) wide;
    double                fracY  = (double) std::clamp (atY - rowTop, 0L, pitch) / (double) pitch;
    double                down   = (double) atY / (double) std::max (1L, m_contentH);



    m_cellPx   = std::clamp (cellPx, 1, kMaxCellPx);
    m_isFitted = false;

    PlaceMap();

    if (!place.has_value())
    {
        m_scroll.y = (long) std::lround (down * (double) m_contentH) - (point.y - m_map.top);

        ClampScroll();
        BuildFrame();
        return;
    }

    placed = &m_placed[place->band];
    pitch  = GetPitch();
    column = place->offset % placed->columns;
    row    = place->offset / placed->columns;
    left   = GetColumnLeft (*placed, column);
    wide   = GetColumnLeft (*placed, column + 1) - left;

    m_scroll.x = left + (long) std::lround (fracX * (double) wide) - (point.x - m_map.left);
    m_scroll.y = placed->rowsTop + (long) std::lround (((double) row + fracY) * (double) pitch) - (point.y - m_map.top);

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ZoomAt
//
//  Each notch up makes a cell kZoomPerNotch times larger, and each down that
//  much smaller, by at least a pixel; the address under the point stays under
//  it. A point off the map zooms about the map's center.
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
//  HeatMapView::ScrollBy
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ScrollBy (int dx, int dy)
{
    m_scroll.x += dx;
    m_scroll.y += dy;

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ResetZoom
//
//  Back to the starting cell size, with the map's top left in view; for a
//  set of ranges, the size that fits them all.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ResetZoom()
{
    if (m_hasRanges)
    {
        FitRanges();
        return;
    }

    m_cellPx = std::max (1, m_scaler.ToPx (kDefaultCellDip));
    m_scroll = {};

    PlaceMap();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ClampScroll
//
//  The map scrolls no further than its far edge reaching the area's; a map
//  that fits its area does not scroll that way at all. The scrollbars follow.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ClampScroll()
{
    m_scroll.x = std::clamp (m_scroll.x, 0L, std::max (0L, m_rowPx - (m_map.right - m_map.left)));
    m_scroll.y = std::clamp (m_scroll.y, 0L, std::max (0L, m_contentH - (m_map.bottom - m_map.top)));

    SyncScrollbars();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetHorizontalBarRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetHorizontalBarRect() const
{
    if (!m_hasHorzBar)
    {
        return {};
    }

    return { m_map.left, m_map.bottom, m_map.right, m_map.bottom + m_scaler.ToPx (kScrollbarDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetVerticalBarRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetVerticalBarRect() const
{
    if (!m_hasVertBar)
    {
        return {};
    }

    return { m_map.right, m_map.top, m_map.right + m_scaler.ToPx (kScrollbarDip), m_map.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SyncScrollbars
//
//  Each bar in pixels: the rows' length, the part in view and the scroll.
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
    info.nPos  = (int) m_scroll.y;
    m_vertBar.SetScrollInfo (info);

    m_horzBar.Configure (DxuiScrollbar::Orientation::Horizontal, bar, bar, pitch);
    m_horzBar.SetTrack  (GetHorizontalBarRect());

    info.nMax  = (int) m_rowPx;
    info.nPage = (UINT) std::max (0L, m_map.right - m_map.left);
    info.nPos  = (int) m_scroll.x;
    m_horzBar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TickScrollbars (int64_t nowMs)
{
    return ((int) m_vertBar.Tick (nowMs) | (int) m_horzBar.Tick (nowMs)) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  An address takes the color of the kind that touched it most, as the mode
//  lets it be seen, mixed toward that color from the cold gray by how hot it
//  is. Where code and data are equally hot, code shows. A write shows over a
//  read as hot. An address nothing touched is the cold gray.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetColor (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette)
{
    constexpr float  kTop  = 255.0f;
    Byte             data  = std::max (read, write);
    uint32_t         hue   = (write >= read) ? palette.write : palette.read;
    Byte             level = data;



    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        hue   = palette.execute;
        level = execute;
    }

    if (level == 0)
    {
        return palette.cold | 0xFF000000u;
    }

    return DxuiColor::Mix (palette.cold | 0xFF000000u, hue | 0xFF000000u,
                           kFaintest + (1.0f - kFaintest) * (float) level / kTop) | 0xFF000000u;
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
    case Mode::Code: return L"Code";
    case Mode::Data: return L"Data";
    default:         return L"All";
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



    switch (m_options.view)
    {
    case Mode::Code: return execute;
    case Mode::Data: return std::max (read, write);
    default:         return std::max ({ execute, read, write });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::BuildCells
//
//  Each address's color; a kind with no levels is cold everywhere.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildCells()
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    bool              hasExecute = m_execute.size() == kCount;
    bool              hasRead    = m_read.size()    == kCount;
    bool              hasWrite   = m_write.size()   == kCount;



    m_cells.resize (kCount);

    for (size_t address = 0; address < kCount; address++)
    {
        m_cells[address] = GetColor (m_options.view,
                                     hasExecute ? m_execute[address] : (Byte) 0,
                                     hasRead    ? m_read[address]    : (Byte) 0,
                                     hasWrite   ? m_write[address]   : (Byte) 0,
                                     m_palette);
    }

    BuildFrame();
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
        long  at   = y + m_scroll.y;
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
                long  at     = x + m_scroll.x;
                long  column = GetColumnAt (placed, at);



                (*across)[(size_t) x] = (at < m_rowPx && at < GetColumnLeft (placed, column + 1) - kStreetPx) ? (int) column : -1;
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
//  The search runs through every range the reach crosses, so a hot cell
//  just past a header is found as one in the same range is.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetPickPlaceAt (POINT point) const
{
    std::optional<Place>  under   = GetPlaceAt (point);
    long                  pitch   = GetPitch();
    long                  reach   = m_scaler.ToPx (kSnapDip);
    long                  atX     = point.x - m_map.left + m_scroll.x;
    long                  atY     = point.y - m_map.top  + m_scroll.y;
    Place                 best;
    Byte                  hottest = 0;
    double                nearest = 0.0;



    if (!under.has_value() || m_cellPx >= m_scaler.ToPx (kComfortCellDip) || GetShownLevel (GetAddressOf (*under)) > 0)
    {
        return under;
    }

    best = *under;

    for (size_t band = 0; band < m_placed.size(); band++)
    {
        const Placed  & placed   = m_placed[band];
        long            firstRow = std::max (0L, atY - reach - placed.rowsTop) / pitch;
        long            lastRow  = std::min (placed.rows - 1, (atY + reach - placed.rowsTop) / pitch);
        long            lastCol  = GetColumnAt (placed, atX + reach);



        if (atY + reach < placed.rowsTop)
        {
            continue;
        }

        for (long row = firstRow; row <= lastRow; row++)
        {
            for (long column = GetColumnAt (placed, atX - reach); column <= lastCol; column++)
            {
                Place   place    = { band, row * placed.columns + column };
                Byte    level    = (place.offset < m_bands[band].count) ? GetShownLevel (GetAddressOf (place)) : (Byte) 0;
                double  left     = (double) GetColumnLeft (placed, column);
                double  wide     = (double) (GetColumnLeft (placed, column + 1) - kStreetPx) - left;
                double  dx       = left + wide / 2.0 - (double) atX;
                double  dy       = (double) (placed.rowsTop + row * pitch) + (double) m_cellPx / 2.0 - (double) atY;
                double  distance = dx * dx + dy * dy;



                if (level == 0 || level < hottest || (level == hottest && distance >= nearest))
                {
                    continue;
                }

                best    = place;
                hottest = level;
                nearest = distance;
            }
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetModeAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Mode> HeatMapView::GetModeAt (POINT point) const
{
    long  tab   = m_scaler.ToPx (kTabDip);
    long  left  = m_boundsDip.left + m_gutterPx;
    long  index = 0;



    if (tab <= 0 || point.y < m_boundsDip.top || point.y >= m_boundsDip.top + m_scaler.ToPx (kBarDip) || point.x < left)
    {
        return std::nullopt;
    }

    index = (point.x - left) / tab;

    if (index >= kModeCount)
    {
        return std::nullopt;
    }

    return (Mode) index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsPressed
//
//  A press on the map, or a scrollbar's thumb being dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsPressed() const
{
    return m_press.has_value() || m_vertBar.IsDragging() || m_horzBar.IsDragging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsOverMap
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsOverMap (POINT point) const
{
    for (const RECT & area : { m_map, GetVerticalBarRect(), GetHorizontalBarRect() })
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
    std::wstring      digits;
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
    digits  = std::to_wstring (rounded);

    for (size_t i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % 3 == 0)
        {
            grouped += L',';
        }

        grouped += digits[i];
    }

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
//  $2000". Once the machine has looked them up for the cell, a line for its
//  last writer and one for its last reader follow.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetTipText (Word address) const
{
    constexpr size_t  kCount = (size_t) kAddressCount;
    std::wstring      text   = HeatMapOptions::DescribeLocation (m_shownBank, address, m_hasAux);
    std::wstring      amount;
    int               shown  = 0;
    const std::pair<const wchar_t *, const std::vector<Byte> *>  kinds[] =
    {
        { L"executed", &m_execute },
        { L"read",     &m_read    },
        { L"written",  &m_write   },
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

    if (m_hoverAccess != nullptr && m_hoverAccess->address == address && m_hoverAccess->bank == m_shownBank)
    {
        text += L"\n" + HeatAccessJump::Describe (true,  m_hoverAccess->writer);
        text += L"\n" + HeatAccessJump::Describe (false, m_hoverAccess->reader);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetTipAt
//
//  The tip is anchored on the cell the pick framed, so it sits clear of the
//  frame and follows the pick from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text) const
{
    std::optional<Place>  pick;



    if (!m_visible || IsPressed())
    {
        return false;
    }

    pick = GetPickPlaceAt (point);

    if (!pick.has_value())
    {
        return false;
    }

    anchor = GetPlaceRect (*pick);
    InflateRect (&anchor, kStreetPx, kStreetPx);
    text   = GetTipText (GetAddressOf (*pick));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Layout
//
//  The first layout sets the starting zoom, which depends on the dpi. Ranges
//  the user has not zoomed are fitted to the pane again at its new size.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
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

    if (m_hasRanges && m_isFitted)
    {
        FitRanges();
        return;
    }

    PlaceMap();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::MeasureGutter
//
//  As wide as the widest row label in the face it is drawn in, so a text
//  zoom widens it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::MeasureGutter (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr int   kPadDip = 8;
    DxuiFontHandle  font    = theme.MonospaceFont();
    float           width   = 0.0f;
    float           height  = 0.0f;
    int             gutter  = 0;
    HRESULT         hr      = S_OK;



    hr = text.MeasureString (L"$FFFF", m_scaler.ToPxf (font.sizeDip), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f)
    {
        return;
    }

    gutter = (int) std::ceil (width) + m_scaler.ToPx (kPadDip);

    if (gutter == m_gutterPx)
    {
        return;
    }

    m_gutterPx = gutter;

    if (m_hasRanges && m_isFitted)
    {
        FitRanges();
        return;
    }

    PlaceMap();
    BuildFrame();
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

    PaintHeaders (text, theme);
    PaintHover   (text, theme);

    if (m_hasVertBar)
    {
        m_vertBar.Paint (painter, theme.ForegroundMuted());
    }

    if (m_hasHorzBar)
    {
        m_horzBar.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintBar
//
//  The modes as tabs, the chosen one underlined in the accent; then a swatch
//  for each color the mode shows, or, while the heat is being rebuilt after
//  a move through history, a note saying so in the key's place, where a
//  narrow pane still has room for it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kSwatchDip  = 10;
    constexpr int   kKeyDip     = 56;
    DxuiFontHandle  font        = theme.BodyFont();
    float           size        = m_scaler.ToPxf (font.sizeDip);
    float           bar         = m_scaler.ToPxf ((float) kBarDip);
    float           tab         = m_scaler.ToPxf ((float) kTabDip);
    float           swatch      = m_scaler.ToPxf ((float) kSwatchDip);
    float           key         = m_scaler.ToPxf ((float) kKeyDip);
    float           underline   = std::max (1.0f, m_scaler.ToPxf (2.0f));
    float           top         = (float) m_boundsDip.top;
    float           x           = (float) (m_boundsDip.left + m_gutterPx);
    float           right       = (float) m_boundsDip.right - m_scaler.ToPxf ((float) kInsetDip);
    HRESULT         hr          = S_OK;
    KeyList         keys;



    for (int index = 0; index < kModeCount; index++)
    {
        Mode  mode     = (Mode) index;
        bool  isChosen = (mode == m_options.view);



        hr = text.DrawString (GetModeLabel (mode).c_str(), x, top, tab, bar, isChosen ? theme.Foreground() : theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (isChosen)
        {
            painter.FillRect (x + tab / 4, top + bar - underline, tab / 2, underline, theme.Accent());
        }

        x += tab;
    }

    if (m_options.view != Mode::Data) { keys.emplace_back (L"Code",  m_palette.execute); }
    if (m_options.view != Mode::Code) { keys.emplace_back (L"Read",  m_palette.read);    }
    if (m_options.view != Mode::Code) { keys.emplace_back (L"Write", m_palette.write);   }

    x += swatch;

    if (m_isRebuilding)
    {
        hr = text.DrawString (kpszRebuildingNote, x, top, std::max (right - x, 0.0f), bar, theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        keys.clear();
    }

    for (const auto & [label, color] : keys)
    {
        if (x + key > right)
        {
            break;
        }

        painter.FillRect (x, top + (bar - swatch) / 2, swatch, swatch, color | 0xFF000000u);

        hr = text.DrawString (label.c_str(), x + swatch * 1.5f, top, key - swatch * 1.5f, bar, theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += key;
    }
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
    constexpr float  kLineHeight = 1.4f;
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



        while (step < placed.rows && (float) (step * pitch) < size * kLineHeight)
        {
            step *= 2;
        }

        //  The first labeled row at or below the area's top.
        row = ((std::max (0L, m_scroll.y - placed.rowsTop) / pitch + step - 1) / step) * step;

        for (; row < placed.rows; row += step)
        {
            y = (float) (m_map.top + placed.rowsTop + row * pitch - m_scroll.y);

            if (y + size > (float) m_map.bottom)
            {
                break;
            }

            label = std::format (L"${:04X}", (Word) (m_bands[band].first + row * placed.columns));

            hr = text.DrawString (label.c_str(), (float) m_boundsDip.left, y, gutter, size * kLineHeight,
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
//  nothing in it, a note saying so where the map would be.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintHeaders (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kPadDip = 4;
    DxuiFontHandle  font    = theme.BodyFont();
    float           size    = m_scaler.ToPxf (font.sizeDip);
    float           pad     = m_scaler.ToPxf ((float) kPadDip);
    float           left    = (float) m_boundsDip.left + pad;
    float           width   = std::max (0.0f, (float) m_map.right - left);
    std::wstring    title;
    HRESULT         hr      = S_OK;



    if (!m_hasRanges)
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



        if (header.top < m_map.top || header.bottom > m_map.bottom)
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
//  HeatMapView::PaintHover
//
//  A frame around the cell the mouse picks, drawn in the streets around it,
//  so a snap to a busier cell shows where it went.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintHover (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    RECT      cell  = {};
    float     left  = 0.0f;
    float     top   = 0.0f;
    float     wide  = 0.0f;
    float     tall  = (float) (m_cellPx + 2 * kStreetPx);
    float     line  = (float) kStreetPx;
    uint32_t  color = theme.Foreground();
    HRESULT   hr    = S_OK;



    if (!m_hover.has_value())
    {
        return;
    }

    cell = GetPlaceRect (*m_hover);
    left = (float) (cell.left - kStreetPx);
    top  = (float) (cell.top  - kStreetPx);
    wide = (float) (cell.right - cell.left + 2 * kStreetPx);

    if (left + wide <= (float) m_map.left || left >= (float) m_map.right || top + tall <= (float) m_map.top || top >= (float) m_map.bottom)
    {
        return;
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





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnPress
//
//  A press on a mode shows it; one on a scrollbar moves it, its thumb by a
//  drag; one on the map starts what is a click or a drag, which the release
//  and the moves decide.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnPress (const DxuiMouseEvent & ev)
{
    std::optional<Mode>  mode = GetModeAt (ev.positionDip);
    POINT                at   = ev.positionDip;



    if (mode.has_value())
    {
        SetMode (*mode);

        if (m_onOptionsChanged)
        {
            m_onOptionsChanged();
        }

        return true;
    }

    if (m_hasVertBar && m_vertBar.HitTest (at.x, at.y))
    {
        m_vertBar.OnMouseDown (at.x, at.y);
        ScrollBy (0, m_vertBar.GetScrollPos() - (int) m_scroll.y);
        return true;
    }

    if (m_hasHorzBar && m_horzBar.HitTest (at.x, at.y))
    {
        m_horzBar.OnMouseDown (at.x, at.y);
        ScrollBy (m_horzBar.GetScrollPos() - (int) m_scroll.x, 0);
        return true;
    }

    if (!GetAddressAt (at).has_value())
    {
        return false;
    }

    m_press       = at;
    m_pressScroll = m_scroll;
    m_isDragging  = false;
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



    if (m_vertBar.IsDragging() || m_horzBar.IsDragging())
    {
        m_vertBar.OnMouseUp();
        m_horzBar.OnMouseUp();
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
//  A scrollbar's thumb follows the mouse while it is dragged. With the button
//  down on the map, once the mouse has gone kDragDip the map follows it;
//  otherwise the pick follows the mouse, and a scrollbar under it widens.
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
        ScrollBy (0, m_vertBar.GetScrollPos() - (int) m_scroll.y);
        return true;
    }

    if (m_horzBar.IsDragging())
    {
        m_horzBar.OnMouseMove (at.x, at.y);
        ScrollBy (m_horzBar.GetScrollPos() - (int) m_scroll.x, 0);
        return true;
    }

    if (!m_press.has_value())
    {
        (void) m_vertBar.SetHover (m_hasVertBar && m_vertBar.HitTest (at.x, at.y), at);
        (void) m_horzBar.SetHover (m_hasHorzBar && m_horzBar.HitTest (at.x, at.y), at);

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

    m_scroll = { m_pressScroll.x - dx, m_pressScroll.y - dy };

    ClampScroll();
    BuildFrame();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnWheel
//
//  Over the map or its scrollbars: Ctrl with the wheel zooms about the mouse;
//  Shift with it, or a sideways wheel, scrolls across when the rows are wider
//  than the area; the wheel alone scrolls up and down, a few rows a notch.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnWheel (const DxuiMouseEvent & ev)
{
    long  pitch = GetPitch();
    long  rows  = std::max (1L, (long) std::lround ((double) m_scaler.ToPx (kWheelStepDip) / (double) pitch));
    int   step  = (int) std::lround (ev.wheelDelta * (float) (rows * pitch));



    if (!IsOverMap (ev.positionDip) || ev.wheelDelta == 0.0f)
    {
        return false;
    }

    if (ev.ctrl && !ev.wheelHorizontal)
    {
        ZoomAt (ev.positionDip, ev.wheelDelta);
    }
    else if (ev.wheelHorizontal || ev.shift)
    {
        if (!m_hasHorzBar)
        {
            return false;
        }

        ScrollBy (ev.wheelHorizontal ? step : -step, 0);
    }
    else
    {
        ScrollBy (0, -step);
    }

    SetHover (GetPickPlaceAt (ev.positionDip));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Leave:
        if (!IsPressed())
        {
            SetHover (std::nullopt);
            (void) m_vertBar.SetHover (false);
            (void) m_horzBar.SetHover (false);
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





