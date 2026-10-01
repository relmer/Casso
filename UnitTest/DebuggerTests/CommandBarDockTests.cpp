#include "Pch.h"

#include "Ui/Debugger/CommandBarDock.h"

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
        Assert::IsTrue (CommandBarDock::PickForDrop (POINT { 500,  10 }, POINT {}, kClient, 96).edge == CommandBarDock::Edge::Top);
        Assert::IsTrue (CommandBarDock::PickForDrop (POINT { 500, 790 }, POINT {}, kClient, 96).edge == CommandBarDock::Edge::Bottom);
        Assert::IsTrue (CommandBarDock::PickForDrop (POINT {  10, 400 }, POINT {}, kClient, 96).edge == CommandBarDock::Edge::Left);
        Assert::IsTrue (CommandBarDock::PickForDrop (POINT { 990, 400 }, POINT {}, kClient, 96).edge == CommandBarDock::Edge::Right);
    }


    TEST_METHOD (ADropKeepsTheBarWhereThePointerGrabbedIt)
    {
        CommandBarDock  top  = CommandBarDock::PickForDrop (POINT { 300,  10 }, POINT { 6, 20 }, kClient, 96);
        CommandBarDock  side = CommandBarDock::PickForDrop (POINT {   5, 300 }, POINT { 6, 20 }, kClient, 96);


        Assert::AreEqual (294, top.offsetDip,  L"along the top, the offset runs across");
        Assert::AreEqual (280, side.offsetDip, L"down a side, it runs down");
    }


    TEST_METHOD (TheTopEdgeIsTheAreasNotTheWindows)
    {
        CommandBarDock  dock = CommandBarDock::PickForDrop (POINT { 20, 112 }, POINT {}, RECT { 0, 100, 1000, 800 }, 96);


        Assert::IsTrue   (dock.edge == CommandBarDock::Edge::Top, L"just under the menu bar is the top edge");
        Assert::AreEqual (20, dock.offsetDip);
    }


    TEST_METHOD (TheOffsetIsSavedInDips)
    {
        Assert::AreEqual (200, CommandBarDock::PickForDrop (POINT { 400, 10 }, POINT {}, kClient, 192).offsetDip);
    }


    TEST_METHOD (TheOffsetIsNeverNegative)
    {
        Assert::AreEqual (0, CommandBarDock::PickForDrop (POINT { 100, 3 }, POINT { 130, 0 }, kClient, 96).offsetDip);
    }


    TEST_METHOD (TheOffsetClampsSoTheBarFits)
    {
        Assert::AreEqual (600, CommandBarDock::ClampOffset (900, 1000, 400));
        Assert::AreEqual (0,   CommandBarDock::ClampOffset (50,  300,  400), L"a bar longer than its edge starts at it");
        Assert::AreEqual (50,  CommandBarDock::ClampOffset (50,  1000, 400));
    }


    TEST_METHOD (ThePlaceRoundTripsThroughText)
    {
        CommandBarDock  dock;


        dock.edge      = CommandBarDock::Edge::Right;
        dock.offsetDip = 120;

        Assert::AreEqual (std::wstring (L"right 120"), dock.ToText());
        Assert::IsTrue   (CommandBarDock::FromText (dock.ToText()) == dock);
    }


    TEST_METHOD (TextThatDoesNotReadGivesTheTop)
    {
        for (const wchar_t * text : { L"", L"middle 4", L"left", L"left x", L"left -3", L"left 4x" })
        {
            Assert::IsTrue (CommandBarDock::FromText (text) == CommandBarDock {}, text);
        }
    }
};
