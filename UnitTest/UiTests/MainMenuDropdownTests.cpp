#include "Pch.h"

#include "Ui/Chrome/MainMenu.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





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
        Assert::AreEqual ((int) IDM_FILE_EXIT, (int) dispatched);
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
        Assert::AreEqual ((int) IDM_FILE_EXIT, (int) dispatched);
        Assert::IsFalse  (menu.IsOpen());
    }
};
