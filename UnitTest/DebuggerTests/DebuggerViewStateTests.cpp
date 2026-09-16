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
        DebuggerController     controller;
        DebuggerViewState      view;



        MachineRig() :
            machine    (std::string ("Apple2e"), TestMachine::Slots::Empty),
            controller (machine, Paused (cpuManager), transport, nullptr, 1)
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
}
