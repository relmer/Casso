#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"





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
//  The grid is laid out over the view's area times the zoom, so a zoomed
//  grid has larger cells, down to one block a cell.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Refresh()
{
    bool        isOpen = m_model != nullptr && m_model->IsOpen();
    uint32_t    count  = isOpen ? m_model->GetBlockCount() : 0;
    uint32_t    block  = 0;
    uint32_t    cell   = 0;
    SectorRole  role   = SectorRole::Free;
    RECT        grid   = {};



    m_bands      = isOpen ? m_model->GetBands() : vector<BlockBand>();
    m_blockCount = count;
    m_current    = isOpen ? m_model->GetBlock() : 0;
    grid         = GetGridRect();

    BuildStops();

    m_grid = BlockMapGrid (count, (int) ((grid.right - grid.left) * m_zoom), (int) ((grid.bottom - grid.top) * m_zoom));

    ClampPan();

    m_roles.assign (m_grid.GetCellCount(), SectorRole::Free);
    m_highlighted.assign (m_grid.GetCellCount(), false);
    m_differs.assign (m_grid.GetCellCount(), false);
    m_leftovers.assign (m_grid.GetCellCount(), false);

    for (block = 0; block < count && m_grid.GetCellCount() != 0; block++)
    {
        cell = m_grid.GetCellOf (block);
        role = m_model->GetRole (block);

        m_roles[cell] = BlockMap::PickRole (m_roles[cell], role);

        if (m_model->HasHighlight() && m_model->IsHighlighted (block))
        {
            m_highlighted[cell] = true;
        }

        if (m_isDiffsShown && m_model->IsDifferent (block))
        {
            m_differs[cell] = true;
        }

        if (m_model->HasLeftovers() && m_model->FindLeftoverRun (block) != nullptr)
        {
            m_leftovers[cell] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::BuildStops
//
//  Even steps up to the first zoom with one block a cell, then a last stop
//  that makes those cells kOneBlockMore times larger.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::BuildStops()
{
    constexpr double  kSearchRatio = 1.05;
    constexpr double  kMostSearch  = 4096.0;



    RECT    grid  = GetGridRect();
    int     w     = (int) (grid.right - grid.left);
    int     h     = (int) (grid.bottom - grid.top);
    double  one   = 1.0;



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

    while (one < kMostSearch && BlockMapGrid (m_blockCount, (int) (w * one), (int) (h * one)).GetBlocksPerCell() > 1)
    {
        one *= kSearchRatio;
    }

    m_stops = ZoomStops::Build (1.0, kZoomRatio, { one, one * kOneBlockMore }, kMinStepRatio);
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

    text   = first == last ? L"Block " + std::to_wstring (first) : L"Blocks " + std::to_wstring (first) + L" to " + std::to_wstring (last);
    text  += L": ";
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
            m_lastClickMs = 0;
            Fit();
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

    PaintBands (painter, text, theme);

    painter.PushClipRect ((float) grid.left, (float) grid.top, (float) (grid.right - grid.left), (float) (grid.bottom - grid.top));
    PaintCells (painter, theme, grid);
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
//  Only the cells in view are drawn. The highlighted file's cells are drawn
//  in the theme's foreground with the role's color inset, and the current
//  block's cell is outlined last.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintCells (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & grid) const
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));
    float                 size    = (float) m_grid.GetCellPx();
    float                 inset   = std::max (1.0f, std::floor (size / 4.0f));
    float                 corner  = std::max (2.0f, std::floor (size / 2.0f));
    float                 outline = m_scaler.ToPxf (kOutlineDip);
    float                 left    = (float) (grid.left - m_panX);
    float                 top     = (float) (grid.top - m_panY);
    uint32_t              cell    = 0;
    POINT                 at      = {};
    float                 x       = 0.0f;
    float                 y       = 0.0f;



    for (cell = 0; cell < m_grid.GetCellCount(); cell++)
    {
        at = m_grid.GetCellOrigin (cell);
        x  = left + at.x;
        y  = top + at.y;

        if (x + size < grid.left || x > grid.right || y + size < grid.top || y > grid.bottom)
        {
            continue;
        }

        if (m_highlighted[cell])
        {
            painter.FillRect (x, y, size, size, theme.Foreground());
            painter.FillRect (x + inset, y + inset, size - inset * 2, size - inset * 2, palette.GetRoleColor (static_cast<MapRole> (m_roles[cell])));
        }
        else
        {
            painter.FillRect (x, y, size, size, palette.GetRoleColor (static_cast<MapRole> (m_roles[cell])));
        }

        if (m_differs[cell])
        {
            painter.FillRect (x + size - corner, y, corner, corner, theme.ErrorForeground());
        }

        if (m_leftovers[cell])
        {
            painter.FillRect (x, y + size - corner, corner, corner, theme.Accent());
        }
    }

    if (m_isOutlined && m_grid.GetCellCount() != 0)
    {
        at = m_grid.GetCellOrigin (m_grid.GetCellOf (m_current));

        painter.OutlineRect (left + at.x - outline * 0.5f, top + at.y - outline * 0.5f, size + outline, size + outline, outline, theme.Accent());
    }
}
