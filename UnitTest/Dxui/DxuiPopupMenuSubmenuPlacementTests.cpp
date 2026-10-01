#include "Pch.h"

#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuSubmenuPlacementTests
//
//  A submenu opens beside its parent row, even when the menu was shown with
//  an empty host client rect, as a toolbar in a window that has not been
//  told its size shows it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupMenuSubmenuPlacementTests)
{
public:

    TEST_METHOD (ASubmenuOpensBesideItsRowWithAnEmptyHostClientRect)
    {
        auto                            plain  = std::make_shared<DxuiCommand>();
        auto                            sub    = std::make_shared<DxuiCommand>();
        auto                            child  = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem>  kids;
        std::vector<DxuiPopupMenuItem>  rows;
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        RECT                            anchor = { 600, 400, 640, 430 };

        plain->id    = 1;
        plain->label = L"Plain";
        sub->id      = 2;
        sub->label   = L"Group by";
        child->id    = 3;
        child->label = L"Bytes";

        kids.push_back (DxuiPopupMenuItem::ForCommand (child));
        rows.push_back (DxuiPopupMenuItem::ForCommand (plain));
        rows.push_back (DxuiPopupMenuItem::ForSubmenu (sub, std::move (kids)));

        menu.ShowUnder (anchor, std::move (rows), text, RECT {});
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_RIGHT);

        Assert::AreEqual ((long) 430, menu.GetRect().top, L"the menu hangs under its anchor");
        Assert::IsTrue   (menu.HasOpenChild(), L"the submenu opened");
        Assert::AreEqual (menu.GetRect().right, menu.GetChild()->GetRect().left, L"the submenu starts at the menu's right edge");
        Assert::IsTrue   (menu.GetChild()->GetRect().top > menu.GetRect().top, L"level with its row, not above the menu");
    }
};
