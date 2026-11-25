#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizeMathTests
//
//  The size and placement math a window uses: clamping a size to its limits,
//  fitting or growing a window rect inside a work area, and the OS track
//  limits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiWindowSizeMathTests)
{
public:

    //  Each dimension is limited to its limits, and a maximum below the
    //  minimum is raised to it.
    TEST_METHOD (ClampSize_HoldsEachDimensionWithinItsLimits)
    {
        SIZE  low     = DxuiWindowSizeMath::ClampSize (SIZE { 10, 10 },     SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  high    = DxuiWindowSizeMath::ClampSize (SIZE { 900, 900 },   SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  inside  = DxuiWindowSizeMath::ClampSize (SIZE { 300, 300 },   SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  crossed = DxuiWindowSizeMath::ClampSize (SIZE { 900, 900 },   SIZE { 100, 200 }, SIZE { 50,  60 });

        Assert::AreEqual (100L, low.cx);
        Assert::AreEqual (200L, low.cy);
        Assert::AreEqual (400L, high.cx);
        Assert::AreEqual (500L, high.cy);
        Assert::AreEqual (300L, inside.cx);
        Assert::AreEqual (300L, inside.cy);
        Assert::AreEqual (100L, crossed.cx);
        Assert::AreEqual (200L, crossed.cy);
    }




    //  A window opened smaller than its content grows to it, and one taller
    //  than the work area is cut to it and moved up to stay inside.
    TEST_METHOD (FitRectToMaxSize_GrowsToTheContentWithinTheWorkArea)
    {
        RECT  work   = { 0, 0, 2000, 1400 };
        RECT  grown  = DxuiWindowSizeMath::FitRectToMaxSize (RECT { 100, 100, 820, 980 },  SIZE { 760, 1100 }, work);
        RECT  capped = DxuiWindowSizeMath::FitRectToMaxSize (RECT { 100, 300, 820, 1180 }, SIZE { 760, 1600 }, work);

        Assert::AreEqual (100L,  grown.left);
        Assert::AreEqual (100L,  grown.top);
        Assert::AreEqual (860L,  grown.right);
        Assert::AreEqual (1200L, grown.bottom);
        Assert::AreEqual (100L,  capped.left);
        Assert::AreEqual (0L,    capped.top);
        Assert::AreEqual (860L,  capped.right);
        Assert::AreEqual (1400L, capped.bottom);
    }




    //  A window whose maximum extends past the work area's right or bottom
    //  edge is moved back until the edge is inside.
    TEST_METHOD (FitRectToMaxSize_SlidesBackInsideTheWorkArea)
    {
        RECT  work = { 0, 0, 1000, 800 };
        RECT  slid = DxuiWindowSizeMath::FitRectToMaxSize (RECT { 700, 500, 900, 700 }, SIZE { 600, 500 }, work);

        Assert::AreEqual (400L,  slid.left);
        Assert::AreEqual (300L,  slid.top);
        Assert::AreEqual (1000L, slid.right);
        Assert::AreEqual (800L,  slid.bottom);
    }




    //  Content taller than the window grows it, moved inside the work area,
    //  and never narrows a window wider than the content; content that fits
    //  does not change the window.
    TEST_METHOD (TryGetGrownRect_GrowsOnlyWhereTheContentExceedsTheWindow)
    {
        RECT  work    = { 0, 0, 2000, 1400 };
        RECT  grown   = {};
        RECT  same    = {};
        bool  isGrown = DxuiWindowSizeMath::TryGetGrownRect (RECT { 100, 300, 1000, 1180 }, SIZE { 760, 1300 }, work, grown);
        bool  isSame  = DxuiWindowSizeMath::TryGetGrownRect (RECT { 100, 100, 1000, 1180 }, SIZE { 760, 900 },  work, same);

        Assert::IsTrue   (isGrown);
        Assert::AreEqual (100L,  grown.left);
        Assert::AreEqual (100L,  grown.top);
        Assert::AreEqual (1000L, grown.right);
        Assert::AreEqual (1400L, grown.bottom);
        Assert::IsFalse  (isSame);
    }




    //  The minimum is scaled to the DPI, and a maximum below it is raised to it.
    TEST_METHOD (TryApplyTrackLimits_ScalesTheMinimum_AndKeepsTheMaximumAboveIt)
    {
        static constexpr UINT  kDoubleDpi = 2 * USER_DEFAULT_SCREEN_DPI;

        MINMAXINFO  info   = {};
        bool        isSet  = DxuiWindowSizeMath::TryApplyTrackLimits (SIZE { 300, 200 }, kDoubleDpi, true, SIZE { 100, 900 }, info);

        Assert::IsTrue   (isSet);
        Assert::AreEqual (600L,  info.ptMinTrackSize.x);
        Assert::AreEqual (400L,  info.ptMinTrackSize.y);
        Assert::AreEqual (600L,  info.ptMaxTrackSize.x, L"a maximum below the minimum is raised to it");
        Assert::AreEqual (900L,  info.ptMaxTrackSize.y);
    }




    //  With neither limit set, the OS limits are not changed.
    TEST_METHOD (TryApplyTrackLimits_WithNoLimits_ChangesNothing)
    {
        MINMAXINFO  info  = {};
        bool        isSet = false;

        info.ptMinTrackSize = POINT { 11, 12 };
        info.ptMaxTrackSize = POINT { 13, 14 };

        isSet = DxuiWindowSizeMath::TryApplyTrackLimits (SIZE { 0, 0 }, USER_DEFAULT_SCREEN_DPI, false, SIZE {}, info);

        Assert::IsFalse  (isSet);
        Assert::AreEqual (11L, info.ptMinTrackSize.x);
        Assert::AreEqual (12L, info.ptMinTrackSize.y);
        Assert::AreEqual (13L, info.ptMaxTrackSize.x);
        Assert::AreEqual (14L, info.ptMaxTrackSize.y);
    }




    //  A maximum alone is raised to at least the OS minimum.
    TEST_METHOD (TryApplyTrackLimits_WithOnlyAMaximum_KeepsItAboveTheOsMinimum)
    {
        MINMAXINFO  info  = {};
        bool        isSet = false;

        info.ptMinTrackSize = POINT { 136, 39 };

        isSet = DxuiWindowSizeMath::TryApplyTrackLimits (SIZE { 0, 0 }, USER_DEFAULT_SCREEN_DPI, true, SIZE { 100, 800 }, info);

        Assert::IsTrue   (isSet);
        Assert::AreEqual (136L, info.ptMinTrackSize.x, L"the OS's minimum is not replaced");
        Assert::AreEqual (136L, info.ptMaxTrackSize.x);
        Assert::AreEqual (800L, info.ptMaxTrackSize.y);
    }
};