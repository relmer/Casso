#include "Pch.h"

#include "Shell/Layout/DriveRowLayout.h"





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
