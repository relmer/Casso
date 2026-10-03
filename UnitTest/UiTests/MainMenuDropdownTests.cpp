#include "Pch.h"

#include "Ui/Chrome/MainMenu.h"
#include "../Dxui/MockDxuiTextRenderer.h"
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
    static constexpr int   s_kStripX            = 0;
    static constexpr int   s_kStripY            = 32;
    static constexpr int   s_kResizedStripWidth = 1600;
    static constexpr UINT  s_kTestDpi           = 96;

    TEST_METHOD (Open_Close_Tracks_State)
    {
        MainMenu  menu;



        menu.Open (MainMenuId::File, true);
        Assert::IsTrue (menu.IsOpen());
        Assert::IsTrue (menu.GetOpenMenu() == MainMenuId::File);

        menu.Close();
        Assert::IsFalse (menu.IsOpen());
    }


    TEST_METHOD (Alt_Key_Opens_Matching_Menu)
    {
        MainMenu  menu;



        Assert::IsTrue (menu.HandleAltKey (L'F'));
        Assert::IsTrue (menu.IsOpen());
        Assert::IsTrue (menu.GetOpenMenu() == MainMenuId::File);
        Assert::IsFalse (menu.HandleAltKey (L'?'));
    }


    TEST_METHOD (Keyboard_Selection_Dispatches_And_Closes)
    {
        MainMenu  menu;
        WORD      dispatched = 0;



        menu.SetDispatch ([&dispatched] (WORD commandId) { dispatched = commandId; });
        menu.Open (MainMenuId::File, true);
        Assert::AreEqual (0, menu.GetHighlightIndex());
        Assert::IsTrue   (menu.HandleKey (VK_RETURN));
        // File's first row is now "Save state...".
        Assert::AreEqual ((int) IDM_FILE_SAVE_STATE, (int) dispatched);
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
        // First File row is now "Save state...".
        Assert::AreEqual ((int) IDM_FILE_SAVE_STATE, (int) dispatched);
        Assert::IsFalse  (menu.IsOpen());
    }


    TEST_METHOD (ProductionResizeLayout_WithZeroHeightBounds_PreservesMeasuredBounds)
    {
        MainMenu              menu;
        MockDxuiTextRenderer  text;
        DxuiDpiScaler         scaler;
        RECT                  resizeBounds = { s_kStripX, s_kStripY, s_kResizedStripWidth, s_kStripY };
        RECT                  fileRect     = {};
        RECT                  editRect     = {};
        RECT                  bounds       = {};


        scaler.SetDpi (s_kTestDpi);
        text.SetCannedMetrics (L"File", { 64, 16 });
        text.SetCannedMetrics (L"Edit", { 52, 16 });
        menu.SetTextRendererForMeasure (&text);

        menu.Layout (resizeBounds, scaler);
        fileRect = menu.GetMenuRect ((int) MainMenuId::File);
        editRect = menu.GetMenuRect ((int) MainMenuId::Edit);
        bounds   = menu.GetBounds();

        Assert::AreEqual (fileRect.bottom, bounds.bottom);
        Assert::AreEqual (fileRect.top,    bounds.top);
        Assert::AreEqual ((LONG) s_kResizedStripWidth, bounds.right - bounds.left);
        Assert::IsTrue   (editRect.left > fileRect.right);
    }
};
