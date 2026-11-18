#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputStateTests
//
//  A key event made outside of a message takes Shift, Ctrl and Alt from the
//  key state the test sets.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiInputStateTests)
{
public:

    TEST_METHOD_CLEANUP (RestoreKeyReader)
    {
        DxuiInputState::SetKeyReader ([] (int) { return SHORT (0); });
    }


    TEST_METHOD (MakeKeyEvent_TakesTheModifiersThatAreDown)
    {
        DxuiKeyEvent  ev;



        DxuiInputState::SetKeyReader ([] (int vk) { return (vk == VK_CONTROL || vk == VK_MENU) ? (SHORT) 0x8000 : SHORT (0); });

        ev = DxuiInputState::MakeKeyEvent (DxuiKeyEventKind::Up, 'C');

        Assert::IsTrue   (ev.kind == DxuiKeyEventKind::Up);
        Assert::AreEqual ((WPARAM) 'C', ev.vk);
        Assert::IsFalse  (ev.shift);
        Assert::IsTrue   (ev.ctrl);
        Assert::IsTrue   (ev.alt);
    }


    TEST_METHOD (MakeKeyEvent_TakesShiftAloneWhenOnlyShiftIsDown)
    {
        DxuiKeyEvent  ev;



        DxuiInputState::SetKeyReader ([] (int vk) { return (vk == VK_SHIFT) ? (SHORT) 0x8000 : SHORT (0); });

        ev = DxuiInputState::MakeKeyEvent (DxuiKeyEventKind::Down, VK_TAB);

        Assert::IsTrue  (ev.shift);
        Assert::IsFalse (ev.ctrl);
        Assert::IsFalse (ev.alt);
    }
};
