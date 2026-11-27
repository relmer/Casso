#include "Pch.h"

#include "Core/DxuiWindowSizeMath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMath::ClampSize
//
//  Each dimension limited to [minSize, maxSize]. A maximum below the
//  minimum is raised to it.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiWindowSizeMath::ClampSize (const SIZE & size, const SIZE & minSize, const SIZE & maxSize)
{
    return SIZE { std::clamp (size.cx, minSize.cx, std::max (maxSize.cx, minSize.cx)),
                  std::clamp (size.cy, minSize.cy, std::max (maxSize.cy, minSize.cy)) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMath::FitRectToMaxSize
//
//  The window at its maximum, cut to the work area, with its top left
//  corner where it is unless the window would extend past the work area's
//  right or bottom edge. It grows as well as shrinks: a size saved from
//  before the content grew, or a design size the content has since
//  outgrown, would otherwise open with a scroll bar on a screen with room
//  to spare.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiWindowSizeMath::FitRectToMaxSize (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx)
{
    LONG  widthPx  = std::min (maxPx.cx, workPx.right  - workPx.left);
    LONG  heightPx = std::min (maxPx.cy, workPx.bottom - workPx.top);
    LONG  left     = std::max (workPx.left, std::min (windowPx.left, workPx.right  - widthPx));
    LONG  top      = std::max (workPx.top,  std::min (windowPx.top,  workPx.bottom - heightPx));



    return RECT { left, top, left + widthPx, top + heightPx };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMath::TryGetGrownRect
//
//  FitRectToMaxSize with the window size as the floor, so a dimension the
//  content does not exceed stays the same size.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowSizeMath::TryGetGrownRect (const RECT & windowPx, const SIZE & maxPx, const RECT & workPx, RECT & outPx)
{
    SIZE  sizePx  = { windowPx.right - windowPx.left, windowPx.bottom - windowPx.top };
    SIZE  grownPx = { std::max (sizePx.cx, maxPx.cx), std::max (sizePx.cy, maxPx.cy) };



    outPx = FitRectToMaxSize (windowPx, grownPx, workPx);

    return outPx.right - outPx.left > sizePx.cx || outPx.bottom - outPx.top > sizePx.cy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMath::TryApplyTrackLimits
//
//  The minimum track size is the minimum client size scaled to the DPI, and
//  the maximum is the one given, never below the minimum. Borderless, so
//  client size and window size coincide.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowSizeMath::TryApplyTrackLimits (const SIZE & minSizeDip, UINT dpi, bool hasMax, const SIZE & maxPx, MINMAXINFO & inOutInfo)
{
    bool  isApplied = false;



    if (minSizeDip.cx > 0 || minSizeDip.cy > 0)
    {
        inOutInfo.ptMinTrackSize.x = MulDiv (minSizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        inOutInfo.ptMinTrackSize.y = MulDiv (minSizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        isApplied = true;
    }

    if (hasMax)
    {
        inOutInfo.ptMaxTrackSize.x = std::max (maxPx.cx, inOutInfo.ptMinTrackSize.x);
        inOutInfo.ptMaxTrackSize.y = std::max (maxPx.cy, inOutInfo.ptMinTrackSize.y);
        isApplied = true;
    }

    return isApplied;
}





