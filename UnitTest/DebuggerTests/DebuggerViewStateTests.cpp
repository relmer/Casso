#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "EmuTests/TestMachine.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "MockDebugTarget.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "UiTests/InMemoryFileSystem.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MachineRig
    //
    //  The debugger as the window sees it: a controller over a real machine, so
    //  the panes read real memory, and a transport so a channel client can share
    //  the session with the window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MachineRig
    {
    public:
        TestMachine            machine;
        CpuManager             cpuManager;
        InMemoryPipeTransport  transport;
        InMemoryFileSystem     files;
        DebuggerController     controller;
        DebuggerViewState      view;



        MachineRig() :
            machine    (std::string ("Apple2e"), TestMachine::Slots::Empty),
            controller (machine, Paused (cpuManager), transport, files, nullptr, 1)
        {
            //  LDA #$41 / STA $0400 / RTS at $0300, with the PC on it.
            machine.GetMemoryBus().WriteByte (0x0300, 0xA9);
            machine.GetMemoryBus().WriteByte (0x0301, 0x41);
            machine.GetMemoryBus().WriteByte (0x0302, 0x8D);
            machine.GetMemoryBus().WriteByte (0x0303, 0x00);
            machine.GetMemoryBus().WriteByte (0x0304, 0x04);
            machine.GetMemoryBus().WriteByte (0x0305, 0x60);

            Cpu6502Registers  r = controller.GetSession().GetTarget().GetRegisters();

            r.pc = 0x0300;
            controller.GetSession().GetTarget().SetRegisters (r);
        }



        static CpuManager & Paused (CpuManager & cpu)
        {
            cpu.SetPaused (true);
            return cpu;
        }



        Reply Run (const std::string & line)
        {
            return DebuggerViewState::ExecuteLine (controller.GetSession(), line, CommandMode::AppleWin);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PaneTests)
    {
    public:

        TEST_METHOD (TheCodePaneStartsAtThePcAndFlagsThatLine)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            Assert::IsFalse  (snapshot.code.empty());
            Assert::AreEqual ((Word) 0x0300, snapshot.code[0].address);
            Assert::IsTrue   (snapshot.code[0].isCurrent,              L"the PC's line is flagged");
            Assert::IsFalse  (snapshot.code[1].isCurrent,              L"and only that line");
            Assert::AreEqual (std::string ("A9 41"), snapshot.code[0].bytes);
            Assert::IsTrue   (snapshot.code[0].instruction.find ("LDA") == 0);
        }



        TEST_METHOD (TheCodePaneCanBeMovedAwayFromThePc)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetCodeAddress (0x0302);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Word) 0x0302, snapshot.code[0].address);
            Assert::IsFalse  (snapshot.code[0].isCurrent, L"the PC is elsewhere");
        }



        TEST_METHOD (RegistersAndFlagsAreRows)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r        = rig.controller.GetSession().GetTarget().GetRegisters();



            r.a = 0x41;
            r.p = 0x81;     // N and C
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 6, snapshot.registers.size());
            Assert::AreEqual (std::string ("A"),  snapshot.registers[0].name);
            Assert::AreEqual (std::string ("41"), snapshot.registers[0].value);
            Assert::AreEqual (std::string ("PC"), snapshot.registers[5].name);
            Assert::AreEqual (std::string ("0300"), snapshot.registers[5].value);
            Assert::AreEqual (std::string ("N......C"), snapshot.flags);
        }



        TEST_METHOD (MemoryRowsCarryTheirRegion)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetMemoryAddress (0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse  (snapshot.memory.empty());
            Assert::AreEqual ((Word) 0x0300, snapshot.memory[0].address);
            Assert::IsTrue   (snapshot.memory[0].bytes.find ("A9 41 8D") == 0);
            Assert::AreEqual (std::string ("RAM"), snapshot.memory[0].region);

            rig.view.SetMemoryAddress (0xF800);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (std::string ("ROM"), snapshot.memory[0].region);
        }



        TEST_METHOD (TheStackWatchesAndBreakpointsAreListed)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("BP 0302");
            rig.Run ("W 0400");

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 1, snapshot.breakpoints.size());
            Assert::AreEqual ((Word) 0x0302, snapshot.breakpoints[0].address);
            Assert::IsTrue   (snapshot.code[1].hasBreakpoint, L"the code pane marks the line");

            Assert::AreEqual ((size_t) 1, snapshot.watches.size());
            Assert::AreEqual ((Word) 0x0400, snapshot.watches[0].address);

            Assert::IsFalse  (snapshot.stack.empty(), L"the stack pane shows the page above SP");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ControlTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ControlTests)
    {
    public:

        //  A click sets a breakpoint where there is none, and clears the one that
        //  is there.
        TEST_METHOD (ClickingALineTogglesItsBreakpoint)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            rig.Run (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0300));
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (snapshot.code[0].hasBreakpoint, L"set");

            rig.Run (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0300));
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (snapshot.code[0].hasBreakpoint, L"and cleared");
        }



        TEST_METHOD (EditingAByteWritesIt)
        {
            MachineRig  rig;
            Reply       reply;



            reply = rig.Run (DebuggerViewState::GetPokeLine (0x0400, 0xC1));

            Assert::IsTrue   (reply.status == CommandStatus::Ok);
            Assert::AreEqual ((Byte) 0xC1, rig.machine.GetMemoryBus().ReadByte (0x0400));
        }



        //  Step, step over, run and run to cursor each become the run a person
        //  typing the command would start.
        TEST_METHOD (RunControlsStartTheMatchingRun)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;



            handlers.Attach (session);

            //  Each run is stopped before the next starts, as the machine would
            //  stop it; a second run while one is going is refused as running.
            for (const std::string & line : { DebuggerViewState::GetStepLine(),
                                              DebuggerViewState::GetStepOverLine(),
                                              DebuggerViewState::GetRunLine(),
                                              DebuggerViewState::GetRunToCursorLine (0x0320) })
            {
                StopEvent  stop;



                DebuggerViewState::ExecuteLine (session, line, CommandMode::AppleWin);

                stop.reason = StopReason::Step;
                session.OnStopped (stop);
            }

            Assert::AreEqual ((size_t) 4, target.runs.size());
            Assert::IsTrue   (target.runs[0].kind == RunKind::StepInto, L"step");
            Assert::IsTrue   (target.runs[1].kind == RunKind::StepOver, L"step over");
            Assert::IsTrue   (target.runs[2].kind == RunKind::Go,       L"run");
            Assert::IsTrue   (target.runs[3].hasUntilPc && target.runs[3].untilPc == 0x0320, L"run to the cursor");
        }



        //  The command box gives the same reply batch mode gives for the line,
        //  in the mode chosen (Story 4 scenario 4).
        TEST_METHOD (TheCommandBoxRepliesAsBatchModeDoes)
        {
            MachineRig  rig;
            Reply       viaWindow;
            Reply       viaSession;



            viaWindow  = DebuggerViewState::ExecuteLine (rig.controller.GetSession(), "300.305", CommandMode::Monitor);
            viaSession = rig.controller.GetSession().ExecuteLine ("300.305", CommandMode::Monitor);
            rig.controller.GetSession().FormatReply (viaSession, CommandMode::Monitor);

            Assert::IsTrue   (viaWindow.status == CommandStatus::Ok);
            Assert::AreEqual (viaSession.text.size(), viaWindow.text.size());

            for (size_t i = 0; i < viaSession.text.size(); i++)
            {
                Assert::AreEqual (viaSession.text[i], viaWindow.text[i]);
            }
        }



        //  SC-006: the window and a channel client share one session. A breakpoint
        //  a client sets appears in the pane, and one set by clicking is in the
        //  client's `bpl`.
        TEST_METHOD (TheWindowAndAClientShareBreakpoints)
        {
            MachineRig            rig;
            ChannelConnectionId   client   = 0;
            DebuggerViewSnapshot  snapshot;
            HRESULT               hr       = rig.controller.Open();



            Assert::IsTrue (SUCCEEDED (hr));
            client = rig.transport.Connect();

            rig.transport.Send (client, R"({"type":"command","id":1,"line":"BP 0302"})");
            rig.controller.Pump();

            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsTrue (snapshot.code[1].hasBreakpoint, L"the client's breakpoint is in the pane");

            rig.Run (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0305));

            rig.transport.Send (client, R"({"type":"command","id":2,"line":"BPL"})");
            rig.controller.Pump();

            Assert::IsTrue (rig.transport.Written (client).back().find ("\"address\":773") != std::string::npos,
                            L"the clicked breakpoint at $0305 is in the client's list");
        }



        //  The window asks for a file name only for a Monitor R or W that has
        //  none, and the name it adds reaches the command intact.
        TEST_METHOD (AnROrWWithNoFileNameIsPromptedFor)
        {
            MonitorState        state;
            MonitorParseResult  parsed;
            std::string         line;



            Assert::IsTrue (DebuggerViewState::GetMissingFileVerb ("300.3FFR", CommandMode::Monitor) == DebugVerb::ReadFile,  L"R");
            Assert::IsTrue (DebuggerViewState::GetMissingFileVerb ("300.3FFW", CommandMode::Monitor) == DebugVerb::WriteFile, L"W");

            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FFR a.bin", CommandMode::Monitor).has_value(), L"named");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FF",        CommandMode::Monitor).has_value(), L"no R or W");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("/R",             CommandMode::Monitor).has_value(), L"an AppleWin line");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FFR",       CommandMode::AppleWin).has_value(), L"AppleWin mode");

            line   = DebuggerViewState::GetLineWithFileName ("300.3FFW", "C:\\My Files\\dump.bin");
            parsed = MonitorParser::Parse (line, state);

            Assert::AreEqual ((size_t) 1, parsed.commands.size());
            Assert::IsTrue   (parsed.commands[0].verb == DebugVerb::WriteFile);
            Assert::AreEqual (std::string ("C:\\My Files\\dump.bin"), parsed.commands[0].text);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WindowCommandTests
    //
    //  The AppleWin names that need the window, carried out on its panes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WindowCommandTests)
    {
    public:

        static Reply RunInWindow (MachineRig & rig, const std::string & line, CommandMode mode = CommandMode::AppleWin)
        {
            return rig.view.ExecuteWindowLine (rig.controller.GetSession(), line, mode);
        }



        TEST_METHOD (CursorCommandsMoveTheCodePane)
        {
            MachineRig  rig;



            Assert::IsTrue   (RunInWindow (rig, "V").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x0302, rig.view.GetCodeAddress().value_or (0), L"V passes LDA #$41");

            RunInWindow (rig, "v");
            Assert::AreEqual ((Word) 0x0305, rig.view.GetCodeAddress().value_or (0), L"and STA $0400");

            RunInWindow (rig, "^");
            Assert::AreEqual ((Word) 0x0302, rig.view.GetCodeAddress().value_or (0), L"^ goes back one instruction");

            RunInWindow (rig, "PAGEDOWN256");
            Assert::AreEqual ((Word) 0x0402, rig.view.GetCodeAddress().value_or (0), L"PAGEDOWN256");

            RunInWindow (rig, "PAGEUP4K");
            Assert::AreEqual ((Word) 0xF402, rig.view.GetCodeAddress().value_or (0), L"PAGEUP4K wraps");

            RunInWindow (rig, ".");
            Assert::IsFalse  (rig.view.GetCodeAddress().has_value(), L". follows the PC again");
        }



        TEST_METHOD (RetAndArrowGoToTheAddressesTheyRead)
        {
            MachineRig        rig;
            Cpu6502Registers  r    = rig.controller.GetSession().GetTarget().GetRegisters();
            Reply             reply;



            r.sp = 0xFD;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            rig.machine.GetMemoryBus().WriteByte (0x01FE, 0x34);
            rig.machine.GetMemoryBus().WriteByte (0x01FF, 0x12);

            RunInWindow (rig, "RET");
            Assert::AreEqual ((Word) 0x1235, rig.view.GetCodeAddress().value_or (0), L"one past the pushed address");

            rig.view.SetCodeAddress (0x0302);
            RunInWindow (rig, "->");
            Assert::AreEqual ((Word) 0x0400, rig.view.GetCodeAddress().value_or (0), L"STA $0400's address");

            rig.view.SetCodeAddress (0x0300);
            reply = RunInWindow (rig, "->");
            Assert::IsTrue   (reply.status == CommandStatus::Error, L"LDA #$41 has no address");
            Assert::AreEqual ((Word) 0x0300, rig.view.GetCodeAddress().value_or (0), L"and the pane stays");
        }



        TEST_METHOD (MiniMemoryCommandsMoveTheMemoryPane)
        {
            MachineRig  rig;



            Assert::IsTrue   (RunInWindow (rig, "MD1 1000").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x1000, rig.view.GetMemoryAddress());

            Assert::IsTrue   (RunInWindow (rig, "/MT2 $2000", CommandMode::Monitor).status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryAddress(), L"a / line in Monitor mode");

            Assert::IsTrue   (RunInWindow (rig, "MA1").status == CommandStatus::Error, L"no address");
            Assert::IsTrue   (RunInWindow (rig, "MA1 XYZ").status == CommandStatus::Error, L"not hex");
            Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryAddress(), L"a bad address leaves the pane");
        }



        TEST_METHOD (LayoutViewAndAppearanceNamesReplyWithoutChangingThePanes)
        {
            MachineRig  rig;



            Assert::IsTrue (RunInWindow (rig, "CODE").status    == CommandStatus::Ok,           L"every pane is shown");
            Assert::IsTrue (RunInWindow (rig, "SOURCE1").status == CommandStatus::NotAvailable, L"no listing link");
            Assert::IsTrue (RunInWindow (rig, "HGR").status     == CommandStatus::NotAvailable, L"a screen view");
            Assert::IsTrue (RunInWindow (rig, "BW").status      == CommandStatus::NotAvailable, L"appearance");

            Assert::IsFalse  (rig.view.GetCodeAddress().has_value());
            Assert::AreEqual ((Word) 0x0000, rig.view.GetMemoryAddress());
        }



        //  Outside the window the same names still need it, and every other line
        //  runs as the command box always ran it.
        TEST_METHOD (OtherLinesAndOtherCallersAreUnchanged)
        {
            MachineRig  rig;
            Reply       viaWindow  = RunInWindow (rig, "U 300");
            Reply       viaSession = rig.Run ("U 300");



            Assert::IsTrue   (rig.controller.GetSession().ExecuteLine ("V", CommandMode::AppleWin).status == CommandStatus::NotAvailable);
            Assert::IsTrue   (RunInWindow (rig, "V", CommandMode::Monitor).status != CommandStatus::Ok ||
                              !rig.view.GetCodeAddress().has_value(), L"a Monitor line without / is the Monitor's");
            Assert::AreEqual (viaSession.text.size(), viaWindow.text.size());
            Assert::AreEqual (viaSession.text.front(), viaWindow.text.front());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CadenceTests
    //
    //  When the CPU thread rebuilds the snapshot. A running machine gets one
    //  every frame, so a device panel or the registers never lag the screen by
    //  more than that; a stopped one is rebuilt only when something changes it,
    //  which is an action from a way in or the machine stopping or starting.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CadenceTests)
    {
    public:

        static constexpr ULONGLONG  kFrame = DebuggerViewState::kBuildIntervalMs;



        TEST_METHOD (ARunningMachineIsRebuiltEveryFrame)
        {
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, false, false, 1000 + kFrame,     1000));
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, false, false, 1000 + kFrame - 1, 1000));
        }


        TEST_METHOD (TheIntervalIsAtMostOneFrame)
        {
            //  FR-051: a panel updates at least once per frame, at 60 Hz.
            Assert::IsTrue (kFrame <= 1000 / 60);
        }


        TEST_METHOD (AStoppedMachineIsNotRebuiltForTimeAlone)
        {
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, true, true, 1000 + 60000, 1000));
        }


        TEST_METHOD (StoppingOrStartingIsDueAtOnce)
        {
            Assert::IsTrue (DebuggerViewState::IsBuildDue (false, true,  false, 1000, 1000),  L"a breakpoint just stopped it");
            Assert::IsTrue (DebuggerViewState::IsBuildDue (false, false, true,  1000, 1000),  L"it was just resumed");
        }


        TEST_METHOD (AnActionIsDueAtOnce)
        {
            Assert::IsTrue (DebuggerViewState::IsBuildDue (true, true,  true,  1000, 1000));
            Assert::IsTrue (DebuggerViewState::IsBuildDue (true, false, false, 1000, 1000));
        }
    };
}
