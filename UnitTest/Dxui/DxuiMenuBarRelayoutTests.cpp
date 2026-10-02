#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBarRelayoutTests
//
//  A host that sets its menus after the bar was laid out still gets titles
//  on the strip. Replacing the items used to clear every title rect until the
//  next layout, which left the strip empty and every click on it a miss.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiMenuBarRelayoutTests)
{
public:

    static constexpr int   kStripX     = 0;
    static constexpr int   kStripY     = 32;
    static constexpr int   kStripWidth = 800;
    static constexpr UINT  kDpi        = 96;


    static std::vector<DxuiMenuBarItem>  MakeItems()
    {
        auto                          command = std::make_shared<DxuiCommand>();
        std::vector<DxuiMenuBarItem>  items;



        command->label = L"Close";

        items.push_back ({ L"&File", 0, { DxuiPopupMenuItem::ForCommand (command) } });
        items.push_back ({ L"&Edit", 0, { DxuiPopupMenuItem::ForCommand (command) } });

        return items;
    }


    TEST_METHOD (SetItems_AfterLayout_PlacesEveryTitle)
    {
        DxuiMenuBar  bar;
        RECT         file = {};
        RECT         edit = {};



        bar.Layout   (kStripX, kStripY, kStripWidth, kDpi);
        bar.SetItems (MakeItems());

        file = bar.GetMenuRect (0);
        edit = bar.GetMenuRect (1);

        Assert::IsTrue (file.right > file.left, L"File has no width on the strip");
        Assert::IsTrue (edit.left >= file.right, L"Edit does not follow File");
        Assert::AreEqual ((LONG) kStripY, file.top);
    }


    TEST_METHOD (SetItems_AfterLayout_TitleTakesAClick)
    {
        DxuiMenuBar  bar;
        RECT         file = {};



        bar.Layout   (kStripX, kStripY, kStripWidth, kDpi);
        bar.SetItems (MakeItems());

        file = bar.GetMenuRect (0);

        bar.HandleMouseDown ((file.left + file.right) / 2, (file.top + file.bottom) / 2);

        Assert::IsTrue (bar.IsOpen());
        Assert::AreEqual (0, bar.OpenIndex());
    }
};
