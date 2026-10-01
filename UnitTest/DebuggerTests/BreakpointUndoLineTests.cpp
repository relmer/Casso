#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointUndoLineTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndoLineRig
    //
    //  The breakpoints pane's actions, undo and redo run on a real session as
    //  the window's CPU thread runs them, with no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class UndoLineRig : public ControllerRig
    {
    public:
        std::vector<std::string>  Step (std::vector<DebuggerAction> actions)
        {
            BreakpointStep  step;

            step.actions = std::move (actions);
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<std::string>  Do (BreakpointStep::Kind kind)
        {
            BreakpointStep  step;

            step.kind = kind;
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<BreakpointInfo>  List()
        {
            BreakpointListData  list;

            BreakpointHandlers::ListAll (controller.GetSession(), list);
            return list.breakpoints;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BreakpointUndoLineTests
    //
    //  An undo or redo restores each breakpoint from the table entry saved
    //  with the step, runs no command, and prints one line saying what it
    //  restored or removed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BreakpointUndoLineTests)
    {
    public:

        TEST_METHOD (UndoingAnEditRestoresTheEntryWithNoBpedit)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP 0300 IF A=41");
            id = rig.List().at (0).id;
            rig.Run (std::format ("BPCHANGE {} eTs", id));

            rig.Step ({ DebuggerActions::GetEditBreakpoint (id, "BPMW 0400", CommandMode::AppleWin) });
            lines = rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((size_t) 1, lines.size(), L"one line, no echoed command");
            Assert::AreEqual (std::format ("Restored breakpoint {} (exec 0300)", id), lines[0]);

            Assert::AreEqual ((size_t) 1,        rig.List().size());
            Assert::AreEqual (id,                rig.List().at (0).id);
            Assert::IsTrue   (rig.List().at (0).kind == BreakpointKind::Address);
            Assert::AreEqual ((Word) 0x0300,     rig.List().at (0).address);
            Assert::AreEqual (std::string ("A=41"), rig.List().at (0).condition);
            Assert::IsFalse  (rig.List().at (0).enabled);
            Assert::IsTrue   (rig.List().at (0).temporary);
            Assert::IsFalse  (rig.List().at (0).stops);
        }


        TEST_METHOD (RedoingAnEditPrintsOneLine)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP 0300");
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetEditBreakpoint (id, "BP 0302", CommandMode::AppleWin) });
            rig.Do (BreakpointStep::Kind::Undo);
            lines = rig.Do (BreakpointStep::Kind::Redo);

            Assert::AreEqual ((size_t) 1, lines.size());
            Assert::AreEqual (std::format ("Restored breakpoint {} (exec 0302)", id), lines[0]);
            Assert::AreEqual ((Word) 0x0302, rig.List().at (0).address);
        }


        TEST_METHOD (UndoingADeletePrintsOneRestoredLine)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP C000");
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetClearBreakpoint (id, CommandMode::AppleWin) });
            lines = rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((size_t) 1, lines.size());
            Assert::AreEqual (std::format ("Restored breakpoint {} (exec C000)", rig.List().at (0).id), lines[0]);
        }


        TEST_METHOD (RedoingADeletePrintsOneRemovedLine)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP C000");
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetClearBreakpoint (id, CommandMode::AppleWin) });
            rig.Do (BreakpointStep::Kind::Undo);
            id    = rig.List().at (0).id;
            lines = rig.Do (BreakpointStep::Kind::Redo);

            Assert::AreEqual ((size_t) 1, lines.size(), L"no echoed BPC");
            Assert::AreEqual (std::format ("Removed breakpoint {} (exec C000)", id), lines[0]);
            Assert::AreEqual ((size_t) 0, rig.List().size());
        }


        TEST_METHOD (UndoingADisablePrintsOneLine)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP 0300");
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetEnableBreakpoint (id, false, CommandMode::AppleWin) });
            lines = rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((size_t) 1, lines.size());
            Assert::AreEqual (std::format ("Restored breakpoint {} (exec 0300)", id), lines[0]);
            Assert::IsTrue   (rig.List().at (0).enabled);
        }


        TEST_METHOD (UndoingDeleteAllPrintsALinePerBreakpoint)
        {
            UndoLineRig               rig;
            std::vector<std::string>  lines;



            rig.Run ("BP 0300");
            rig.Run ("BPMW 0400:0410");
            rig.Step ({ DebuggerActions::GetClearAllBreakpoints (CommandMode::AppleWin) });
            lines = rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((size_t) 2, lines.size());
        }
    };
}
