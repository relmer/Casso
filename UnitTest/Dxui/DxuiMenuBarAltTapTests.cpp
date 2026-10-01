#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBarAltTapTests
//
//  Access-key underlines follow the Windows rule: a tap of Alt on its own
//  toggles them, and they stay until the next tap or until a menu closes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiMenuBarAltTapTests)
{
public:

    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }



    TEST_METHOD (AltTap_TogglesCuesOnThenOff)
    {
        DxuiMenuBar  bar;



        Assert::IsFalse (bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU));
        Assert::IsTrue  (bar.TrackAltTap (DxuiKeyEventKind::Up, VK_MENU));
        Assert::IsTrue  (bar.AreCuesToggled());

        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU);
        Assert::IsTrue  (bar.TrackAltTap (DxuiKeyEventKind::Up, VK_MENU));
        Assert::IsFalse (bar.AreCuesToggled());
    }



    TEST_METHOD (AltWithAnotherKey_DoesNotToggle)
    {
        DxuiMenuBar  bar;



        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU);
        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, 'F');
        (void) bar.TrackAltTap (DxuiKeyEventKind::Up,   'F');

        Assert::IsFalse (bar.TrackAltTap (DxuiKeyEventKind::Up, VK_MENU));
        Assert::IsFalse (bar.AreCuesToggled());
    }



    TEST_METHOD (AltTap_HeldRepeatStillToggles)
    {
        DxuiMenuBar  bar;



        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU);
        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU);

        Assert::IsTrue (bar.TrackAltTap (DxuiKeyEventKind::Up, VK_MENU));
        Assert::IsTrue (bar.AreCuesToggled());
    }



    TEST_METHOD (Close_ClearsToggledCues)
    {
        DxuiMenuBar  bar;



        (void) bar.TrackAltTap (DxuiKeyEventKind::Down, VK_MENU);
        (void) bar.TrackAltTap (DxuiKeyEventKind::Up,   VK_MENU);
        bar.Close();

        Assert::IsFalse (bar.AreCuesToggled());
    }
};
