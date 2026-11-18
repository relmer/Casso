#include "Pch.h"

#include "CassoExplorer/Model/IconSizeSteps.h"




//  File Explorer's sizes for one notch, measured at 100, 125 and 150%: from each
//  size up to the next row's, the size one notch up and one notch down.
static constexpr IconSizeSteps::Rung  s_kRungs96[] =
{
    { 16, 18, 16 }, { 18, 20, 17 }, { 19, 20, 18 }, { 20, 22, 19 }, { 21, 23, 20 }, { 23, 25, 22 },
    { 24, 26, 23 }, { 25, 27, 23 }, { 26, 28, 24 }, { 27, 29, 25 }, { 28, 30, 26 }, { 29, 31, 27 },
    { 30, 32, 28 }, { 31, 33, 29 }, { 32, 34, 30 }, { 33, 35, 31 }, { 34, 37, 32 }, { 35, 38, 33 },
    { 37, 39, 34 }, { 38, 41, 35 }, { 39, 42, 37 }, { 41, 44, 38 }, { 42, 45, 39 }, { 44, 47, 41 },
    { 45, 49, 42 }, { 47, 50, 44 }, { 49, 52, 45 }, { 50, 54, 47 }, { 52, 56, 49 }, { 54, 58, 50 },
    { 56, 60, 52 }, { 58, 62, 54 }, { 60, 64, 56 }, { 62, 67, 58 }, { 64, 69, 60 }, { 67, 72, 62 },
    { 69, 74, 64 }, { 72, 77, 67 }, { 74, 80, 69 }, { 77, 83, 72 }, { 80, 86, 74 }, { 83, 89, 77 },
    { 86, 92, 80 }, { 89, 95, 83 }, { 92, 99, 86 }, { 95, 102, 89 }, { 99, 106, 92 }, { 102, 110, 95 },
    { 106, 114, 99 }, { 110, 118, 102 }, { 114, 122, 106 }, { 118, 126, 110 }, { 122, 131, 114 }, { 126, 136, 118 },
    { 131, 140, 122 }, { 136, 145, 126 }, { 140, 151, 131 }, { 145, 156, 136 }, { 151, 162, 140 }, { 156, 168, 145 },
    { 162, 174, 151 }, { 168, 180, 156 }, { 174, 186, 162 }, { 180, 193, 168 }, { 186, 200, 174 }, { 193, 207, 180 },
    { 200, 215, 186 }, { 207, 223, 193 }, { 215, 231, 200 }, { 223, 239, 207 }, { 231, 248, 215 }, { 239, 256, 223 },
    { 248, 256, 231 }, { 256, 256, 239 },
};

static constexpr IconSizeSteps::Rung  s_kRungs120[] =
{
    { 16, 18, 16 }, { 18, 20, 17 }, { 19, 20, 18 }, { 20, 22, 19 }, { 21, 23, 20 }, { 23, 25, 22 },
    { 24, 26, 23 }, { 25, 27, 23 }, { 26, 28, 24 }, { 27, 29, 25 }, { 28, 30, 26 }, { 29, 31, 27 },
    { 30, 32, 28 }, { 31, 33, 29 }, { 32, 34, 30 }, { 33, 35, 31 }, { 34, 37, 32 }, { 35, 38, 33 },
    { 37, 39, 34 }, { 38, 41, 35 }, { 39, 42, 37 }, { 41, 44, 38 }, { 42, 45, 39 }, { 44, 47, 41 },
    { 45, 49, 42 }, { 47, 50, 44 }, { 49, 52, 45 }, { 50, 54, 47 }, { 52, 56, 49 }, { 54, 58, 50 },
    { 56, 60, 52 }, { 58, 62, 54 }, { 60, 64, 56 }, { 62, 67, 58 }, { 64, 69, 60 }, { 67, 72, 62 },
    { 69, 74, 64 }, { 72, 77, 67 }, { 74, 80, 69 }, { 77, 83, 72 }, { 80, 86, 74 }, { 83, 89, 77 },
    { 86, 92, 80 }, { 89, 95, 83 }, { 92, 99, 86 }, { 95, 102, 89 }, { 99, 106, 92 }, { 102, 111, 95 },
    { 106, 114, 99 }, { 110, 118, 102 }, { 113, 122, 106 }, { 118, 125, 111 }, { 122, 131, 114 }, { 127, 136, 118 },
    { 131, 140, 122 }, { 135, 144, 125 }, { 140, 150, 131 }, { 146, 156, 136 }, { 152, 162, 140 }, { 156, 168, 144 },
    { 162, 174, 150 }, { 168, 180, 156 }, { 174, 186, 162 }, { 179, 193, 168 }, { 185, 200, 174 }, { 193, 208, 180 },
    { 200, 215, 186 }, { 207, 224, 193 }, { 215, 231, 200 }, { 223, 238, 208 }, { 231, 249, 215 }, { 240, 256, 224 },
    { 248, 256, 231 }, { 254, 256, 238 },
};

static constexpr IconSizeSteps::Rung  s_kRungs144[] =
{
    { 16, 18, 16 }, { 18, 20, 17 }, { 19, 20, 18 }, { 20, 22, 19 }, { 21, 23, 20 }, { 23, 25, 22 },
    { 24, 26, 23 }, { 25, 27, 23 }, { 26, 28, 24 }, { 27, 29, 25 }, { 28, 30, 26 }, { 29, 31, 27 },
    { 30, 32, 28 }, { 31, 33, 29 }, { 32, 34, 30 }, { 33, 35, 31 }, { 34, 37, 32 }, { 35, 38, 33 },
    { 37, 39, 34 }, { 38, 41, 35 }, { 39, 42, 37 }, { 41, 44, 38 }, { 42, 45, 39 }, { 44, 47, 41 },
    { 45, 49, 42 }, { 47, 50, 44 }, { 49, 52, 45 }, { 50, 54, 47 }, { 52, 56, 49 }, { 54, 58, 50 },
    { 56, 60, 52 }, { 58, 62, 54 }, { 60, 64, 56 }, { 62, 67, 58 }, { 64, 69, 60 }, { 67, 72, 62 },
    { 69, 74, 64 }, { 72, 77, 67 }, { 74, 80, 69 }, { 77, 83, 72 }, { 80, 86, 74 }, { 83, 89, 77 },
    { 86, 92, 80 }, { 89, 95, 83 }, { 92, 96, 86 }, { 95, 102, 89 }, { 100, 107, 92 }, { 102, 111, 95 },
    { 106, 113, 96 }, { 110, 117, 102 }, { 115, 122, 107 }, { 119, 126, 111 }, { 121, 130, 113 }, { 125, 137, 117 },
    { 132, 141, 122 }, { 136, 145, 126 }, { 140, 151, 130 }, { 144, 156, 137 }, { 151, 162, 141 }, { 155, 169, 145 },
    { 162, 175, 151 }, { 168, 179, 156 }, { 174, 186, 162 }, { 181, 192, 169 }, { 185, 201, 175 }, { 194, 207, 179 },
    { 200, 215, 186 }, { 206, 224, 192 }, { 215, 230, 201 }, { 223, 239, 207 }, { 232, 247, 215 }, { 238, 256, 224 },
    { 249, 256, 230 }, { 253, 256, 239 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  IconSizeSteps::MakeSize
//
//  A size that is its view's default has an icon size of 0.
//
////////////////////////////////////////////////////////////////////////////////

IconSizeSteps::Size IconSizeSteps::MakeSize (int iconDip)
{
    Size  size;



    size.view = (iconDip >= s_kLargestDip)        ? DxuiListView::View::ExtraLargeIcons
              : (iconDip >= s_kLargeFromDip)   ? DxuiListView::View::LargeIcons
              : (iconDip >= s_kMediumFromDip)  ? DxuiListView::View::MediumIcons
                                               : DxuiListView::View::SmallIcons;

    size.iconDip = (DxuiListItemLayout::GetItemMetrics (size.view).iconDip == iconDip) ? 0 : iconDip;

    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IconSizeSteps::Step
//
//  A size off the run, as Medium's default of 48, moves to the next size on
//  it in that direction.
//
////////////////////////////////////////////////////////////////////////////////

IconSizeSteps::Size IconSizeSteps::Step (const Size & current, bool larger, UINT dpi)
{
    static constexpr DxuiListView::View  s_kOtherViews[] = { DxuiListView::View::List, DxuiListView::View::Details, DxuiListView::View::Tiles, DxuiListView::View::Content };
    const auto *                         other           = std::find (std::begin (s_kOtherViews), std::end (s_kOtherViews), current.view);
    int                                  iconDip         = current.iconDip;



    if (other != std::end (s_kOtherViews))
    {
        if (larger && other == std::begin (s_kOtherViews))
        {
            return MakeSize (s_kSmallestDip);
        }

        if (!larger && other + 1 == std::end (s_kOtherViews))
        {
            return current;
        }

        return Size { larger ? other[-1] : other[1], 0 };
    }

    iconDip = (iconDip > 0) ? iconDip : DxuiListItemLayout::GetItemMetrics (current.view).iconDip;

    //  Nothing smaller than the smallest size; the next view is.
    {
        const Rung *  rungs = (dpi >= 132) ? s_kRungs144 : (dpi >= 108) ? s_kRungs120 : s_kRungs96;
        size_t        count = (dpi >= 132) ? std::size (s_kRungs144) : (dpi >= 108) ? std::size (s_kRungs120) : std::size (s_kRungs96);
        const Rung *  rung  = rungs;

        for (size_t at = 0; at < count && rungs[at].fromDip <= iconDip; at++)
        {
            rung = &rungs[at];
        }

        if (!larger && iconDip <= s_kSmallestDip)
        {
            return Size { s_kOtherViews[0], 0 };
        }

        return MakeSize (larger ? rung->upDip : rung->downDip);
    }
}