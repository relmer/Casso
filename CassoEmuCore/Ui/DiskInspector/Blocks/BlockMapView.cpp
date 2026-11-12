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

    Refresh();
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



    if (m_model != nullptr && !m_model->GetBands().empty())
    {
        grid.top += (LONG) (m_scaler.ToPxf (kBandDip) + m_scaler.ToPxf (kBandGapDip));
    }

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::Refresh()
{
    RECT        grid  = GetGridRect();
    uint32_t    count = m_model != nullptr ? m_model->GetBlockCount() : 0;
    uint32_t    block = 0;
    uint32_t    cell  = 0;
    SectorRole  role  = SectorRole::Free;



    m_grid = BlockMapGrid (count, grid.right - grid.left, grid.bottom - grid.top);

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

        if (m_model->IsDifferent (block))
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
    text  += BlockMapGrid::GetRoleName (m_roles[cell]);
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
////////////////////////////////////////////////////////////////////////////////

bool BlockMapView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT     bounds = GetBounds();
    RECT     grid   = GetGridRect();
    int64_t  cell   = m_grid.FindCellAt (ev.positionDip.x - grid.left, ev.positionDip.y - grid.top);
    bool     isBand = ev.positionDip.y >= bounds.top && ev.positionDip.y < grid.top;
    double   at     = 0.0;



    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hoverCell = -1;

        if (m_onHover)
        {
            m_onHover (L"");
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Enter)
    {
        if (cell != m_hoverCell)
        {
            m_hoverCell = cell;

            if (m_onHover)
            {
                m_onHover (cell >= 0 ? DescribeCell ((uint32_t) cell) : std::wstring());
            }
        }

        return false;
    }

    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left || m_model == nullptr || !m_onSelect)
    {
        return false;
    }

    if (cell >= 0)
    {
        m_onSelect (m_grid.GetFirstBlock ((uint32_t) cell));
        return true;
    }

    if (isBand && bounds.right > bounds.left)
    {
        at = (double) (ev.positionDip.x - bounds.left) / (bounds.right - bounds.left) * m_model->GetBlockCount();

        for (const BlockBand & band : m_model->GetBands())
        {
            if (!band.isMap && band.Contains ((uint32_t) at))
            {
                m_onSelect (band.first);
                return true;
            }
        }
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
    PaintCells (painter, theme, grid);
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
    float     count  = (float) std::max<uint32_t> (1, m_model->GetBlockCount());
    float     x      = 0.0f;
    float     w      = 0.0f;
    float     pad    = m_scaler.ToPxf (kTextPadDip);
    HRESULT   hr     = S_OK;



    for (const BlockBand & band : m_model->GetBands())
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
//  The highlighted file's cells are drawn in the theme's foreground with
//  the role's color inset, and the current block's cell is outlined last.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapView::PaintCells (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & grid) const
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));
    float                 size    = (float) m_grid.GetCellPx();
    float                 inset   = std::max (1.0f, std::floor (size / 4.0f));
    float                 corner  = std::max (2.0f, std::floor (size / 2.0f));
    float                 outline = m_scaler.ToPxf (kOutlineDip);
    uint32_t              cell    = 0;
    POINT                 at      = {};



    for (cell = 0; cell < m_grid.GetCellCount(); cell++)
    {
        at = m_grid.GetCellOrigin (cell);

        if (m_highlighted[cell])
        {
            painter.FillRect ((float) (grid.left + at.x), (float) (grid.top + at.y), size, size, theme.Foreground());
            painter.FillRect ((float) (grid.left + at.x) + inset, (float) (grid.top + at.y) + inset, size - inset * 2, size - inset * 2,
                              palette.GetRoleColor (static_cast<MapRole> (m_roles[cell])));
        }
        else
        {
            painter.FillRect ((float) (grid.left + at.x), (float) (grid.top + at.y), size, size,
                              palette.GetRoleColor (static_cast<MapRole> (m_roles[cell])));
        }
    }

    for (cell = 0; cell < m_grid.GetCellCount(); cell++)
    {
        at = m_grid.GetCellOrigin (cell);

        if (m_differs[cell])
        {
            painter.FillRect ((float) (grid.left + at.x) + size - corner, (float) (grid.top + at.y), corner, corner, theme.ErrorForeground());
        }

        if (m_leftovers[cell])
        {
            painter.FillRect ((float) (grid.left + at.x), (float) (grid.top + at.y) + size - corner, corner, corner, theme.Accent());
        }
    }

    if (m_grid.GetCellCount() != 0)
    {
        at = m_grid.GetCellOrigin (m_grid.GetCellOf (m_model->GetBlock()));

        painter.OutlineRect ((float) (grid.left + at.x) - outline * 0.5f, (float) (grid.top + at.y) - outline * 0.5f,
                             size + outline, size + outline, outline, theme.Accent());
    }
}
