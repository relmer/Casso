#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DirectPaneActionTests
//
//  The watches pane's edits and undo, and the breakpoints pane's changes,
//  undo and redo, run their commands directly (FR-135). What the user typed
//  is evaluated once, in the session's mode, when the action runs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DirectPaneActionTests)
{
public:

    static void AssertParsesTo (ControllerRig & rig, const std::string & line, const DebuggerAction & action)
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
        std::wstring         what   (line.begin(), line.end());



        Assert::IsTrue   (parsed.status == ParseStatus::Ok, what.c_str());
        Assert::AreEqual ((int) parsed.command.verb,  (int) action.command.verb,  what.c_str());
        Assert::AreEqual (parsed.command.sourceName,  action.command.sourceName,  what.c_str());
        Assert::AreEqual (parsed.command.a1,          action.command.a1,          what.c_str());
        Assert::AreEqual (parsed.command.hasA1,       action.command.hasA1,       what.c_str());
        Assert::IsTrue   (parsed.command.values == action.command.values,         what.c_str());
        Assert::IsTrue   (parsed.command.mask   == action.command.mask,           what.c_str());
        Assert::AreEqual (parsed.command.text,        action.command.text,        what.c_str());
        Assert::AreEqual (parsed.command.count,       action.command.count,       what.c_str());
        Assert::AreEqual (line, action.echo, what.c_str());
    }



    static Byte Peek (ControllerRig & rig, Word address)
    {
        return rig.machine.GetMemoryBus().ReadByte (address);
    }



    TEST_METHOD (ThePanesBuildersAreTheCommandsTheirLinesParseTo)
    {
        ControllerRig  rig;
        CommandMode    aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "BPC *",             DebuggerActions::GetClearAllBreakpoints  (aw));
        AssertParsesTo (rig, "BPE *",             DebuggerActions::GetEnableAllBreakpoints (true,  aw));
        AssertParsesTo (rig, "BPD *",             DebuggerActions::GetEnableAllBreakpoints (false, aw));
        AssertParsesTo (rig, "BPEDIT 3 BP 0302",  DebuggerActions::GetEditBreakpoint       (3, "BP 0302", aw));
        AssertParsesTo (rig, "BPCHANGE 3 eTs",    DebuggerActions::GetChangeBreakpoint     (3, false, true, false, aw));
        AssertParsesTo (rig, "MEW 0400 1234",     DebuggerActions::GetEnterWord            (0x0400, (Word) 0x1234, aw));
        AssertParsesTo (rig, "MEB 0402 7F",       DebuggerActions::GetEnterByte            (0x0402, (Byte) 0x7F, aw));
        AssertParsesTo (rig, "R PC 300",          DebuggerActions::GetSetRegister          ("PC", (Word) 0x0300, aw));
    }



    TEST_METHOD (AWatchValueEditRunsDirectlyWithWhatWasTypedEvaluated)
    {
        ControllerRig                rig;
        DebuggerViewSnapshot         snapshot;
        std::vector<DebuggerAction>  actions;



        snapshot.watches = { { 3, 0x0400, "0000" } };
        actions          = DebuggerViewState::GetWatchEditActions (snapshot, 3, std::nullopt, 1, "BEEF", CommandMode::AppleWin);

        Assert::AreEqual ((size_t) 1, actions.size());
        Assert::AreEqual ((int) DebugVerb::EnterWords, (int) actions[0].command.verb, L"built directly, not parsed from text");

        rig.view.ExecuteAction (rig.controller.GetSession(), actions[0]);

        Assert::AreEqual ((Byte) 0xEF, Peek (rig, 0x0400));
        Assert::AreEqual ((Byte) 0xBE, Peek (rig, 0x0401));
    }



    TEST_METHOD (AnAutomaticWatchEditWritesItsRegisterAndAddress)
    {
        ControllerRig         rig;
        DebuggerViewSnapshot  snapshot;



        snapshot.autoWatches = { { "R:A", "A", "10" }, { "M:0402", "$0402", "00" } };

        for (const DebuggerAction & action : DebuggerViewState::GetWatchEditActions (snapshot, std::nullopt, 0, 1, "42", CommandMode::AppleWin))
        {
            rig.view.ExecuteAction (rig.controller.GetSession(), action);
        }

        for (const DebuggerAction & action : DebuggerViewState::GetWatchEditActions (snapshot, std::nullopt, 1, 1, "99", CommandMode::AppleWin))
        {
            rig.view.ExecuteAction (rig.controller.GetSession(), action);
        }

        Assert::AreEqual ((Byte) 0x42, rig.controller.GetSession().GetTarget().GetRegisters().a);
        Assert::AreEqual ((Byte) 0x99, Peek (rig, 0x0402));
    }



    TEST_METHOD (AMovedWatchRunsAsAClearAndAnAdd)
    {
        ControllerRig         rig;
        DebuggerViewSnapshot  snapshot;



        rig.Run ("W 0400");
        snapshot = rig.view.Build (rig.controller.GetSession());
        Assert::AreEqual ((size_t) 1, snapshot.watches.size());

        for (const DebuggerAction & action : DebuggerViewState::GetWatchEditActions (snapshot, snapshot.watches[0].id, std::nullopt, 0, "0500", CommandMode::AppleWin))
        {
            rig.view.ExecuteAction (rig.controller.GetSession(), action);
        }

        snapshot = rig.view.Build (rig.controller.GetSession());
        Assert::AreEqual ((size_t) 1, snapshot.watches.size());
        Assert::AreEqual ((Word) 0x0500, snapshot.watches[0].address);
    }



    TEST_METHOD (AWatchValuesUndoWritesBackTheWordItHeld)
    {
        ControllerRig                                rig;
        DebuggerViewSnapshot                         before;
        std::optional<DebuggerViewState::WatchUndo>  undo;
        std::optional<std::vector<DebuggerAction>>   actions;



        before.watches = { { 3, 0x0400, "1234" } };
        undo           = DebuggerViewState::GetWatchUndo (before, 3, std::nullopt, 1, CommandMode::AppleWin);
        Assert::IsTrue (undo.has_value());

        actions = DebuggerViewState::GetWatchUndoActions (before, *undo, CommandMode::AppleWin);
        Assert::IsTrue (actions.has_value());

        for (const DebuggerAction & action : *actions)
        {
            rig.view.ExecuteAction (rig.controller.GetSession(), action);
        }

        Assert::AreEqual ((Byte) 0x34, Peek (rig, 0x0400));
        Assert::AreEqual ((Byte) 0x12, Peek (rig, 0x0401));
    }



    TEST_METHOD (TypedTextThatDoesNotEvaluateWritesNothing)
    {
        ControllerRig                rig;
        DebuggerViewSnapshot         snapshot;
        std::vector<std::string>     lines;
        std::vector<DebuggerAction>  actions;
        Byte                         held = 0;



        held             = Peek (rig, 0x0400);
        snapshot.watches = { { 3, 0x0400, "0000" } };
        actions          = DebuggerViewState::GetWatchEditActions (snapshot, 3, std::nullopt, 1, "NOSUCHSYMBOL", CommandMode::AppleWin);
        lines            = rig.view.ExecuteAction (rig.controller.GetSession(), actions.at (0));

        Assert::AreEqual (held, Peek (rig, 0x0400));
        Assert::IsTrue   (lines.size() > 1, L"the reply says why");
    }



    //  An undo's commands are echoed in the session's mode, as a typed line
    //  would be, not in the AppleWin words the pane once sent.
    TEST_METHOD (ABreakpointUndoInAnotherModePrintsWhatItRemoved)
    {
        ControllerRig             rig;
        BreakpointStep            step;
        BreakpointStep            undo;
        std::vector<std::string>  lines;



        rig.Run ("MODE GSSQUARED");

        step.actions = { DebuggerActions::GetDefineBreakpoint ("BP 0300", CommandMode::GSSquared) };
        rig.view.ExecuteBreakpointStep (rig.controller.GetSession(), step);
        Assert::AreEqual ((size_t) 1, rig.view.Build (rig.controller.GetSession()).breakpoints.size());

        undo.kind = BreakpointStep::Kind::Undo;
        lines     = rig.view.ExecuteBreakpointStep (rig.controller.GetSession(), undo);

        Assert::AreEqual ((size_t) 0, rig.view.Build (rig.controller.GetSession()).breakpoints.size());
        Assert::AreEqual ((size_t) 1, lines.size(), L"no command is echoed");
        Assert::IsTrue   (lines[0].starts_with ("Removed breakpoint "));
    }
};
