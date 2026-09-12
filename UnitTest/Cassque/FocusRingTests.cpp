#include "Pch.h"

#include "Cassque/Model/FocusRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FocusRingTests
//
//  The order Tab walks Cassque's window in: the toolbar's usable buttons,
//  the tab strip, the tree, the list, the preview when it is shown -- and
//  where a walk goes when it runs off either end or starts from a stop that
//  has since dropped out.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FocusRingTests)
{
public:

    using Kind = FocusStop::Kind;

    static FocusStop  Button (int entry)  { return FocusStop { Kind::ToolbarEntry, entry }; }
    static FocusStop  Pane   (Kind kind)  { return FocusStop { kind }; }


    TEST_METHOD (Order_ButtonsThenTabsTreeListPreview)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ true, true, true }, true);


        Assert::AreEqual ((size_t) 7, stops.size());
        Assert::IsTrue (stops[0] == Button (0), L"The walk starts at the leftmost toolbar button");
        Assert::IsTrue (stops[2] == Button (2), L"and crosses the toolbar left to right");
        Assert::IsTrue (stops[3] == Pane (Kind::Tabs),    L"then the tab strip");
        Assert::IsTrue (stops[4] == Pane (Kind::Tree),    L"the folder tree");
        Assert::IsTrue (stops[5] == Pane (Kind::List),    L"the file list");
        Assert::IsTrue (stops[6] == Pane (Kind::Preview), L"and the preview, as Explorer reads");
    }


    TEST_METHOD (ButtonsThatCannotBeUsedAreSkipped)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ false, false, true, true }, true);


        Assert::IsTrue (stops[0] == Button (2),
            L"With Back and Forward unavailable the walk starts at Up");
        Assert::AreEqual ((size_t) 6, stops.size());
    }


    TEST_METHOD (HiddenPreviewIsLeftOut)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ true }, false);


        Assert::IsTrue (stops.back() == Pane (Kind::List),
            L"A hidden preview is no stop at all, so the list is the last");
    }


    TEST_METHOD (Tab_WrapsFromTheLastStopToTheFirst)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ true, true }, true);


        Assert::IsTrue (FocusRing::GetNext (stops, Pane (Kind::Preview), true) == Button (0),
            L"Tab past the preview comes back round to the first button");
        Assert::IsTrue (FocusRing::GetNext (stops, Pane (Kind::Tree), true) == Pane (Kind::List));
    }


    TEST_METHOD (ShiftTab_WrapsFromTheFirstStopToTheLast)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ true, true }, true);


        Assert::IsTrue (FocusRing::GetNext (stops, Button (0), false) == Pane (Kind::Preview),
            L"Shift+Tab before the first button goes round to the preview");
        Assert::IsTrue (FocusRing::GetNext (stops, Pane (Kind::Tabs), false) == Button (1),
            L"and back from the tab strip reaches the last button");
    }


    TEST_METHOD (AStopThatDroppedOut_StartsTheWalkFromAnEnd)
    {
        std::vector<FocusStop>  stops = FocusRing::BuildStops ({ false, true }, true);


        Assert::IsTrue (FocusRing::GetNext (stops, Button (0), true) == Button (1),
            L"Focus on a button that has just become unusable walks forward from the front");
        Assert::IsTrue (FocusRing::GetNext (stops, Button (0), false) == Pane (Kind::Preview),
            L"and backward from the back");
    }


    TEST_METHOD (NoStops_LeavesFocusWhereItIs)
    {
        std::vector<FocusStop>  none;


        Assert::IsTrue (FocusRing::GetNext (none, Pane (Kind::List), true) == Pane (Kind::List));
    }
};
