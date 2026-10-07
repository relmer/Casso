#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGrip
//
//  The classic resize gripper in a window's bottom-right corner: six dots in
//  a 45-degree triangle (one, two, then three per row toward the corner).
//  No icon font has a gripper; MDL2's nearest candidate, E788, is an
//  outlined triangle, so the dots are drawn with the painter.
//
//  It reports a bottom-right resize hit for its whole square, so the area
//  it marks is the area that resizes. Laid out and painted in pixels, as a
//  dialog's content is. Hidden while the window is maximized.
//
////////////////////////////////////////////////////////////////////////////////

class SizeGrip : public IDxuiControl
{
public:
    static constexpr int  kRows    = 3;
    static constexpr int  kSizeDip = 12;   // inside the 16-dip button-row edge pad, so it never meets a button

    struct Dot
    {
        float  x    = 0.0f;
        float  y    = 0.0f;
        float  size = 0.0f;
    };

    static RECT              GetGripRect (const RECT & clientPx, int sizePx);
    static std::vector<Dot>  GetDots     (const RECT & gripPx, float dotPx, float stepPx);

    void                Layout       (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiHitTestKind     ClassifyHit  (POINT clientPx) const override;

private:
    DxuiDpiScaler  m_scaler;
};
