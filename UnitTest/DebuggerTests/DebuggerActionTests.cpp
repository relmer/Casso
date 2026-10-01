#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActionTests
//
//  The window's controls run their commands directly (FR-135). Each action is
//  the command its typed line parses to, so what it does is unchanged, and
//  the command it echoes is display only.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerActionTests)
{
public:

    static void AssertSameCommand (const DebugCommand & expected, const DebugCommand & actual, const wchar_t * what)
    {
        Assert::AreEqual ((int) expected.verb,  (int) actual.verb,  what);
        Assert::AreEqual (expected.sourceName,  actual.sourceName,  what);
        Assert::AreEqual (expected.a1,          actual.a1,          what);
        Assert::AreEqual (expected.a2,          actual.a2,          what);
        Assert::AreEqual (expected.a3,          actual.a3,          what);
        Assert::AreEqual (expected.hasA1,       actual.hasA1,       what);
        Assert::AreEqual (expected.hasA2,       actual.hasA2,       what);
        Assert::AreEqual (expected.hasA3,       actual.hasA3,       what);
        Assert::IsTrue   (expected.values == actual.values,         what);
        Assert::IsTrue   (expected.mask   == actual.mask,           what);
        Assert::AreEqual (expected.text,        actual.text,        what);
        Assert::AreEqual (expected.count,       actual.count,       what);
        Assert::AreEqual ((int) expected.mode,  (int) actual.mode,  what);
    }



    static void AssertParsesTo (ControllerRig & rig, const std::string & line, const DebuggerAction & action)
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
        std::wstring         what   (line.begin(), line.end());



        Assert::IsTrue (parsed.status == ParseStatus::Ok, what.c_str());
        AssertSameCommand (parsed.command, action.command, what.c_str());
        Assert::AreEqual (line, action.echo, what.c_str());
    }



    TEST_METHOD (EachActionIsTheCommandItsLineParsesTo)
    {
        ControllerRig         rig;
        DebuggerViewSnapshot  none;
        CommandMode           aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "G",            DebuggerActions::GetRun              (aw));
        AssertParsesTo (rig, "T",            DebuggerActions::GetStepInto         (aw));
        AssertParsesTo (rig, "P",            DebuggerActions::GetStepOver         (aw));
        AssertParsesTo (rig, "RTS",          DebuggerActions::GetStepOut          (aw));
        AssertParsesTo (rig, "G 0300",       DebuggerActions::GetRunToCursor      (0x0300));
        AssertParsesTo (rig, "BP 0300",      DebuggerActions::GetToggleBreakpoint (none, 0x0300, aw));
        AssertParsesTo (rig, "BPE 3",        DebuggerActions::GetEnableBreakpoint (3, true,  aw));
        AssertParsesTo (rig, "BPD 3",        DebuggerActions::GetEnableBreakpoint (3, false, aw));
        AssertParsesTo (rig, "BPC 3",        DebuggerActions::GetClearBreakpoint  (3, aw));
        AssertParsesTo (rig, "WC 2",         DebuggerActions::GetClearWatch       (2, aw));
        AssertParsesTo (rig, "PANEL mmu",    DebuggerActions::GetPanel            ("mmu", true,  aw));
        AssertParsesTo (rig, "PANEL CLOSE mmu", DebuggerActions::GetPanel         ("mmu", false, aw));
        AssertParsesTo (rig, "MEB 0400 41",  DebuggerActions::GetPoke             (0x0400, 0x41, aw));
        AssertParsesTo (rig, "HISTORY ON",   DebuggerActions::GetTraceToggle      (false, aw));
        AssertParsesTo (rig, "HISTORY OFF",  DebuggerActions::GetTraceToggle      (true,  aw));
        AssertParsesTo (rig, "MODE MONITOR", DebuggerActions::GetSetMode          (CommandMode::Monitor, aw));
        AssertParsesTo (rig, "R P 30",       DebuggerActions::GetSetRegister      ("P", 0x30, aw));
    }



    //  GSSquared's bp is not parsed: the action runs as it is, and the echo
    //  shows GSSquared's word behind the prompt.
    TEST_METHOD (AnActionRunsDirectlyAndEchoesTheModesWord)
    {
        ControllerRig             rig;
        DebuggerViewSnapshot      none;
        DebuggerViewSnapshot      after;
        std::vector<std::string>  lines;



        rig.Run ("MODE GSSQUARED");

        lines = rig.view.ExecuteAction (rig.controller.GetSession(),
                                        DebuggerActions::GetToggleBreakpoint (none, 0x0300, CommandMode::GSSquared));
        after = rig.view.Build (rig.controller.GetSession());

        Assert::IsFalse  (lines.empty());
        Assert::AreEqual (std::string (">bp 0300"), lines[0]);
        Assert::AreEqual ((size_t) 1, after.breakpoints.size());
        Assert::AreEqual ((Word) 0x0300, after.breakpoints[0].address);
    }



    //  A second toggle on the same line clears the breakpoint by its id.
    TEST_METHOD (ASecondToggleClearsTheBreakpoint)
    {
        ControllerRig         rig;
        DebuggerViewSnapshot  none;
        DebuggerViewSnapshot  set;
        DebuggerViewSnapshot  cleared;



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetToggleBreakpoint (none, 0x0300, CommandMode::AppleWin));
        set = rig.view.Build (rig.controller.GetSession());

        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetToggleBreakpoint (set, 0x0300, CommandMode::AppleWin));
        cleared = rig.view.Build (rig.controller.GetSession());

        Assert::AreEqual ((size_t) 1, set.breakpoints.size());
        Assert::AreEqual ((size_t) 0, cleared.breakpoints.size());
    }



    //  PANEL is the window's own, so its action opens the panel on the panes.
    TEST_METHOD (APanelActionOpensThePanel)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;



        lines = rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetPanel ("mmu", true, CommandMode::AppleWin));

        Assert::IsTrue   (rig.view.IsPanelOpen ("mmu"));
        Assert::AreEqual (std::string (">PANEL mmu"), lines[0]);
    }



    TEST_METHOD (APokeWritesMemory)
    {
        ControllerRig  rig;



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetPoke (0x0400, 0x5A, CommandMode::AppleWin));

        Assert::AreEqual ((Byte) 0x5A, rig.machine.GetMemoryBus().ReadByte (0x0400));
    }



    //  Run to cursor echoes Casso's G in every mode, since GSSquared's g takes
    //  no address.
    TEST_METHOD (RunToCursorEchoesCassosG)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;



        rig.Run ("MODE GSSQUARED");

        lines = rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetRunToCursor (0x0305));

        Assert::AreEqual (std::string (">G 0305"), lines[0]);
    }
};
