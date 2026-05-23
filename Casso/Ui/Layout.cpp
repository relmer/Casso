#include "Pch.h"

#include "Layout.h"





namespace
{
    constexpr UINT  s_kBaseDpi  = 96;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stack
//
//  Lays children out along the primary axis. Fixed-size children (weight
//  == 0) consume their declared size; weighted children split whatever
//  pixels remain in proportion to their `weight`.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> Layout::Stack (
    const RECT                          & containerRect,
    LayoutOrientation                     orientation,
    int                                   spacingPx,
    const std::vector<LayoutChildSize>  & children)
{
    std::vector<RECT>  result;
    int                fixedTotal   = 0;
    int                weightTotal  = 0;
    int                axisLength   = 0;
    int                crossLength  = 0;
    int                leftover     = 0;
    int                cursor       = 0;
    bool               horizontal   = (orientation == LayoutOrientation::Horizontal);



    result.reserve (children.size());

    if (children.empty())
    {
        return result;
    }

    axisLength  = horizontal
        ? (containerRect.right  - containerRect.left)
        : (containerRect.bottom - containerRect.top);
    crossLength = horizontal
        ? (containerRect.bottom - containerRect.top)
        : (containerRect.right  - containerRect.left);

    for (const LayoutChildSize & c : children)
    {
        int  size = horizontal ? c.widthPx : c.heightPx;

        if (c.weight > 0)
        {
            weightTotal += c.weight;
        }
        else
        {
            fixedTotal += size;
        }
    }

    leftover = axisLength
             - fixedTotal
             - spacingPx * std::max (0, (int) children.size() - 1);

    if (leftover < 0)
    {
        leftover = 0;
    }

    cursor = horizontal ? containerRect.left : containerRect.top;

    for (const LayoutChildSize & c : children)
    {
        RECT  r        = {};
        int   axisSize = horizontal ? c.widthPx : c.heightPx;

        if ((c.weight > 0) && (weightTotal > 0))
        {
            axisSize = (leftover * c.weight) / weightTotal;
        }

        if (horizontal)
        {
            r.left   = cursor;
            r.right  = cursor + axisSize;
            r.top    = containerRect.top;
            r.bottom = containerRect.top + crossLength;
        }
        else
        {
            r.left   = containerRect.left;
            r.right  = containerRect.left + crossLength;
            r.top    = cursor;
            r.bottom = cursor + axisSize;
        }

        result.push_back (r);
        cursor += axisSize + spacingPx;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Grid
//
//  Evenly-divides the container rect into a `columns` x `rows` grid
//  (with uniform inter-cell spacing) and emits the first `childCount`
//  cell rects in row-major order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> Layout::Grid (
    const RECT  & containerRect,
    int           columns,
    int           rows,
    int           spacingPx,
    int           childCount)
{
    std::vector<RECT>  result;
    int                containerW = containerRect.right  - containerRect.left;
    int                containerH = containerRect.bottom - containerRect.top;
    int                cellW      = 0;
    int                cellH      = 0;



    if ((columns <= 0) || (rows <= 0) || (childCount <= 0))
    {
        return result;
    }

    result.reserve ((size_t) childCount);

    cellW = (containerW - spacingPx * (columns - 1)) / columns;
    cellH = (containerH - spacingPx * (rows    - 1)) / rows;

    if (cellW < 0) { cellW = 0; }
    if (cellH < 0) { cellH = 0; }

    for (int i = 0; i < childCount; i++)
    {
        int   row = i / columns;
        int   col = i % columns;
        RECT  r   = {};

        if (row >= rows)
        {
            break;
        }

        r.left   = containerRect.left + col * (cellW + spacingPx);
        r.top    = containerRect.top  + row * (cellH + spacingPx);
        r.right  = r.left + cellW;
        r.bottom = r.top  + cellH;
        result.push_back (r);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScalePx
//
////////////////////////////////////////////////////////////////////////////////

int Layout::ScalePx (int basePx, UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;

    return MulDiv (basePx, (int) effectiveDpi, (int) s_kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScaleFloat
//
////////////////////////////////////////////////////////////////////////////////

float Layout::ScaleFloat (float basePx, UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;

    return basePx * ((float) effectiveDpi / (float) s_kBaseDpi);
}
