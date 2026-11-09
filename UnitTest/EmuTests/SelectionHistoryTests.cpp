#include "Pch.h"

#include "Ui/DiskInspector/SelectionHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionHistoryTests
//
//  Back and Forward through the user's selections (FR-164).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SelectionHistoryTests)
{
public:

    static InspectorSelection MakeSector (int quarterTrack, int sectorIndex)
    {
        InspectorSelection  selection;



        selection.quarterTrack = quarterTrack;
        selection.sectorIndex  = sectorIndex;
        return selection;
    }



    TEST_METHOD (BackAndForwardStepThroughTheRecordedSelections)
    {
        SelectionHistory    history;
        InspectorSelection  selection;



        history.Record (MakeSector (0, 0));
        history.Record (MakeSector (0, 5));
        history.Record (MakeSector (4, 2));

        Assert::IsTrue  (history.GoBack (selection));
        Assert::IsTrue  (selection == MakeSector (0, 5));
        Assert::IsTrue  (history.GoBack (selection));
        Assert::IsTrue  (selection == MakeSector (0, 0));
        Assert::IsFalse (history.GoBack (selection), L"nothing before the first");
        Assert::IsTrue  (history.GoForward (selection));
        Assert::IsTrue  (selection == MakeSector (0, 5));
        Assert::IsTrue  (history.GoForward (selection));
        Assert::IsTrue  (selection == MakeSector (4, 2));
        Assert::IsFalse (history.GoForward (selection), L"nothing after the last");
    }



    TEST_METHOD (ASelectionAfterAStepBackReplacesTheOnesAhead)
    {
        SelectionHistory    history;
        InspectorSelection  selection;



        history.Record (MakeSector (0, 0));
        history.Record (MakeSector (0, 1));
        history.Record (MakeSector (0, 2));
        Assert::IsTrue (history.GoBack (selection));
        history.Record (MakeSector (8, 3));

        Assert::IsFalse (history.CanGoForward());
        Assert::IsTrue  (history.GoBack (selection));
        Assert::IsTrue  (selection == MakeSector (0, 1));
    }



    TEST_METHOD (TheSameSelectionTwiceIsRecordedOnce)
    {
        SelectionHistory    history;
        InspectorSelection  selection;



        history.Record (MakeSector (0, 0));
        history.Record (MakeSector (0, 3));
        history.Record (MakeSector (0, 3));

        Assert::IsTrue  (history.GoBack (selection));
        Assert::IsTrue  (selection == MakeSector (0, 0));
        Assert::IsFalse (history.CanGoBack());
    }



    TEST_METHOD (TheOldestSelectionIsDroppedPastTheLimit)
    {
        SelectionHistory    history;
        InspectorSelection  selection;
        size_t              steps = 0;



        for (int i = 0; i < static_cast<int> (SelectionHistory::kMaxEntries) + 5; i++)
        {
            history.Record (MakeSector (i, 0));
        }

        while (history.GoBack (selection))
        {
            steps++;
        }

        Assert::AreEqual (SelectionHistory::kMaxEntries - 1, steps);
        Assert::AreEqual (5, selection.quarterTrack, L"the five oldest are gone");
    }
};
