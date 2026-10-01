#include "Pch.h"

#include "Widgets/DxuiToolbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarOverflowTests
//
//  Every toolbar overflows: entries that do not fit even as icons move into
//  a "..." See more menu, whether or not its host asked for one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarOverflowTests)
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

            entries[(size_t) i].command = command;
        }

        return entries;
    }


    TEST_METHOD (AToolbarThatNeverAskedForSeeMoreStillOverflows)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        bar.SetEntries   (MakeEntries (6));
        bar.PlanForWidth (120, scaler);

        Assert::IsTrue  (bar.IsInSeeMore (6),                       L"the rightmost entry goes into the menu");
        Assert::IsFalse (bar.IsInSeeMore (1),                       L"the leftmost stays on the strip");
        Assert::IsFalse (bar.IsInSeeMore (DxuiToolbar::kSeeMoreId), L"and the button shows");
    }


    TEST_METHOD (AToolbarWithRoomShowsNoSeeMoreButton)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        bar.SetEntries   (MakeEntries (3));
        bar.PlanForWidth (4000, scaler);

        Assert::IsFalse (bar.IsInSeeMore (3));
        Assert::IsTrue  (bar.IsInSeeMore (DxuiToolbar::kSeeMoreId), L"an empty menu shows no button");
    }
};
