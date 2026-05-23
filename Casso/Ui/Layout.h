#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Tiny set of stack/grid layout primitives for the native UI runtime
//  plus DPI helpers. Pure logic — measurement / arrangement takes a
//  rect, a desired child size list, and returns child rects.
//
////////////////////////////////////////////////////////////////////////////////

enum class LayoutOrientation
{
    Horizontal = 0,
    Vertical   = 1,
};


struct LayoutChildSize
{
    int  widthPx   = 0;
    int  heightPx  = 0;
    int  weight    = 0;     // 0 = fixed; > 0 splits leftover space proportionally
};


class Layout
{
public:
    static std::vector<RECT>  Stack       (const RECT                            & containerRect,
                                           LayoutOrientation                       orientation,
                                           int                                     spacingPx,
                                           const std::vector<LayoutChildSize>    & children);

    static std::vector<RECT>  Grid        (const RECT                            & containerRect,
                                           int                                     columns,
                                           int                                     rows,
                                           int                                     spacingPx,
                                           int                                     childCount);

    static int                ScalePx     (int basePx, UINT dpi);
    static float              ScaleFloat  (float basePx, UINT dpi);
};
