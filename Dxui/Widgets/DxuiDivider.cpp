#include "Pch.h"

#include "Widgets/DxuiDivider.h"

#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDivider::Paint
//
//  One DIP thick, never less than one pixel, so the rule stays a hairline at
//  every scale rather than vanishing below 100%.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDivider::Paint (IDxuiPainter & painter, IDxuiTextRenderer & /*text*/, const IDxuiTheme & theme)
{
    constexpr float  kHalf     = 0.5f;
    RECT             bounds    = GetBounds();
    float            width     = (float) (bounds.right - bounds.left);
    float            thickness = (float) std::max (1, MulDiv (1, (int) m_dpi, kBaseDpi));
    float            top       = (float) (bounds.top + bounds.bottom) * kHalf - thickness * kHalf;



    if (width <= 0.0f)
    {
        return;
    }

    painter.FillRect ((float) bounds.left, floorf (top), width, thickness, theme.Divider());
}
