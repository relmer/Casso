#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/Source/SourcePathList.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "FakeDiagnosticsProvider.h"
#include "ControllerRig.h"
#include "DebuggerViewStateTestRig.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "TestHelpers.h"
#include "MockDebugTarget.h"
#include "MockExpressionContext.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/WatchEdits.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/InstructionTouches.h"
#include "Ui/Debugger/InstructionEffect.h"
#include "Ui/Debugger/StopChanges.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "UnreadableStackTarget.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"
#include "Core/TextEncoding.h"






using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchEditTests
    //
    //  The actions a watch edit takes and what puts it back.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WatchEditTests)
    {
    public:
        TEST_METHOD (AWatchEditBecomesTheCommandAnyoneCouldHaveTyped)
        {
            DebuggerViewSnapshot  snapshot;
            auto                  lines = [&snapshot] (std::optional<int> id, std::optional<int> index, int column, const char * typed)
            {
                std::vector<DebuggerAction>  got = WatchEdits::GetWatchEditActions (snapshot, id, index, column, typed, CommandMode::AppleWin);
                std::string                  all;

                for (const DebuggerAction & action : got)
                {
                    all += (all.empty() ? "" : " | ") + action.echo;
                }

                return all;
            };



            snapshot.registers   = { { "A", "10" }, { "P", "24" } };
            snapshot.watches     = { { 3, 0x0400, "1234" } };
            snapshot.autoWatches = { { "R:A", "A", "10" }, { "F:C", "C", "0" }, { "M:0402", "$0402", "7F" } };

            //  A manual watch: the expression moves it, the value writes the
            //  word it watches.
            Assert::AreEqual (std::string ("WC 3 | W EXPR 0500"), lines (3, std::nullopt, 0, "0500"));
            Assert::AreEqual (std::string ("MEW 0400 BEEF"),  lines (3, std::nullopt, 1, " BEEF "), L"typed space is not part of it");

            //  An automatic watch writes what it stands for.
            Assert::AreEqual (std::string ("R A 42"),         lines (std::nullopt, 0, 1, "42"));
            Assert::AreEqual (std::string ("R P 25"),         lines (std::nullopt, 1, 1, "1"),  L"carry set in P $24");
            Assert::AreEqual (std::string ("MEB 0402 99"),    lines (std::nullopt, 2, 1, "99"));

            //  And what cannot be written sends nothing.
            Assert::AreEqual (std::string(), lines (std::nullopt, 0, 0, "X"),  L"an automatic watch's expression is not the user's");
            Assert::AreEqual (std::string(), lines (std::nullopt, 1, 1, "7"),  L"a flag is 0 or 1");
            Assert::AreEqual (std::string(), lines (3, std::nullopt, 1, "  "), L"nothing typed");
            Assert::AreEqual (std::string(), lines (9, std::nullopt, 1, "12"), L"a watch that is gone");
        }


        //  Text after BASIC is checked before the edit runs, since the edit
        //  clears the watch it replaces first.
        TEST_METHOD (AWatchOfTextAfterBasicMustBeAVariable)
        {
            DebuggerViewSnapshot  snapshot;



            Assert::IsFalse (WatchEdits::GetWatchError (snapshot, "BASIC 9X").empty(), L"not a variable");
            Assert::IsTrue  (WatchEdits::GetWatchError (snapshot, "BASIC A%").empty());
            Assert::IsTrue  (WatchEdits::GetWatchError (snapshot, "BASIC D(3, 4)").empty(), L"spaces as a program may have them");
        }


        //  A byte watch's edit writes one byte, which is all its undo puts
        //  back; a value too wide for it, or a second value, writes nothing.
        TEST_METHOD (AByteWatchsEditWritesOnlyItsByte)
        {
            MachineRig                   rig;
            DebuggerViewSnapshot         snapshot;
            std::vector<DebuggerAction>  actions;
            Byte                         first = rig.machine.GetMemoryBus().ReadByte (0x0400);
            Byte                         next  = rig.machine.GetMemoryBus().ReadByte (0x0401);



            snapshot.autoWatches = { { "M:0400", "$0400", "00" } };

            for (const char * typed : { "1234", "12 34" })
            {
                actions = WatchEdits::GetWatchEditActions (snapshot, std::nullopt, 0, 1, typed, CommandMode::AppleWin);
                Assert::AreEqual ((size_t) 1, actions.size());

                rig.view.ExecuteAction (rig.controller.GetSession(), actions[0]);

                Assert::AreEqual ((int) first, (int) rig.machine.GetMemoryBus().ReadByte (0x0400), TextEncoding::NarrowToWide (typed).c_str());
                Assert::AreEqual ((int) next, (int) rig.machine.GetMemoryBus().ReadByte (0x0401), TextEncoding::NarrowToWide (typed).c_str());
            }

            actions = WatchEdits::GetWatchEditActions (snapshot, std::nullopt, 0, 1, "5A", CommandMode::AppleWin);
            rig.view.ExecuteAction (rig.controller.GetSession(), actions[0]);

            Assert::AreEqual ((int) 0x5A, (int) rig.machine.GetMemoryBus().ReadByte (0x0400));
            Assert::AreEqual ((int) next, (int) rig.machine.GetMemoryBus().ReadByte (0x0401));
        }


        TEST_METHOD (AWatchEditsUndoPutsBackWhatItOverwrote)
        {
            DebuggerViewSnapshot                  before;
            std::optional<WatchEdits::WatchUndo>  undo;



            before.registers   = { { "A", "10" }, { "P", "24" } };
            before.watches     = { { 3, 0x0400, "1234" }, { 5, 0x0500, "--" } };
            before.autoWatches = { { "R:A", "A", "10" }, { "F:C", "C", "0" }, { "M:0402", "$0402", "7F" }, { "M:C030", "SPKR", "--" } };

            undo = WatchEdits::GetWatchUndo (before, 3, std::nullopt, 1, CommandMode::AppleWin);
            Assert::IsTrue   (undo.has_value());
            Assert::AreEqual (std::string ("MEW 0400 1234"), undo->actions.at (0).echo, L"a manual value: the word it held");

            undo = WatchEdits::GetWatchUndo (before, std::nullopt, 0, 1, CommandMode::AppleWin);
            Assert::AreEqual (std::string ("R A 10"),       undo->actions.at (0).echo, L"a register: its old value");

            undo = WatchEdits::GetWatchUndo (before, std::nullopt, 1, 1, CommandMode::AppleWin);
            Assert::AreEqual (std::string ("R P 24"),       undo->actions.at (0).echo, L"a flag: the whole status register as it was");

            undo = WatchEdits::GetWatchUndo (before, std::nullopt, 2, 1, CommandMode::AppleWin);
            Assert::AreEqual (std::string ("MEB 0402 7F"),  undo->actions.at (0).echo, L"an address: the byte it held");

            //  A moved watch: no line yet, since the watch the move makes is
            //  numbered only once it runs -- but what to put back, and which
            //  ids existed, so the new one can be found then.
            undo = WatchEdits::GetWatchUndo (before, 3, std::nullopt, 0, CommandMode::AppleWin);
            Assert::IsTrue   (undo->actions.empty());
            Assert::IsTrue   (undo->restoreText == std::optional<std::string> ("0400"));
            Assert::AreEqual ((size_t) 2, undo->movedFromIds.size());

            //  A value that could not be read has nothing to put back.
            Assert::IsFalse (WatchEdits::GetWatchUndo (before, 5, std::nullopt, 1, CommandMode::AppleWin).has_value(), L"an unread manual value");
            Assert::IsFalse (WatchEdits::GetWatchUndo (before, std::nullopt, 3, 1, CommandMode::AppleWin).has_value(), L"a soft switch");
        }


        TEST_METHOD (AMovedWatchsUndoFindsTheWatchTheMoveMadeAndNoOther)
        {
            DebuggerViewSnapshot                        before;
            DebuggerViewSnapshot                        moved;
            DebuggerViewSnapshot                        later;
            std::optional<WatchEdits::WatchUndo>        undo;
            std::optional<std::vector<DebuggerAction>>  lines;



            before.watches = { { 3, 0x0400, "1234" }, { 5, 0x0500, "5678" } };
            moved.watches  = { { 5, 0x0500, "5678" }, { 6, 0x0600, "0000" } };
            later.watches  = { { 5, 0x0500, "5678" }, { 7, 0x0700, "0000" } };

            undo = WatchEdits::GetWatchUndo (before, 3, std::nullopt, 0, CommandMode::AppleWin);
            Assert::IsTrue (undo.has_value());

            //  Undone before any snapshot shows the move: nothing yet, and
            //  the undo is still there to try again.
            lines = WatchEdits::GetWatchUndoActions (before, *undo, CommandMode::AppleWin);
            Assert::IsFalse (lines.has_value(), L"no snapshot shows the moved watch yet");

            //  The snapshot after the move shows watch 6; that one was moved.
            WatchEdits::NoteMovedWatch (moved, *undo);

            lines = WatchEdits::GetWatchUndoActions (moved, *undo, CommandMode::AppleWin);
            Assert::IsTrue   (lines.has_value());
            Assert::AreEqual ((size_t) 2, lines->size());
            Assert::AreEqual (std::string ("WC 6"),   lines->at (0).echo);
            Assert::AreEqual (std::string ("W EXPR 0400"), lines->at (1).echo);

            //  Watch 6 removed and watch 7 added since: 7 is not the moved
            //  watch, and nothing is removed.
            lines = WatchEdits::GetWatchUndoActions (later, *undo, CommandMode::AppleWin);
            Assert::IsTrue (lines.has_value());
            Assert::IsTrue (lines->empty(), L"an unrelated watch is left alone");
        }
    };
}
