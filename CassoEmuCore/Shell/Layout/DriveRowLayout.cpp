#include "Pch.h"

#include "Shell/Layout/DriveRowLayout.h"


//
//  Shrink factor matching the case-top depth ratio in DriveWidget: the back
//  edge is about 20% narrower than the front, so the back center shifts about
//  a fifth of the way toward the shared vanishing point.
//
static constexpr int  s_kSkewNumerator   = 27;
static constexpr int  s_kSkewDenominator = 100;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayout::ComputeRowOriginX
//
////////////////////////////////////////////////////////////////////////////////

int DriveRowLayout::ComputeRowOriginX (
    int clientWidthPx,
    int widgetWidthPx,
    int gapPx,
    int visibleCount)
{
    int  showing = std::clamp (visibleCount, 1, 2);
    int  totalW  = widgetWidthPx * showing + gapPx * (showing - 1);
    int  originX = std::max (0, (clientWidthPx - totalW) / 2);



    return (originX);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayout::ApplyLoneDriveCaptionOffset
//
////////////////////////////////////////////////////////////////////////////////

int DriveRowLayout::ApplyLoneDriveCaptionOffset (
    int originX,
    int captionLeadPx,
    int visibleCount)
{
    int  adjusted = originX;



    if (visibleCount == 1)
    {
        adjusted = std::max (0, originX - captionLeadPx / 2);
    }

    return (adjusted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayout::ComputeWidgetX
//
////////////////////////////////////////////////////////////////////////////////

int DriveRowLayout::ComputeWidgetX (int originX, int index, int widgetWidthPx, int gapPx)
{
    int  widgetX = originX + index * (widgetWidthPx + gapPx);



    return (widgetX);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayout::ComputePerspectiveSkewPx
//
////////////////////////////////////////////////////////////////////////////////

int DriveRowLayout::ComputePerspectiveSkewPx (int clientWidthPx, int widgetX, int widgetWidthPx)
{
    int  widgetCenterX = widgetX + widgetWidthPx / 2;
    int  vanishingX    = clientWidthPx / 2;
    int  skewPx        = MulDiv (vanishingX - widgetCenterX, s_kSkewNumerator, s_kSkewDenominator);



    return (skewPx);
}
