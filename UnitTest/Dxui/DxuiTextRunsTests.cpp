#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRunsTests
//
//  A message box's text split into lines and its `**` runs: the marks are
//  dropped, a run never crosses a line, and a line with no marks is a single
//  regular run. Then the runs wrapped a word at a time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextRunsTests)
{
public:

    TEST_METHOD (PlainLines_AreOneRegularRunEach)
    {
        std::vector<std::vector<DxuiTextRun>>  lines;

        DxuiParseTextRuns (L"First line\n\nThird line", lines);

        Assert::AreEqual ((size_t) 3, lines.size());
        Assert::AreEqual ((size_t) 1, lines[0].size());
        Assert::AreEqual (std::wstring (L"First line"), lines[0][0].text);
        Assert::IsFalse  (lines[0][0].semiBold);
        Assert::IsTrue   (lines[1].empty(), L"an empty line has no runs");
        Assert::AreEqual (std::wstring (L"Third line"), lines[2][0].text);
    }


    TEST_METHOD (Marks_SplitALineIntoRuns)
    {
        std::vector<std::vector<DxuiTextRun>>  lines;

        DxuiParseTextRuns (L"1. **IN#2**, then press Return", lines);

        Assert::AreEqual ((size_t) 1, lines.size());
        Assert::AreEqual ((size_t) 3, lines[0].size());
        Assert::AreEqual (std::wstring (L"1. "), lines[0][0].text);
        Assert::IsFalse  (lines[0][0].semiBold);
        Assert::AreEqual (std::wstring (L"IN#2"), lines[0][1].text);
        Assert::IsTrue   (lines[0][1].semiBold);
        Assert::AreEqual (std::wstring (L", then press Return"), lines[0][2].text);
        Assert::IsFalse  (lines[0][2].semiBold);
    }


    TEST_METHOD (UnpairedMark_SetsTheRestOfItsLineSemiBold)
    {
        std::vector<std::vector<DxuiTextRun>>  lines;

        DxuiParseTextRuns (L"Type **14B\nThen wait", lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual ((size_t) 2, lines[0].size());
        Assert::IsTrue   (lines[0][1].semiBold);
        Assert::AreEqual (std::wstring (L"14B"), lines[0][1].text);
        Assert::IsFalse  (lines[1][0].semiBold, L"the next line starts regular");
    }


    TEST_METHOD (ALongLine_WrapsAtTheWordThatDoesntFit)
    {
        std::vector<DxuiPlacedTextRun>  placed;
        int                             rows = LayOut (L"aaa bbb ccc", 75.0f, placed);

        Assert::AreEqual (2, rows);
        Assert::AreEqual ((size_t) 3, placed.size());
        Assert::AreEqual (0,     placed[1].row);
        Assert::AreEqual (40.0f, placed[1].x);
        Assert::AreEqual (std::wstring (L"ccc"), placed[2].text);
        Assert::AreEqual (1,     placed[2].row);
        Assert::AreEqual (0.0f,  placed[2].x);
    }


    TEST_METHOD (ASemiBoldWord_WrapsWithThePunctuationAfterIt)
    {
        std::vector<DxuiPlacedTextRun>  placed;
        int                             rows = LayOut (L"1. **IN#2**, then", 60.0f, placed);

        Assert::AreEqual (3, rows);
        Assert::AreEqual ((size_t) 4, placed.size());
        Assert::AreEqual (std::wstring (L"IN#2"), placed[1].text);
        Assert::IsTrue   (placed[1].semiBold);
        Assert::AreEqual (1,     placed[1].row);
        Assert::AreEqual (std::wstring (L", "), placed[2].text);
        Assert::IsFalse  (placed[2].semiBold);
        Assert::AreEqual (1,     placed[2].row, L"the comma stays with IN#2");
        Assert::AreEqual (40.0f, placed[2].x);
    }


    TEST_METHOD (AWord_FitsOnARow_WithoutItsTrailingSpaces)
    {
        std::vector<DxuiPlacedTextRun>  placed;
        int                             rows = LayOut (L"aaaa bbbb  ", 90.0f, placed);

        Assert::AreEqual (1, rows);
        Assert::AreEqual (50.0f, placed[1].x);
    }


    TEST_METHOD (EachLine_StartsARow_AndAnEmptyLineTakesOne)
    {
        std::vector<DxuiPlacedTextRun>  placed;
        int                             rows = LayOut (L"**a**\n\nb", 100.0f, placed);

        Assert::AreEqual (3, rows);
        Assert::AreEqual ((size_t) 2, placed.size());
        Assert::AreEqual (2,    placed[1].row);
        Assert::AreEqual (0.0f, placed[1].x);
    }


private:
    //  The text parsed and laid out, with each character 10 pixels wide.
    static int  LayOut (const wchar_t * text, float widthPx, std::vector<DxuiPlacedTextRun> & outPlaced)
    {
        std::vector<std::vector<DxuiTextRun>>  lines;

        DxuiParseTextRuns (text, lines);

        return DxuiLayOutTextRuns (lines, widthPx, [] (const std::wstring & piece, bool) { return 10.0f * (float) piece.size(); }, outPlaced);
    }
};
