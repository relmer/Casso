#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointHistoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryRig
    //
    //  The breakpoints pane's actions, undo and redo run on a real session as
    //  the window's CPU thread runs them, with no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HistoryRig : public ControllerRig
    {
    public:
        std::vector<std::string>  Step (std::vector<DebuggerAction> actions)
        {
            BreakpointStep  step;

            step.actions = std::move (actions);
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<std::string>  Do (BreakpointStep::Kind kind, const std::string & path = std::string())
        {
            BreakpointStep  step;

            step.kind = kind;
            step.path = path;
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<BreakpointInfo>  List()
        {
            BreakpointListData  list;

            BreakpointHandlers::ListAll (controller.GetSession(), list);
            return list.breakpoints;
        }

        bool  CanUndo() { return view.Build (controller.GetSession()).canUndoBreakpoints; }
        bool  CanRedo() { return view.Build (controller.GetSession()).canRedoBreakpoints; }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndoTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UndoTests)
    {
    public:

        TEST_METHOD (ADeletedBreakpointComesBackWithItsKindRangeConditionStateAndWhenHit)
        {
            HistoryRig      rig;
            BreakpointInfo  before;
            BreakpointInfo  after;



            rig.Run ("BPMW 0400:04FF IF A=41");
            before = rig.List().at (0);
            rig.Run (std::format ("BPCHANGE {} eTs", before.id));
            before = rig.List().at (0);

            rig.Step ({ DebuggerActions::GetClearBreakpoint (before.id, CommandMode::AppleWin) });
            Assert::AreEqual ((size_t) 0, rig.List().size());

            rig.Do (BreakpointStep::Kind::Undo);
            Assert::AreEqual ((size_t) 1, rig.List().size());
            after = rig.List().at (0);

            Assert::AreEqual (BreakpointHandlers::MakeDefinition (before), BreakpointHandlers::MakeDefinition (after));
            Assert::IsFalse  (after.enabled);
            Assert::IsTrue   (after.temporary);
            Assert::IsFalse  (after.stops);

            rig.Do (BreakpointStep::Kind::Redo);
            Assert::AreEqual ((size_t) 0, rig.List().size(), L"redo deletes it again, under its new id");
        }


        TEST_METHOD (DisableAllIsOneStep)
        {
            HistoryRig  rig;



            rig.Run ("BP 0300");
            rig.Run ("BP 0302");
            rig.Run ("BPMR 0400");

            rig.Step ({ DebuggerActions::GetEnableAllBreakpoints (false, CommandMode::AppleWin) });
            Assert::IsFalse (rig.List().at (2).enabled);

            rig.Do (BreakpointStep::Kind::Undo);

            for (const BreakpointInfo & info : rig.List())
            {
                Assert::IsTrue (info.enabled);
            }

            Assert::IsFalse (rig.CanUndo(), L"the three were one step");
            Assert::IsTrue  (rig.CanRedo());
        }


        TEST_METHOD (DeleteAllIsOneStepAndUndoingItRestoresEvery)
        {
            HistoryRig  rig;



            rig.Run ("BP 0300");
            rig.Run ("BPMW 0400:0410");
            rig.Step ({ DebuggerActions::GetClearAllBreakpoints (CommandMode::AppleWin) });
            Assert::AreEqual ((size_t) 0, rig.List().size());

            rig.Do (BreakpointStep::Kind::Undo);
            Assert::AreEqual ((size_t) 2, rig.List().size());
        }


        TEST_METHOD (ANewActionClearsRedo)
        {
            HistoryRig  rig;



            rig.Step ({ DebuggerActions::GetDefineBreakpoint ("BP 0300", CommandMode::AppleWin) });
            rig.Do   (BreakpointStep::Kind::Undo);
            Assert::IsTrue (rig.CanRedo());

            rig.Step ({ DebuggerActions::GetDefineBreakpoint ("BP 0302", CommandMode::AppleWin) });
            Assert::IsFalse (rig.CanRedo());
        }


        TEST_METHOD (AnEditIsUndoneToItsOldDefinition)
        {
            HistoryRig  rig;
            int         id = 0;



            rig.Run ("BP 0300");
            id = rig.List().at (0).id;
            rig.Step ({ DebuggerActions::GetEditBreakpoint (id, "BP 0302", CommandMode::AppleWin) });
            rig.Do   (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((Word) 0x0300, rig.List().at (0).address);
            Assert::AreEqual (id,            rig.List().at (0).id);
        }


        TEST_METHOD (AStepOnABreakpointAddedBackFollowsItsNewId)
        {
            HistoryRig  rig;
            int         id = 0;



            rig.Run ("BP 0300");
            id = rig.List().at (0).id;
            rig.Step ({ DebuggerActions::GetClearBreakpoint (id, CommandMode::AppleWin) });
            rig.Do   (BreakpointStep::Kind::Undo);
            id = rig.List().at (0).id;

            rig.Step ({ DebuggerActions::GetEnableBreakpoint (id, false, CommandMode::AppleWin) });
            rig.Do   (BreakpointStep::Kind::Undo);
            Assert::IsTrue (rig.List().at (0).enabled);

            rig.Do   (BreakpointStep::Kind::Undo);
            Assert::AreEqual ((size_t) 1, rig.List().size(), L"undoing the delete's undo is not possible: it is the oldest step");
        }


        //  The console's changes are not the pane's to undo, and an undo
        //  whose breakpoint the console removed leaves the others alone.
        TEST_METHOD (AnUndoWhoseBreakpointTheConsoleRemovedDoesNothing)
        {
            HistoryRig  rig;
            int         id = 0;



            rig.Step ({ DebuggerActions::GetDefineBreakpoint ("BP 0300", CommandMode::AppleWin) });
            id = rig.List().at (0).id;
            rig.Run (std::format ("BPC {}", id));
            rig.Run ("BP 0302");

            rig.Do (BreakpointStep::Kind::Undo);

            Assert::AreEqual ((size_t) 1,      rig.List().size());
            Assert::AreEqual ((Word) 0x0302,   rig.List().at (0).address);
        }


        TEST_METHOD (TheStepEchoesEachLineItRuns)
        {
            HistoryRig                rig;
            std::vector<std::string>  lines;



            lines = rig.Step ({ DebuggerActions::GetDefineBreakpoint ("BP 0300", CommandMode::AppleWin) });
            Assert::IsTrue (std::ranges::find (lines, DebugSession::GetPrompt (CommandMode::AppleWin) + std::string ("BP 0300")) != lines.end());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ImportTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ImportTests)
    {
    public:

        TEST_METHOD (AnExportImportsBackBesideTheBreakpointsAlreadySet)
        {
            HistoryRig                   rig;
            std::vector<BreakpointInfo>  saved;
            std::vector<BreakpointInfo>  now;



            rig.Run ("BP 0300");
            rig.Run ("BPMW 0400:04FF");
            rig.Run (std::format ("BPCHANGE {} eTs", rig.List().at (1).id));
            saved = rig.List();
            rig.Run ("BPSAVE C:\\Work\\bp.txt");

            rig.Run ("BPC *");
            rig.Run ("BP 0302");
            rig.Do  (BreakpointStep::Kind::Import, "C:\\Work\\bp.txt");
            now = rig.List();

            Assert::AreEqual ((size_t) 3, now.size(), L"the import adds; its BPC * is skipped");
            Assert::AreEqual ((Word) 0x0302, now[0].address);

            for (size_t i = 0; i < saved.size(); i++)
            {
                Assert::AreEqual (BreakpointHandlers::MakeDefinition (saved[i]), BreakpointHandlers::MakeDefinition (now[i + 1]));
                Assert::AreEqual (saved[i].enabled,   now[i + 1].enabled);
                Assert::AreEqual (saved[i].temporary, now[i + 1].temporary);
                Assert::AreEqual (saved[i].stops,     now[i + 1].stops);
            }

            rig.Do (BreakpointStep::Kind::Undo);
            Assert::AreEqual ((size_t) 1, rig.List().size(), L"the import is one step");
        }


        TEST_METHOD (LinesThatAreNotBreakpointsAreSkippedAndCounted)
        {
            HistoryRig                rig;
            std::vector<std::string>  lines;
            Byte                      a = 0;



            rig.files.WriteAllText (L"C:\\Work\\mixed.txt", "BPC *\nR A 55\n\nBP 0300\n");
            rig.Run ("BP 0302");
            a = rig.controller.GetSession().GetTarget().GetRegisters().a;

            lines = rig.Do (BreakpointStep::Kind::Import, "C:\\Work\\mixed.txt");

            Assert::AreEqual ((size_t) 2, rig.List().size());
            Assert::AreEqual (a, rig.controller.GetSession().GetTarget().GetRegisters().a, L"the register line is not run");
            Assert::IsTrue   (lines.back().find ("skipped 2 lines") != std::string::npos);
        }
    };
}
