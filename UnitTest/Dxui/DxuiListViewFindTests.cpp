#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewFindTests
//
//  Find over a list's visible columns, as the event logs have it: every match
//  in row, column and character order, the next and the one before going
//  round, the count, and a fill for the selected match apart from the
//  others. The matcher takes case, whole words and regular expressions.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewFindTests)
{
public:

    static void  ConfigureList (DxuiListView & list)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);

        list.SetColumns ({ { L"Cycle", 0, false, DxuiTextHAlign::Right }, { L"Event", 0, false, DxuiTextHAlign::Left }, { L"Detail", 0, true, DxuiTextHAlign::Left } });
        list.SetRows    ({ { { L"100" }, { L"Head step" }, { L"track 1 -> 2" } },
                           { { L"200" }, { L"Address"   }, { L"T1 S2 V254"   } },
                           { { L"300" }, { L"Head step" }, { L"track 2 -> 3" } } });
        list.Layout     (RECT { 0, 0, 600, 300 }, scaler);
    }


    TEST_METHOD (TheMatcherTakesCaseWordsAndExpressions)
    {
        using Ranges = std::vector<std::pair<int, int>>;

        Assert::IsTrue (DxuiTextFind::FindAll (L"Head step HEAD", L"head", false, false, false) == Ranges { { 0, 4 }, { 10, 14 } });
        Assert::IsTrue (DxuiTextFind::FindAll (L"Head step HEAD", L"head", true,  false, false).empty(), L"in the needle's case");
        Assert::IsTrue (DxuiTextFind::FindAll (L"track tracks",   L"track", false, true, false) == Ranges { { 0, 5 } }, L"a whole word");
        Assert::IsTrue (DxuiTextFind::FindAll (L"T1 S22 V254",    L"S\\d+", false, false, true) == Ranges { { 3, 6 } });
        Assert::IsTrue (DxuiTextFind::FindAll (L"T1",             L"(",     false, false, true).empty(), L"not a valid expression");
    }


    TEST_METHOD (FindGoesFromMatchToMatchRoundTheEnd)
    {
        DxuiListView  list;
        int           index = 0;
        int           count = 0;

        ConfigureList (list);

        Assert::IsTrue   (DxuiFindResult::Found == list.SelectMatch (L"step", false, false, false, true, index, count));
        Assert::AreEqual (1, index);
        Assert::AreEqual (2, count);
        Assert::AreEqual (0, list.GetSelectedRow());

        Assert::IsTrue   (DxuiFindResult::Found == list.SelectMatch (L"step", false, false, false, true, index, count));
        Assert::AreEqual (2, index);
        Assert::AreEqual (2, list.GetSelectedRow());

        Assert::IsTrue   (DxuiFindResult::Wrapped == list.SelectMatch (L"step", false, false, false, true, index, count), L"round to the first");
        Assert::AreEqual (1, index);

        Assert::IsTrue   (DxuiFindResult::Wrapped == list.SelectMatch (L"step", false, false, false, false, index, count), L"back round to the last");
        Assert::AreEqual (2, index);

        Assert::IsTrue   (DxuiFindResult::NotFound != list.SelectMatch (L"200", false, false, false, true, index, count), L"a right-aligned column");
        Assert::AreEqual (1, list.GetSelectedRow());

        list.SetColumnVisible (0, false);
        Assert::IsTrue   (DxuiFindResult::NotFound == list.SelectMatch (L"200", false, false, false, true, index, count), L"a hidden column is not searched");
        Assert::AreEqual (0, count);
    }


    TEST_METHOD (TheSelectedMatchHasAFillApartFromTheOthers)
    {
        DxuiListView                                  list;
        int                                           index   = 0;
        int                                           count   = 0;
        std::vector<std::tuple<int, int, uint32_t>>   first;
        std::vector<std::tuple<int, int, uint32_t>>   other;

        ConfigureList (list);
        list.SetFindColors (0x40FFFF00, 0xFF0080FF);

        Assert::IsTrue (DxuiFindResult::Found == list.SelectMatchInView (L"step", false, false, false, index, count));

        first = list.GetMatchBands (0, 1, list.GetCellsOfRow (0)[1], 0);
        other = list.GetMatchBands (2, 1, list.GetCellsOfRow (2)[1], 0);

        Assert::AreEqual ((size_t) 1, first.size());
        Assert::AreEqual (0xC00080FFu, std::get<2> (first[0]), L"the selected match");
        Assert::AreEqual (0x40FFFF00u, std::get<2> (other[0]), L"another match");

        list.ClearFindHighlights();
        Assert::IsTrue (list.GetMatchBands (0, 1, list.GetCellsOfRow (0)[1], 0).empty());
    }
};