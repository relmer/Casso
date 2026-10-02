#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarCompactTests
//
//  A compact strip is a tool window's, as Visual Studio draws one inside a
//  pane: a 26 DIP band, 12 DIP icons in buttons 4 DIPs wider each side, 2
//  DIPs of air above and below, and 1 DIP between neighbors.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarCompactTests)
{
public:

    static std::vector<DxuiToolbar::Entry> MakeEntries (int count)
    {
        std::vector<DxuiToolbar::Entry>  entries ((size_t) count);

        for (int i = 0; i < count; i++)
        {
            auto  command = std::make_shared<DxuiCommand>();

            command->id    = i + 1;
            command->label = L"Command";
            command->glyph = L"x";

            entries[(size_t) i].command  = command;
            entries[(size_t) i].iconOnly = true;
        }

        return entries;
    }


    TEST_METHOD (ACompactStripHasSmallButtonsInAThinBand)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first  = {};
        RECT           second = {};

        scaler.SetDpi (96);
        bar.SetCompact (true);
        bar.SetEntries (MakeEntries (2));
        bar.Layout     (RECT { 0, 0, 400, DxuiToolbar::kCompactBandDp }, scaler);

        Assert::AreEqual (26, bar.GetBandDp(), L"the band");
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::IsTrue   (bar.TryGetEntryRect (2, second));
        Assert::AreEqual (2L,  first.top,                   L"2 DIPs above");
        Assert::AreEqual (22L, first.bottom - first.top,    L"a 22 DIP button");
        Assert::AreEqual (20L, first.right  - first.left,   L"a 12 DIP icon with 4 each side");
        Assert::AreEqual (2L,  first.left,                  L"2 DIPs in from the edge");
        Assert::AreEqual (1L,  second.left - first.right,   L"1 DIP between neighbors");
    }


    TEST_METHOD (TheCommandBarKeepsItsOwnSpacing)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first = {};

        scaler.SetDpi (96);
        bar.SetEntries (MakeEntries (1));
        bar.Layout     (RECT { 0, 0, 400, 42 }, scaler);

        Assert::AreEqual (42, bar.GetBandDp());
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::AreEqual (35L, first.right - first.left);
    }
};
