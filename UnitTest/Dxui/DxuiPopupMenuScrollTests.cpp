#include "Pch.h"

#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuScrollTests
//
//  A menu taller than it may be shows as many whole rows as fit and scrolls
//  the rest: by the wheel, or by moving the highlight past an edge. The
//  limit is pinned here, since a headless menu has no monitor to measure.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupMenuScrollTests)
{
public:

    static constexpr int  s_kRows    = 30;
    static constexpr int  s_kVisible = 10;



    struct Fixture
    {
        std::vector<std::shared_ptr<DxuiCommand>>  commands;
        DxuiPopupMenu                              menu;
        MockDxuiTextRenderer                       text;
        int                                        rowPx = 0;

        Fixture()
        {
            std::vector<DxuiPopupMenuItem>  rows;
            int                             i = 0;

            for (i = 0; i < s_kRows; i++)
            {
                std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

                command->id       = i + 1;
                command->label    = L"Folder";
                command->dispatch = [] () {};

                rows.push_back (DxuiPopupMenuItem::ForCommand (command));
                commands.push_back (std::move (command));
            }

            rowPx = menu.GetMetrics().rowHeightPx;

            //  A limit between whole rows, which rounds down to them.
            menu.SetMaxHeightPx (s_kVisible * rowPx + rowPx / 2);
            menu.ShowAt (0, 0, std::move (rows), text, RECT { 0, 0, 4000, 4000 });
        }
    };



    TEST_METHOD (ATallMenu_ShowsTheWholeRowsThatFit)
    {
        Fixture  f;
        RECT     r = f.menu.GetRect();

        Assert::IsTrue   (f.menu.IsScrollable());
        Assert::AreEqual ((LONG) (s_kVisible * f.rowPx), r.bottom - r.top);
    }



    TEST_METHOD (AShortMenu_DoesNotScroll)
    {
        DxuiPopupMenu                  menu;
        MockDxuiTextRenderer           text;
        std::shared_ptr<DxuiCommand>   command = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem> rows;

        command->label = L"Only";
        rows.push_back (DxuiPopupMenuItem::ForCommand (command));

        menu.SetMaxHeightPx (10 * menu.GetMetrics().rowHeightPx);
        menu.ShowAt (0, 0, std::move (rows), text, RECT { 0, 0, 4000, 4000 });

        Assert::IsFalse (menu.IsScrollable());
    }



    TEST_METHOD (Scrolling_StopsAtEitherEnd)
    {
        Fixture  f;

        f.menu.ScrollByRows (-3);
        Assert::AreEqual (0, f.menu.GetScrollRow(), L"Not above the first row");

        f.menu.ScrollByRows (1000);
        Assert::AreEqual (s_kRows - s_kVisible, f.menu.GetScrollRow(), L"Not past the last row");
    }



    TEST_METHOD (AScrolledMenu_HitTestsTheRowShownThere)
    {
        Fixture  f;
        RECT     r = f.menu.GetRect();

        f.menu.ScrollByRows (5);

        //  The first row on screen is the sixth.
        Assert::AreEqual (5, f.menu.HitTestRow (r.left + 4, r.top + f.rowPx / 2));
    }



    TEST_METHOD (AHighlightPastTheBottom_ScrollsItIntoView)
    {
        Fixture  f;
        int      i = 0;

        for (i = 0; i < s_kVisible + 2; i++)
        {
            f.menu.OnKey (VK_DOWN);
        }

        Assert::AreEqual (s_kVisible + 1, f.menu.GetHighlight());
        Assert::AreEqual (2, f.menu.GetScrollRow(), L"The highlighted row is the last in view");
    }



    TEST_METHOD (AHighlightAboveTheTop_ScrollsItIntoView)
    {
        Fixture  f;

        f.menu.ScrollByRows (10);
        f.menu.SetHighlight (4);

        Assert::AreEqual (4, f.menu.GetScrollRow(), L"The highlighted row is the first in view");
    }



    TEST_METHOD (UpFromTheTop_WrapsToTheLastRowInView)
    {
        Fixture  f;

        f.menu.OnKey (VK_DOWN);
        f.menu.OnKey (VK_UP);

        Assert::AreEqual (s_kRows - 1, f.menu.GetHighlight());
        Assert::AreEqual (s_kRows - s_kVisible, f.menu.GetScrollRow());
    }
};
