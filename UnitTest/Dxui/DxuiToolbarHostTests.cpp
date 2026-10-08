#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHostTests
//
//  The toolbar host's geometry: where a docked toolbar sits along each edge,
//  which side of the dock site it shares, and where its floating window goes
//  when its scale changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarHostTests)
{
public:

    static constexpr RECT  kArea   = { 0, 100, 1000, 800 };
    static constexpr int   kBand   = 40;
    static constexpr int   kMargin = 8;


    static DxuiToolbarDock MakeDock (DxuiToolbarDock::Edge edge, int offsetDip)
    {
        DxuiToolbarDock  dock;


        dock.edge      = edge;
        dock.offsetDip = offsetDip;
        return dock;
    }


    TEST_METHOD (ATopBarStartsAMarginInAndRunsAlongTheTop)
    {
        RECT  bar = DxuiToolbarHost::GetDockedRect (MakeDock (DxuiToolbarDock::Edge::Top, 50), kArea, 300, kBand, kMargin, 96);


        Assert::AreEqual (58L,  bar.left);
        Assert::AreEqual (100L, bar.top,    L"the area's top, not the window's");
        Assert::AreEqual (358L, bar.right);
        Assert::AreEqual (140L, bar.bottom);
    }


    TEST_METHOD (ABottomBarPastTheEndIsClampedToFit)
    {
        RECT  bar = DxuiToolbarHost::GetDockedRect (MakeDock (DxuiToolbarDock::Edge::Bottom, 900), kArea, 300, kBand, kMargin, 96);


        Assert::AreEqual (692L, bar.left, L"the far end stops a margin short of the area's right");
        Assert::AreEqual (760L, bar.top);
        Assert::AreEqual (992L, bar.right);
        Assert::AreEqual (800L, bar.bottom);
    }


    TEST_METHOD (ASideBarLongerThanItsEdgeTakesTheWholeEdge)
    {
        RECT  bar = DxuiToolbarHost::GetDockedRect (MakeDock (DxuiToolbarDock::Edge::Left, 100), kArea, 900, kBand, kMargin, 96);


        Assert::AreEqual (0L,   bar.left);
        Assert::AreEqual (100L, bar.top);
        Assert::AreEqual (40L,  bar.right);
        Assert::AreEqual (800L, bar.bottom);
    }


    TEST_METHOD (TheOffsetScalesWithTheDpi)
    {
        RECT  bar = DxuiToolbarHost::GetDockedRect (MakeDock (DxuiToolbarDock::Edge::Right, 100), kArea, 200, kBand, kMargin, 192);


        Assert::AreEqual (960L,  bar.left);
        Assert::AreEqual (300L,  bar.top, L"100 DIPs at 200% is 200 pixels down from the area's top");
        Assert::AreEqual (1000L, bar.right);
        Assert::AreEqual (500L,  bar.bottom);
    }


    TEST_METHOD (EachEdgeSharesTheMatchingDockSiteSide)
    {
        Assert::IsTrue (DxuiToolbarHost::EdgeToDockSide (DxuiToolbarDock::Edge::Top)    == DxuiDockSide::Top);
        Assert::IsTrue (DxuiToolbarHost::EdgeToDockSide (DxuiToolbarDock::Edge::Bottom) == DxuiDockSide::Bottom);
        Assert::IsTrue (DxuiToolbarHost::EdgeToDockSide (DxuiToolbarDock::Edge::Left)   == DxuiDockSide::Left);
        Assert::IsTrue (DxuiToolbarHost::EdgeToDockSide (DxuiToolbarDock::Edge::Right)  == DxuiDockSide::Right);
    }


    //  The system suggests the old size scaled by the ratio of the scales,
    //  which rounds differently from the toolbar measured at the new one;
    //  the window takes the measured size at once, so no frame fits it again.
    TEST_METHOD (AFloatingWindowAtANewScaleTakesTheToolbarsSizeThereOnce)
    {
        RECT  rect = DxuiToolbarHost::GetDpiChangedRect (RECT { 1200, 300, 1651, 360 }, SIZE { 456, 60 });


        Assert::AreEqual (1200L, rect.left,   L"where the system suggests");
        Assert::AreEqual (300L,  rect.top);
        Assert::AreEqual (1656L, rect.right,  L"at the toolbar's own length at the new scale");
        Assert::AreEqual (360L,  rect.bottom);
    }


    TEST_METHOD (ResettingTheDockSavesTheDefaultPlace)
    {
        DxuiToolbarHost  host;
        std::wstring     saved;


        host.SetDock   (MakeDock (DxuiToolbarDock::Edge::Left, 120));
        host.SetOnSave ([&saved] (const std::wstring & text) { saved = text; });
        host.ResetDock();

        Assert::AreEqual (std::wstring (L"top 0"), saved);
        Assert::IsTrue   (host.GetDock() == DxuiToolbarDock {});
    }


    TEST_METHOD (WithNoToolbarTheHostTakesNoInput)
    {
        DxuiToolbarHost  host;
        DxuiMouseEvent   ev = {};


        ev.kind = DxuiMouseEventKind::Down;

        Assert::IsFalse (host.RouteDrag (ev));
        Assert::IsFalse (host.IsDragging());
        Assert::IsFalse (host.IsFloating());
    }
};
