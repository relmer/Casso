#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarDockTests
//
//  The command bar's place: which edge a drop picks, the offset it keeps,
//  and the text it is saved as.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandBarDockTests)
{
public:

    static constexpr RECT  kClient = { 0, 0, 1000, 800 };


    TEST_METHOD (ADropPicksTheNearestEdge)
    {
        Assert::IsTrue (DxuiToolbarDock::PickForDrop (POINT { 500,  10 }, POINT {}, kClient, 96).edge == DxuiToolbarDock::Edge::Top);
        Assert::IsTrue (DxuiToolbarDock::PickForDrop (POINT { 500, 790 }, POINT {}, kClient, 96).edge == DxuiToolbarDock::Edge::Bottom);
        Assert::IsTrue (DxuiToolbarDock::PickForDrop (POINT {  10, 400 }, POINT {}, kClient, 96).edge == DxuiToolbarDock::Edge::Left);
        Assert::IsTrue (DxuiToolbarDock::PickForDrop (POINT { 990, 400 }, POINT {}, kClient, 96).edge == DxuiToolbarDock::Edge::Right);
    }


    TEST_METHOD (ADropKeepsTheBarWhereThePointerGrabbedIt)
    {
        DxuiToolbarDock  top  = DxuiToolbarDock::PickForDrop (POINT { 300,  10 }, POINT { 6, 20 }, kClient, 96);
        DxuiToolbarDock  side = DxuiToolbarDock::PickForDrop (POINT {   5, 300 }, POINT { 6, 20 }, kClient, 96);


        Assert::AreEqual (294, top.offsetDip,  L"along the top, the offset runs across");
        Assert::AreEqual (280, side.offsetDip, L"down a side, it runs down");
    }


    TEST_METHOD (TheTopEdgeIsTheAreasNotTheWindows)
    {
        DxuiToolbarDock  dock = DxuiToolbarDock::PickForDrop (POINT { 20, 112 }, POINT {}, RECT { 0, 100, 1000, 800 }, 96);


        Assert::IsTrue   (dock.edge == DxuiToolbarDock::Edge::Top, L"just under the menu bar is the top edge");
        Assert::AreEqual (20, dock.offsetDip);
    }


    TEST_METHOD (TheOffsetIsSavedInDips)
    {
        Assert::AreEqual (200, DxuiToolbarDock::PickForDrop (POINT { 400, 10 }, POINT {}, kClient, 192).offsetDip);
    }


    TEST_METHOD (TheOffsetIsNeverNegative)
    {
        Assert::AreEqual (0, DxuiToolbarDock::PickForDrop (POINT { 100, 3 }, POINT { 130, 0 }, kClient, 96).offsetDip);
    }


    TEST_METHOD (TheOffsetClampsSoTheBarFits)
    {
        Assert::AreEqual (600, DxuiToolbarDock::ClampOffset (900, 1000, 400));
        Assert::AreEqual (0,   DxuiToolbarDock::ClampOffset (50,  300,  400), L"a bar longer than its edge starts at it");
        Assert::AreEqual (50,  DxuiToolbarDock::ClampOffset (50,  1000, 400));
    }


    TEST_METHOD (ThePlaceRoundTripsThroughText)
    {
        DxuiToolbarDock  dock;


        dock.edge      = DxuiToolbarDock::Edge::Right;
        dock.offsetDip = 120;

        Assert::AreEqual (std::wstring (L"right 120"), dock.ToText());
        Assert::IsTrue   (DxuiToolbarDock::FromText (dock.ToText()) == dock);
    }


    TEST_METHOD (TextThatDoesNotReadGivesTheTop)
    {
        for (const wchar_t * text : { L"", L"middle 4", L"left", L"left x", L"left -3", L"left 4x" })
        {
            Assert::IsTrue (DxuiToolbarDock::FromText (text) == DxuiToolbarDock {}, text);
        }
    }
};
