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


    //  While a find has matches, the scrollbar has a mark for each row with
    //  one, the selected match's as the current one; closing the find
    //  clears them.
    TEST_METHOD (EachRowWithAMatchIsMarkedOnTheScrollbar)
    {
        DxuiListView  list;
        int           index = 0;
        int           count = 0;

        ConfigureList (list);

        (void) list.SelectMatch (L"step", false, false, false, true, index, count);

        Assert::AreEqual ((size_t) 2, list.GetScrollMarks().size(), L"rows 0 and 2");
        Assert::AreEqual (0.5f / 3.0f, list.GetScrollMarks()[0].at, 0.001f);
        Assert::AreEqual (2.5f / 3.0f, list.GetScrollMarks()[1].at, 0.001f);
        Assert::IsTrue   (list.GetScrollMarks()[0].isCurrent, L"the selected match");
        Assert::IsFalse  (list.GetScrollMarks()[1].isCurrent);

        list.ClearFindHighlights();

        Assert::IsTrue (list.GetScrollMarks().empty(), L"no marks once the find closes");
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
        constexpr uint32_t                           kDarkBack = 0xFF1A2333u;
        DxuiListView                                 list;
        int                                          index     = 0;
        int                                          count     = 0;
        std::vector<std::tuple<int, int, uint32_t>>  first;
        std::vector<std::tuple<int, int, uint32_t>>  other;

        ConfigureList (list);
        list.SetFindColors (0xFF4A3A10, 0xFFF0C000);

        Assert::IsTrue (DxuiFindResult::Found == list.SelectMatchInView (L"step", false, false, false, index, count));

        first = list.GetMatchBands (0, 1, list.GetCellsOfRow (0)[1], 0, kDarkBack);
        other = list.GetMatchBands (2, 1, list.GetCellsOfRow (2)[1], 0, kDarkBack);

        Assert::AreEqual ((size_t) 1, first.size());
        Assert::AreNotEqual (std::get<2> (first[0]), std::get<2> (other[0]), L"the selected match's fill differs from the others'");

        //  Each fill is opaque with a contrast of 3:1 against the background, and the
        //  ink chosen for it reads at 4.5:1.
        for (uint32_t fill : { std::get<2> (first[0]), std::get<2> (other[0]) })
        {
            uint32_t  ink = DxuiColor::ChooseInkFor (fill, 0xFF101010u, 0xFFF8F8F8u);

            Assert::AreEqual (0xFF000000u, fill & 0xFF000000u, L"opaque");
            Assert::IsTrue   (DxuiColor::ComputeContrastRatio (fill, kDarkBack) >= 3.0f, L"apart from the background");
            Assert::IsTrue   (DxuiColor::ComputeContrastRatio (ink,  fill)      >= 4.5f, L"the matched text reads on it");
        }

        list.ClearFindHighlights();
        Assert::IsTrue (list.GetMatchBands (0, 1, list.GetCellsOfRow (0)[1], 0, kDarkBack).empty());
    }

    //  A column marked not searched has no matches; and with matches marked
    //  for a host's find, the selected row's match is drawn as current.
    TEST_METHOD (AnUnsearchedColumnAndAHostsSelectedMatch)
    {
        constexpr uint32_t  kDarkBack = 0xFF1A2333u;
        DxuiListView        list;
        int                 index     = 0;
        int                 count     = 0;
        uint32_t            selected  = 0;
        uint32_t            other     = 0;

        ConfigureList (list);
        list.SetFindColors (0xFF4A3A10, 0xFFF0C000);

        list.SetColumnSearched (2, false);
        Assert::IsTrue (DxuiFindResult::NotFound == list.SelectMatch (L"track", false, false, false, true, index, count), L"the Detail column");

        list.SetSelectedRow (2);
        list.HighlightFindMatches (L"step", false, false, false);

        selected = std::get<2> (list.GetMatchBands (2, 1, list.GetCellsOfRow (2)[1], 0, kDarkBack).at (0));
        other    = std::get<2> (list.GetMatchBands (0, 1, list.GetCellsOfRow (0)[1], 0, kDarkBack).at (0));

        Assert::AreNotEqual (selected, other, L"the selected row's match is the current one");
        Assert::AreEqual    (2, list.GetSelectedRow(), L"the row the host selected");
    }
};