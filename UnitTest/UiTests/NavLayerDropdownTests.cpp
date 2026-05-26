#include "Pch.h"

#include "Ui/Chrome/NavLayer.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (NavLayerDropdownTests)
{
public:

    TEST_METHOD (Open_Close_Tracks_State)
    {
        NavLayer  nav;



        nav.Open (NavMenu::File, true);
        Assert::IsTrue (nav.IsOpen());
        Assert::IsTrue (nav.OpenMenu() == NavMenu::File);

        nav.Close();
        Assert::IsFalse (nav.IsOpen());
    }


    TEST_METHOD (Alt_Key_Opens_Matching_Menu)
    {
        NavLayer  nav;



        Assert::IsTrue (nav.HandleAltKey (L'F'));
        Assert::IsTrue (nav.IsOpen());
        Assert::IsTrue (nav.OpenMenu() == NavMenu::File);
        Assert::IsFalse (nav.HandleAltKey (L'?'));
    }


    TEST_METHOD (Keyboard_Selection_Dispatches_And_Closes)
    {
        NavLayer  nav;
        WORD      dispatched = 0;



        nav.SetDispatch ([&dispatched] (WORD commandId) { dispatched = commandId; });
        nav.Open (NavMenu::File, true);
        Assert::IsTrue (nav.HandleKey (VK_DOWN));
        Assert::AreEqual (0, nav.HighlightIndex());
        Assert::IsTrue (nav.HandleKey (VK_RETURN));
        Assert::AreEqual ((int) IDM_FILE_EXIT, (int) dispatched);
        Assert::IsFalse (nav.IsOpen());
    }


    TEST_METHOD (Mouse_Click_Dispatches_Row)
    {
        NavLayer  nav;
        WORD      dispatched = 0;



        nav.SetDispatch ([&dispatched] (WORD commandId) { dispatched = commandId; });
        nav.Layout (0, 32, 800, 96);
        nav.Open (NavMenu::File, true);
        Assert::IsTrue (nav.HandleMouseMove (10, 32 + 28 + 4));
        Assert::IsTrue (nav.HandleMouseUp (10, 32 + 28 + 4));
        Assert::AreEqual ((int) IDM_FILE_EXIT, (int) dispatched);
        Assert::IsFalse (nav.IsOpen());
    }
};
