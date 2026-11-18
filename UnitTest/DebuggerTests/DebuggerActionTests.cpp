#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/BreakpointActions.h"
#include "Ui/Debugger/MemoryActions.h"
#include "Ui/Debugger/RunActions.h"
#include "Ui/Debugger/SessionActions.h"
#include "Ui/Debugger/SymbolActions.h"
#include "Ui/Debugger/WatchActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Core/TextEncoding.h"


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



    //  The action as the window sends it while the console lists actions as
    //  commands.
    static DebuggerAction Echoed (DebuggerAction action)
    {
        action.isEchoed = true;
        return action;
    }



    static void AssertParsesTo (ControllerRig & rig, const std::string & line, const DebuggerAction & action)
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
        std::wstring         what   = TextEncoding::NarrowToWide (line);



        Assert::IsTrue (parsed.status == ParseStatus::Ok, what.c_str());
        AssertSameCommand (parsed.command, action.command, what.c_str());
        Assert::AreEqual (line, action.echo, what.c_str());
    }



    TEST_METHOD (EachActionIsTheCommandItsLineParsesTo)
    {
        ControllerRig         rig;
        DebuggerViewSnapshot  none;
        CommandMode           aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "G",            RunActions::GetRun              (aw));
        AssertParsesTo (rig, "T",            RunActions::GetStepInto         (aw));
        AssertParsesTo (rig, "P",            RunActions::GetStepOver         (aw));
        AssertParsesTo (rig, "RTS",          RunActions::GetStepOut          (aw));
        AssertParsesTo (rig, "G 0300",       RunActions::GetRunToCursor      (0x0300));
        AssertParsesTo (rig, "BP 0300",      BreakpointActions::GetToggleBreakpoint (none, 0x0300, aw));
        AssertParsesTo (rig, "BPE 3",        BreakpointActions::GetEnableBreakpoint (3, true,  aw));
        AssertParsesTo (rig, "BPD 3",        BreakpointActions::GetEnableBreakpoint (3, false, aw));
        AssertParsesTo (rig, "BPC 3",        BreakpointActions::GetClearBreakpoint  (3, aw));
        AssertParsesTo (rig, "WC 2",         WatchActions::GetClearWatch       (2, aw));
        AssertParsesTo (rig, "PANEL mmu",    SessionActions::GetPanel            ("mmu", true,  aw));
        AssertParsesTo (rig, "PANEL CLOSE mmu", SessionActions::GetPanel         ("mmu", false, aw));
        AssertParsesTo (rig, "MEB 0400 41",  MemoryActions::GetPoke             (0x0400, 0x41, aw));
        AssertParsesTo (rig, "HISTORY ON",   SessionActions::GetTraceToggle      (false, aw));
        AssertParsesTo (rig, "HISTORY OFF",  SessionActions::GetTraceToggle      (true,  aw));
        AssertParsesTo (rig, "MODE MONITOR", SessionActions::GetSetMode          (CommandMode::Monitor, aw));
        AssertParsesTo (rig, "R P 30",       MemoryActions::GetSetRegister      ("P", 0x30, aw));
        AssertParsesTo (rig, "PROJECT MERGE", SessionActions::GetProject         ("MERGE", "", aw));
        AssertParsesTo (rig, "PROJECT SAVEAS \"game.cassoproj\"", SessionActions::GetProject ("SAVEAS", "game.cassoproj", aw));
        AssertParsesTo (rig, "DB 0300:030F",  SymbolActions::GetMarkData       ("DB", 0x0300, 0x030F, aw));
        AssertParsesTo (rig, "ASC 0300:030F", SymbolActions::GetMarkData       ("ASC", 0x0300, 0x030F, aw));
        AssertParsesTo (rig, "X 0300:030F",   SymbolActions::GetClearData      (0x0300, 0x030F, aw));
        AssertParsesTo (rig, "SYM LOOP = 0300", SymbolActions::GetAddSymbol    ("LOOP", 0x0300, aw));
        AssertParsesTo (rig, "BMA 0300",      WatchActions::GetAddBookmark    (0x0300, aw));
        AssertParsesTo (rig, "BMC 2",         WatchActions::GetClearBookmark  (2, aw));
        AssertParsesTo (rig, "STOPWATCH 0300 0310", RunActions::GetStopwatch (0x0300, 0x0310, aw));
        AssertParsesTo (rig, "JSR 0300",      RunActions::GetCallSubroutine (0x0300, aw));
        AssertParsesTo (rig, "SKIP 0300",     RunActions::GetSkipRoutine    ("", 0x0300, aw));
        AssertParsesTo (rig, "SYMUSER ON",    SymbolActions::GetEnableSymbols  ("USER", true, aw));
        AssertParsesTo (rig, "SYMMAIN OFF",   SymbolActions::GetEnableSymbols  ("MAIN", false, aw));
        AssertParsesTo (rig, "SYMUSER CLEAR", SymbolActions::GetClearSymbols   ("USER", aw));
        AssertParsesTo (rig, "SYMUSER SAVE \"out.sym\"", SymbolActions::GetSaveSymbols ("USER", "out.sym", aw));
        AssertParsesTo (rig, "SYMUSER ! LOOP", SymbolActions::GetRemoveSymbol  ("USER", "LOOP", aw));
        AssertParsesTo (rig, "PROFILE ON",    SessionActions::GetProfile        ("ON", aw));
        AssertParsesTo (rig, "SKIP -0300",    RunActions::GetRemoveSkip     ({ "", 0x0300, 0x0300 }, aw));
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

        lines = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(),
                                                      Echoed (BreakpointActions::GetToggleBreakpoint (none, 0x0300, CommandMode::GSSquared)));
        after = rig.view.Build (rig.controller.GetSession());

        Assert::IsFalse  (lines.empty());
        Assert::AreEqual (std::string (">bp 0300"), lines[0]);
        Assert::AreEqual ((size_t) 1, after.breakpoints.size());
        Assert::AreEqual ((Word) 0x0300, after.breakpoints[0].address);
    }



    //  A second toggle on the same line clears the breakpoint by its id.
    TEST_METHOD (ASecondToggleClearsTheBreakpoint)
    {
        std::unique_ptr<ControllerRig>   held    = std::make_unique<ControllerRig>();
        ControllerRig                  & rig     = *held;
        DebuggerViewSnapshot             none;
        DebuggerViewSnapshot             set;
        DebuggerViewSnapshot             cleared;



        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), BreakpointActions::GetToggleBreakpoint (none, 0x0300, CommandMode::AppleWin));
        set = rig.view.Build (rig.controller.GetSession());

        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), BreakpointActions::GetToggleBreakpoint (set, 0x0300, CommandMode::AppleWin));
        cleared = rig.view.Build (rig.controller.GetSession());

        Assert::AreEqual ((size_t) 1, set.breakpoints.size());
        Assert::AreEqual ((size_t) 0, cleared.breakpoints.size());
    }



    //  PANEL is the window's own, so its action opens the panel on the panes.
    TEST_METHOD (APanelActionOpensThePanel)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;



        lines = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), Echoed (SessionActions::GetPanel ("mmu", true, CommandMode::AppleWin)));

        Assert::IsTrue   (rig.view.GetPanels().IsPanelOpen ("mmu"));
        Assert::AreEqual (std::string (">PANEL mmu"), lines[0]);
    }



    TEST_METHOD (APokeWritesMemory)
    {
        ControllerRig  rig;



        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), MemoryActions::GetPoke (0x0400, 0x5A, CommandMode::AppleWin));

        Assert::AreEqual ((Byte) 0x5A, rig.machine.GetMemoryBus().ReadByte (0x0400));
    }



    //  File > Unload symbol or debug file's row for the debug file runs SRC
    //  CLEAR, which unloads the debug file rather than only its symbols.
    TEST_METHOD (TheDebugFilesUnloadRowIsSrcClear)
    {
        DebuggerAction  action = SymbolActions::GetUnloadDebugFile (CommandMode::AppleWin);



        Assert::AreEqual (std::string ("SRC CLEAR"), action.echo);
        Assert::AreEqual (std::string ("CLEAR"),     action.command.text);
        Assert::IsTrue   (action.command.verb == DebugVerb::ShowSource);
    }



    //  A memory window's undo writes only while memory contains the action's
    //  required bytes, whatever the window last read.
    TEST_METHOD (APatchWithExpectedBytesWritesOnlyOverThem)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;
        const Byte                written[]  = { 0xA9, 0xEA };
        const Byte                replaced[] = { 0x11, 0x22 };



        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), MemoryActions::GetPatch (0x0400, written, CommandMode::AppleWin));
        rig.machine.GetMemoryBus().WriteByte (0x0401, 0x33);

        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), MemoryActions::GetPatch (0x0400, replaced, CommandMode::AppleWin, written));
        lines = rig.view.Build (rig.controller.GetSession()).actionReply;

        Assert::AreEqual ((Byte) 0xA9, rig.machine.GetMemoryBus().ReadByte (0x0400), L"nothing was written");
        Assert::IsTrue   (std::ranges::any_of (lines, [] (const std::string & line) { return line.find ("$0401 has changed since the edit") != std::string::npos; }), L"the reply has the reason");

        rig.machine.GetMemoryBus().WriteByte (0x0401, 0xEA);
        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), MemoryActions::GetPatch (0x0400, replaced, CommandMode::AppleWin, written));

        Assert::AreEqual ((Byte) 0x11, rig.machine.GetMemoryBus().ReadByte (0x0400));
        Assert::AreEqual ((Byte) 0x22, rig.machine.GetMemoryBus().ReadByte (0x0401));
    }



    //  Run to cursor echoes Casso's G in every mode, since GSSquared's g takes
    //  no address.
    TEST_METHOD (RunToCursorEchoesCassosG)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;



        rig.Run ("MODE GSSQUARED");

        lines = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), Echoed (RunActions::GetRunToCursor (0x0305)));

        Assert::AreEqual (std::string (">G 0305"), lines[0]);
    }



    //  An action from the window that isn't echoed doesn't add a console
    //  line, even when it fails: its error goes in the snapshot. The next
    //  action's snapshot has no reply, so the error is cleared.
    TEST_METHOD (AnActionThatIsNotEchoedPutsItsErrorInTheSnapshot)
    {
        ControllerRig             rig;
        DebuggerViewSnapshot      none;
        DebuggerViewSnapshot      snapshot;
        std::vector<std::string>  set;
        std::vector<std::string>  failed;
        uint32_t                  serial = 0;



        set    = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), BreakpointActions::GetToggleBreakpoint (none, 0x0300, CommandMode::AppleWin));
        failed = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), WatchActions::GetClearWatch (7, CommandMode::AppleWin));

        Assert::IsTrue (set.empty(),    L"a breakpoint set from a control printed lines");
        Assert::IsTrue (failed.empty(), L"a failed action printed its error in the console");

        snapshot = rig.view.Build (rig.controller.GetSession());
        serial   = snapshot.actionReplySerial;

        Assert::IsFalse (snapshot.actionReply.empty(), L"the error is in the snapshot");

        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), BreakpointActions::GetToggleBreakpoint (none, 0x0310, CommandMode::AppleWin));
        snapshot = rig.view.Build (rig.controller.GetSession());

        Assert::AreNotEqual (serial, snapshot.actionReplySerial, L"a new action has a new serial");
        Assert::IsTrue      (snapshot.actionReply.empty(), L"and its success clears the error");
    }



    //  The snapshot has where the window displays the reply: the action's
    //  pane, or a message box, and whether the reply is an error.
    TEST_METHOD (TheSnapshotHasWhereTheReplyGoes)
    {
        ControllerRig         rig;
        DebuggerAction        failing = WatchActions::GetClearWatch (7, CommandMode::AppleWin);
        DebuggerViewSnapshot  snapshot;



        failing.replyPane = L"Watches";
        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), failing);
        snapshot = rig.view.Build (rig.controller.GetSession());

        Assert::AreEqual (std::wstring (L"Watches"), snapshot.actionReplyPane);
        Assert::IsFalse  (snapshot.isActionReplyBoxed);
        Assert::IsTrue   (snapshot.isActionReplyError);

        failing.replyPane.clear();
        failing.isReplyBoxed = true;
        rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), failing);

        Assert::IsTrue (rig.view.Build (rig.controller.GetSession()).isActionReplyBoxed);
    }



    //  An action whose reply is its result, as a search's matches are, has
    //  the reply in the snapshot and no console line.
    TEST_METHOD (AnActionsResultIsInTheSnapshot)
    {
        ControllerRig             rig;
        std::vector<std::string>  lines;



        rig.machine.GetMemoryBus().WriteByte (0x6000, 0x5A);
        lines = rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), MemoryActions::GetSearchMemory ("5A", CommandMode::AppleWin));

        Assert::IsTrue  (lines.empty(), L"the search's matches went to the console");
        Assert::IsFalse (rig.view.Build (rig.controller.GetSession()).actionReply.empty(), L"the search's matches are missing");
    }
};
