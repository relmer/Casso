#include "Pch.h"

#include "DxuiTabDragModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragModel::GetGapIndex
//
//  Coordinates are doubled so that a center is exact for an odd width.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabDragModel::GetGapIndex (
    const std::vector<int>  & widths,
    int                       dragged,
    int                       originLeft,
    int                       draggedLeft,
    int                       draggedWidth)
{
    int  gap          = 0;
    int  left         = originLeft;
    int  i            = 0;
    int  draggedTwice = draggedLeft * 2 + draggedWidth;



    for (i = 0; i < (int) widths.size(); i++)
    {
        if (i == dragged)
        {
            continue;
        }

        if (left * 2 + widths[(size_t) i] < draggedTwice)
        {
            gap++;
        }

        left += widths[(size_t) i];
    }

    return gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragModel::GetLefts
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabDragModel::GetLefts (
    const std::vector<int>  & widths,
    int                       dragged,
    int                       originLeft,
    int                       gapIndex,
    int                       gapWidth,
    std::vector<int>        & outLefts,
    int                     & outEndRight)
{
    int   left       = originLeft;
    int   slot       = 0;
    int   i          = 0;
    int   gapLeft    = originLeft;
    int   closedLeft = originLeft;
    bool  gapSet     = false;



    outLefts.assign (widths.size(), originLeft);

    for (i = 0; i < (int) widths.size(); i++)
    {
        //  Closed up, the dragged tab's place is its own slot, after the
        //  tabs before it.
        if (i == dragged)
        {
            closedLeft = left;
            continue;
        }

        if (gapIndex >= 0 && slot == gapIndex && !gapSet)
        {
            gapLeft  = left;
            gapSet   = true;
            left    += gapWidth;
        }

        outLefts[(size_t) i]  = left;
        left                 += widths[(size_t) i];
        slot++;
    }

    //  A gap past the last tab opens at the end of the row.
    if (gapIndex >= 0 && !gapSet)
    {
        gapLeft  = left;
        left    += gapWidth;
    }

    if (dragged >= 0 && dragged < (int) widths.size())
    {
        outLefts[(size_t) dragged] = (gapIndex >= 0) ? gapLeft : closedLeft;
    }

    outEndRight = left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragModel::IsTornOff
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabDragModel::IsTornOff (const RECT & strip, POINT point, int thresholdPx)
{
    return point.x < strip.left   - thresholdPx
        || point.x >= strip.right  + thresholdPx
        || point.y < strip.top    - thresholdPx
        || point.y >= strip.bottom + thresholdPx;
}
