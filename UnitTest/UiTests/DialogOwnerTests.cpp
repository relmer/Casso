#include "Pch.h"

#include "Shell/EmulatorShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialogOwnerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TestWindow
    //
    //  A hidden popup window on this thread, optionally owned by another,
    //  destroyed with the object.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TestWindow
    {
    public:
        explicit TestWindow (HWND owner = nullptr)
        {
            m_hwnd = CreateWindowExW (0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, owner, nullptr, GetModuleHandleW (nullptr), nullptr);
            Assert::IsNotNull (m_hwnd, L"the test window is created");
        }

        ~TestWindow()
        {
            if (m_hwnd != nullptr)
            {
                DestroyWindow (m_hwnd);
            }
        }

        TestWindow             (const TestWindow &) = delete;
        TestWindow & operator= (const TestWindow &) = delete;

        HWND  Get() const { return m_hwnd; }

    private:
        HWND  m_hwnd = nullptr;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialogOwnerTests
    //
    //  A modal the shell shows goes over the Casso window the user is in: the
    //  debugger window, or the window that owns a floating pane, rather than
    //  the emulator window a maximized debugger covers. Anything else, such
    //  as another program's window, gets the emulator window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialogOwnerTests)
    {
    public:

        TEST_METHOD (TheActiveCassoWindowOwnsTheDialog)
        {
            TestWindow  emulator;
            TestWindow  debugger;



            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (debugger.Get(), emulator.Get()) == debugger.Get());
        }


        TEST_METHOD (AFloatingPanesOwnerOwnsTheDialog)
        {
            TestWindow  emulator;
            TestWindow  debugger;
            TestWindow  floating (debugger.Get());



            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (floating.Get(), emulator.Get()) == debugger.Get());
        }


        TEST_METHOD (AnotherProgramsWindowLeavesTheEmulatorWindow)
        {
            TestWindow  emulator;



            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (GetDesktopWindow(), emulator.Get()) == emulator.Get(), L"another process's window");
            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (nullptr,            emulator.Get()) == emulator.Get(), L"no foreground window");
        }


        //  A window that a modal already disabled cannot take another.
        TEST_METHOD (ADisabledWindowLeavesTheEmulatorWindow)
        {
            TestWindow  emulator;
            TestWindow  debugger;



            EnableWindow (debugger.Get(), FALSE);

            Assert::IsTrue (EmulatorShell::ChooseDialogOwner (debugger.Get(), emulator.Get()) == emulator.Get());
        }
    };
}
