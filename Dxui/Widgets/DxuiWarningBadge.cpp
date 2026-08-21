#include "Pch.h"

#include "Widgets/DxuiWarningBadge.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWarningBadge::Draw
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWarningBadge::Draw (IDxuiPainter & painter,
                             float left, float top, float w, float h,
                             uint32_t fill, uint32_t edge, uint32_t mark)
{
    float  apexX  = left + w * 0.5f;
    float  baseY  = top + h;
    float  barW   = (std::max) (1.0f, w * 0.15f);
    float  barX   = apexX - barW * 0.5f;
    float  barTop = top + h * 0.34f;
    float  barH   = h * 0.32f;
    float  dotY   = top + h * 0.75f;



    painter.FillConvexQuad (apexX,      top,
                            left + w,   baseY,
                            left,       baseY,
                            apexX,      top,
                            fill);

    // Outline the two slanted sides and the base so the mark keeps its shape
    // against a light surface as well as a dark one.
    painter.DrawLineApprox (apexX, top,   left + w, baseY, 1.0f, edge);
    painter.DrawLineApprox (apexX, top,   left,     baseY, 1.0f, edge);
    painter.DrawLineApprox (left,  baseY, left + w, baseY, 1.0f, edge);

    // Exclamation mark: a bar, a gap, then a dot.
    painter.FillRect (barX, barTop, barW, barH, mark);
    painter.FillRect (barX, dotY,   barW, barW, mark);
}
