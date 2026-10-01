#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointUndoAddTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndoAddRig
    //
    //  The breakpoints pane's actions and undo run on a real session as the
    //  window's CPU thread runs them, with no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class UndoAddRig : public ControllerRig
    {
    public:
        std::vector<std::string>  Step (std::vector<DebuggerAction> actions)
        {
            BreakpointStep  step;

            step.actions = std::move (actions);
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<std::string>  Undo()
        {
            BreakpointStep  step;

            step.kind = BreakpointStep::Kind::Undo;
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
    //  BreakpointUndoAddTests
    //
    //  Undoing a delete adds the breakpoint back from the table entry saved
    //  with the step, through the breakpoint handler, so no definition is
    //  built for it or run at the console.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BreakpointUndoAddTests)
    {
    public:

        TEST_METHOD (UndoingADeleteRunsNoDefinition)
        {
            UndoAddRig                rig;
            BreakpointInfo            before;
            BreakpointInfo            after;
            std::vector<std::string>  lines;



            rig.Run ("BPMW 0400:04FF IF A=41");
            before = rig.List().at (0);
            rig.Run (std::format ("BPCHANGE {} eTs", before.id));
            before = rig.List().at (0);

            rig.Step ({ DebuggerActions::GetClearBreakpoint (before.id, CommandMode::AppleWin) });
            lines = rig.Undo();

            for (const std::string & line : lines)
            {
                Assert::IsTrue (line.find ("BPMW") == std::string::npos, L"no definition is run to add it back");
            }

            Assert::AreEqual ((size_t) 1, rig.List().size());
            after = rig.List().at (0);

            Assert::AreNotEqual (before.id, after.id, L"it comes back under a new id");
            Assert::AreEqual    (BreakpointHandlers::MakeDefinition (before), BreakpointHandlers::MakeDefinition (after));
            Assert::AreEqual    (before.condition, after.condition);
            Assert::IsFalse     (after.enabled);
            Assert::IsTrue      (after.temporary);
            Assert::IsFalse     (after.stops);
        }


        TEST_METHOD (UndoingADeleteOfAnAddressBreakpointRunsNoDefinition)
        {
            UndoAddRig                rig;
            std::vector<std::string>  lines;
            int                       id = 0;



            rig.Run ("BP 0300:0310 IF X=2");
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetClearBreakpoint (id, CommandMode::AppleWin) });
            lines = rig.Undo();

            for (const std::string & line : lines)
            {
                Assert::IsTrue (line.find ("BP ") == std::string::npos, L"no definition is run to add it back");
            }

            Assert::AreEqual ((size_t) 1,      rig.List().size());
            Assert::AreEqual ((Word) 0x0300,   rig.List().at (0).address);
            Assert::AreEqual ((Word) 0x0310,   rig.List().at (0).last);
        }
    };
}