#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TabDragModelTests
//
//  The geometry of a dragged tab: which slot the gap opens at for a pointer,
//  where every other tab goes around it, and how far off the strip a tab is
//  torn off.
//
//  Widths are uneven on purpose, so a model that assumes equal tabs picks the
//  wrong slot. The gap follows the dragged tab's center against the other
//  tabs' centers with the row closed up, so it depends on the pointer alone
//  and cannot flip between two slots as the tabs slide.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TabDragModelTests)
{
public:

    TEST_METHOD (GapIndex_FollowsTheDraggedTabsCenter)
    {
        std::vector<int>  widths = { 80, 80, 80 };

        Assert::AreEqual (0, DxuiTabDragModel::GetGapIndex (widths, 0, 0, -100, 80), L"Past the first tab's center to the left, the gap is first");
        Assert::AreEqual (0, DxuiTabDragModel::GetGapIndex (widths, 0, 0, -1,   80), L"center 39, just short of Hardware's, closed up at 40");
        Assert::AreEqual (1, DxuiTabDragModel::GetGapIndex (widths, 0, 0, 50,   80), L"past it, the gap is second");
        Assert::AreEqual (2, DxuiTabDragModel::GetGapIndex (widths, 0, 0, 500,  80), L"and past every tab, it is last");
    }


    TEST_METHOD (GapIndex_MeasuresTheOtherTabsClosedUp)
    {
        // Dragging the wide middle tab: the others close up to 0-40 and 40-100,
        // centers 20 and 70, whatever the gap is doing.
        std::vector<int>  widths = { 40, 120, 60 };

        Assert::AreEqual (0, DxuiTabDragModel::GetGapIndex (widths, 1, 0, -50, 120), L"center 10, left of both");
        Assert::AreEqual (1, DxuiTabDragModel::GetGapIndex (widths, 1, 0, 0,   120), L"center 60, between them");
        Assert::AreEqual (2, DxuiTabDragModel::GetGapIndex (widths, 1, 0, 20,  120), L"center 80, right of both");
    }


    TEST_METHOD (GapIndex_CountsFromTheOrigin)
    {
        std::vector<int>  widths = { 80, 80 };

        Assert::AreEqual (1, DxuiTabDragModel::GetGapIndex (widths, -1, 0,  30, 80), L"A tab dragged in has no entry; center 70 is past the first tab's 40");
        Assert::AreEqual (0, DxuiTabDragModel::GetGapIndex (widths, -1, 50, 30, 80), L"with the row starting at 50, the first tab's center is 90");
    }


    TEST_METHOD (Lefts_OpenTheGapAsWideAsTheDraggedTab)
    {
        std::vector<int>  widths = { 80, 80, 80 };
        std::vector<int>  lefts;
        int               end    = 0;

        DxuiTabDragModel::GetLefts (widths, 0, 0, 1, 80, lefts, end);

        Assert::AreEqual (0,   lefts[1], L"Hardware closes up into the dragged tab's place");
        Assert::AreEqual (80,  lefts[0], L"the gap is where Hardware was");
        Assert::AreEqual (160, lefts[2], L"and Display waits past it");
        Assert::AreEqual (240, end,      L"The row ends past the gap");
    }


    TEST_METHOD (Lefts_GapAtTheEndOfTheRow)
    {
        std::vector<int>  widths = { 80, 80, 80 };
        std::vector<int>  lefts;
        int               end    = 0;

        DxuiTabDragModel::GetLefts (widths, 0, 0, 2, 80, lefts, end);

        Assert::AreEqual (0,   lefts[1]);
        Assert::AreEqual (80,  lefts[2]);
        Assert::AreEqual (160, lefts[0], L"The gap after the last tab");
        Assert::AreEqual (240, end,      L"pushes the + button along");
    }


    TEST_METHOD (Lefts_ClosedUpWhenTornOff)
    {
        std::vector<int>  widths = { 80, 80, 80 };
        std::vector<int>  lefts;
        int               end    = 0;

        DxuiTabDragModel::GetLefts (widths, 1, 10, -1, 80, lefts, end);

        Assert::AreEqual (10, lefts[0]);
        Assert::AreEqual (90, lefts[2], L"Display closes up into the torn-off tab's place");
        Assert::AreEqual (90, lefts[1], L"which is where the torn-off tab would land back");
        Assert::AreEqual (170, end,     L"and the + button follows the last tab left");
    }


    TEST_METHOD (Lefts_ATabDraggedInMovesEveryTabPastTheGap)
    {
        std::vector<int>  widths = { 80, 80 };
        std::vector<int>  lefts;
        int               end    = 0;

        DxuiTabDragModel::GetLefts (widths, -1, 0, 0, 100, lefts, end);

        Assert::AreEqual ((size_t) 2, lefts.size(), L"A tab dragged in has no entry of its own");
        Assert::AreEqual (100, lefts[0], L"The gap is the incoming tab's width");
        Assert::AreEqual (180, lefts[1]);
        Assert::AreEqual (260, end);
    }


    TEST_METHOD (TornOff_OnlyPastTheThresholdOffAnyEdge)
    {
        RECT  strip = { 0, 0, 300, 24 };

        Assert::IsFalse (DxuiTabDragModel::IsTornOff (strip, POINT { 100, 10 },  24), L"On the strip");
        Assert::IsFalse (DxuiTabDragModel::IsTornOff (strip, POINT { 100, 47 },  24), L"Below it, within the threshold");
        Assert::IsTrue  (DxuiTabDragModel::IsTornOff (strip, POINT { 100, 48 },  24), L"and past it");
        Assert::IsFalse (DxuiTabDragModel::IsTornOff (strip, POINT { 100, -24 }, 24), L"Above it, within the threshold");
        Assert::IsTrue  (DxuiTabDragModel::IsTornOff (strip, POINT { 100, -25 }, 24));
        Assert::IsFalse (DxuiTabDragModel::IsTornOff (strip, POINT { -24, 10 },  24), L"Off the left end, within the threshold");
        Assert::IsTrue  (DxuiTabDragModel::IsTornOff (strip, POINT { -25, 10 },  24));
        Assert::IsFalse (DxuiTabDragModel::IsTornOff (strip, POINT { 323, 10 },  24), L"Off the right end, within the threshold");
        Assert::IsTrue  (DxuiTabDragModel::IsTornOff (strip, POINT { 324, 10 },  24));
    }
};
