#include "Pch.h"

#include "Widgets/DxuiWarningBadge.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWarningBadge::Measure
//
////////////////////////////////////////////////////////////////////////////////

DxuiWarningBadge::Geometry DxuiWarningBadge::Measure (float left, float top, float w, float h)
{
    Geometry  g;



    g.left   = left;
    g.top    = top;
    g.right  = left + w;
    g.apexX  = left + w * 0.5f;
    g.baseY  = top + h;
    g.barW   = (std::max) (1.0f, w * 0.15f);
    g.barX   = g.apexX - g.barW * 0.5f;
    g.barTop = top + h * 0.34f;
    g.barH   = h * 0.32f;
    g.dotY   = top + h * 0.75f;

    return g;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWarningBadge::Draw
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWarningBadge::Draw (IDxuiPainter & painter,
                             float left, float top, float w, float h,
                             uint32_t fill, uint32_t edge, uint32_t mark)
{
    Geometry  g = Measure (left, top, w, h);



    painter.FillConvexQuad (g.apexX,  g.top,
                            g.right,  g.baseY,
                            g.left,   g.baseY,
                            g.apexX,  g.top,
                            fill);

    // Outline the two slanted sides and the base so the mark keeps its shape
    // against a light surface as well as a dark one.
    painter.DrawLine (g.apexX, g.top,   g.right, g.baseY, 1.0f, edge);
    painter.DrawLine (g.apexX, g.top,   g.left,  g.baseY, 1.0f, edge);
    painter.DrawLine (g.left,  g.baseY, g.right, g.baseY, 1.0f, edge);

    // Exclamation mark: a bar, a gap, then a dot.
    painter.FillRect (g.barX, g.barTop, g.barW, g.barH, mark);
    painter.FillRect (g.barX, g.dotY,   g.barW, g.barW, mark);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWarningBadge::Draw
//
//  The same mark through the text renderer, for a caller that draws its
//  icons there: the painter's output is drawn before the text renderer's,
//  so a mark drawn with the painter would be under the icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWarningBadge::Draw (IDxuiTextRenderer & text,
                             float left, float top, float w, float h,
                             uint32_t fill, uint32_t edge, uint32_t mark)
{
    HRESULT     hr          = S_OK;
    Geometry    g           = Measure (left, top, w, h);
    DxuiPointF  triangle[3] = { { g.apexX, g.top }, { g.right, g.baseY }, { g.left, g.baseY } };



    hr = text.FillPolygon (triangle, ARRAYSIZE (triangle), fill);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (g.apexX, g.top, g.right, g.baseY, 1.0f, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (g.apexX, g.top, g.left, g.baseY, 1.0f, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (g.left, g.baseY, g.right, g.baseY, 1.0f, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillRect (g.barX, g.barTop, g.barW, g.barH, mark);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillRect (g.barX, g.dotY, g.barW, g.barW, mark);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
