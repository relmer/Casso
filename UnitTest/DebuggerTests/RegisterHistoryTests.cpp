#include "Pch.h"

#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"

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

        //  A run that stops again at the same PC within one frame is never
        //  seen running, but its cycles differ from those of the stop the
        //  edit was made at, and the history goes.
        TEST_METHOD (AStopAgainAtTheSamePcClearsTheHistory)
        {
            RegisterHistory  registers;
            StackHistory     stack;



            registers.Record ("X", 0x05, 0x07, 0x0300, 1000);
            stack.Record     (0x01FD, 0x12, 0x34, 0x0300, 1000);

            registers.OnSnapshot (true, 0x0300, 1000);
            stack.OnSnapshot     (true, 0x0300, 1000);
            Assert::IsTrue (registers.CanUndo() && stack.CanUndo(), L"the same stop");

            registers.OnSnapshot (true, 0x0300, 1006);
            stack.OnSnapshot     (true, 0x0300, 1006);
            Assert::IsFalse (registers.CanUndo(), L"another stop at the same PC");
            Assert::IsFalse (stack.CanUndo());
        }


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



        //  The registers', the stack's and a memory window's undo come back
        //  from the text a debug project saves; text that does not read
        //  empties the history.
        TEST_METHOD (TheHistoriesComeBackFromTheirText)
        {
            RegisterHistory  registers;
            RegisterHistory  readRegisters;
            StackHistory     stack;
            StackHistory     readStack;
            MemoryEditModel  memory;
            MemoryEditModel  readMemory;



            registers.Record ("A", 0x01, 0x02, 0x0300);
            registers.Record ("X", 0x10, 0x20, 0x0300);
            (void) registers.TryUndo (CommandMode::AppleWin);
            readRegisters.FromText (registers.ToText());

            Assert::AreEqual (registers.ToText(), readRegisters.ToText());
            Assert::IsTrue   (readRegisters.CanUndo() && readRegisters.CanRedo());

            stack.Record (0x01FD, 0x12, 0x34, 0x0300);
            readStack.FromText (stack.ToText());
            Assert::AreEqual (stack.ToText(), readStack.ToText());

            readMemory.SetHistoryText ("0300:0001:A9EA|");
            Assert::AreEqual (std::string ("0300:0001:A9EA|"), readMemory.GetHistoryText());
            Assert::IsTrue   (readMemory.GetUndoText().find (L"$0300") != std::wstring::npos);

            readRegisters.FromText ("nonsense");
            Assert::IsFalse  (readRegisters.CanUndo());
            readMemory.SetHistoryText ("0300:00:A9EA|");
            Assert::AreEqual (std::string ("|"), readMemory.GetHistoryText(), L"replaced and written differ in length");
        }
    };
}
