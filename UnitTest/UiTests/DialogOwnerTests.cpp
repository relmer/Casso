#include "Pch.h"

#include "Shell/EmulatorShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialogOwnerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialogOwnerTests
    //
    //  A modal the shell shows goes over the Casso window the user is in: the
    //  debugger window, or the window that owns a floating pane, rather than
    //  the emulator window a maximized debugger covers. Anything else, such
    //  as another program's window, gets the emulator window. The handles
    //  are values, never windows: the choice is made from what
    //  DescribeDialogOwner reads, with no window of the system's.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialogOwnerTests)
    {
    public:

        TEST_METHOD (TheActiveCassoWindowOwnsTheDialog)
        {
            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (Candidate (s_kDebugger, true, true), s_kEmulator) == s_kDebugger);
        }


        TEST_METHOD (AnotherProgramsWindowLeavesTheEmulatorWindow)
        {
            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (Candidate (s_kDebugger, false, true), s_kEmulator) == s_kEmulator, L"another thread's window");
            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (EmulatorShell::DialogOwnerCandidate(), s_kEmulator) == s_kEmulator, L"no foreground window");
        }


        //  A window that a modal already disabled cannot take another.
        TEST_METHOD (ADisabledWindowLeavesTheEmulatorWindow)
        {
            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (Candidate (s_kDebugger, true, false), s_kEmulator) == s_kEmulator);
        }


        //  With no foreground window, the candidate is empty.
        TEST_METHOD (NoForegroundWindowIsNoCandidate)
        {
            EmulatorShell::DialogOwnerCandidate  candidate = EmulatorShell::DescribeDialogOwner (nullptr);



            Assert::IsNull  (candidate.rootOwner);
            Assert::IsFalse (candidate.isThisThread);
            Assert::IsFalse (candidate.isEnabled);
        }

    private:

        static inline const HWND  s_kEmulator = reinterpret_cast<HWND> (static_cast<uintptr_t> (0x1000));
        static inline const HWND  s_kDebugger = reinterpret_cast<HWND> (static_cast<uintptr_t> (0x2000));

        static EmulatorShell::DialogOwnerCandidate Candidate (HWND rootOwner, bool isThisThread, bool isEnabled)
        {
            EmulatorShell::DialogOwnerCandidate  candidate;



            candidate.rootOwner    = rootOwner;
            candidate.isThisThread = isThisThread;
            candidate.isEnabled    = isEnabled;

            return candidate;
        }
    };
}
