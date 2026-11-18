#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IconSizeSteps
//
//  What Ctrl with the wheel does to a list's view, as in File Explorer. Its
//  icon sizes are one ladder with small differences by scale, measured at 100,
//  125 and 150%, and a notch moves two rungs from the rung at or below the
//  current size, so the sizes up differ from the sizes down. Past the smallest
//  size each notch moves on to the next view, List, Details, Tiles and Content, where it
//  stops; the other way, it stops at Extra large.
//
//  A SIZE GOES WITH THE VIEW THAT HAS ITS LAYOUT. A name goes under an icon of
//  33 and up, and beside a smaller one, so those sizes are Medium icons and
//  Small icons. A view's default size is an icon size of 0.
//
////////////////////////////////////////////////////////////////////////////////

class IconSizeSteps
{
public:
    struct Size
    {
        DxuiListView::View  view    = DxuiListView::View::Details;
        int                 iconDip = 0;

        bool operator== (const Size &) const = default;
    };

    struct Rung
    {
        int  fromDip = 0;
        int  upDip   = 0;
        int  downDip = 0;
    };

    static Size  Step (const Size & current, bool larger, UINT dpi);

private:
    static constexpr int  s_kLargestDip    = 256;
    static constexpr int  s_kSmallestDip   = 16;
    static constexpr int  s_kLargeFromDip  = 72;
    static constexpr int  s_kMediumFromDip = 33;

    static Size  MakeSize (int iconDip);
};