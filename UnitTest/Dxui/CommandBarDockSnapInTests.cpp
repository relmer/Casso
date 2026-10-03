#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDockSnapInTests
//
//  A floating bar dragged into a dock band snaps into it, keeping the place
//  the pointer grabbed, and goes on as a drag of the docked bar.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandBarDockSnapInTests)
{
public:

    static constexpr RECT  kClient = { 0, 0, 1000, 800 };
    static constexpr int   kBand   = 50;
    static constexpr int   kPull   = 40;
    static constexpr int   kMargin = 8;


    TEST_METHOD (AFlatBarsGrabCountsTheMarginAcross)
    {
        POINT  grab = DxuiToolbarDock::GrabForDocking (POINT { 10, 12 }, false, kMargin);


        Assert::AreEqual (18L, grab.x, L"across the top or bottom the docked bar starts a margin in");
        Assert::AreEqual (10L, grab.y);
    }


    TEST_METHOD (AnUprightBarsGrabMeasuresDown)
    {
        POINT  grab = DxuiToolbarDock::GrabForDocking (POINT { 12, 70 }, true, kMargin);


        Assert::AreEqual (78L, grab.x);
        Assert::AreEqual (70L, grab.y, L"down a side the distance runs down the bar");
    }


    TEST_METHOD (ASnappedBarSlidesWithThePointerAndHoldsUntilPulled)
    {
        POINT            grab = DxuiToolbarDock::GrabForDocking (POINT { 12, 70 }, true, kMargin);
        DxuiToolbarDock  dock = DxuiToolbarDock::PickForDrop (POINT { 20, 300 }, grab, kClient, 96);
        DxuiToolbarDock  slid;


        Assert::IsTrue   (dock.edge == DxuiToolbarDock::Edge::Left);
        Assert::AreEqual (230, dock.offsetDip, L"the bar snaps in with the grabbed point still under the pointer");

        slid = DxuiToolbarDock::SlideAlong (POINT { 30, 500 }, grab, dock, kClient, 96);

        Assert::AreEqual (430, slid.offsetDip);
        Assert::IsFalse  (DxuiToolbarDock::IsPulledOut (POINT { kBand + kPull, 500 }, slid.edge, kClient, kBand, kPull), L"leaving the band is not yet a pull");
    }
};
