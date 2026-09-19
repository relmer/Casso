#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "EmuTests/TestMachine.h"
#include "FakeDiagnosticsProvider.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "TestHelpers.h"
#include "MockDebugTarget.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Core/UnicodeSymbols.h"
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
    //  MemoryWindowTests
    //
    //  Up to four memory windows (FR-034), each read on the CPU thread from its
    //  own address into the snapshot, a region for every byte.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryWindowTests)
    {
    public:

        static const DebuggerViewSnapshot::MemoryWindow & Window (const DebuggerViewSnapshot & snapshot, int id)
        {
            for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
            {
                if (window.id == id)
                {
                    return window;
                }
            }

            Assert::Fail (L"no such window in the snapshot");
            return snapshot.memoryWindows.front();
        }



        TEST_METHOD (TheFirstWindowIsAlwaysThere)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            Assert::AreEqual ((size_t) 1, snapshot.memoryWindows.size());
            Assert::AreEqual (1,          snapshot.memoryWindows[0].id);
            Assert::AreEqual ((size_t) DebuggerViewState::kMemoryWindowBytes, snapshot.memoryWindows[0].bytes.size());
            Assert::AreEqual (snapshot.memoryWindows[0].bytes.size(),         snapshot.memoryWindows[0].regions.size());
        }


        TEST_METHOD (MoreWindowsOpenAndClose)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.OpenMemoryWindow (2, 0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 2,     snapshot.memoryWindows.size());
            Assert::AreEqual ((Word) 0x0300,  Window (snapshot, 2).first);
            Assert::AreEqual ((Byte) 0xA9,    *Window (snapshot, 2).bytes[0]);
            Assert::AreEqual ((Byte) 0x8D,    *Window (snapshot, 2).bytes[2]);

            rig.view.CloseMemoryWindow (2);
            Assert::AreEqual ((size_t) 1, rig.view.Build (rig.controller.GetSession()).memoryWindows.size());
        }


        TEST_METHOD (OnlyWindowsTwoToFourOpenAndClose)
        {
            MachineRig  rig;



            rig.view.OpenMemoryWindow  (5, 0x0300);
            rig.view.CloseMemoryWindow (1);

            Assert::IsFalse (rig.view.GetMemoryWindowAddress (5).has_value());
            Assert::IsTrue  (rig.view.GetMemoryWindowAddress (1).has_value(), L"the first window cannot be closed");
        }


        TEST_METHOD (AWindowStartsOnARowBoundary)
        {
            MachineRig  rig;



            rig.view.OpenMemoryWindow (2, 0x0305);

            Assert::AreEqual ((Word) 0x0300, Window (rig.view.Build (rig.controller.GetSession()), 2).first);
        }


        TEST_METHOD (IoBytesAreEmptyAndMarked)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.OpenMemoryWindow (2, 0xC000);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (Window (snapshot, 2).bytes[0].has_value(), L"reading I/O would change the machine");
            Assert::IsTrue  (Window (snapshot, 2).regions[0] == MemoryRegion::Io);
        }


        TEST_METHOD (AnEditShowsInEveryWindowAtThatAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetMemoryAddress (0x0300);
            rig.view.OpenMemoryWindow (2, 0x0300);
            rig.Run ("PATCH 0300 5A");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Byte) 0x5A, *Window (snapshot, 1).bytes[0]);
            Assert::AreEqual ((Byte) 0x5A, *Window (snapshot, 2).bytes[0]);
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
    //  ActionTests
    //
    //  What each keyboard-scheme action sends. A key and the button it stands
    //  for produce the same command line, so a key is only a faster click.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ActionTests)
    {
    public:

        using Action = DebuggerKeySchemes::Action;



        static DebuggerViewSnapshot TwoLines()
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.pc = 0x0300;
            snapshot.code.push_back ({ 0x0300, "A9 41",    "LDA #$41",   "", true,  false });
            snapshot.code.push_back ({ 0x0302, "8D 00 04", "STA $0400",  "", false, false });
            return snapshot;
        }



        TEST_METHOD (RunAndStepsAreTheirCommands)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::AreEqual (DebuggerViewState::GetRunLine(),      *DebuggerViewState::GetActionLine (Action::Run,      &snapshot, -1));
            Assert::AreEqual (DebuggerViewState::GetStepLine(),     *DebuggerViewState::GetActionLine (Action::StepInto, &snapshot, -1));
            Assert::AreEqual (DebuggerViewState::GetStepOverLine(), *DebuggerViewState::GetActionLine (Action::StepOver, &snapshot, -1));
            Assert::AreEqual (std::string ("RTS"),                  *DebuggerViewState::GetActionLine (Action::StepOut,  &snapshot, -1));
        }


        TEST_METHOD (PauseIsNotACommandLine)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::IsFalse (DebuggerViewState::GetActionLine (Action::Pause, &snapshot, 0).has_value());
        }


        TEST_METHOD (CursorActionsUseTheSelectedLine)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::AreEqual (DebuggerViewState::GetRunToCursorLine (0x0302),
                              *DebuggerViewState::GetActionLine (Action::RunToCursor, &snapshot, 1));
            Assert::AreEqual (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0302),
                              *DebuggerViewState::GetActionLine (Action::ToggleBreakpoint, &snapshot, 1));
        }


        TEST_METHOD (ToggleWithNothingSelectedUsesThePcLine)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::AreEqual (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0300),
                              *DebuggerViewState::GetActionLine (Action::ToggleBreakpoint, &snapshot, -1));
        }


        TEST_METHOD (RunToCursorWithNothingSelectedDoesNothing)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::IsFalse (DebuggerViewState::GetActionLine (Action::RunToCursor, &snapshot, -1).has_value());
            Assert::IsFalse (DebuggerViewState::GetActionLine (Action::RunToCursor, nullptr,   0).has_value(), L"no snapshot yet");
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




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourcePaneTests
    //
    //  The source pane's share of the snapshot and the pane itself (FR-054 to
    //  FR-059). The debug file matches the rig's program: main.a65 line 2 is
    //  the LDA, line 3 a macro invocation whose body, macros.inc line 5, is
    //  the STA, and line 4 the RTS.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static const char * const  s_kMainText = "; main\n        lda #$41\n        store\n        rts\n";

    TEST_CLASS (SourcePaneTests)
    {
    public:

        static void LoadDebugFile (MachineRig & rig)
        {
            DebugFile  file;



            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 }, { 1, "macros.inc", 30, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 }, { 2, 0, 5, 1 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm,   0, { 0 } },
                           { 1, 0, 3, DebugLineType::Asm,   0, { 1 } },
                           { 2, 1, 5, DebugLineType::Macro, 1, { 1 } },
                           { 3, 0, 4, DebugLineType::Asm,   0, { 2 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        static void SetPc (MachineRig & rig, Word pc)
        {
            Cpu6502Registers  r = rig.controller.GetSession().GetTarget().GetRegisters();



            r.pc = pc;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
        }



        TEST_METHOD (NoDebugFileNoSourceState)
        {
            MachineRig  rig;



            Assert::IsFalse (rig.view.Build (rig.controller.GetSession()).source.has_value());
        }


        TEST_METHOD (TheLineAtPcAndEachCodeRowsLine)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (snapshot.source.has_value());
            Assert::AreEqual (0, snapshot.source->fileId);
            Assert::AreEqual (2, snapshot.source->line);
            Assert::AreEqual (0, snapshot.source->depth);
            Assert::AreEqual (2, snapshot.code[0].sourceLine);
            Assert::AreEqual (3, snapshot.code[1].sourceLine, L"the outermost line: the invocation, not the body");
            Assert::AreEqual (std::wstring (L"C:\\Work\\main.dbg"), snapshot.source->debugFilePath);
        }


        TEST_METHOD (InsideAMacroBothEndsAreGiven)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            SetPc (rig, 0x0302);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (3, snapshot.source->line);
            Assert::AreEqual (1, snapshot.source->bodyFileId);
            Assert::AreEqual (5, snapshot.source->bodyLine);
            Assert::AreEqual (1, snapshot.source->depth);
        }


        TEST_METHOD (LinesMapToTheirFirstAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            DebuggerViewSnapshot  again;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());
            again    = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Word) 0x0302, snapshot.source->lineAddresses->at ({ 0, 3 }));
            Assert::AreEqual ((Word) 0x0302, snapshot.source->lineAddresses->at ({ 1, 5 }));
            Assert::AreEqual ((Word) 0x0305, snapshot.source->lineAddresses->at ({ 0, 4 }));
            Assert::IsTrue   (snapshot.source->lineAddresses == again.source->lineAddresses, L"built once per load");
        }


        TEST_METHOD (ABreakpointMarksEveryLineAtItsAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            int                   id       = 0;



            LoadDebugFile (rig);
            rig.Run ("BP 302");
            snapshot = rig.view.Build (rig.controller.GetSession());
            id       = snapshot.breakpoints.at (0).id;

            Assert::AreEqual ((size_t) 2, snapshot.source->breakpointLines.size(), L"the invocation and the body line");
            Assert::AreEqual (std::format ("BPC {}", id), SourcePane::GetToggleLine (*snapshot.source, 0, 3));
            Assert::AreEqual (std::format ("BPC {}", id), SourcePane::GetToggleLine (*snapshot.source, 1, 5));
            Assert::AreEqual (std::string ("BP main.a65:4"), SourcePane::GetToggleLine (*snapshot.source, 0, 4));
        }


        TEST_METHOD (SplitLinesExpandsTabsAndEveryLineEnding)
        {
            std::vector<std::wstring>  lines = SourcePane::SplitLines ("a\tb\r\nc\rd\n\te");



            Assert::AreEqual ((size_t) 4,                   lines.size());
            Assert::AreEqual (std::wstring (L"a       b"),  lines[0], L"to the next multiple of eight, not eight spaces");
            Assert::AreEqual (std::wstring (L"c"),          lines[1]);
            Assert::AreEqual (std::wstring (L"        e"),  lines[3]);
        }


        TEST_METHOD (RowsCarryTheMarkersAndLineNumbers)
        {
            std::vector<std::wstring>        lines (12, L"x");
            std::vector<DxuiTextView::Row>   rows  = SourcePane::BuildRows (lines, 3, { 3, 10 });



            Assert::AreEqual ((size_t) 12, rows.size());
            Assert::AreEqual (std::wstring (L" 3"), rows[2].cells[1], L"numbers right-aligned to the widest");
            Assert::AreEqual (std::wstring (1, s_kchBullet) + s_kpszTriangleRight, rows[2].cells[0]);
            Assert::AreEqual (std::wstring (1, s_kchBullet) + L" ",                rows[9].cells[0]);
            Assert::AreEqual (std::wstring (L"  "),                                 rows[0].cells[0]);
        }


        TEST_METHOD (TheBannerSaysWhatTheFileNeedsSaid)
        {
            Assert::IsTrue (SourcePane::GetBannerText (SourceMatch::Exact, "a.s", true, 0, false, "", 0).empty());
            Assert::IsTrue (SourcePane::GetBannerText (SourceMatch::Mismatch, "a.s", true, 0, false, "", 0).find (L"may not match") != std::wstring::npos);
            Assert::IsTrue (SourcePane::GetBannerText (SourceMatch::NotFound, "a.s", false, 0, false, "", 0).find (L"Drop it") != std::wstring::npos);
            Assert::IsTrue (SourcePane::GetBannerText (SourceMatch::NotFound, "a.s", true, 0, false, "", 0).find (L"no line mapping") != std::wstring::npos);
            Assert::IsTrue (SourcePane::GetBannerText (SourceMatch::Exact, "a.s", true, 1, false, "m.inc", 5).find (L"m.inc line 5") != std::wstring::npos);
        }


        TEST_METHOD (ThePaneLoadsTheFileAtPcAndMarksItsLine)
        {
            MachineRig            rig;
            DxuiTextView          view;
            DxuiActionBanner      banner;
            int                   finds    = 0;
            std::string           ran;
            SourcePane            pane (&view, &banner,
                                        [&] (const DebugSourceFile & record, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            finds++;
                                            lookup.match = SourceMatch::Exact;
                                            lookup.text  = (record.id == 0) ? s_kMainText : "; macros\n\n\n\n        sta $0400\n";
                                            return lookup;
                                        },
                                        [&] (const std::string & line) { ran = line; },
                                        [] (Word) {});
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            SetPc (rig, 0x0302);
            snapshot = rig.view.Build (rig.controller.GetSession());
            pane.Apply (snapshot);
            pane.Apply (snapshot);

            Assert::AreEqual (1,          finds,                      L"found once, not every snapshot");
            Assert::IsTrue   (pane.IsActive());
            Assert::AreEqual ((size_t) 4, view.GetRows().size());
            Assert::AreEqual (std::wstring (L" ") + s_kpszTriangleRight, view.GetRows()[2].cells[0], L"the invocation line");
            Assert::IsTrue   (pane.HasBanner(),                           L"inside a macro");

            pane.ToggleBody();
            Assert::AreEqual ((size_t) 5, view.GetRows().size(),         L"the body's file");
            Assert::AreEqual (std::wstring (L" ") + s_kpszTriangleRight, view.GetRows()[4].cells[0]);
        }


        TEST_METHOD (ADroppedFileThatMatchesNothingIsPlainText)
        {
            MachineRig            rig;
            DxuiTextView          view;
            DxuiActionBanner      banner;
            SourceLookup          dropped;
            SourcePane            pane (&view, &banner,
                                        [] (const DebugSourceFile &, const std::wstring &, const std::string &) { return SourceLookup(); },
                                        [] (const std::string &) {},
                                        [] (Word) {});



            LoadDebugFile (rig);
            pane.Apply (rig.view.Build (rig.controller.GetSession()));
            Assert::IsTrue (banner.GetText().find (L"was not found") != std::wstring::npos);

            dropped.text = "hello\nworld\n";
            pane.ShowDropped (dropped, -1);
            pane.Apply (rig.view.Build (rig.controller.GetSession()));

            Assert::AreEqual ((size_t) 2, view.GetRows().size(), L"the dropped text stays while the PC is in the same file");
            Assert::IsTrue   (banner.GetText().find (L"no line mapping") != std::wstring::npos);
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiagnosticsPanelTests
    //
    //  Device panels in the window's snapshot, the PANEL command, and the rows
    //  a panel draws, all from a provider that stands for no real device.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiagnosticsPanelTests)
    {
    public:

        class PanelRig
        {
        public:
            TestCpu                    cpu;
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;
            FakeDiagnosticsProvider    provider;
            DebuggerViewState          view;



            PanelRig()
            {
                cpu.InitForTest();
                target.instructionSet = cpu.GetInstructionSet();
                handlers.Attach (session);
                target.diagnosticsProviders = { &provider };
            }



            Reply Run (const std::string & line, CommandMode mode = CommandMode::AppleWin)
            {
                return view.ExecuteWindowLine (session, line, mode);
            }
        };



        static std::string Join (const std::vector<std::string> & lines)
        {
            std::string  text;



            for (const std::string & line : lines)
            {
                text += line + "\n";
            }

            return text;
        }



        //  A panel nobody opened is listed and never asked for its rows.
        TEST_METHOD (AClosedPanelIsListedButNotBuilt)
        {
            PanelRig              rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.session);



            Assert::AreEqual ((size_t) 1, snapshot.panels.size());
            Assert::AreEqual (std::string ("fake"), snapshot.panels[0].id);
            Assert::AreEqual (std::string ("Fake"), snapshot.panels[0].title);
            Assert::IsFalse  (snapshot.panels[0].open);
            Assert::IsTrue   (snapshot.diagnostics.empty());
            Assert::AreEqual (0, rig.provider.calls, L"a closed panel costs the device nothing");
        }


        //  Every build carries every open panel, so a panel follows the device
        //  at the cadence the view is built: each frame while running, and on
        //  stop (SC-015).
        TEST_METHOD (AnOpenPanelIsInEverySnapshot)
        {
            PanelRig              rig;
            DebuggerViewSnapshot  first;
            DebuggerViewSnapshot  second;



            rig.view.OpenPanel ("fake");
            first              = rig.view.Build (rig.session);
            rig.provider.value = 0x01;
            second             = rig.view.Build (rig.session);

            Assert::AreEqual ((size_t) 1, second.diagnostics.size());
            Assert::AreEqual (std::string ("Fake"),  second.diagnostics[0].device);
            Assert::AreEqual (std::string ("$80"),   first.diagnostics[0].groups[0].rows[0].value);
            Assert::AreEqual (std::string ("$01"),   second.diagnostics[0].groups[0].rows[0].value, L"the new state, one build later");
            Assert::AreEqual (2, rig.provider.calls, L"once per build");
            Assert::IsTrue   (second.panels[0].open);

            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, false, false, DebuggerViewState::kBuildIntervalMs, 0), L"a frame later while running");
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, false, false, DebuggerViewState::kBuildIntervalMs - 1, 0));
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, true,  false, 1, 0), L"at once on stop");
        }


        TEST_METHOD (ThePanelCommandOpensListsAndCloses)
        {
            PanelRig  rig;
            Reply     reply;



            reply = rig.Run ("PANEL LIST");
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (Join (reply.text).find ("fake") != std::string::npos);

            reply = rig.Run ("panel FAKE");
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"by id, either case");
            Assert::IsTrue (Join (rig.Run ("PANEL").text).find ("(open)") != std::string::npos, L"PANEL alone lists");

            reply = rig.Run ("PANEL CLOSE Fake");
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"), L"by title");

            (void) rig.Run ("/PANEL fake", CommandMode::Monitor);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"as an AppleWin line from Monitor mode");
        }


        TEST_METHOD (APanelTheMachineLacksIsAnError)
        {
            PanelRig  rig;
            Reply     reply = rig.Run ("PANEL mmu");



            Assert::IsTrue  (reply.status == CommandStatus::Error);
            Assert::IsTrue  (Join (reply.text).find ("PANEL LIST") != std::string::npos);
            Assert::IsFalse (rig.view.IsPanelOpen ("mmu"));
            Assert::IsTrue  (rig.Run ("PANEL CLOSE").status == CommandStatus::Error, L"CLOSE needs a name");
        }


        //  Batch and the pipe have no window to put a panel in.
        TEST_METHOD (OutsideTheWindowPanelIsNotAvailable)
        {
            PanelRig  rig;
            Reply     reply = DebuggerViewState::ExecuteLine (rig.session, "PANEL fake", CommandMode::AppleWin);



            Assert::IsTrue  (reply.status == CommandStatus::NotAvailable);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"));
        }


        //  A machine switch or an emptied slot takes the device away; its panel
        //  closes and stays closed when a device of that id returns.
        TEST_METHOD (APanelClosesWhenItsDeviceLeaves)
        {
            PanelRig              rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.OpenPanel ("fake");
            rig.target.diagnosticsProviders.clear();
            snapshot = rig.view.Build (rig.session);

            Assert::IsTrue  (snapshot.panels.empty());
            Assert::IsTrue  (snapshot.diagnostics.empty());
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"));

            rig.target.diagnosticsProviders = { &rig.provider };
            snapshot = rig.view.Build (rig.session);
            Assert::IsTrue (snapshot.diagnostics.empty());
        }


        TEST_METHOD (TheMenuSendsPanelLines)
        {
            Assert::AreEqual (std::string ("PANEL disk"),        DebuggerViewState::GetPanelLine ("disk", true,  CommandMode::AppleWin));
            Assert::AreEqual (std::string ("PANEL CLOSE disk"),  DebuggerViewState::GetPanelLine ("disk", false, CommandMode::AppleWin));
            Assert::AreEqual (std::string ("/PANEL disk"),       DebuggerViewState::GetPanelLine ("disk", true,  CommandMode::Monitor));
        }


        //  A real machine's MMU panel arrives with its map.
        TEST_METHOD (TheMmuPanelOfARealMachine)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            (void) rig.view.ExecuteWindowLine (rig.controller.GetSession(), "PANEL mmu", CommandMode::AppleWin);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 1, snapshot.diagnostics.size());
            Assert::AreEqual (std::string ("mmu"), snapshot.diagnostics[0].id);
            Assert::IsTrue   (std::holds_alternative<DiagnosticsMemoryMap> (snapshot.diagnostics[0].visual));
        }


        //  The rows a panel draws: the group's title on a row of its own, each
        //  row under it, and a bit's name dimmed while the bit is clear.
        TEST_METHOD (APanelRendersASyntheticSnapshot)
        {
            FakeDiagnosticsProvider                       provider;
            DiagnosticsSnapshot                           snapshot;
            std::vector<std::vector<DxuiListView::Cell>>  rows;



            provider.GetDiagnostics (snapshot);
            rows = DiagnosticsPane::MakeRows (snapshot);

            Assert::AreEqual ((size_t) 3, rows.size(), L"the group, then two rows");
            Assert::AreEqual (std::wstring (L"Group"),       rows[0][0].text);
            Assert::AreEqual (std::wstring (L"  Register"),  rows[1][0].text);
            Assert::AreEqual (std::wstring (L"$80"),         rows[1][1].text);
            Assert::AreEqual (std::wstring (L"HI LO"),       rows[1][2].text);
            Assert::AreEqual ((size_t) 1, rows[1][2].dimRanges.size(), L"only LO is clear");
            Assert::AreEqual (3, rows[1][2].dimRanges[0].first);
            Assert::AreEqual (5, rows[1][2].dimRanges[0].second);
            Assert::AreEqual (std::wstring (L""),            rows[2][2].text, L"a row with no decode");
        }


        //  The graphic follows the payload's kind; only a change of kind asks
        //  the frame to lay out again.
        TEST_METHOD (APanelShowsTheGraphicItsPayloadAsksFor)
        {
            DxuiListView         list;
            MemoryMapBar         map;
            DiskHeadView         head;
            MeterBar             meters;
            DiagnosticsPane      pane ("fake", L"Fake", &list, &map, &head, &meters);
            DiagnosticsSnapshot  snapshot;



            snapshot.groups.push_back ({ "Group", { { "Row", "1", {} } } });
            Assert::IsFalse (pane.Apply (snapshot), L"no graphic, as before");

            snapshot.visual = DiagnosticsDiskHead { 17, 139, 0x04, true, 0 };
            Assert::IsTrue   (pane.Apply (snapshot));
            Assert::IsFalse  (pane.Apply (snapshot), L"the same kind again");
            Assert::AreEqual (17, head.GetHead().quarterTrack);

            pane.GetFrame()->Layout (RECT { 0, 0, 400, 300 }, DxuiDpiScaler());
            Assert::IsTrue  (head.IsVisible());
            Assert::IsFalse (map.IsVisible());
            Assert::IsFalse (meters.IsVisible());
            Assert::AreEqual (2, list.GetRowCount());
        }
    };
}