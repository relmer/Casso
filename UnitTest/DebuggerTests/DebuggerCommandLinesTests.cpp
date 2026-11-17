#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/Source/SourcePathList.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "FakeDiagnosticsProvider.h"
#include "ControllerRig.h"
#include "DebuggerViewStateTestRig.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "TestHelpers.h"
#include "MockDebugTarget.h"
#include "MockExpressionContext.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/InstructionTouches.h"
#include "Ui/Debugger/InstructionEffect.h"
#include "Ui/Debugger/StopChanges.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "UnreadableStackTarget.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"
#include "Core/TextEncoding.h"






using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewStateTests
{
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

            Assert::IsTrue (LineAt (snapshot, 0x0300).hasBreakpoint, L"set");

            rig.Run (DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0300));
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (LineAt (snapshot, 0x0300).hasBreakpoint, L"and cleared");
        }



        //  The panes read the machine while the line assembler waits for the next
        //  line, and the reads neither assemble nor come back empty.
        TEST_METHOD (ThePanesStayFilledWhileAssembling)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            Assert::IsTrue (rig.Run ("A 300").status == CommandStatus::Ok);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (rig.controller.GetSession().IsAssembling(), L"the assembly is still open");
            Assert::AreEqual ((size_t) 9, snapshot.registers.size(), L"six registers, the cycle and trip counts and the last branch");
            Assert::IsFalse  (snapshot.memory.empty());
            Assert::AreEqual ((Byte) 0xA9, rig.machine.GetMemoryBus().ReadByte (0x0300), L"nothing was assembled");

            Assert::IsTrue   (rig.Run ("NOP").status == CommandStatus::Ok);
            Assert::AreEqual ((Byte) 0xEA, rig.machine.GetMemoryBus().ReadByte (0x0300), L"the next line typed goes to $0300");
        }



        //  A WinDbg command stopped by the running machine says so under the
        //  name typed, not the AppleWin command it became.
        TEST_METHOD (AWinDbgWriteWhileRunningShowsTheWinDbgCommand)
        {
            MachineRig  rig;
            Reply       reply;



            Assert::IsTrue (rig.Run ("G").status == CommandStatus::Ok);

            reply = rig.Run ("eb 300 00", CommandMode::WinDbg);

            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::IsTrue   (reply.error.detail.starts_with ("eb "), TextEncoding::NarrowToWide (reply.error.detail).c_str());
        }



        //  A watchpoint on a code line is not an execution breakpoint there: the
        //  line shows no mark, and a click sets one rather than clearing the
        //  watchpoint.
        TEST_METHOD (ClickingALineWithAWatchpointSetsABreakpoint)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("BPMW 300");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse   (LineAt (snapshot, 0x0300).hasBreakpoint, L"no mark");
            Assert::AreEqual  (std::string ("BP 0300"), DebuggerViewState::GetToggleBreakpointLine (snapshot, 0x0300));
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



        TEST_METHOD (BareDump_ContinuesFromTheUsersDumpAcrossARebuild)
        {
            MachineRig  rig;
            Reply       reply;



            rig.Run ("D 300");
            rig.view.Build (rig.controller.GetSession());
            reply = rig.Run ("D");

            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (reply.text.at (0).starts_with ("0340:"), std::wstring (reply.text.at (0).begin(), reply.text.at (0).end()).c_str());
        }



        TEST_METHOD (MonitorMoveWithoutDestination_FailsAndLeavesZeroPage)
        {
            MachineRig  rig;
            Reply       reply;



            rig.machine.GetMemoryBus().WriteByte (0x0000, 0x5A);
            reply = rig.Run ("300.305M", CommandMode::Monitor);

            Assert::IsTrue   (reply.status == CommandStatus::Error, L"no destination is an error");
            Assert::AreEqual ((int) 0x5A, (int) rig.machine.GetMemoryBus().ReadByte (0x0000), L"zero page untouched");
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
            Assert::IsTrue (LineAt (snapshot, 0x0302).hasBreakpoint, L"the client's breakpoint is in the pane");

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



            Assert::IsTrue (DebuggerViewState::GetMissingFileVerb ("300.3FFR", CommandMode::Monitor, false) == DebugVerb::ReadFile,  L"R");
            Assert::IsTrue (DebuggerViewState::GetMissingFileVerb ("300.3FFW", CommandMode::Monitor, false) == DebugVerb::WriteFile, L"W");

            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FFR a.bin", CommandMode::Monitor, false).has_value(), L"named");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FF",        CommandMode::Monitor, false).has_value(), L"no R or W");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("/R",             CommandMode::Monitor, false).has_value(), L"an AppleWin line");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("300.3FFR",       CommandMode::AppleWin, false).has_value(), L"AppleWin mode");
            Assert::IsFalse (DebuggerViewState::GetMissingFileVerb ("LSR",            CommandMode::Monitor,  true).has_value(),  L"an assembly line");

            line   = DebuggerViewState::GetLineWithFileName ("300.3FFW", "C:\\My Files\\dump.bin");
            parsed = MonitorParser::Parse (line, state);

            Assert::AreEqual ((size_t) 1, parsed.commands.size());
            Assert::IsTrue   (parsed.commands[0].verb == DebugVerb::WriteFile);
            Assert::AreEqual (std::string ("C:\\My Files\\dump.bin"), parsed.commands[0].text);
        }

        //  GSSquared steps and resumes by key: at an empty command line in
        //  GSSquared mode, Space and F10 step and Return resumes, whatever the
        //  scheme. With text typed, or in another mode, the keys are left
        //  alone.
        TEST_METHOD (GSSquaredMode_EmptyLine_SpaceAndF10Step_ReturnResumes)
        {
            using Action = DebuggerKeySchemes::Action;

            auto  action = [] (CommandMode mode, WPARAM vk, bool shift, bool isEmpty)
            {
                return DebuggerViewState::GetConsoleKeyAction (mode, vk, false, false, shift, isEmpty);
            };



            Assert::IsTrue (action (CommandMode::GSSquared, VK_SPACE,  false, true) == Action::StepInto, L"Space");
            Assert::IsTrue (action (CommandMode::GSSquared, VK_F10,    false, true) == Action::StepInto, L"F10");
            Assert::IsTrue (action (CommandMode::GSSquared, VK_RETURN, false, true) == Action::Run,      L"Return");

            Assert::IsFalse (action (CommandMode::GSSquared, VK_SPACE,  false, false).has_value(), L"text typed");
            Assert::IsFalse (action (CommandMode::GSSquared, VK_RETURN, false, false).has_value(), L"Return runs the line");
            Assert::IsFalse (action (CommandMode::GSSquared, VK_F10,    true,  true).has_value(),  L"Shift+F10 is the context menu");
            Assert::IsFalse (action (CommandMode::GSSquared, 'O',       false, true).has_value(),  L"O is the scheme's");
            Assert::IsFalse (action (CommandMode::AppleWin,  VK_SPACE,  false, true).has_value(),  L"AppleWin mode");
            Assert::IsFalse (action (CommandMode::Monitor,   VK_RETURN, false, true).has_value(),  L"Monitor mode");
            Assert::IsFalse (DebuggerViewState::GetConsoleKeyAction (CommandMode::GSSquared, VK_SPACE, true, false, false, true).has_value(), L"Ctrl+Space");
        }

        //  In Monitor mode Return on an empty line shows the next row of bytes,
        //  so the command line keeps it rather than handing it to the scheme.
        TEST_METHOD (MonitorMode_EmptyLineReturn_StaysInCommandLine)
        {
            Assert::IsTrue  (DebuggerViewState::DoesConsoleKeepKey (CommandMode::Monitor,   VK_RETURN, false, false));
            Assert::IsFalse (DebuggerViewState::DoesConsoleKeepKey (CommandMode::Monitor,   VK_SPACE,  false, false), L"Space");
            Assert::IsFalse (DebuggerViewState::DoesConsoleKeepKey (CommandMode::Monitor,   VK_RETURN, true,  false), L"Ctrl+Return");
            Assert::IsFalse (DebuggerViewState::DoesConsoleKeepKey (CommandMode::AppleWin,  VK_RETURN, false, false), L"AppleWin mode");
            Assert::IsFalse (DebuggerViewState::DoesConsoleKeepKey (CommandMode::GSSquared, VK_RETURN, false, false), L"GSSquared mode");
        }

        //  The controls send AppleWin lines; in GSSquared mode they go in its
        //  words, and each one runs there.
        TEST_METHOD (GSSquaredMode_ControlLines_AreInItsWords)
        {
            MachineRig  rig;



            Assert::AreEqual (std::string ("s"),              DebuggerViewState::GetModeLine ("T",            CommandMode::GSSquared));
            Assert::AreEqual (std::string ("o"),              DebuggerViewState::GetModeLine ("P",            CommandMode::GSSquared));
            Assert::AreEqual (std::string ("r"),              DebuggerViewState::GetModeLine ("RTS",          CommandMode::GSSquared));
            Assert::AreEqual (std::string ("g"),              DebuggerViewState::GetModeLine ("G",            CommandMode::GSSquared));
            Assert::AreEqual (std::string ("bp 0300"),        DebuggerViewState::GetModeLine ("BP 0300",      CommandMode::GSSquared));
            Assert::AreEqual (std::string ("nobp 3"),         DebuggerViewState::GetModeLine ("BPC 3",        CommandMode::GSSquared));
            Assert::AreEqual (std::string ("0300: 41"),       DebuggerViewState::GetModeLine ("MEB 0300 41",  CommandMode::GSSquared));
            Assert::AreEqual (std::string ("SRC ON"),         DebuggerViewState::GetModeLine ("SRC ON",       CommandMode::GSSquared));
            Assert::AreEqual (std::string ("T"),              DebuggerViewState::GetModeLine ("T",            CommandMode::AppleWin));

            rig.controller.GetSession().ExecuteLine ("MODE GSSQUARED");

            for (const char * line : { "BP 0300", "MEB 0300 41", "BPC 0" })
            {
                Reply  reply = DebuggerViewState::ExecuteLine (rig.controller.GetSession(),
                                                               DebuggerViewState::GetModeLine (line, CommandMode::GSSquared),
                                                               CommandMode::GSSquared);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, std::wstring (line, line + strlen (line)).c_str());
            }
        }

        //  A watch edit takes any value AppleWin's MEB takes, in GSSquared mode
        //  too, though its deposit takes only hex bytes.
        TEST_METHOD (GSSquaredMode_AWatchEditTakesAnyByteExpression)
        {
            MachineRig  rig;



            rig.controller.GetSession().ExecuteLine ("MODE GSSQUARED");

            for (const auto & [line, expected] : { std::pair<const char *, Byte> { "MEB 0400 $41", 0x41 },
                                                   std::pair<const char *, Byte> { "MEB 0400 0n66", 0x42 },
                                                   std::pair<const char *, Byte> { "MEB 0400 A",   0x0A },
                                                   std::pair<const char *, Byte> { "MEB 0400 44",  0x44 } })
            {
                Reply  reply = DebuggerViewState::ExecuteLine (rig.controller.GetSession(),
                                                               DebuggerViewState::GetModeLine (line, CommandMode::GSSquared),
                                                               CommandMode::GSSquared);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, std::wstring (line, line + strlen (line)).c_str());
                Assert::AreEqual (expected, rig.machine.GetMemoryBus().ReadByte (0x0400), std::wstring (line, line + strlen (line)).c_str());
            }
        }

        //  The breakpoint checkbox, the register edits and auto-watch send
        //  AppleWin lines whose names are also GSSquared words; each still
        //  does what the control meant.
        TEST_METHOD (GSSquaredMode_DisableAndRegisterLines_Run)
        {
            MachineRig  rig;



            rig.controller.GetSession().ExecuteLine ("MODE GSSQUARED");

            for (const char * line : { "BP 0300", "BPD 0", "BPE 0", "R A 41", "R P 30", "R S F0" })
            {
                Reply  reply = DebuggerViewState::ExecuteLine (rig.controller.GetSession(),
                                                               DebuggerViewState::GetModeLine (line, CommandMode::GSSquared),
                                                               CommandMode::GSSquared);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                                  std::wstring (line, line + strlen (line)).append (L": ").append (reply.error.detail.begin(), reply.error.detail.end()).c_str());
            }

            Assert::AreEqual ((int) 0x41, (int) rig.controller.GetSession().GetTarget().GetRegisters().a);
            Assert::AreEqual ((int) 0xF0, (int) rig.controller.GetSession().GetTarget().GetRegisters().sp);
        }

        //  The Dialect menu sends MODE through the marker of whatever dialect
        //  is in force (FR-014), and every dialect has to take it that way, or
        //  one of them is a trap.
        TEST_METHOD (EveryDialect_TakesMode)
        {
            MachineRig  rig;



            for (CommandMode from : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                Reply  reply;

                rig.controller.GetSession().ExecuteLine ("MODE " + CommandModeNames::GetUpperName (from));
                Assert::IsTrue (rig.controller.GetSession().GetMode() == from);
                reply = DebuggerViewState::ExecuteLine (rig.controller.GetSession(), DebuggerViewState::GetModeLine ("MODE APPLEWIN", from), from);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
                Assert::IsTrue   (rig.controller.GetSession().GetMode() == CommandMode::AppleWin);
            }
        }


        //  Monitor mode reads an AppleWin line after its `/`, so the Step
        //  button steps there instead of reaching the Monitor's own T.
        TEST_METHOD (MonitorMode_ControlLines_TakeTheSlash)
        {
            MachineRig  rig;
            Reply       reply;



            Assert::AreEqual (std::string ("/T"),       DebuggerViewState::GetModeLine ("T",       CommandMode::Monitor));
            Assert::AreEqual (std::string ("/BP 0300"), DebuggerViewState::GetModeLine ("BP 0300", CommandMode::Monitor));

            rig.controller.GetSession().ExecuteLine ("MODE MONITOR");

            for (const char * line : { "BP 0300", "MEB 0300 41", "BPC 0", "SRC OFF" })
            {
                reply = DebuggerViewState::ExecuteLine (rig.controller.GetSession(),
                                                        DebuggerViewState::GetModeLine (line, CommandMode::Monitor),
                                                        CommandMode::Monitor);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, std::wstring (line, line + strlen (line)).c_str());
            }
        }
    };





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



            Assert::AreEqual (std::string ("G"),                    *DebuggerViewState::GetActionLine (Action::Run,      &snapshot, -1));
            Assert::AreEqual (std::string ("T"),                    *DebuggerViewState::GetActionLine (Action::StepInto, &snapshot, -1));
            Assert::AreEqual (std::string ("P"),                    *DebuggerViewState::GetActionLine (Action::StepOver, &snapshot, -1));
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



            Assert::AreEqual (std::string ("G 0302"),
                              *DebuggerViewState::GetActionLine (Action::RunToCursor, &snapshot, 1));
            Assert::AreEqual (std::string ("BP 0302"),
                              *DebuggerViewState::GetActionLine (Action::ToggleBreakpoint, &snapshot, 1));
        }


        TEST_METHOD (ToggleWithNothingSelectedUsesThePcLine)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::AreEqual (std::string ("BP 0300"),
                              *DebuggerViewState::GetActionLine (Action::ToggleBreakpoint, &snapshot, -1));
        }


        TEST_METHOD (RunToCursorWithNothingSelectedDoesNothing)
        {
            DebuggerViewSnapshot  snapshot = TwoLines();



            Assert::IsFalse (DebuggerViewState::GetActionLine (Action::RunToCursor, &snapshot, -1).has_value());
            Assert::IsFalse (DebuggerViewState::GetActionLine (Action::RunToCursor, nullptr,   0).has_value(), L"no snapshot yet");
        }
    };
}
