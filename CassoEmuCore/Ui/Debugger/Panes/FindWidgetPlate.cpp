#include "Pch.h"

#include "Ui/Debugger/Panes/FindWidgetPlate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetPlate::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetPlate::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetPlate::Paint
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetPlate::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const RECT  & rc     = GetBounds();
    float         x      = (float) rc.left;
    float         y      = (float) rc.top;
    float         width  = (float) (rc.right  - rc.left);
    float         height = (float) (rc.bottom - rc.top);
    float         radius = m_scaler.ToPxf (theme.CornerRadiusDip());
    float         edge   = m_scaler.ToPxf (1.0f);



    (void) text;

    if (!IsVisible() || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    //  A shadow under the plate and the tooltip's stronger edge set it apart
    //  from the text it floats over, which can be the same color as its fill.
    DxuiShadow::Paint (painter, x, y, width, height, radius, m_scaler.ToPxf (1.0f), theme.LiftShadow());

    painter.FillRoundedRect    (x, y, width, height, radius, theme.BackgroundElevated());
    painter.OutlineRoundedRect (x, y, width, height, radius, edge, theme.TooltipBorder());
}
