#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/FocusManager.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (FocusManagerTests)
{
public:

    TEST_METHOD (Register_FirstBecomesCurrent)
    {
        FocusManager  mgr;

        mgr.RegisterFocusable (42);

        Assert::AreEqual (42, mgr.Current());
        Assert::AreEqual (1,  mgr.Count());
    }


    TEST_METHOD (AdvanceForward_WrapsAround)
    {
        FocusManager  mgr;

        mgr.RegisterFocusable (1);
        mgr.RegisterFocusable (2);
        mgr.RegisterFocusable (3);

        Assert::AreEqual (1, mgr.Current());
        mgr.AdvanceForward();
        Assert::AreEqual (2, mgr.Current());
        mgr.AdvanceForward();
        Assert::AreEqual (3, mgr.Current());
        mgr.AdvanceForward();
        Assert::AreEqual (1, mgr.Current());
    }


    TEST_METHOD (AdvanceBackward_WrapsAround)
    {
        FocusManager  mgr;

        mgr.RegisterFocusable (1);
        mgr.RegisterFocusable (2);
        mgr.RegisterFocusable (3);

        mgr.AdvanceBackward();
        Assert::AreEqual (3, mgr.Current());
        mgr.AdvanceBackward();
        Assert::AreEqual (2, mgr.Current());
    }


    TEST_METHOD (HandleKey_EnterActivatesCurrent)
    {
        FocusManager  mgr;
        int           activatedId = 0;

        mgr.SetActivateHandler ([&] (int id) { activatedId = id; });
        mgr.RegisterFocusable (7);

        Assert::IsTrue   (mgr.HandleKey (FocusKey::Enter));
        Assert::AreEqual (7, activatedId);
    }


    TEST_METHOD (HandleKey_EscapeCallsDismiss)
    {
        FocusManager  mgr;
        bool          dismissed = false;

        mgr.SetDismissHandler ([&] () { dismissed = true; });

        Assert::IsTrue (mgr.HandleKey (FocusKey::Escape));
        Assert::IsTrue (dismissed);
    }


    TEST_METHOD (ClassifyKey_TabAndShiftTab)
    {
        Assert::IsTrue (FocusManager::ClassifyKey (VK_TAB, false) == FocusKey::Tab);
        Assert::IsTrue (FocusManager::ClassifyKey (VK_TAB, true)  == FocusKey::ShiftTab);
        Assert::IsTrue (FocusManager::ClassifyKey (VK_ESCAPE, false) == FocusKey::Escape);
        Assert::IsTrue (FocusManager::ClassifyKey (VK_RETURN, false) == FocusKey::Enter);
        Assert::IsTrue (FocusManager::ClassifyKey ('A', false)       == FocusKey::None);
    }
};

}   // namespace UiTests
