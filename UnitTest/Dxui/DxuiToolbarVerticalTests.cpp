#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarVerticalTests
//
//  A strip docked to a side runs its entries top to bottom as icons, and a
//  movable strip has a grab handle at its leading end.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarVerticalTests)
{
public:

    static std::vector<DxuiToolbar::Entry> MakeEntries()
    {
        std::vector<DxuiToolbar::Entry>  entries (3);

        for (int i = 0; i < 3; i++)
        {
            auto  command = std::make_shared<DxuiCommand>();

            command->id    = i + 1;
            command->label = L"Command";
            command->glyph = L"x";

            entries[(size_t) i].command = command;
        }

        return entries;
    }


    TEST_METHOD (AVerticalStripStacksItsEntriesUnlabeled)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           rc[3] = {};


        scaler.SetDpi (96);
        bar.SetVertical (true);
        bar.SetEntries  (MakeEntries());
        bar.Layout      (RECT { 0, 0, 42, 600 }, scaler);

        for (int i = 0; i < 3; i++)
        {
            Assert::IsTrue  (bar.TryGetEntryRect (i + 1, rc[i]));
            Assert::IsFalse (bar.IsLabeled (i + 1), L"a vertical strip shows icons alone");
            Assert::IsTrue  (rc[i].right <= 42, L"each entry fits the band's width");
        }

        Assert::AreEqual (rc[0].left, rc[1].left, L"one column");
        Assert::IsTrue   (rc[1].top >= rc[0].bottom, L"the second is under the first");
        Assert::IsTrue   (rc[2].top >= rc[1].bottom, L"and the third under it");
    }


    TEST_METHOD (TheGrabHandleLeadsAHorizontalStrip)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first = {};


        scaler.SetDpi (96);
        bar.SetGrabHandle (true);
        bar.SetEntries    (MakeEntries());
        bar.Layout        (RECT { 100, 0, 900, 42 }, scaler);

        Assert::AreEqual ((LONG) 100 + DxuiToolbar::kGripDp, bar.GetGripRect().right);
        Assert::IsTrue   (bar.IsOnGrip (104, 20));
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::IsTrue   (first.left >= bar.GetGripRect().right, L"the entries start past the handle");
        Assert::IsFalse  (bar.IsOnGrip (first.left + 2, 20));
    }


    TEST_METHOD (TheGrabHandleLeadsAVerticalStrip)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first = {};


        scaler.SetDpi (96);
        bar.SetVertical   (true);
        bar.SetGrabHandle (true);
        bar.SetEntries    (MakeEntries());
        bar.Layout        (RECT { 0, 50, 42, 600 }, scaler);

        Assert::AreEqual ((LONG) 50 + DxuiToolbar::kGripDp, bar.GetGripRect().bottom);
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::IsTrue   (first.top >= bar.GetGripRect().bottom);
    }


    TEST_METHOD (TheNaturalLengthOfAVerticalStripIsShorter)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        int            across = 0;
        int            down   = 0;


        scaler.SetDpi (96);
        bar.SetEntries (MakeEntries());
        across = bar.GetNaturalLengthPx (scaler);

        bar.SetVertical (true);
        down = bar.GetNaturalLengthPx (scaler);

        Assert::IsTrue (down < across, L"icons alone take less room than labeled entries");
    }
};
