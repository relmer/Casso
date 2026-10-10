#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/Reply.h"
#include "Core/TextEncoding.h"





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
//  All of memory again, fitted to the pane.
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

    m_hover.reset();
    ResetZoom();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::FitRanges
//
//  The largest cell whose rows all fit the pane, with no scrolling either
//  way, for the ranges or for all of memory; a pane too small for even one
//  pixel a cell scrolls at one. While the user has not zoomed, a resize fits
//  them again. The fitted size is the zoom's 100%.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::FitRanges()
{
    m_isFitted  = true;
    m_scroll    = {};
    m_fittedFor = m_boundsDip;

    for (int cell = kMaxCellPx; cell >= 1; cell--)
    {
        m_cellPx = cell;
        PlaceMap();

        if (!m_hasVertBar && !m_hasHorzBar)
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

    PlaceZoomWidget();
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
        m_isFrameStale = true;
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
    m_isFrameStale = true;
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

void HeatMapView::ScrollBy (int dx, int dy)
{
    m_scroll.x += dx;
    m_scroll.y += dy;

    ClampScroll();
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
    return GetColor (mode, execute, read, write, palette, false, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  An operand byte's code color is drawn part of the way to the cold gray,
//  so an instruction's opcode stands out from the bytes after it. With
//  Blend, an address takes the colors of every kind the mode shows that
//  touched it, each weighted by how hot it is, at the hottest's brightness;
//  one both run as code and written takes the self-modifying color instead.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetColor (
    Mode               mode,
    Byte               execute,
    Byte               read,
    Byte               write,
    const Palette    & palette,
    bool               isOperand,
    bool               isBlend)
{
    constexpr float  kTop            = 255.0f;
    constexpr int    kRedShift       = 16;
    constexpr int    kGreenShift     = 8;
    constexpr int    kBlueShift      = 0;
    uint32_t         cold            = palette.cold | 0xFF000000u;
    uint32_t         code            = isOperand ? DxuiColor::Mix (palette.execute | 0xFF000000u, cold, kOperandDim) : (palette.execute | 0xFF000000u);
    Byte             ran             = (mode == Mode::Data) ? (Byte) 0 : execute;
    Byte             got             = (mode == Mode::Code) ? (Byte) 0 : read;
    Byte             put             = (mode == Mode::Code) ? (Byte) 0 : write;
    Byte             data            = std::max (read, write);
    uint32_t         hue             = (write >= read) ? palette.write : palette.read;
    Byte             level           = data;
    bool             isSelfModifying = ran > 0 && put > 0;
    float            total           = (float) ran + (float) got + (float) put;
    auto             mixChannel      = [&] (int shift)
    {
        float  sum = (float) ((code >> shift) & 0xFF) * ran + (float) ((palette.read >> shift) & 0xFF) * got + (float) ((palette.write >> shift) & 0xFF) * put;

        return (uint32_t) std::lround (sum / total) << shift;
    };



    if (isBlend)
    {
        level = std::max ({ ran, got, put });

        if (level > 0)
        {
            hue = isSelfModifying ? palette.selfModifying : (mixChannel (kRedShift) | mixChannel (kGreenShift) | mixChannel (kBlueShift));
        }
    }
    else if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        hue   = code;
        level = execute;
    }

    if (level == 0)
    {
        return cold;
    }

    return DxuiColor::Mix (cold, hue | 0xFF000000u, kFaintest + (1.0f - kFaintest) * (float) level / kTop) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetChannelColor
//
//  Changed shows the changes alone, mixed toward their color from the cold
//  gray as the others are. All and Data draw a read before written over
//  whatever else the address is, since it is the rare thing worth seeing.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetChannelColor (
    Mode              mode,
    Byte              unwritten,
    Byte              changed,
    uint32_t          color,
    const Palette   & palette)
{
    constexpr float  kTop  = 255.0f;
    uint32_t         hue   = palette.unwritten;
    Byte             level = unwritten;



    if (mode == Mode::Changed)
    {
        hue   = palette.changed;
        level = changed;
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
                                     IsOperand ((Word) address),
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
    case BreakpointKind::Io:
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
//  HeatMapView::IsPressed
//
//  A press on the map, or a scrollbar's thumb being dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsPressed() const
{
    return m_press.has_value() || m_vertBar.IsDragging() || m_horzBar.IsDragging() || m_zoomWidget.IsDragging();
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
//  $2000". A byte only ever run as an operand is "executed as an operand".
//  A line follows for each overlay on the cell (DescribeMarks). Once the
//  machine has looked them up for the cell, a line for its last writer and
//  one for its last reader follow.
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

    return text;
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

    if (m_pc == address)
    {
        text += L"\nPC";
    }

    if (m_stack == address)
    {
        text += L"\nStack pointer";
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
        case BreakKind::Read:    text += std::format (L"\nRead watchpoint #{}",           breakpoint.id); break;
        case BreakKind::Write:   text += std::format (L"\nWrite watchpoint #{}",          breakpoint.id); break;
        default:                 text += std::format (L"\nRead and write watchpoint #{}", breakpoint.id); break;
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
//  HeatMapView::TryGetTipAt
//
//  The tip is anchored on the cell the pick framed, so it sits clear of the
//  frame and follows the pick from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text) const
{
    std::optional<Place>  pick;



    if (!m_visible || IsPressed() || m_zoomWidget.HitTest (point) != HeatMapZoomWidget::Hit::None)
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
//  HeatMapView::ComputeFitCellPx
//
//  The cell FitRanges would choose for the pane as it is now, leaving the
//  zoom and the scroll the user has as they were.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapView::ComputeFitCellPx()
{
    int    zoomed = m_cellPx;
    POINT  scroll = m_scroll;
    int    fit    = 1;



    for (int cell = kMaxCellPx; cell >= 1; cell--)
    {
        m_cellPx = cell;
        PlaceMap();

        if (!m_hasVertBar && !m_hasHorzBar)
        {
            fit = cell;
            break;
        }
    }

    m_cellPx = zoomed;
    m_scroll = scroll;
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
    HRESULT         hr      = S_OK;



    hr = text.MeasureString (L"$FFFF", m_scaler.ToPxf (font.sizeDip), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f)
    {
        return;
    }

    gutter = (int) std::ceil (width) + DxuiPaneMetrics::GetContentTextInsetPx (m_scaler) + m_scaler.ToPx (kPadDip);

    if (gutter == m_gutterPx)
    {
        return;
    }

    m_gutterPx = gutter;

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
    PaintHeaders  (text, theme);
    PaintHover    (text, theme);

    PlaceZoomWidget();
    m_zoomWidget.Paint (text, theme, m_cellPx, GetStartCellPx(), kMaxCellPx);

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
        long            top      = m_scroll.y - placed.rowsTop;
        long            bottom   = top + height;
        long            firstRow = std::max (0L, top / pitch);
        long            lastRow  = std::min (placed.rows - 1, bottom / pitch);
        long            firstCol = GetColumnAt (placed, m_scroll.x);
        long            lastCol  = GetColumnAt (placed, m_scroll.x + width);



        if (bottom < 0)
        {
            continue;
        }

        for (long row = firstRow; row <= lastRow; row++)
        {
            for (long column = firstCol; column <= lastCol; column++)
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
//  line where the cells are large enough, then its outlines. A cell is
//  checked against the marks before its outlines are gathered, so a map
//  with none costs a comparison a cell.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintOverlays (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    float             minFont    = m_scaler.ToPxf (kMinDetailFontDip);
    float             pad        = m_scaler.ToPxf ((float) kDetailPadDip);
    bool              hasDetail  = m_values.size() == kCount && (float) m_cellPx >= minFont * kLinePerEm + pad * 2.0f;
    bool              hasMarks   = m_pc.has_value() || m_stack.has_value() || !m_breakpoints.empty();
    HRESULT           hr         = S_OK;



    if (!hasDetail && !hasMarks)
    {
        return;
    }

    hr = text.PushClipRect ((float) m_map.left, (float) m_map.top, (float) (m_map.right - m_map.left), (float) (m_map.bottom - m_map.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    ForEachVisible ([&] (const Place & place, Word address)
    {
        Word  cpu      = HeatMapOptions::GetCpuAddress (m_shownBank, address);
        bool  isMarked = m_pc == address || m_stack == address ||
                         std::ranges::any_of (m_breakpoints, [cpu] (const Breakpoint & each) { return cpu >= each.first && cpu <= each.last; });



        if (hasDetail)
        {
            PaintDetail (text, theme, place, address);
        }

        if (isMarked)
        {
            PaintOutlines (text, place, address);
        }
    });

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
//  form longer than kDetailChars is made smaller to fit, but no smaller
//  than kMinDetailFontDip, and is clipped to its cell beyond that.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintDetail (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Place & place, Word address) const
{
    DxuiFontHandle  font     = theme.MonospaceFont();
    RECT            cell     = GetPlaceRect (place);
    long            wide     = cell.right  - cell.left;
    long            tall     = cell.bottom - cell.top;
    float           minFont  = m_scaler.ToPxf (kMinDetailFontDip);
    float           pad      = m_scaler.ToPxf ((float) kDetailPadDip);
    Detail          detail   = GetDetail (wide, tall, m_scaler.ToPxf (font.sizeDip), minFont, pad);
    int16_t         stored   = m_values[address];
    Byte            value    = (Byte) stored;
    bool            isOpcode = m_opcodes.size() == (size_t) kAddressCount && m_opcodes[address] != 0;
    uint32_t        color    = GetDetailTextColor (GetCellColor (address), theme.Foreground(), m_palette.background);
    float           lineH    = detail.fontPx * kLinePerEm;
    float           top      = (float) cell.top + ((float) tall - lineH * (float) detail.lines) / 2.0f;
    std::wstring    second;
    float           fit      = 0.0f;
    HRESULT         hr       = S_OK;



    if (detail.lines == 0 || stored < 0)
    {
        return;
    }

    hr = text.DrawString (std::format (L"{:02X}", value).c_str(), (float) cell.left, top, (float) wide, lineH, color,
                          detail.fontPx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (detail.lines < 2)
    {
        return;
    }

    second = GetDetailLine (value, isOpcode, (m_opcodeForms != nullptr && m_opcodeForms->size() > value) ? (*m_opcodeForms)[value] : std::string());
    fit    = ((float) wide - pad * 2.0f) / ((float) second.size() * kAdvancePerEm);

    hr = text.PushClipRect ((float) cell.left, (float) cell.top, (float) wide, (float) tall);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (second.c_str(), (float) cell.left, top + lineH, (float) wide, lineH, color,
                          std::max (minFont, std::min (detail.fontPx, fit)), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
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

void HeatMapView::PaintOutlines (IDxuiTextRenderer & text, const Place & place, Word address) const
{
    constexpr int          kThickCellPx = 12;
    RECT                   cell         = GetPlaceRect (place);
    long                   line         = (m_cellPx >= kThickCellPx) ? 2 : 1;
    std::vector<uint32_t>  colors       = GetOutlineColors (address);
    RECT                   ring         = {};
    float                  left         = 0.0f;
    float                  top          = 0.0f;
    float                  wide         = 0.0f;
    float                  tall         = 0.0f;
    float                  thick        = (float) line;
    HRESULT                hr           = S_OK;



    for (size_t k = 0; k < colors.size(); k++)
    {
        ring = cell;
        InflateRect (&ring, kStreetPx - (long) k * line, kStreetPx - (long) k * line);

        if (ring.right - ring.left <= line * 2 || ring.bottom - ring.top <= line * 2)
        {
            break;
        }

        left = (float) ring.left;
        top  = (float) ring.top;
        wide = (float) (ring.right  - ring.left);
        tall = (float) (ring.bottom - ring.top);

        hr = text.FillRect (left,                top,                wide,  thick, colors[k] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top + tall - thick, wide,  thick, colors[k] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top,                thick, tall,  colors[k] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left + wide - thick, top,                thick, tall,  colors[k] | 0xFF000000u);
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
//  A press on a scrollbar moves it, its thumb by a drag; one on the map
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
    m_isFrameStale = true;
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
        m_zoomWidget.SetOpen (!m_zoomWidget.IsOpen());
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
//  zooming there brings scrollbars and takes them away, and the slider
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
//  The pane's bottom right, inside whichever scrollbar shows there, so the
//  zoom widget sits its own margin in from the corner. The map starts
//  fitted, with no scrollbars, so that is where it usually is.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetZoomCorner() const
{
    long  right  = m_boundsDip.right  - (m_hasVertBar ? m_scaler.ToPx (kScrollbarDip) : 0);
    long  bottom = m_boundsDip.bottom - (m_hasHorzBar ? m_scaler.ToPx (kScrollbarDip) : 0);



    return { m_boundsDip.left + m_gutterPx, m_boundsDip.top + m_scaler.ToPx (kBarDip), right, bottom };
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
//  HeatMapView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
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





