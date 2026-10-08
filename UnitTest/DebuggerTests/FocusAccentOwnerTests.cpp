#include "Pch.h"

#include "Ui/Debugger/FocusAccentOwner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAccentOwnerTests
//
//  Of the debugger's main window and its floating panes, the window that
//  took the keyboard focus last shows the accent. The main window taking the
//  focus back clears a float's accent, focus going to another application
//  changes nothing, and a float that docks gives the accent up.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (FocusAccentOwnerTests)
    {
    public:

        TEST_METHOD (TheMainWindowShowsTheAccentUntilAFloatTakesTheFocus)
        {
            FocusAccentOwner  owner;



            Assert::IsTrue   (owner.GetFocusedFloat().empty(), L"no float has had the focus");

            owner.OnMainFocusChanged  (false);
            owner.OnFloatFocusChanged (L"stack", true);
            Assert::AreEqual (std::wstring (L"stack"), owner.GetFocusedFloat(), L"the float that took the focus last");

            owner.OnFloatFocusChanged (L"stack", false);
            owner.OnFloatFocusChanged (L"regs",  true);
            Assert::AreEqual (std::wstring (L"regs"), owner.GetFocusedFloat(), L"another float taking it takes the accent");
        }


        TEST_METHOD (TheMainWindowTakingTheFocusBackClearsTheFloat)
        {
            FocusAccentOwner  owner;



            owner.OnFloatFocusChanged (L"stack", true);
            owner.OnFloatFocusChanged (L"stack", false);
            owner.OnMainFocusChanged  (true);

            Assert::IsTrue (owner.GetFocusedFloat().empty(), L"the main window shows the accent again");
        }


        TEST_METHOD (FocusGoingToAnotherApplicationChangesNothing)
        {
            FocusAccentOwner  owner;



            owner.OnFloatFocusChanged (L"stack", true);
            owner.OnFloatFocusChanged (L"stack", false);
            Assert::AreEqual (std::wstring (L"stack"), owner.GetFocusedFloat(), L"the float keeps the accent");

            owner.OnMainFocusChanged  (true);
            owner.OnMainFocusChanged  (false);
            Assert::IsTrue (owner.GetFocusedFloat().empty(), L"and the main window keeps it");
        }


        TEST_METHOD (AFloatThatDocksGivesTheAccentUp)
        {
            FocusAccentOwner  owner;



            owner.OnFloatFocusChanged (L"stack", true);
            owner.OnFloatDocked       (L"regs");
            Assert::AreEqual (std::wstring (L"stack"), owner.GetFocusedFloat(), L"another float docking leaves it");

            owner.OnFloatDocked       (L"stack");
            Assert::IsTrue (owner.GetFocusedFloat().empty(), L"the float that had it docks");
        }
    };
}
