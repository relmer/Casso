#include "Pch.h"

#include "Widgets/ListView/DxuiListIcons.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListIcons::PaintSmallArtFrame
//
//  Explorer's frame around a big icon box whose art is smaller than it,
//  measured at 150% in the dark theme: two pixels a shade darker than the
//  background at the box's edge, then three pixels inside them that fade from
//  well lighter than the background back to it. The light theme takes the
//  same steps the other way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListIcons::PaintSmallArtFrame (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg)
{
    static constexpr int  kRing[] = { 69, 46, 23 };
    int                   r       = (int) ((bg >> 16) & 0xFF);
    int                   g       = (int) ((bg >>  8) & 0xFF);
    int                   b       = (int) ( bg        & 0xFF);
    bool                  dark    = (r * 299 + g * 587 + b * 114) / 1000 < 128;
    int                   sign    = dark ? 1 : -1;
    int                   inset   = 2;



    painter.OutlineRect (x, y, sizePx, sizePx, 2.0f, ShiftGray (bg, -4 * sign));

    for (int step : kRing)
    {
        painter.OutlineRect (x + (float) inset, y + (float) inset, sizePx - (float) (2 * inset), sizePx - (float) (2 * inset), 1.0f, ShiftGray (bg, step * sign));
        inset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListIcons::ShiftGray
//
//  An opaque color this much lighter (or, negative, darker) in each channel.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiListIcons::ShiftGray (uint32_t argb, int delta)
{
    int  r = std::clamp ((int) ((argb >> 16) & 0xFF) + delta, 0, 255);
    int  g = std::clamp ((int) ((argb >>  8) & 0xFF) + delta, 0, 255);
    int  b = std::clamp ((int) ( argb        & 0xFF) + delta, 0, 255);



    return 0xFF000000u | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListIcons::PaintBrokenBadge
//
//  A red disc with a white cross over the icon's lower-left corner, about
//  half the icon across and never under the size given, so it reads on a
//  16 dip icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListIcons::PaintBrokenBadge (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx)
{
    HRESULT  hr    = S_OK;
    float    d     = (std::max) (minPx, iconPx * 0.45f);
    float    cx    = iconX + d * 0.5f;
    float    cy    = iconY + iconPx - d * 0.5f;
    float    arm   = d * 0.2f;
    float    thick = (std::max) (1.0f, d * 0.12f);



    hr = text.FillEllipse (cx, cy, d * 0.5f, d * 0.5f, s_kBrokenBadgeArgb);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy - arm, cx + arm, cy + arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy + arm, cx + arm, cy - arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
