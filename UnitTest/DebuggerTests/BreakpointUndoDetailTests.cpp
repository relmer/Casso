#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointUndoDetailTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DetailRig
    //
    //  The breakpoints pane's actions, undo and redo on a real session, with
    //  no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DetailRig : public ControllerRig
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

        void  SetHits (int id, uint32_t hits)
        {
            BreakpointTable & table = controller.GetSession().GetBreakpoints();
            Breakpoint        entry;

            Assert::IsTrue (table.TryFind (id, entry));
            entry.hits = hits;
            table.TryClear (id);
            Assert::IsTrue (table.TryAdopt (entry));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BreakpointUndoDetailTests
    //
    //  An undo or redo that changes several breakpoints prints one line for
    //  each, and undoing an edit brings back the hit count it had.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BreakpointUndoDetailTests)
    {
    public:

        TEST_METHOD (UndoingDeleteAllPrintsOneLinePerBreakpoint)
        {
            DetailRig                 rig;
            std::vector<std::string>  lines;
            std::vector<std::string>  expected;



            rig.Run ("BP 0300");
            rig.Run ("BP C000");
            rig.Step ({ DebuggerActions::GetClearAllBreakpoints (CommandMode::AppleWin) });
            lines = rig.Do (BreakpointStep::Kind::Undo);

            for (const BreakpointInfo & info : rig.List())
            {
                expected.push_back (std::format ("Restored breakpoint {} (exec {:04X})", info.id, info.address));
            }

            Assert::AreEqual ((size_t) 2, lines.size());
            std::ranges::sort (lines);
            std::ranges::sort (expected);
            Assert::AreEqual (expected[0], lines[0]);
            Assert::AreEqual (expected[1], lines[1]);
        }


        TEST_METHOD (RedoingDeleteAllPrintsOneRemovedLinePerBreakpoint)
        {
            DetailRig                 rig;
            std::vector<std::string>  lines;



            rig.Run ("BP 0300");
            rig.Run ("BP C000");
            rig.Step ({ DebuggerActions::GetClearAllBreakpoints (CommandMode::AppleWin) });
            rig.Do (BreakpointStep::Kind::Undo);
            lines = rig.Do (BreakpointStep::Kind::Redo);

            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::IsTrue (lines[0].starts_with ("Removed breakpoint "));
            Assert::IsTrue (lines[1].starts_with ("Removed breakpoint "));
        }


        TEST_METHOD (UndoingAnEditKeepsTheHitCount)
        {
            DetailRig  rig;
            int        id = 0;



            rig.Run ("BP 0300");
            id = rig.List().at (0).id;
            rig.SetHits (id, 7);

            rig.Step ({ DebuggerActions::GetEditBreakpoint (id, "BP 0302", CommandMode::AppleWin) });
            Assert::AreEqual ((uint32_t) 0, rig.List().at (0).hits, L"an edit starts the count afresh");

            rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((Word) 0x0300,  rig.List().at (0).address);
            Assert::AreEqual ((uint32_t) 7,   rig.List().at (0).hits);
        }
    };
}
