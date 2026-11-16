#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockLegendView.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"





static constexpr int  s_kMarkRows = 1;     // the row for a block that differs





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLegendView::GetRoles
//
////////////////////////////////////////////////////////////////////////////////

const vector<SectorRole> & BlockLegendView::GetRoles()
{
    static const vector<SectorRole>  s_roles =
    {
        SectorRole::BootArea,
        SectorRole::VtocOrKeyBlock,
        SectorRole::CatalogOrDirectory,
        SectorRole::Subdirectory,
        SectorRole::VolumeBitmap,
        SectorRole::IndexBlock,
        SectorRole::FileData,
        SectorRole::Free,
        SectorRole::BadBlocksFile,
        SectorRole::AllocatedUnowned,
        SectorRole::OwnedMarkedFree,
        SectorRole::CrossLinked,
    };



    return s_roles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLegendView::GetSizePx
//
////////////////////////////////////////////////////////////////////////////////

SIZE BlockLegendView::GetSizePx (const DxuiDpiScaler & scaler) const
{
    int  rows = (int) GetRoles().size() + s_kMarkRows;



    return { scaler.ToPx ((int) kWidthDip), scaler.ToPx ((int) (2 * kPadDip)) + rows * scaler.ToPx ((int) kRowDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLegendView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockLegendView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));
    RECT                  bounds  = GetBounds();
    float                 pad     = m_scaler.ToPxf (kPadDip);
    float                 row     = m_scaler.ToPxf (kRowDip);
    float                 swatch  = m_scaler.ToPxf (kSwatchDip);
    float                 x       = bounds.left + pad;
    float                 y       = bounds.top + pad;
    float                 textX   = x + swatch + pad;
    float                 textW   = bounds.right - textX - pad;
    float                 corner  = m_scaler.ToPxf (8.0f);
    HRESULT               hr      = S_OK;



    if (!IsVisible())
    {
        return;
    }

    painter.FillRoundedRect    ((float) bounds.left, (float) bounds.top, (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top), corner, theme.BackgroundElevated());
    painter.OutlineRoundedRect ((float) bounds.left, (float) bounds.top, (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top), corner, 1.0f, theme.Border());

    auto  line = [&] (uint32_t color, LPCWSTR label, bool isCorner)
    {
        if (isCorner)
        {
            painter.FillRect (x, y + (row - swatch) / 2, swatch, swatch, palette.GetRoleColor (MapRole::Free));
            painter.FillRect (x + swatch / 2, y + (row - swatch) / 2, swatch / 2, swatch / 2, color);
        }
        else
        {
            painter.FillRect (x, y + (row - swatch) / 2, swatch, swatch, color);
        }

        hr = text.DrawString (label, textX, y, textW, row, theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        y += row;
    };

    for (SectorRole role : GetRoles())
    {
        line (palette.GetRoleColor (static_cast<MapRole> (role)), BlockMapGrid::GetRoleName (role), false);
    }

    line (theme.ErrorForeground(), L"Differs from the other image", true);
}
