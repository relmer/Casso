#include "Pch.h"

#include "CassoExplorer/Model/IconSizeSteps.h"





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



    size.view = (iconDip >= kSizes[0])         ? DxuiListView::View::ExtraLargeIcons
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

IconSizeSteps::Size IconSizeSteps::Step (const Size & current, bool larger)
{
    static constexpr DxuiListView::View  s_kOtherViews[] = { DxuiListView::View::List, DxuiListView::View::Details, DxuiListView::View::Tiles, DxuiListView::View::Content };
    const auto *                         other           = std::find (std::begin (s_kOtherViews), std::end (s_kOtherViews), current.view);
    int                                  iconDip         = current.iconDip;



    if (other != std::end (s_kOtherViews))
    {
        if (larger && other == std::begin (s_kOtherViews))
        {
            return MakeSize (kSizes[std::size (kSizes) - 1]);
        }

        if (!larger && other + 1 == std::end (s_kOtherViews))
        {
            return current;
        }

        return Size { larger ? other[-1] : other[1], 0 };
    }

    iconDip = (iconDip > 0) ? iconDip : DxuiListItemLayout::GetItemMetrics (current.view).iconDip;

    if (larger)
    {
        for (size_t at = std::size (kSizes); at-- > 0; )
        {
            if (kSizes[at] > iconDip)
            {
                return MakeSize (kSizes[at]);
            }
        }

        return current;
    }

    for (int size : kSizes)
    {
        if (size < iconDip)
        {
            return MakeSize (size);
        }
    }

    return Size { s_kOtherViews[0], 0 };
}