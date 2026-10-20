#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRunsTests
//
//  A message box's text split into lines and its `**` runs: the marks are
//  dropped, a run never crosses a line, and a line with no marks is a single
//  regular run.
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
};
