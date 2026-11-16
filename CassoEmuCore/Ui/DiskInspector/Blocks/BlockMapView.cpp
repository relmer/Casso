#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);

    m_scaler = scaler;

    if (!m_isFrozen)
    {
        Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetGridRect
//
//  Below the bands when there are any.
//
////////////////////////////////////////////////////////////////////////////////

RECT BlockMapView::GetGridRect() const
{
    RECT  grid = GetBounds();



    if (!m_bands.empty())
    {
        grid.top += (LONG) (m_scaler.ToPxf (kBandDip) + m_scaler.ToPxf (kBandGapDip));
    }

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::Refresh
//
//  The grid is the volume fitted to the view, its blocks drawn the zoom
//  times larger, so a zoomed grid has the same rows with larger cells, down
//  to one block a cell.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Refresh()
{
    bool          isOpen = m_model != nullptr && m_model->IsOpen();
    uint32_t      count  = isOpen ? m_model->GetBlockCount() : 0;
    uint32_t      block  = 0;
    uint32_t      cell   = 0;
    SectorRole    role   = SectorRole::Free;
    RECT          grid   = {};
    BlockMapGrid  fit;



    m_bands      = isOpen ? m_model->GetBands() : vector<BlockBand>();
    m_blockCount = count;
    m_current    = isOpen ? m_model->GetBlock() : 0;
    grid         = GetGridRect();

    BuildStops();

    fit    = BlockMapGrid (count, (int) (grid.right - grid.left), (int) (grid.bottom - grid.top));
    m_grid = m_zoom <= 1.0 ? fit : BlockMapGrid (count, fit.GetBlockColumns(), fit.GetBlockPx() * m_zoom);

    ClampPan();

    m_roles.assign (m_grid.GetCellCount(), SectorRole::Free);
    m_highlighted.assign (m_grid.GetCellCount(), false);
    m_numbers.assign (m_grid.GetCellCount(), 0);
    m_differs.assign (m_grid.GetCellCount(), false);
    m_hasHighlight = false;

    for (block = 0; block < count && m_grid.GetCellCount() != 0; block++)
    {
        cell = m_grid.GetCellOf (block);
        role = m_model->GetRole (block);

        m_roles[cell] = BlockMap::PickRole (m_roles[cell], role);

        if (m_model->HasHighlight() && m_model->IsHighlighted (block))
        {
            m_highlighted[cell] = true;
            m_hasHighlight      = true;
            m_numbers[cell]     = m_grid.GetBlocksPerCell() == 1 ? GetNumberInFile (block) : 0;
        }

        if (m_isDiffsShown && m_model->IsDifferent (block))
        {
            m_differs[cell] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::BuildStops
//
//  Even steps from the whole volume up to the zoom at which a cell of one
//  block is kMostCellDip across.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::BuildStops()
{
    RECT          grid = GetGridRect();
    int           w    = (int) (grid.right - grid.left);
    int           h    = (int) (grid.bottom - grid.top);
    double        most = 1.0;
    BlockMapGrid  fit;



    if (!m_stops.empty() && w == m_stopsSize.cx && h == m_stopsSize.cy && m_blockCount == m_stopsCount)
    {
        return;
    }

    m_stopsSize  = { w, h };
    m_stopsCount = m_blockCount;
    m_stops.clear();

    if (!m_isZoomable || w <= 0 || h <= 0 || m_blockCount == 0)
    {
        m_stops.push_back (1.0);
        return;
    }

    fit     = BlockMapGrid (m_blockCount, w, h);
    most    = fit.GetBlockPx() <= 0.0 ? 1.0 : std::max (1.0, (m_scaler.ToPxf (kMostCellDip) + BlockMapGrid::kGapPx) / fit.GetBlockPx());
    m_stops = ZoomStops::Build (1.0, kZoomRatio, { most }, kMinStepRatio);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::ClampPan
//
//  The view stays over the grid's cells.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::ClampPan()
{
    RECT    grid     = GetGridRect();
    double  pitch    = (double) (m_grid.GetCellPx() + BlockMapGrid::kGapPx);
    double  contentW = m_grid.GetColumns() * pitch;
    double  contentH = m_grid.GetRows() * pitch;



    m_panX = std::clamp (m_panX, 0.0, std::max (0.0, contentW - (grid.right - grid.left)));
    m_panY = std::clamp (m_panY, 0.0, std::max (0.0, contentH - (grid.bottom - grid.top)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::ZoomAbout
//
//  The point of the volume under `anchorPx` stays under it.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::ZoomAbout (double zoom, POINT anchorPx)
{
    RECT    grid  = GetGridRect();
    double  ax    = (double) (anchorPx.x - grid.left);
    double  ay    = (double) (anchorPx.y - grid.top);
    double  ratio = 0.0;



    zoom = m_stops.empty() ? 1.0 : std::clamp (zoom, 1.0, m_stops.back());

    if (zoom == m_zoom)
    {
        return;
    }

    ratio  = zoom / m_zoom;
    m_panX = (ax + m_panX) * ratio - ax;
    m_panY = (ay + m_panY) * ratio - ay;
    m_zoom = zoom;

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::StepZoom
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::StepZoom (int steps)
{
    RECT  grid = GetGridRect();



    StepZoomAt (steps, POINT { (grid.left + grid.right) / 2, (grid.top + grid.bottom) / 2 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::StepZoomAt
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::StepZoomAt (int steps, POINT anchorPx)
{
    if (m_stops.empty())
    {
        return;
    }

    ZoomAbout (ZoomStops::Step (m_stops, m_zoom, steps), anchorPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::Fit
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Fit()
{
    m_zoom = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::FindCellAt
//
////////////////////////////////////////////////////////////////////////////////

int64_t BlockMapView::FindCellAt (POINT pointPx) const
{
    RECT  grid = GetGridRect();



    if (PtInRect (&grid, pointPx) == FALSE)
    {
        return -1;
    }

    return m_grid.FindCellAt ((int) (pointPx.x - grid.left + m_panX), (int) (pointPx.y - grid.top + m_panY));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::DescribeBlocks
//
//  "Block 24" for a cell of one block, or "16 blocks, 1,024 to 1,795" for
//  a square of them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockMapView::DescribeBlocks (uint32_t cell) const
{
    uint32_t  count = m_grid.CountBlocks (cell);



    if (count <= 1)
    {
        return L"Block " + NumberFormat::FormatInteger (m_grid.GetFirstBlock (cell));
    }

    return NumberFormat::FormatInteger (count) + L" blocks, " + NumberFormat::FormatInteger (m_grid.GetFirstBlock (cell)) + L" to " + NumberFormat::FormatInteger (m_grid.GetLastBlock (cell));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::DescribeCell
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockMapView::DescribeCell (uint32_t cell) const
{
    uint32_t      first  = m_grid.GetFirstBlock (cell);
    uint32_t      last   = m_grid.GetLastBlock (cell);
    std::wstring  text;
    std::wstring  owners;



    if (m_model == nullptr || cell >= m_roles.size())
    {
        return text;
    }

    text   = DescribeBlocks (cell) + L": ";
    text  += first == last && m_model->GetMapLabel (first) != nullptr ? m_model->GetMapLabel (first) : BlockMapGrid::GetRoleName (m_roles[cell]);
    owners = m_model->GetOwners (first);

    if (!owners.empty())
    {
        std::replace (owners.begin(), owners.end(), L'\n', L',');
        text += L". " + owners;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::OnMouse
//
//  A zoomable map zooms on the wheel about the pointer, pans on a drag,
//  fits on a double-click, and selects on a click. Any other map selects on
//  a press. A press on a band steps to the band's first block.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT     bounds   = GetBounds();
    RECT     grid     = GetGridRect();
    POINT    p        = ev.positionDip;
    int64_t  cell     = FindCellAt (p);
    bool     isBand   = p.x >= bounds.left && p.x < bounds.right && p.y >= bounds.top && p.y < grid.top;
    bool     isInside = PtInRect (&bounds, p) != FALSE;
    int64_t  now      = (int64_t) GetTickCount64();
    double   at       = 0.0;



    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hoverCell = -1;

        if (m_onHover)
        {
            m_onHover (L"");
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        if (m_isZoomable && isInside && !ev.wheelHorizontal)
        {
            StepZoomAt (ZoomStops::GetWheelSteps (ev.wheelDelta), p);
            return true;
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Enter)
    {
        if (m_isPressed && !m_isPanning && (std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2 || std::abs (p.y - m_pressAt.y) > GetSystemMetrics (SM_CYDRAG) / 2))
        {
            m_isPanning = true;
        }

        if (m_isPanning)
        {
            m_panX -= p.x - m_lastAt.x;
            m_panY -= p.y - m_lastAt.y;
            m_lastAt = p;
            ClampPan();
            return true;
        }

        if (cell != m_hoverCell)
        {
            m_hoverCell = cell;

            if (m_onHover)
            {
                m_onHover (cell >= 0 ? DescribeCell ((uint32_t) cell) : std::wstring());
            }
        }

        return m_isPressed;
    }

    if (ev.button != DxuiMouseButton::Left || m_model == nullptr || !m_onSelect)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && isBand && bounds.right > bounds.left)
    {
        at = (double) (p.x - bounds.left) / (bounds.right - bounds.left) * m_blockCount;

        for (const BlockBand & band : m_bands)
        {
            if (!band.isMap && band.Contains ((uint32_t) at))
            {
                m_onSelect (band.first);
                return true;
            }
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && cell >= 0 && !m_isZoomable)
    {
        m_onSelect (m_grid.GetFirstBlock ((uint32_t) cell));
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && isInside)
    {
        m_isPressed = true;
        m_isPanning = false;
        m_pressAt   = p;
        m_lastAt    = p;
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up && m_isPressed)
    {
        m_isPressed = false;

        if (m_isPanning)
        {
            m_isPanning = false;
        }
        else if (now - m_lastClickMs <= (int64_t) GetDoubleClickTime())
        {
            //  A double-click on a block opens it; off the blocks, it fits the
            //  whole volume.
            m_lastClickMs = 0;

            if (cell >= 0 && m_onOpen)
            {
                m_onOpen (m_grid.GetFirstBlock ((uint32_t) cell));
            }
            else
            {
                Fit();
            }
        }
        else
        {
            m_lastClickMs = now;

            if (cell >= 0)
            {
                m_onSelect (m_grid.GetFirstBlock ((uint32_t) cell));
            }
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  grid = GetGridRect();



    if (!IsVisible() || m_model == nullptr)
    {
        return;
    }

    m_isDark = DiskInspectorPalette::IsDarkSurface (theme.Background());

    PaintBands (painter, text, theme);

    painter.PushClipRect ((float) grid.left, (float) grid.top, (float) (grid.right - grid.left), (float) (grid.bottom - grid.top));
    PaintCells (painter, text, theme, grid);
    painter.PopClipRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::PaintBands
//
//  Each partition across the map's width in proportion to its blocks, with
//  its label, and a partition map's blocks muted.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintBands (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kTextPadDip = 3.0f;



    RECT      bounds = GetBounds();
    float     width  = (float) (bounds.right - bounds.left);
    float     height = m_scaler.ToPxf (kBandDip);
    float     count  = (float) std::max<uint32_t> (1, m_blockCount);
    float     x      = 0.0f;
    float     w      = 0.0f;
    float     pad    = m_scaler.ToPxf (kTextPadDip);
    HRESULT   hr     = S_OK;



    for (const BlockBand & band : m_bands)
    {
        x = bounds.left + width * band.first / count;
        w = std::max (1.0f, width * band.count / count);

        painter.FillRect    (x, (float) bounds.top, w, height, band.isMap ? theme.BackgroundElevated() : theme.SelectionBackground());
        painter.OutlineRect (x, (float) bounds.top, w, height, 1.0f, theme.Border());

        if (w > pad * 2)
        {
            hr = text.DrawString (band.label.c_str(), x + pad, (float) bounds.top, w - pad * 2, height,
                                  theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::PaintCells
//
//  As the disk inspector's File map grid draws its cells: only those in
//  view; while blocks are highlighted, every other cell dimmed and a
//  marching marquee around the highlighted ones, each with its number in
//  its file where the cell has room; the cell under the pointer lit faintly
//  and outlined; and the current block lit by the shimmer and outlined in
//  the focus ring.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintCells (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & grid) const
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));
    float                 size    = (float) m_grid.GetCellPx();
    float                 corner  = std::max (2.0f, std::floor (size / 2.0f));
    float                 outline = m_scaler.ToPxf (kOutlineDip);
    float                 left    = (float) (grid.left - m_panX);
    float                 top     = (float) (grid.top - m_panY);
    bool                  isRoom  = size >= m_scaler.ToPxf (kNumberDip);
    float                 textPx  = std::min (m_scaler.ToPxf (kFontDip), size / 2.5f);
    uint32_t              cell    = 0;
    uint32_t              color   = 0;
    POINT                 at      = {};
    float                 x       = 0.0f;
    float                 y       = 0.0f;
    RECT                  box     = {};
    HRESULT               hr      = S_OK;



    for (cell = 0; cell < m_grid.GetCellCount(); cell++)
    {
        at = m_grid.GetCellOrigin (cell);
        x  = left + at.x;
        y  = top + at.y;

        if (x + size < grid.left || x > grid.right || y + size < grid.top || y > grid.bottom || !m_grid.HasBlocks (cell))
        {
            continue;
        }

        color = GetCellColor (palette, cell);
        painter.FillRect (x, y, size, size, color);

        if (m_differs[cell])
        {
            painter.FillRect (x + size - corner, y, corner, corner, theme.ErrorForeground());
        }

        //  A highlighted block's number in its file, or with no highlight,
        //  each block's number, once the cell has room.
        if (isRoom && m_grid.GetBlocksPerCell() == 1 && (!m_hasHighlight || m_numbers[cell] > 0))
        {
            hr = text.DrawString ((m_hasHighlight ? std::to_wstring (m_numbers[cell]) : NumberFormat::FormatInteger (m_grid.GetFirstBlock (cell))).c_str(), x, y, size, size,
                                  m_hasHighlight ? theme.Foreground() : (DiskInspectorPalette::GetTextColorOn (color) & 0x00FFFFFFu) | 0xB0000000u, textPx,
                                  m_hasHighlight ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                                  m_hasHighlight ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (m_isZoomable && (int64_t) cell == m_hoverCell)
        {
            painter.FillRect    (x, y, size, size, InspectorMarquee::GetHoverFill());
            painter.OutlineRect (x, y, size, size, 1.0f, InspectorMarquee::GetHoverLine());
        }
    }

    if (m_hasHighlight)
    {
        PaintMarquee (painter, palette, grid);
    }

    if (m_isOutlined && m_grid.GetCellCount() != 0)
    {
        at  = m_grid.GetCellOrigin (m_grid.GetCellOf (m_current));
        x   = left + at.x;
        y   = top + at.y;
        box = { (LONG) std::max (x, (float) grid.left), (LONG) std::max (y, (float) grid.top), (LONG) std::min (x + size, (float) grid.right), (LONG) std::min (y + size, (float) grid.bottom) };

        InspectorMarquee::PaintShimmer (painter, x, y, size, size, InspectorMarquee::GetClockMs(), box);
        painter.OutlineRect (x - 1.0f, y - 1.0f, size + 2.0f, size + 2.0f, outline, theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::PaintMarquee
//
//  The File map grid's marching marquee around the highlighted cells: an
//  edge of a highlighted cell is drawn where its neighbor isn't
//  highlighted, clockwise around the cells, the dashes counted from the
//  grid's origin so that edges in a line join up.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintMarquee (IDxuiPainter & painter, const DiskInspectorPalette & palette, const RECT & grid) const
{
    InspectorMarquee  marquee (palette, m_scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx, (int64_t) GetTickCount64());
    int               columns = m_grid.GetColumns();
    int               rows    = m_grid.GetRows();
    float             pitch   = (float) (m_grid.GetCellPx() + BlockMapGrid::kGapPx);
    float             left    = (float) (grid.left - m_panX);
    float             top     = (float) (grid.top - m_panY);
    float             x0      = 0.0f;
    float             y0      = 0.0f;
    float             x1      = 0.0f;
    float             y1      = 0.0f;
    int               r       = 0;
    int               c       = 0;



    auto  isMarked = [&] (int row, int column)
    {
        int64_t  cell = (int64_t) row * columns + column;

        return row >= 0 && row < rows && column >= 0 && column < columns && cell < (int64_t) m_highlighted.size() && m_highlighted[(size_t) cell];
    };

    marquee.SetCullRect (grid);

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < columns; c++)
        {
            if (!isMarked (r, c))
            {
                continue;
            }

            //  On pixel centers, so each one-pixel line covers one pixel.
            x0 = std::floor (left + c * pitch) + 0.5f;
            y0 = std::floor (top + r * pitch) + 0.5f;
            x1 = std::floor (left + (c + 1) * pitch) - 0.5f;
            y1 = std::floor (top + (r + 1) * pitch) - 0.5f;

            if (!isMarked (r - 1, c))
            {
                marquee.DrawLine (painter, x0, y0, x1, y0, x0);
            }

            if (!isMarked (r, c + 1))
            {
                marquee.DrawLine (painter, x1, y0, x1, y1, y0);
            }

            if (!isMarked (r + 1, c))
            {
                marquee.DrawLine (painter, x1, y1, x0, y1, -x1);
            }

            if (!isMarked (r, c - 1))
            {
                marquee.DrawLine (painter, x0, y1, x0, y0, -y1);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetCellColor
//
//  The cell's role color, dimmed while other blocks are highlighted, so the
//  highlighted ones are the brightest on the map.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapView::GetCellColor (const DiskInspectorPalette & palette, uint32_t cell) const
{
    uint32_t  color = palette.GetRoleColor (static_cast<MapRole> (m_roles[cell]));



    return m_hasHighlight && !m_highlighted[cell] ? InspectorMarquee::Dim (palette, color) : color;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::IsAnimating
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::IsAnimating() const
{
    return IsVisible() && m_model != nullptr && m_model->IsOpen() && (m_hasHighlight || m_isOutlined);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetTooltip
//
//  The blocks, their role, and "Used by" with the files, a line each, as
//  the File map grid's tip has them.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    int64_t       cell   = FindCellAt (pointPx);
    uint32_t      first  = 0;
    uint32_t      last   = 0;
    std::wstring  owners;



    if (cell < 0 || m_model == nullptr || (size_t) cell >= m_roles.size())
    {
        return false;
    }

    first   = m_grid.GetFirstBlock ((uint32_t) cell);
    last    = m_grid.GetLastBlock ((uint32_t) cell);
    owners  = m_model->GetOwners (first);
    outText = DescribeBlocks ((uint32_t) cell) + L"\n" + (first == last && m_model->GetMapLabel (first) != nullptr ? m_model->GetMapLabel (first) : BlockMapGrid::GetRoleName (m_roles[(size_t) cell]));

    if (!owners.empty())
    {
        std::replace (owners.begin(), owners.end(), L'\n', L',');
        outText += L"\nUsed by " + owners;
    }

    outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetTooltipSwatches
//
//  The cell's color, as drawn, beside the line with its role.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    int64_t  cell = FindCellAt (pointPx);



    if (cell < 0 || (size_t) cell >= m_roles.size())
    {
        return false;
    }

    outSwatches = { { 1, GetCellColor (DiskInspectorPalette::MakeFallback (m_isDark), (uint32_t) cell) } };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetNumberInFile
//
//  A block of a file's data's number in the file, from 1, or 0 for any
//  other block.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapView::GetNumberInFile (uint32_t block) const
{
    uint32_t          inner = 0;
    const BlockMap  * map   = m_model->FindMap (block, inner);



    if (map == nullptr || inner >= map->GetBlockCount() || map->blocks[inner].structure != BlockStructure::FileData)
    {
        return 0;
    }

    return map->blocks[inner].node + 1;
}






