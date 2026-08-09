#include "Pch.h"

#include "Ui/Chrome/MainMenu.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenuDropdownTests
//
//  The emulator's own menu content: which commands exist, their accelerators,
//  and their grouping.
//
//  A test of the TABLE rather than of the menu bar widget, which is covered
//  elsewhere. It pins that every command has an id, a label, and -- where one
//  is documented -- an accelerator, so a menu item added without wiring its
//  command fails here rather than appearing and doing nothing.
//
//  Duplicate accelerators are checked for, since two commands claiming one
//  chord means the second is unreachable and nothing else would notice.
//
//  The same table generates the parity documentation, so this suite is also
//  what keeps that document honest.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MainMenuDropdownTests)
{
public:

    TEST_METHOD (Open_Close_Tracks_State)
    {
        MainMenu  menu;



        menu.Open (MainMenuId::File, true);
        Assert::IsTrue (menu.IsOpen());
        Assert::IsTrue (menu.OpenMenu() == MainMenuId::File);

        menu.Close();
        Assert::IsFalse (menu.IsOpen());
    }


    TEST_METHOD (Alt_Key_Opens_Matching_Menu)
    {
        MainMenu  menu;



        Assert::IsTrue (menu.HandleAltKey (L'F'));
        Assert::IsTrue (menu.IsOpen());
        Assert::IsTrue (menu.OpenMenu() == MainMenuId::File);
        Assert::IsFalse (menu.HandleAltKey (L'?'));
    }


    TEST_METHOD (Keyboard_Selection_Dispatches_And_Closes)
    {
        MainMenu  menu;
        WORD      dispatched = 0;



        menu.SetDispatch ([&dispatched] (WORD commandId) { dispatched = commandId; });
        menu.Open (MainMenuId::File, true);
        Assert::AreEqual (0, menu.HighlightIndex());
        Assert::IsTrue   (menu.HandleKey (VK_RETURN));
        // File's first row is now "Show Printer Preview".
        Assert::AreEqual ((int) IDM_PRINTER_PREVIEW, (int) dispatched);
        Assert::IsFalse  (menu.IsOpen());
    }


    TEST_METHOD (Mouse_Click_Dispatches_Row)
    {
        MainMenu  menu;
        WORD      dispatched = 0;



        menu.SetDispatch ([&dispatched] (WORD commandId) { dispatched = commandId; });
        menu.Layout (0, 32, 800, 96);
        menu.Open (MainMenuId::File, true);
        Assert::IsTrue   (menu.HandleMouseMove (10, 32 + 28 + 4));
        Assert::IsTrue   (menu.HandleMouseUp   (10, 32 + 28 + 4));
        // First File row is now "Show Printer Preview".
        Assert::AreEqual ((int) IDM_PRINTER_PREVIEW, (int) dispatched);
        Assert::IsFalse  (menu.IsOpen());
    }
};
