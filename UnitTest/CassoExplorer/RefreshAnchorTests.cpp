#include "Pch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshAnchorTests
//
//  Where a view lands after its rows are re-read. Every case is a hundred rows
//  named f0 to f99, ten of them on screen, and a change to that list.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RefreshAnchorTests)
{
public:

    static constexpr int  kRows     = 100;
    static constexpr int  kCapacity = 10;



    static std::vector<std::wstring>  MakeKeys (int count)
    {
        std::vector<std::wstring>  keys;
        int                        i = 0;

        for (i = 0; i < count; i++)
        {
            keys.push_back (std::format (L"f{}", i));
        }

        return keys;
    }



    static std::vector<std::wstring>  Without (std::vector<std::wstring> keys, std::initializer_list<int> gone)
    {
        std::vector<std::wstring>  names;

        for (int index : gone)
        {
            names.push_back (std::format (L"f{}", index));
        }

        for (const std::wstring & name : names)
        {
            keys.erase (std::remove (keys.begin(), keys.end(), name), keys.end());
        }

        return keys;
    }



    static RefreshAnchor::Before  MakeView (int top, std::vector<int> selected, int focused)
    {
        RefreshAnchor::Before  before;

        before.keys     = MakeKeys (kRows);
        before.topRow   = top;
        before.capacity = kCapacity;
        before.selected = std::move (selected);
        before.focused  = focused;

        return before;
    }



    TEST_METHOD (NothingChanged_NothingMoves)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (50, { 55 }, 55), MakeKeys (kRows));

        Assert::AreEqual (50, after.topRow);
        Assert::AreEqual (55, after.focused);
        Assert::AreEqual ((size_t) 1, after.selected.size());
        Assert::AreEqual (55, after.selected[0]);
    }



    TEST_METHOD (ARowAddedAbove_TheFocusedItemKeepsItsScreenRow)
    {
        std::vector<std::wstring>  keys = MakeKeys (kRows);
        RefreshAnchor::After       after;

        keys.insert (keys.begin(), L"new");

        after = RefreshAnchor::Compute (MakeView (50, { 55 }, 55), keys);

        //  The file moved down one, and so did the view, so it is still the
        //  sixth row on screen.
        Assert::AreEqual (56, after.focused);
        Assert::AreEqual (51, after.topRow);
        Assert::AreEqual (after.focused - after.topRow, 5);
    }



    TEST_METHOD (ARowRemovedAbove_TheFocusedItemKeepsItsScreenRow)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (50, { 55 }, 55), Without (MakeKeys (kRows), { 0 }));

        Assert::AreEqual (54, after.focused);
        Assert::AreEqual (49, after.topRow);
    }



    TEST_METHOD (AFocusScrolledOffScreen_TheTopItemKeepsTheTopRow)
    {
        std::vector<std::wstring>  keys = MakeKeys (kRows);
        RefreshAnchor::After       after;

        keys.insert (keys.begin(), L"new");

        //  Selected f5, then scrolled away to f50: there is no screen row to
        //  keep, so the view stays on the item it was showing.
        after = RefreshAnchor::Compute (MakeView (50, { 5 }, 5), keys);

        Assert::AreEqual (51, after.topRow, L"f50 is still the top row");
        Assert::AreEqual (6,  after.focused, L"and the selection followed its file");
    }



    TEST_METHOD (AnUnreachableScreenRow_FallsBackRatherThanClamping)
    {
        //  f9 on the bottom screen row with the view at the top. Removing f0..f5
        //  moves f9 to index 3, which could only stay on the bottom row with a
        //  top row of -6.
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (0, { 9 }, 9), Without (MakeKeys (kRows), { 0, 1, 2, 3, 4, 5 }));

        Assert::AreEqual (3, after.focused);
        Assert::AreEqual (0, after.topRow, L"f6, the first survivor from the old top, is the top row");
    }



    TEST_METHOD (AtTheEnd_TheBottomItemKeepsTheBottomRow)
    {
        std::vector<std::wstring>  keys = MakeKeys (kRows);
        RefreshAnchor::After       after;

        keys.push_back (L"f100");

        //  At the end with nothing focused. A file appended past the end does
        //  not drag the view after it: f99 stays on the bottom row.
        after = RefreshAnchor::Compute (MakeView (90, {}, -1), keys);

        Assert::AreEqual (90, after.topRow);
    }



    TEST_METHOD (AtTheEnd_ADeletedBottomItemHandsTheRowToTheOneAboveIt)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (90, {}, -1), Without (MakeKeys (kRows), { 99 }));

        //  f98 is now the last row, and it takes the bottom screen row.
        Assert::AreEqual (89, after.topRow);
    }



    TEST_METHOD (ADeletedTopItemHandsTheRowToTheOneBelowIt)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (50, {}, -1), Without (MakeKeys (kRows), { 50 }));

        //  f51 moves up into index 50 and takes the top row.
        Assert::AreEqual (50, after.topRow);
    }



    TEST_METHOD (AListThatFits_IsNotAtAnEnd)
    {
        RefreshAnchor::Before      before;
        std::vector<std::wstring>  keys;
        RefreshAnchor::After       after;

        before.keys     = MakeKeys (5);
        before.topRow   = 0;
        before.capacity = kCapacity;

        keys = MakeKeys (6);

        after = RefreshAnchor::Compute (before, keys);

        Assert::AreEqual (0, after.topRow);
    }



    TEST_METHOD (ADeletedFocus_MovesToTheNearestSelectedItem)
    {
        //  f40, f45 and f60 selected, f45 focused and deleted. f40 is five rows
        //  away and f60 fifteen.
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (40, { 40, 45, 60 }, 45), Without (MakeKeys (kRows), { 45 }));

        Assert::AreEqual ((size_t) 2, after.selected.size());
        Assert::AreEqual (40, after.focused, L"The nearest survivor, not the first");
    }



    TEST_METHOD (ASelectionKeepsOnlyWhatSurvived)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (0, { 10, 20, 30 }, 10), Without (MakeKeys (kRows), { 20 }));

        Assert::AreEqual ((size_t) 2, after.selected.size());
        Assert::AreEqual (10, after.selected[0]);
        Assert::AreEqual (29, after.selected[1], L"f30 moved up one");
    }



    TEST_METHOD (EverythingDeleted_LeavesAnEmptyViewAtTheTop)
    {
        RefreshAnchor::After  after = RefreshAnchor::Compute (MakeView (50, { 55 }, 55), {});

        Assert::AreEqual (0,  after.topRow);
        Assert::AreEqual (-1, after.focused);
        Assert::IsTrue   (after.selected.empty());
    }
};
