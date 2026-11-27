#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMath
//
//  Window size and placement math: clamping a size to its limits, and fitting
//  or growing a window rect to a maximum size inside a work area.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWindowSizeMath
{
public:
    //  Each dimension of size limited to [minSize, maxSize]. A maximum below
    //  the minimum is raised to it.
    static SIZE  ClampSize (const SIZE & size, const SIZE & minSize, const SIZE & maxSize);

    //  The window rect at maxPx, cut to the work area and moved only as far
    //  as it takes to stay inside it.
    static RECT  FitRectToMaxSize (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx);

    //  The window rect grown to maxPx in whichever dimension maxPx exceeds
    //  it, cut to the work area and moved inside it; false when it would not
    //  grow in either dimension.
    static bool  TryGetGrownRect (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx, RECT & outPx);

    //  The OS track limits for a window: minSizeDip scaled to the DPI as the
    //  minimum when either dimension is positive, and maxPx, when hasMax, as
    //  a maximum never below the minimum. False when neither limit is set.
    static bool  TryApplyTrackLimits (const SIZE & minSizeDip, UINT dpi, bool hasMax, const SIZE & maxPx, MINMAXINFO & inOutInfo);
};