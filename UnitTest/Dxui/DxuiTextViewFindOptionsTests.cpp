#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewFindOptionsTests
//
//  The find bar's search: plain substrings with no option on, whole words,
//  case, regular expressions, and the match's place among all of them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewFindOptionsTests)
{
public:
    static DxuiTextView::Row MakeRow (const std::wstring & text)
    {
        DxuiTextView::Row  row;



        row.cells.push_back (text);
        return row;
    }



    TEST_METHOD (NoOption_FindsEveryPlainSubstringIgnoringCase)
    {
        std::vector<DxuiTextView::FindMatch>  matches;



        Assert::IsTrue   (DxuiTextView::FindAllInRows ({ L"STA $C030", L"sta.x", L"NOP" }, L"st", false, false, false, matches));
        Assert::AreEqual ((size_t) 2, matches.size());
        Assert::AreEqual (1, matches[1].start.row);
    }



    TEST_METHOD (Regex_FindsMatchesOfTheirOwnLength)
    {
        std::vector<DxuiTextView::FindMatch>  matches;



        Assert::IsTrue   (DxuiTextView::FindAllInRows ({ L"STA $C030", L"LDA $0400" }, L"\\$[0-9A-F]+", true, false, true, matches));
        Assert::AreEqual ((size_t) 2, matches.size());
        Assert::AreEqual (4, matches[0].start.offset);
        Assert::AreEqual (5, matches[0].length, L"$C030 is five characters");
    }



    TEST_METHOD (Regex_NotMatchingCase_IgnoresCase)
    {
        std::vector<DxuiTextView::FindMatch>  matches;



        Assert::IsTrue   (DxuiTextView::FindAllInRows ({ L"STA", L"sta" }, L"^st.$", false, false, true, matches));
        Assert::AreEqual ((size_t) 2, matches.size());
    }



    TEST_METHOD (Regex_DotIsPlainWithTheOptionOff)
    {
        std::vector<DxuiTextView::FindMatch>  matches;



        Assert::IsTrue   (DxuiTextView::FindAllInRows ({ L"STA", L"S.A" }, L"s.a", false, false, false, matches));
        Assert::AreEqual ((size_t) 1, matches.size());
        Assert::AreEqual (1, matches[0].start.row);
    }



    TEST_METHOD (Regex_NotValid_MatchesNothing)
    {
        std::vector<DxuiTextView::FindMatch>  matches;



        Assert::IsFalse  (DxuiTextView::FindAllInRows ({ L"STA (" }, L"(", false, false, true, matches));
        Assert::IsTrue   (matches.empty());
    }



    TEST_METHOD (SelectMatch_GivesTheMatchsPlaceAmongAll)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows    = { MakeRow (L"STA $C030"), MakeRow (L"NOP"), MakeRow (L"STA $0400") };
        int                             index   = 0;
        int                             count   = 0;



        view.SetRows (std::move (rows));

        Assert::IsTrue   (view.SelectMatch (L"\\$[0-9A-F]+", true, false, true, true, index, count) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"$C030"), view.GetSelectionText());
        Assert::AreEqual (1, index);
        Assert::AreEqual (2, count);

        Assert::IsTrue   (view.SelectMatch (L"\\$[0-9A-F]+", true, false, true, true, index, count) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"$0400"), view.GetSelectionText());
        Assert::AreEqual (2, index);

        Assert::IsTrue   (view.SelectMatch (L"\\$[0-9A-F]+", true, false, true, true, index, count) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (1, index, L"round to the first");

        Assert::IsTrue   (view.SelectMatch (L"\\$[0-9A-F]+", true, false, true, false, index, count) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (2, index, L"back round to the last");
    }
};