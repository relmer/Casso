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
    m_grid = m_zoom <= 1.0 ? fit : BlockMapGrid (count, fit.GetBlockColumns(), fit.GetBlockPx() * m_zoom, fit.GetGroup());

    ClampPan();

    m_roles.assign (m_grid.GetCellCount(), SectorRole::Free);
    m_highlighted.assign (m_grid.GetCellCount(), false);
    m_differs.assign (m_grid.GetCellCount(), false);
    m_hasHighlight = false;
    m_highlightBox = {};

    for (block = 0; block < count && m_grid.GetCellCount() != 0; block++)
    {
        cell = m_grid.GetCellOf (block);
        role = m_model->GetRole (block);

        m_roles[cell] = BlockMap::PickRole (m_roles[cell], role);

        if (m_model->HasHighlight() && m_model->IsHighlighted (block))
        {
            LONG  column = (LONG) (block % (uint32_t) m_grid.GetBlockColumns());
            LONG  row    = (LONG) (block / (uint32_t) m_grid.GetBlockColumns());

            m_highlightBox = m_hasHighlight ? RECT { std::min (m_highlightBox.left, column), std::min (m_highlightBox.top, row),
                                                     std::max (m_highlightBox.right, column + 1), std::max (m_highlightBox.bottom, row + 1) }
                                            : RECT { column, row, column + 1, row + 1 };

            m_highlighted[cell] = true;
            m_hasHighlight      = true;
        }

        if (m_isDiffsShown && m_model->IsDifferent (block))
        {
            m_differs[cell] = true;
        }
    }

    ReadLabels (grid);
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
    double  pitch    = m_grid.GetPitch();
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



    zoom       = m_stops.empty() ? 1.0 : std::clamp (zoom, 1.0, m_stops.back());
    m_isFlying = false;

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
//  BlockMapView::GetBoxView
//
//  The view that centers a box of the fit grid, in pixels, at the zoom that
//  makes it kFlightFill of the view across or down, whichever is less.
//  The pan is clamped to the grid's cells.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::View BlockMapView::GetBoxView (double left, double top, double right, double bottom) const
{
    RECT                      grid  = GetGridRect();
    double                    w     = (double) (grid.right - grid.left);
    double                    h     = (double) (grid.bottom - grid.top);
    double                    most  = m_stops.empty() ? 1.0 : m_stops.back();
    BlockMapGrid              fit   = BlockMapGrid (m_blockCount, (int) w, (int) h);
    double                    gridW = fit.GetBlockColumns() * fit.GetBlockPx();
    double                    gridH = std::ceil ((double) m_blockCount / fit.GetBlockColumns()) * fit.GetBlockPx();
    InspectorViewModel::View  view;



    view.zoom  = std::clamp (std::min (w * kFlightFill / std::max (1.0, right - left), h * kFlightFill / std::max (1.0, bottom - top)), 1.0, most);
    view.pan.x = -std::clamp (view.zoom * (left + right) / 2 - w / 2, 0.0, std::max (0.0, view.zoom * gridW - w));
    view.pan.y = -std::clamp (view.zoom * (top + bottom) / 2 - h / 2, 0.0, std::max (0.0, view.zoom * gridH - h));

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::StartFlight
//
//  From the view as it is, out only as far as the view with both its center
//  and the highlighted blocks in it, then in to the view centered on the
//  highlighted blocks. Blocks near enough that the middle view is no wider
//  than the view at either end are flown to directly.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::StartFlight (double nowMs)
{
    RECT          grid   = GetGridRect();
    BlockMapGrid  fit;
    double        s      = 0.0;
    double        cx     = 0.0;
    double        cy     = 0.0;
    double        left   = 0.0;
    double        top    = 0.0;
    double        right  = 0.0;
    double        bottom = 0.0;



    if (!m_isZoomable || !m_hasHighlight || grid.right <= grid.left || grid.bottom <= grid.top)
    {
        return;
    }

    fit    = BlockMapGrid (m_blockCount, (int) (grid.right - grid.left), (int) (grid.bottom - grid.top));
    s      = fit.GetBlockPx();
    cx     = (m_panX + (grid.right - grid.left) / 2.0) / m_zoom;
    cy     = (m_panY + (grid.bottom - grid.top) / 2.0) / m_zoom;
    left   = m_highlightBox.left * s;
    top    = m_highlightBox.top * s;
    right  = m_highlightBox.right * s;
    bottom = m_highlightBox.bottom * s;

    m_flightFrom    = GetView();
    m_flightTo      = GetBoxView (left, top, right, bottom);
    m_flightMid     = GetBoxView (std::min (cx, left), std::min (cy, top), std::max (cx, right), std::max (cy, bottom));
    m_isTwoLegs     = m_flightMid.zoom < std::min (m_flightFrom.zoom, m_flightTo.zoom) - 1e-9;
    m_flightStartMs = nowMs;
    m_isFlying      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::StepFlight
//
//  The view for the moment, eased as the platter's flight is, the two legs
//  splitting it in the proportion of their changes in zoom by scale.
//  Returns whether the flight goes on.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::StepFlight (double nowMs)
{
    double                    out      = m_isTwoLegs ? std::log (m_flightFrom.zoom / m_flightMid.zoom) : 0.0;
    double                    in       = m_isTwoLegs ? std::log (m_flightTo.zoom / m_flightMid.zoom) : 0.0;
    double                    share    = (out + in > 0) ? out / (out + in) : 0.0;
    double                    duration = m_isTwoLegs ? 2 * InspectorViewModel::kFlightLegMs : InspectorViewModel::kFlightLegMs;
    double                    t        = std::clamp ((nowMs - m_flightStartMs) / duration, 0.0, 1.0);
    double                    e        = InspectorViewModel::Ease (t);
    InspectorViewModel::View  view;



    if (!m_isFlying)
    {
        return false;
    }

    if (out + in <= 0)
    {
        view = InspectorViewModel::GetBetweenView (m_flightFrom, m_flightTo, e);
    }
    else if (e < share)
    {
        view = InspectorViewModel::GetBetweenView (m_flightFrom, m_flightMid, e / share);
    }
    else
    {
        view = InspectorViewModel::GetBetweenView (m_flightMid, m_flightTo, (e - share) / (1.0 - share));
    }

    m_zoom     = view.zoom;
    m_panX     = -view.pan.x;
    m_panY     = -view.pan.y;
    m_isFlying = t < 1.0;

    Refresh();

    return m_isFlying;
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
    m_zoom     = 1.0;
    m_panX     = 0.0;
    m_panY     = 0.0;
    m_isFlying = false;

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
            m_isFlying = false;
            m_panX    -= p.x - m_lastAt.x;
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
    RECT     grid = GetGridRect();
    HRESULT  hr   = S_OK;



    if (!IsVisible() || m_model == nullptr)
    {
        return;
    }

    m_isDark = DiskInspectorPalette::IsDarkSurface (theme.Background());

    PaintBands (painter, text, theme);

    painter.PushClipRect ((float) grid.left, (float) grid.top, (float) (grid.right - grid.left), (float) (grid.bottom - grid.top));
    hr = text.PushClipRect ((float) grid.left, (float) grid.top, (float) (grid.right - grid.left), (float) (grid.bottom - grid.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintCells (painter, text, theme, grid);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
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
//  marching marquee around the highlighted ones; each run of blocks with
//  its file or structure where the cells have room; the cell under the
//  pointer lit faintly and outlined; and the current block lit by the
//  shimmer and outlined in the focus ring.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintCells (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & grid) const
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));
    float                 corner  = std::max (2.0f, std::floor (m_grid.GetCellPx() / 2.0f));
    float                 outline = m_scaler.ToPxf (kOutlineDip);
    float                 left    = (float) std::floor (grid.left - m_panX);
    float                 top     = (float) std::floor (grid.top - m_panY);
    uint32_t              cell    = 0;
    uint32_t              color   = 0;
    RECT                  at      = {};
    float                 x       = 0.0f;
    float                 y       = 0.0f;
    float                 w       = 0.0f;
    float                 h       = 0.0f;
    RECT                  box     = {};



    for (cell = 0; cell < m_grid.GetCellCount(); cell++)
    {
        at = m_grid.GetCellRect (cell);
        x  = left + at.left;
        y  = top + at.top;
        w  = (float) (at.right - at.left);
        h  = (float) (at.bottom - at.top);

        if (x + w < grid.left || x > grid.right || y + h < grid.top || y > grid.bottom || !m_grid.HasBlocks (cell))
        {
            continue;
        }

        color = GetCellColor (palette, cell);
        painter.FillRect (x, y, w, h, color);

        if (m_differs[cell])
        {
            painter.FillRect (x + w - corner, y, corner, corner, theme.ErrorForeground());
        }

        if (m_isZoomable && (int64_t) cell == m_hoverCell)
        {
            painter.FillRect    (x, y, w, h, InspectorMarquee::GetHoverFill());
            painter.OutlineRect (x, y, w, h, 1.0f, InspectorMarquee::GetHoverLine());
        }
    }

    PaintLabels (text, palette, grid, left, top);

    if (m_hasHighlight)
    {
        PaintMarquee (painter, palette, grid);
    }

    if (m_isOutlined && m_grid.GetCellCount() != 0)
    {
        at  = m_grid.GetCellRect (m_grid.GetCellOf (m_current));
        x   = left + at.left;
        y   = top + at.top;
        w   = (float) (at.right - at.left);
        h   = (float) (at.bottom - at.top);
        box = { (LONG) std::max (x, (float) grid.left), (LONG) std::max (y, (float) grid.top), (LONG) std::min (x + w, (float) grid.right), (LONG) std::min (y + h, (float) grid.bottom) };

        InspectorMarquee::PaintShimmer (painter, x, y, w, h, InspectorMarquee::GetClockMs(), box);
        painter.OutlineRect (x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, outline, theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::PaintLabels
//
//  Each run of cells in a row with the same file or structure has its name
//  centered across the part of the run in view, where the name fits.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintLabels (IDxuiTextRenderer & text, const DiskInspectorPalette & palette, const RECT & grid, float left, float top) const
{
    float     pad     = m_scaler.ToPxf (kLabelPadDip);
    float     textPx  = std::min (m_scaler.ToPxf (kFontDip), m_grid.GetCellPx() / 1.6f);
    int       columns = m_grid.GetColumns();
    int       row     = 0;
    int       first   = 0;
    int       last    = 0;
    uint32_t  cell    = 0;
    RECT      from    = {};
    RECT      to      = {};
    float     width   = 0.0f;
    float     height  = 0.0f;
    float     x0      = 0.0f;
    float     x1      = 0.0f;
    HRESULT   hr      = S_OK;



    if (m_labels.empty())
    {
        return;
    }

    for (row = 0; row < m_grid.GetRows(); row++)
    {
        for (first = 0; first < columns; first = last + 1)
        {
            cell = (uint32_t) (row * columns + first);
            last = first;

            if (cell >= m_labels.size() || m_labels[cell].empty())
            {
                continue;
            }

            while (last + 1 < columns && cell + (uint32_t) (last + 1 - first) < m_labels.size() && m_labels[cell + (uint32_t) (last + 1 - first)] == m_labels[cell])
            {
                last++;
            }

            from = m_grid.GetCellRect (cell);
            to   = m_grid.GetCellRect (cell + (uint32_t) (last - first));

            if (top + from.bottom < grid.top || top + from.top > grid.bottom || left + to.right < grid.left || left + from.left > grid.right)
            {
                continue;
            }

            x0 = std::max (left + from.left, (float) grid.left);
            x1 = std::min (left + to.right, (float) grid.right);
            hr = text.MeasureString (m_labels[cell].c_str(), textPx, DxuiTheme::kBodyFace, width, height);

            if (FAILED (hr) || width + 2 * pad > x1 - x0)
            {
                continue;
            }

            hr = text.DrawString (m_labels[cell].c_str(), x0, top + from.top, x1 - x0, (float) (from.bottom - from.top),
                                  DiskInspectorPalette::GetTextColorOn (GetCellColor (palette, cell)), textPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::ReadLabels
//
//  The label of each cell in view, once a cell is one block and tall
//  enough for a line of text. A block's label is read from the model the
//  first time it is in view and cached until the map's generation changes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::ReadLabels (const RECT & grid)
{
    int64_t   firstCell = 0;
    int64_t   lastCell  = 0;
    uint32_t  cell      = 0;
    uint32_t  block     = 0;



    m_labels.clear();

    if (m_grid.GetBlocksPerCell() != 1 || m_grid.GetCellPx() < m_scaler.ToPxf (kLabelDip) || m_grid.GetColumns() == 0)
    {
        return;
    }

    if (m_labelGeneration != m_model->GetMapGeneration() || m_blockLabels.size() != m_blockCount)
    {
        m_labelGeneration = m_model->GetMapGeneration();
        m_blockLabels.assign (m_blockCount, std::wstring());
        m_isLabelRead.assign (m_blockCount, false);
    }

    m_labels.assign (m_grid.GetCellCount(), std::wstring());

    firstCell = (int64_t) (m_panY / m_grid.GetPitch()) * m_grid.GetColumns();
    lastCell  = (int64_t) ((m_panY + (grid.bottom - grid.top)) / m_grid.GetPitch() + 1) * m_grid.GetColumns();

    for (cell = (uint32_t) std::max<int64_t> (0, firstCell); cell < m_grid.GetCellCount() && (int64_t) cell <= lastCell; cell++)
    {
        block = m_grid.GetFirstBlock (cell);

        if (block >= m_blockCount)
        {
            continue;
        }

        if (!m_isLabelRead[block])
        {
            m_blockLabels[block] = GetBlockLabel (block);
            m_isLabelRead[block] = true;
        }

        m_labels[cell] = m_blockLabels[block];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::GetBlockLabel
//
//  The name of the file that uses a block, without its directories, or the
//  structure the block is part of. Free space has no label.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockMapView::GetBlockLabel (uint32_t block) const
{
    std::wstring  owners = m_model->GetOwners (block);
    SectorRole    role   = m_model->GetRole (block);
    size_t        end    = owners.find (L'\n');
    size_t        slash  = 0;



    if (!owners.empty())
    {
        owners = owners.substr (0, end);
        slash  = owners.find_last_of (L'/');

        return slash == std::wstring::npos ? owners : owners.substr (slash + 1);
    }

    if (m_model->GetMapLabel (block) != nullptr)
    {
        return m_model->GetMapLabel (block);
    }

    return role == SectorRole::Free ? std::wstring() : BlockMapGrid::GetRoleName (role);
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
    double            pitch   = m_grid.GetPitch();
    float             left    = (float) std::floor (grid.left - m_panX);
    float             top     = (float) std::floor (grid.top - m_panY);
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
            x0 = left + (float) std::floor (c * pitch) + 0.5f;
            y0 = top + (float) std::floor (r * pitch) + 0.5f;
            x1 = left + (float) std::floor ((c + 1) * pitch) - 0.5f;
            y1 = top + (float) std::floor ((r + 1) * pitch) - 0.5f;

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
//  highlighted ones are the brightest on the map. Free space is not
//  dimmed: dimmed, it is the color of the gaps between cells.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapView::GetCellColor (const DiskInspectorPalette & palette, uint32_t cell) const
{
    uint32_t  color = palette.GetRoleColor (static_cast<MapRole> (m_roles[cell]));



    return m_hasHighlight && !m_highlighted[cell] && m_roles[cell] != SectorRole::Free ? InspectorMarquee::Dim (palette, color) : color;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::IsAnimating
//
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::IsAnimating() const
{
    return IsVisible() && m_model != nullptr && m_model->IsOpen() && (m_hasHighlight || m_isOutlined || m_isFlying);
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






