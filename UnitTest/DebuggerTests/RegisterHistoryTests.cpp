#include "Pch.h"

#include "Ui/Debugger/RegisterHistory.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace RegisterHistoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RegisterHistoryTests
    //
    //  The registers pane's undo and redo, which belong to the stop the edit
    //  was made at.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RegisterHistoryTests)
    {
    public:

        TEST_METHOD (UndoAndRedoWriteTheValuesBack)
        {
            RegisterHistory                history;
            std::optional<DebuggerAction>  action;



            history.Record ("S", 0xF0, 0x80, 0x0300);
            history.OnSnapshot (true, 0x0300);

            action = history.TryUndo (CommandMode::AppleWin);
            Assert::IsTrue   (action.has_value());
            Assert::AreEqual (std::string ("S"), action->command.text);
            Assert::AreEqual ((uint32_t) 0xF0, (uint32_t) action->command.a1);
            Assert::IsFalse  (history.CanUndo());

            action = history.TryRedo (CommandMode::AppleWin);
            Assert::IsTrue   (action.has_value());
            Assert::AreEqual ((uint32_t) 0x80, (uint32_t) action->command.a1);
            Assert::IsTrue   (history.CanUndo());
            Assert::IsFalse  (history.CanRedo());
        }


        TEST_METHOD (RunningClearsTheHistory)
        {
            RegisterHistory  history;



            history.Record ("P", 0x30, 0x31, 0x0300);
            history.TryUndo (CommandMode::AppleWin);
            history.Record ("S", 0xF0, 0x80, 0x0300);
            history.OnSnapshot (false, 0x0300);

            Assert::IsFalse (history.CanUndo());
            Assert::IsFalse (history.TryUndo (CommandMode::AppleWin).has_value());
        }


        TEST_METHOD (StoppingAtAnotherPcClearsTheHistory)
        {
            RegisterHistory  history;



            history.Record ("S", 0xF0, 0x80, 0x0300);
            history.TryUndo (CommandMode::AppleWin);
            history.Record ("S", 0xF0, 0x70, 0x0300);
            history.OnSnapshot (true, 0x0302);

            Assert::IsFalse (history.CanUndo());
            Assert::IsFalse (history.CanRedo());
        }


        TEST_METHOD (TheSameStopKeepsTheHistory)
        {
            RegisterHistory  history;



            history.Record ("S", 0xF0, 0x80, 0x0300);
            history.OnSnapshot (true, 0x0300);

            Assert::IsTrue (history.CanUndo());
        }
    };
}
