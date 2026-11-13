#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Core/TextEncoding.h"
#include "Debugger/DiskBreakSpecText.h"


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
        std::wstring         what   = TextEncoding::NarrowToWide (line);



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



    //  An edit from the window has its definition as fields, and its echo
    //  is the line that would make the same edit.
    TEST_METHOD (AnEditHasItsDefinitionAsFieldsAndItsLineAsItsEcho)
    {
        DebuggerAction  action = DebuggerActions::GetEditBreakpoint (3, BreakpointDefinition::MakeExecute ("0302"), CommandMode::AppleWin);



        Assert::IsTrue   (action.command.verb == DebugVerb::EditBreakpoint);
        Assert::AreEqual ((uint32_t) 3, action.command.count);
        Assert::AreEqual (std::string(), action.command.text, L"no definition text for the handler to parse");
        Assert::IsTrue   (action.breakpoint.has_value());
        Assert::AreEqual (std::string ("BPEDIT 3 BP 0302"), action.echo);
    }



    //  Each type's fields make the command its line parses to, so a
    //  breakpoint set in the window is the one the console would set.
    TEST_METHOD (ADefinitionMakesTheCommandItsLineParsesTo)
    {
        using Type = BreakpointDefinition::Type;

        ControllerRig                      rig;
        std::vector<BreakpointDefinition>  definitions =
        {
            { Type::Execute,       false, false, "0300" },
            { Type::Execute,       false, false, "0300:030F", "", "A == 1" },
            { Type::Execute,       false, false, ">300" },
            { Type::Read,          false, false, "0400" },
            { Type::Write,         true,  false, "0400:04FF", "", "X == 2" },
            { Type::ReadWrite,     false, false, "C000" },
            { Type::Value,         false, false, "0006", "07", "X == 1" },
            { Type::Condition,     false, false, "", "", "A == 41" },
            { Type::Opcode,        false, false, "", "00 RTS" },
            { Type::Brk,           false, false, "", "", "X == 2" },
            { Type::Interrupt,     false, false },
            { Type::BasicError,    false, false },
            { Type::UnwrittenRead, false, false },
            { Type::BasicLine,     false, false, "100" },
            { Type::BasicVariable, false, false, "d(3,4)" },
            { Type::VideoLine,     false, false, "64:6B" },
            { Type::Beam,          false, false, "10", "1A" },
            { Type::Beam,          false, false, "vbl" },
            { Type::Disk,          false, false, "", "", "", 0, 1, "11 0" },
            { Type::Disk,          false, false, "", "", "DISK.QTRACK == 0", 0, 0, "11 0" },
            { Type::UnwrittenRead, false, true },
            { Type::VideoLine,     false, true },
            { Type::Beam,          false, true },
            { Type::Brk,           false, true },
        };



        for (const BreakpointDefinition & definition : definitions)
        {
            std::string          line   = DebuggerActions::GetBreakpointEcho (definition);
            std::wstring         what   = TextEncoding::NarrowToWide (line);
            AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
            DebugCommand         made;
            std::string          error;

            Assert::IsTrue   (parsed.status == ParseStatus::Ok, what.c_str());
            Assert::IsTrue   (AppleWinParser::TryMakeBreakpoint (definition, rig.controller.GetSession(), made, error), (what + L": " + TextEncoding::NarrowToWide (error)).c_str());
            Assert::AreEqual ((int) parsed.command.verb, (int) made.verb,  what.c_str());
            Assert::AreEqual (parsed.command.a1,         made.a1,          what.c_str());
            Assert::AreEqual (parsed.command.a2,         made.a2,          what.c_str());
            Assert::AreEqual (parsed.command.hasA1,      made.hasA1,       what.c_str());
            Assert::AreEqual (parsed.command.hasA2,      made.hasA2,       what.c_str());
            Assert::AreEqual (parsed.command.text,       made.text,        what.c_str());
            Assert::AreEqual (parsed.command.count,      made.count,       what.c_str());
            Assert::AreEqual (parsed.command.symbol,     made.symbol,      what.c_str());
            Assert::IsTrue   (parsed.command.values == made.values,        what.c_str());
            Assert::AreEqual (parsed.command.expression.text, made.expression.text, what.c_str());
            Assert::AreEqual (parsed.command.disk.has_value(), made.disk.has_value(), what.c_str());

            if (made.disk.has_value())
            {
                Assert::AreEqual (DiskBreakSpecText::FormatArguments (*parsed.command.disk), DiskBreakSpecText::FormatArguments (*made.disk), what.c_str());
            }
        }
    }



    //  A field that can't be read is an error with no command's name in it.
    TEST_METHOD (ABadFieldIsAnErrorInTheFieldsWords)
    {
        using Type = BreakpointDefinition::Type;

        ControllerRig  rig;
        DebugCommand   made;
        std::string    error;



        Assert::IsFalse (AppleWinParser::TryMakeBreakpoint ({ Type::Value, false, false, "0300", "100" }, rig.controller.GetSession(), made, error));
        Assert::AreEqual (std::string ("$100 is not a byte. The value is one byte, $00-$FF."), error);

        Assert::IsFalse (AppleWinParser::TryMakeBreakpoint ({ Type::SourceLine, false, false, "abc", "", "", 0, 0, "", "main.a65" }, rig.controller.GetSession(), made, error));
        Assert::AreEqual (std::string ("abc is not a line number."), error);

        Assert::IsFalse (AppleWinParser::TryMakeBreakpoint ({ Type::Execute }, rig.controller.GetSession(), made, error));
        Assert::AreEqual (std::string ("The address is empty."), error);
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



    //  A register watch's value edits as R, and its undo sets the register
    //  back.
    TEST_METHOD (ARegisterWatchValueEditSetsTheRegister)
    {
        ControllerRig                                rig;
        DebuggerViewSnapshot                         snapshot;
        std::vector<DebuggerAction>                  actions;
        std::optional<DebuggerViewState::WatchUndo>  undo;



        snapshot.watches                    = { { 3, 0x0000, "12" } };
        snapshot.watches[0].expression      = "a";
        snapshot.watches[0].isValueEditable = true;
        snapshot.watches[0].isRegister      = true;

        actions = DebuggerViewState::GetWatchEditActions (snapshot, 3, std::nullopt, 1, "41", CommandMode::AppleWin);
        undo    = DebuggerViewState::GetWatchUndo (snapshot, 3, std::nullopt, 1, CommandMode::AppleWin);

        Assert::AreEqual ((size_t) 1, actions.size());
        Assert::AreEqual ((int) DebugVerb::SetRegister, (int) actions[0].command.verb);

        rig.view.ExecuteAction (rig.controller.GetSession(), actions[0]);
        Assert::AreEqual ((Byte) 0x41, rig.controller.GetSession().GetTarget().GetRegisters().a);

        Assert::IsTrue (undo.has_value() && undo->actions.size() == 1 && undo->actions[0].command.verb == DebugVerb::SetRegister);
    }



    //  A symbol watch whose symbol has a register's name is memory: its value
    //  edits as a word at the symbol's address, not as the register.
    TEST_METHOD (ASymbolWatchWithARegistersNameEditsMemory)
    {
        DebuggerViewSnapshot         snapshot;
        std::vector<DebuggerAction>  actions;



        snapshot.watches                    = { { 3, 0x0400, "0000" } };
        snapshot.watches[0].expression      = "x";
        snapshot.watches[0].isValueEditable = true;

        actions = DebuggerViewState::GetWatchEditActions (snapshot, 3, std::nullopt, 1, "BEEF", CommandMode::AppleWin);

        Assert::AreEqual ((size_t) 1, actions.size());
        Assert::AreEqual ((int) DebugVerb::EnterWords, (int) actions[0].command.verb, L"the word at the symbol, not register X");
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
        Assert::IsTrue   (lines.empty(), L"the console has no line of it");
        Assert::IsFalse  (rig.view.Build (rig.controller.GetSession()).actionReply.empty(), L"the reply has the reason");
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

        step.actions = { DebuggerActions::GetDefineBreakpoint (BreakpointDefinition::MakeExecute ("0300"), CommandMode::GSSquared) };
        rig.view.ExecuteBreakpointStep (rig.controller.GetSession(), step);
        Assert::AreEqual ((size_t) 1, rig.view.Build (rig.controller.GetSession()).breakpoints.size());

        undo.kind = BreakpointStep::Kind::Undo;
        lines     = rig.view.ExecuteBreakpointStep (rig.controller.GetSession(), undo);

        Assert::AreEqual ((size_t) 0, rig.view.Build (rig.controller.GetSession()).breakpoints.size());
        Assert::AreEqual ((size_t) 1, lines.size(), L"no command is echoed");
        Assert::IsTrue   (lines[0].starts_with ("Removed breakpoint "));
    }
};
