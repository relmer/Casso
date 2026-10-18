#include "Pch.h"

#include "Ui/Dialogs/SizeGrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip::GetGripRect
//
//  A square of `sizePx` flush with the client's bottom-right corner.
//
////////////////////////////////////////////////////////////////////////////////

RECT SizeGrip::GetGripRect (const RECT & clientPx, int sizePx)
{
    return RECT { clientPx.right - sizePx, clientPx.bottom - sizePx, clientPx.right, clientPx.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip::GetDots
//
//  Row r (0 at the top) holds r + 1 dots, right-aligned to the grip's right
//  edge, so the dots form a triangle whose hypotenuse runs at 45 degrees
//  from the bottom-left of the grip to its top-right. `stepPx` is the
//  distance from one dot to the next, across and down.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SizeGrip::Dot> SizeGrip::GetDots (const RECT & gripPx, float dotPx, float stepPx)
{
    std::vector<Dot>  dots;
    Dot               dot;
    int               row    = 0;
    int               column = 0;
    float             right  = (float) gripPx.right  - stepPx + dotPx;
    float             bottom = (float) gripPx.bottom - stepPx + dotPx;



    for (row = 0; row < kRows; row++)
    {
        for (column = 0; column <= row; column++)
        {
            dot.x    = right  - dotPx - stepPx * (float) column;
            dot.y    = bottom - dotPx - stepPx * (float) (kRows - 1 - row);
            dot.size = dotPx;
            dots.push_back (dot);
        }
    }

    return dots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip::Layout
//
////////////////////////////////////////////////////////////////////////////////

void SizeGrip::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip::Paint
//
//  Muted, in the theme's secondary text color, so it reads as a hint rather
//  than a control.
//
////////////////////////////////////////////////////////////////////////////////

void SizeGrip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kDotDip  = 2.0f;
    constexpr float  kStepDip = 4.0f;



    (void) text;

    if (!IsVisible())
    {
        return;
    }

    for (const Dot & dot : GetDots (GetBounds(), m_scaler.ToPxf (kDotDip), m_scaler.ToPxf (kStepDip)))
    {
        painter.FillRect (dot.x, dot.y, dot.size, dot.size, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip::ClassifyHit
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind SizeGrip::ClassifyHit (POINT clientPx) const
{
    (void) clientPx;

    return DxuiHitTestKind::ResizeCornerBR;
}
