#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IconSizeSteps
//
//  What Ctrl with the wheel does to a list's view, as in File Explorer. The
//  icon views are one run of sizes, measured from File Explorer at 150%, and
//  each notch moves one size along it. Past the smallest size each notch
//  moves on to the next view, List, Details, Tiles and Content, where it
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

    static Size  Step (const Size & current, bool larger);

    static constexpr int  kSizes[] = { 256, 239, 224, 207, 192, 175, 162, 151, 141, 130, 117, 107, 96, 89, 83, 77, 72, 67, 62, 58, 54, 50, 47, 44, 41, 38, 35, 33,
                                       31, 29, 27, 25, 23, 22, 20, 19, 18, 17, 16 };

private:
    static constexpr int  s_kLargeFromDip  = 72;
    static constexpr int  s_kMediumFromDip = 33;

    static Size  MakeSize (int iconDip);
};