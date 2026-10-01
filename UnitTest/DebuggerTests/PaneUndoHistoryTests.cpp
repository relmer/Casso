#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"
#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/WatchHistory.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace PaneUndoHistoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StackHistoryTests
    //
    //  The stack pane's undo and redo, which belong to the stop the edit was
    //  made at.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StackHistoryTests)
    {
    public:

        TEST_METHOD (UndoAndRedoWriteTheByteBack)
        {
            StackHistory                   history;
            std::optional<DebuggerAction>  action;



            history.Record (0x01FD, 0x12, 0x34, 0x0300);
            history.OnSnapshot (true, 0x0300);

            action = history.TryUndo (CommandMode::AppleWin);
            Assert::IsTrue   (action.has_value());
            Assert::AreEqual (std::string ("MEB 01FD 12"), action->echo);
            Assert::IsFalse  (history.CanUndo());

            action = history.TryRedo (CommandMode::AppleWin);
            Assert::IsTrue   (action.has_value());
            Assert::AreEqual (std::string ("MEB 01FD 34"), action->echo);
            Assert::IsFalse  (history.CanRedo());
        }


        TEST_METHOD (RunningOrSteppingClearsTheHistory)
        {
            StackHistory  history;



            history.Record (0x01FD, 0x12, 0x34, 0x0300);
            history.OnSnapshot (true, 0x0302);
            Assert::IsFalse (history.CanUndo(), L"a step to another PC");

            history.Record (0x01FD, 0x12, 0x34, 0x0302);
            history.OnSnapshot (false, 0x0302);
            Assert::IsFalse (history.CanUndo(), L"a run");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchHistoryTests
    //
    //  The watch pane's undo and redo. A value edit is redone by the actions
    //  it ran; a moved watch is undone only.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WatchHistoryTests)
    {
    public:

        TEST_METHOD (UndoThenRedoRunsTheEditAgain)
        {
            WatchHistory                                history;
            DebuggerViewState::WatchUndo                undo;
            DebuggerViewSnapshot                        now;
            std::optional<std::vector<DebuggerAction>>  actions;



            undo.actions = { DebuggerActions::GetEnterWord (0x0300, (Word) 0x1111, CommandMode::AppleWin) };
            history.Record (undo, { DebuggerActions::GetEnterWord (0x0300, (Word) 0x2222, CommandMode::AppleWin) });

            actions = history.TryUndo (now, CommandMode::AppleWin);
            Assert::IsTrue   (actions.has_value());
            Assert::AreEqual (undo.actions[0].echo, (*actions)[0].echo);
            Assert::IsTrue   (history.CanRedo());

            actions = history.TryRedo();
            Assert::IsTrue   (actions.has_value());
            Assert::AreEqual (DebuggerActions::GetEnterWord (0x0300, (Word) 0x2222, CommandMode::AppleWin).echo, (*actions)[0].echo);
            Assert::IsTrue   (history.CanUndo());
            Assert::IsFalse  (history.CanRedo());
        }


        TEST_METHOD (ANewEditClearsTheRedo)
        {
            WatchHistory                  history;
            DebuggerViewState::WatchUndo  undo;
            DebuggerViewSnapshot          now;



            undo.actions = { DebuggerActions::GetEnterWord (0x0300, (Word) 0x1111, CommandMode::AppleWin) };
            history.Record (undo, {});
            (void) history.TryUndo (now, CommandMode::AppleWin);
            history.Record (undo, {});

            Assert::IsFalse (history.CanRedo());
        }


        TEST_METHOD (AMovedWatchIsUndoneButNotRedone)
        {
            WatchHistory                  history;
            DebuggerViewState::WatchUndo  undo;
            DebuggerViewSnapshot          now;



            undo.restoreAddress = 0x0300;
            undo.movedFromIds   = { 1 };
            history.Record (undo, {});

            Assert::IsFalse (history.TryUndo (now, CommandMode::AppleWin).has_value(), L"no snapshot shows the moved watch yet");

            now.watches = { { 2, 0x0400, "00", true } };
            history.OnSnapshot (now);

            Assert::IsTrue  (history.TryUndo (now, CommandMode::AppleWin).has_value());
            Assert::IsFalse (history.CanRedo());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryEditRedoTests
    //
    //  A memory window's redo: an undone edit's bytes written again, and a new
    //  edit clearing what could be redone.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryEditRedoTests)
    {
    public:

        struct Rig
        {
            MemoryEditModel           model;
            std::vector<std::string>  lines;

            Rig()
            {
                std::vector<std::optional<Byte>>  bytes;
                std::vector<MemoryRegion>         regions (16, MemoryRegion::MainRam);

                for (int i = 0; i < 16; i++)
                {
                    bytes.push_back ((Byte) i);
                }

                model.SetContents (0x0300, bytes, regions);
                model.SetOnPatch ([this] (Word address, std::span<const Byte> data) { lines.push_back (DebuggerActions::GetPatch (address, data, CommandMode::AppleWin).echo); });
            }

            bool  Write (Word address, std::initializer_list<uint8_t> bytes)
            {
                std::vector<uint8_t>  data (bytes);

                return model.WriteBytes (address, data);
            }
        };



        TEST_METHOD (RedoWritesTheUndoneBytesAgain)
        {
            Rig  rig;



            Assert::IsTrue   (rig.Write (0x0302, { 0xAA, 0xBB }));
            Assert::IsTrue   (rig.model.Undo());
            Assert::IsTrue   (rig.model.CanRedo());
            Assert::IsTrue   (rig.model.Redo());
            Assert::AreEqual (std::string ("PATCH 0302 AA BB"), rig.lines.back());
            Assert::IsTrue   (rig.model.CanUndo());
            Assert::IsFalse  (rig.model.CanRedo());
        }


        TEST_METHOD (ANewEditClearsTheRedo)
        {
            Rig  rig;



            Assert::IsTrue  (rig.Write (0x0302, { 0xAA }));
            Assert::IsTrue  (rig.model.Undo());
            Assert::IsTrue  (rig.Write (0x0303, { 0xCC }));
            Assert::IsFalse (rig.model.CanRedo());
            Assert::IsFalse (rig.model.Redo());
        }
    };
}
