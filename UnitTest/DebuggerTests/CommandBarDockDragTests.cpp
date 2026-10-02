#include "Pch.h"

#include "Ui/Debugger/CommandBarDock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDockDragTests
//
//  A drag of the docked bar: it slides along the band it is in, near
//  another edge or not, and leaves only past a pull; a floating bar takes
//  the orientation of the last band it came near.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandBarDockDragTests)
{
public:

    static constexpr RECT  kClient = { 0, 0, 1000, 800 };
    static constexpr int   kBand   = 50;
    static constexpr int   kPull   = 40;


    TEST_METHOD (ADragAlongTheTopKeepsTheTopNearACorner)
    {
        CommandBarDock  start;
        CommandBarDock  slid;


        start.edge = CommandBarDock::Edge::Top;
        slid       = CommandBarDock::SlideAlong (POINT { 8, 30 }, POINT { 0, 0 }, start, kClient, 96);

        Assert::IsTrue   (slid.edge == CommandBarDock::Edge::Top, L"the left edge is nearer, but the band the bar is in wins");
        Assert::AreEqual (8, slid.offsetDip);
        Assert::IsFalse  (slid.floating);
    }


    TEST_METHOD (ADragDownASideRunsDownIt)
    {
        CommandBarDock  start;
        CommandBarDock  slid;


        start.edge = CommandBarDock::Edge::Right;
        slid       = CommandBarDock::SlideAlong (POINT { 970, 790 }, POINT { 0, 30 }, start, kClient, 96);

        Assert::IsTrue   (slid.edge == CommandBarDock::Edge::Right, L"the bottom edge is nearer, but the right wins");
        Assert::AreEqual (760, slid.offsetDip);
    }


    TEST_METHOD (TheBarStaysDockedUntilPulledPastTheThreshold)
    {
        Assert::IsFalse (CommandBarDock::IsPulledOut (POINT { 500, kBand + kPull     }, CommandBarDock::Edge::Top, kClient, kBand, kPull));
        Assert::IsTrue  (CommandBarDock::IsPulledOut (POINT { 500, kBand + kPull + 1 }, CommandBarDock::Edge::Top, kClient, kBand, kPull));
        Assert::IsFalse (CommandBarDock::IsPulledOut (POINT { 960, 400 }, CommandBarDock::Edge::Right, kClient, kBand, kPull));
        Assert::IsTrue  (CommandBarDock::IsPulledOut (POINT { 900, 400 }, CommandBarDock::Edge::Right, kClient, kBand, kPull));
    }


    TEST_METHOD (ThePullCountsOutwardAndOffTheEndsToo)
    {
        Assert::IsFalse (CommandBarDock::IsPulledOut (POINT { 500, -kPull     }, CommandBarDock::Edge::Top,  kClient, kBand, kPull), L"a little past the edge is still in the band");
        Assert::IsTrue  (CommandBarDock::IsPulledOut (POINT { 500, -kPull - 1 }, CommandBarDock::Edge::Top,  kClient, kBand, kPull));
        Assert::IsTrue  (CommandBarDock::IsPulledOut (POINT { 1000 + kPull + 1, 10 }, CommandBarDock::Edge::Top, kClient, kBand, kPull));
        Assert::IsFalse (CommandBarDock::IsPulledOut (POINT { 10, 800 + kPull }, CommandBarDock::Edge::Left, kClient, kBand, kPull));
    }


    TEST_METHOD (AFloatingBarTurnsVerticalNearASideAndStaysSo)
    {
        Assert::IsTrue  (CommandBarDock::PickFloatVertical (POINT {  20, 400 }, kClient, kBand, false), L"near the left edge");
        Assert::IsTrue  (CommandBarDock::PickFloatVertical (POINT { 500, 400 }, kClient, kBand, true),  L"away from every edge it keeps its orientation");
        Assert::IsFalse (CommandBarDock::PickFloatVertical (POINT { 500, 400 }, kClient, kBand, false));
        Assert::IsFalse (CommandBarDock::PickFloatVertical (POINT { 500, 780 }, kClient, kBand, true),  L"near the bottom it turns back");
        Assert::IsTrue  (CommandBarDock::PickFloatVertical (POINT { 1200, 400 }, kClient, kBand, true), L"off the window it keeps its orientation");
    }


    TEST_METHOD (AFloatingBarsOrientationRoundTripsThroughText)
    {
        CommandBarDock  dock;
        CommandBarDock  back;


        dock.floating      = true;
        dock.floatPx       = POINT { 300, -40 };
        dock.floatVertical = true;

        back = CommandBarDock::FromText (dock.ToText());

        Assert::IsTrue (back.floating && back.floatVertical);
        Assert::AreEqual (300L, back.floatPx.x);
        Assert::IsFalse (CommandBarDock::FromText (L"float 300 -40").floatVertical, L"text saved before the orientation reads back horizontal");
    }
};
