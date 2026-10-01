#include "Pch.h"

#include "Widgets/DxuiToolbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarLabelsTests
//
//  A strip with its labels off, as a floating toolbar is, shows icons alone
//  however much room it has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarLabelsTests)
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


    TEST_METHOD (AStripWithoutLabelsShowsIconsInAnyRoom)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;


        scaler.SetDpi (96);
        bar.SetLabels  (false);
        bar.SetEntries (MakeEntries());
        bar.Layout     (RECT { 0, 0, 2000, 42 }, scaler);

        for (int i = 1; i <= 3; i++)
        {
            Assert::IsFalse (bar.IsLabeled (i));
        }
    }


    TEST_METHOD (AStripWithoutLabelsIsAsShortAsAVerticalOne)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        int            iconsOnly = 0;
        int            down      = 0;


        scaler.SetDpi (96);
        bar.SetEntries (MakeEntries());
        bar.SetLabels  (false);
        iconsOnly = bar.GetNaturalLengthPx (scaler);

        bar.SetLabels   (true);
        bar.SetVertical (true);
        down = bar.GetNaturalLengthPx (scaler);

        Assert::AreEqual (down, iconsOnly);
    }
};
