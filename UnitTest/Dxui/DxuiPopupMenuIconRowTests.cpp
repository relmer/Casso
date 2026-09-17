#include "Pch.h"

#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuIconRowTests
//
//  Explorer's row of icon buttons along the edge of a context menu nearest
//  the pointer: first when the menu hangs down, last when it rises.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupMenuIconRowTests)
{
public:

    struct Fixture
    {
        std::shared_ptr<DxuiCommand>  cut            = std::make_shared<DxuiCommand>();
        std::shared_ptr<DxuiCommand>  paste          = std::make_shared<DxuiCommand>();
        std::shared_ptr<DxuiCommand>  open           = std::make_shared<DxuiCommand>();
        DxuiPopupMenu                 menu;
        MockDxuiTextRenderer          text;
        int                           lastDispatched = 0;

        Fixture()
        {
            cut->id       = 1;
            cut->label    = L"Cut";
            cut->glyph    = L"\xE8C6";
            cut->dispatch = [this] () { lastDispatched = 1; };

            paste->id        = 2;
            paste->label     = L"Paste";
            paste->glyph     = L"\xE77F";
            paste->isEnabled = [] () { return false; };
            paste->dispatch  = [this] () { lastDispatched = 2; };

            open->id       = 3;
            open->label    = L"Open";
            open->dispatch = [this] () { lastDispatched = 3; };
        }

        std::vector<DxuiPopupMenuItem>  Rows()
        {
            std::vector<DxuiPopupMenuItem>  rows;

            rows.push_back (DxuiPopupMenuItem::ForIconRow ({ cut, paste }));
            rows.push_back (DxuiPopupMenuItem::ForSeparator());
            rows.push_back (DxuiPopupMenuItem::ForCommand (open));
            return rows;
        }

        int  ButtonCenterX (int button) const
        {
            int  size = menu.GetMetrics().rowHeightPx * 150 / 100;

            return menu.GetRect().left + menu.GetMetrics().leftPadPx + button * size + size / 2;
        }
    };



    TEST_METHOD (AMenuHangingDown_PutsTheButtonsFirst)
    {
        Fixture  f;

        f.menu.ShowAt (10, 10, f.Rows(), f.text, RECT { 0, 0, 2000, 2000 });

        Assert::IsTrue (f.menu.GetRows().front().kind == DxuiPopupMenuItem::Kind::IconRow);
        Assert::IsTrue (f.menu.GetRows()[1].kind == DxuiPopupMenuItem::Kind::Separator);
    }



    TEST_METHOD (AMenuRisingFromTheBottom_PutsTheButtonsLast)
    {
        Fixture  f;

        f.menu.ShowAt (10, 1990, f.Rows(), f.text, RECT { 0, 0, 2000, 2000 });

        Assert::IsTrue (f.menu.GetRows().back().kind == DxuiPopupMenuItem::Kind::IconRow);
        Assert::IsTrue (f.menu.GetRows()[f.menu.GetRows().size() - 2].kind == DxuiPopupMenuItem::Kind::Separator);
        Assert::IsTrue (f.menu.GetRows().front().kind == DxuiPopupMenuItem::Kind::Command, L"No separator is left at the top");
    }



    TEST_METHOD (ClickingAButton_RunsItsCommand)
    {
        Fixture  f;
        RECT     r   = {};
        int      y   = 0;
        int      x   = 0;

        f.menu.ShowAt (10, 10, f.Rows(), f.text, RECT { 0, 0, 2000, 2000 });

        r = f.menu.GetRect();
        y = r.top + f.menu.GetMetrics().rowHeightPx / 2;
        x = f.ButtonCenterX (0);

        f.menu.OnMouseMove (x, y);
        Assert::AreEqual (0, f.menu.GetIconHighlight());

        f.menu.OnLButtonDown (x, y);
        f.menu.OnLButtonUp   (x, y);

        Assert::AreEqual (1, f.lastDispatched);
        Assert::IsFalse  (f.menu.IsVisible());
    }



    TEST_METHOD (ADisabledButton_DoesNothing)
    {
        Fixture  f;
        int      y = 0;
        int      x = 0;

        f.menu.ShowAt (10, 10, f.Rows(), f.text, RECT { 0, 0, 2000, 2000 });

        y = f.menu.GetRect().top + f.menu.GetMetrics().rowHeightPx / 2;
        x = f.ButtonCenterX (1);

        f.menu.OnMouseMove   (x, y);
        f.menu.OnLButtonDown (x, y);
        f.menu.OnLButtonUp   (x, y);

        Assert::AreEqual (0, f.lastDispatched);
        Assert::IsTrue   (f.menu.IsVisible());
    }



    TEST_METHOD (TheKeyboard_PassesOverTheButtons)
    {
        Fixture  f;

        f.menu.ShowAt (10, 10, f.Rows(), f.text, RECT { 0, 0, 2000, 2000 });

        f.menu.OnKey (VK_DOWN);

        Assert::AreEqual (2, f.menu.GetHighlight(), L"Open, not the icon row");
    }
};
