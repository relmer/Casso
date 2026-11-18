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
#include "Ui/Debugger/DebuggerCommandLines.h"
#include "Ui/Debugger/DebuggerViewCommands.h"
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
            auto                  rig      = std::make_unique<PanelRig>();   // heap: the rig and a snapshot overflow the frame (C6262)
            DebuggerViewSnapshot  snapshot = rig->view.Build (rig->session);



            Assert::AreEqual ((size_t) 1, snapshot.panels.size());
            Assert::AreEqual (std::string ("fake"), snapshot.panels[0].id);
            Assert::AreEqual (std::string ("Disk widget"), snapshot.panels[0].title);
            Assert::IsFalse  (snapshot.panels[0].open);
            Assert::IsTrue   (snapshot.diagnostics.empty());
            Assert::AreEqual (0, rig->provider.calls, L"a closed panel costs the device nothing");
        }


        //  Every build carries every open panel, so a panel follows the device
        //  at the cadence the view is built: each frame while running, and on
        //  stop (SC-015).
        //  A step over a short call sends the views its stop alone; one that
        //  runs past the hold is drawn as running.
        TEST_METHOD (AStepIsHeldUntilItRunsPastTheHold)
        {
            Assert::IsTrue  (DebuggerViewState::IsStepHeld (true,  0),                                   L"as the step starts");
            Assert::IsTrue  (DebuggerViewState::IsStepHeld (true,  DebuggerViewState::kStepHoldMs - 1), L"a short call");
            Assert::IsFalse (DebuggerViewState::IsStepHeld (true,  DebuggerViewState::kStepHoldMs),     L"a long call is drawn as running");
            Assert::IsFalse (DebuggerViewState::IsStepHeld (false, 0),                                   L"a run");
        }


        TEST_METHOD (AnOpenPanelIsInEverySnapshot)
        {
            PanelRig                               rig;
            std::unique_ptr<DebuggerViewSnapshot>  firstOwner  = std::make_unique<DebuggerViewSnapshot>();
            std::unique_ptr<DebuggerViewSnapshot>  secondOwner = std::make_unique<DebuggerViewSnapshot>();
            DebuggerViewSnapshot                 & first       = *firstOwner;
            DebuggerViewSnapshot                 & second      = *secondOwner;



            rig.view.OpenPanel ("fake");
            first              = rig.view.Build (rig.session);
            rig.provider.value = 0x01;
            second             = rig.view.Build (rig.session);

            Assert::AreEqual ((size_t) 1, second.diagnostics.size());
            Assert::AreEqual (std::string ("Disk widget"), second.diagnostics[0].device);
            Assert::AreEqual (std::string ("$80"),   first.diagnostics[0].groups[0].rows[0].value);
            Assert::AreEqual (std::string ("$01"),   second.diagnostics[0].groups[0].rows[0].value, L"the new state, one build later");
            Assert::AreEqual (2, rig.provider.calls, L"once per build");
            Assert::IsTrue   (second.panels[0].open);

            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, false, false, DebuggerViewState::kBuildIntervalMs, 0), L"a frame later while running");
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, false, false, DebuggerViewState::kBuildIntervalMs - 1, 0));
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, true,  false, 1, 0), L"at once on stop");
        }


        //  The panel controls send their line in the session's words, and
        //  PANEL still reaches the window from WinDbg and GSSquared modes.
        TEST_METHOD (ThePanelCommandReachesTheWindowFromEveryMode)
        {
            for (CommandMode mode : { CommandMode::WinDbg, CommandMode::GSSquared, CommandMode::Monitor })
            {
                auto   rig = std::make_unique<PanelRig>();   // heap: two rigs overflow the frame (C6262)
                Reply  reply;



                reply = rig->Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", true), mode), mode);
                Assert::IsTrue (reply.status == CommandStatus::Ok, L"the control opens the panel");
                Assert::IsTrue (rig->view.IsPanelOpen ("fake"));

                reply = rig->Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", false), mode), mode);
                Assert::IsTrue  (reply.status == CommandStatus::Ok, L"the control closes the panel");
                Assert::IsFalse (rig->view.IsPanelOpen ("fake"));
            }

            {
                auto   rig   = std::make_unique<PanelRig>();
                Reply  reply = rig->Run ("!panel fake", CommandMode::WinDbg);

                Assert::IsTrue (reply.status == CommandStatus::Ok, L"typed in WinDbg's words");
                Assert::IsTrue (rig->view.IsPanelOpen ("fake"));
            }
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

            reply = rig.Run ("PANEL CLOSE Disk widget");
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"), L"by a title that holds a space");

            reply = rig.Run ("PANEL disk WIDGET");
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsTrue  (rig.view.IsPanelOpen ("fake"), L"by title, either case");
            (void) rig.Run ("PANEL CLOSE fake");

            (void) rig.Run ("/PANEL fake", CommandMode::Monitor);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"as an AppleWin line from Monitor mode");
        }


        //  The Panels menu sends PANEL through GetModeLine, so each dialect's
        //  form of it opens and closes a panel in the window.
        TEST_METHOD (ThePanelCommandRunsInEveryDialect)
        {
            PanelRig  rig;
            Reply     reply;



            reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", true), CommandMode::GSSquared), CommandMode::GSSquared);
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"GSSquared");

            reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", false), CommandMode::GSSquared), CommandMode::GSSquared);
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"), L"GSSquared close");

            reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", true), CommandMode::WinDbg), CommandMode::WinDbg);
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"WinDbg");

            reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", false), CommandMode::WinDbg), CommandMode::WinDbg);
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"), L"WinDbg close");
        }


        //  GSSquared's debug and nodebug are its words for PANEL.
        TEST_METHOD (GSSquaredDebugAndNoDebugOpenListAndClosePanels)
        {
            PanelRig  rig;
            Reply     reply;



            reply = rig.Run ("debug \"fake\"", CommandMode::GSSquared);
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"), L"debug name opens");

            reply = rig.Run ("debug", CommandMode::GSSquared);
            Assert::IsTrue (Join (reply.text).find ("(open)") != std::string::npos, L"debug alone lists");

            reply = rig.Run ("nodebug fake", CommandMode::GSSquared);
            Assert::IsTrue  (reply.status == CommandStatus::Ok);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"), L"nodebug name closes");
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
            Reply     reply = DebuggerViewCommands::ExecuteLine (rig.session, "PANEL fake", CommandMode::AppleWin);



            Assert::IsTrue  (reply.status == CommandStatus::NotAvailable);
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"));
        }


        //  A machine switch or an emptied slot takes the device away; its panel
        //  closes and stays closed when a device of that id returns.
        TEST_METHOD (APanelClosesWhenItsDeviceLeaves)
        {
            auto                  rig      = std::make_unique<PanelRig>();   // heap: the rig and a snapshot overflow the frame (C6262)
            DebuggerViewSnapshot  snapshot;



            rig->view.OpenPanel ("fake");
            rig->target.diagnosticsProviders.clear();
            snapshot = rig->view.Build (rig->session);

            Assert::IsTrue  (snapshot.panels.empty());
            Assert::IsTrue  (snapshot.diagnostics.empty());
            Assert::IsFalse (rig->view.IsPanelOpen ("fake"));

            rig->target.diagnosticsProviders = { &rig->provider };
            snapshot = rig->view.Build (rig->session);
            Assert::IsTrue (snapshot.diagnostics.empty());
        }


        //  The window opens its saved panels, and its menu opens and closes
        //  them, through each mode's line for PANEL.
        TEST_METHOD (ThePanelLineRunsInEveryMode)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                PanelRig  rig;
                Reply     reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", true), mode), mode);



                Assert::IsTrue  (reply.status == CommandStatus::Ok);
                Assert::IsTrue  (rig.view.IsPanelOpen ("fake"));

                reply = rig.Run (DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("fake", false), mode), mode);
                Assert::IsTrue  (reply.status == CommandStatus::Ok);
                Assert::IsFalse (rig.view.IsPanelOpen ("fake"));
            }
        }


        TEST_METHOD (TheMenuSendsPanelLines)
        {
            Assert::AreEqual (std::string ("PANEL disk"),        DebuggerCommandLines::GetPanelLine ("disk", true));
            Assert::AreEqual (std::string ("PANEL CLOSE disk"),  DebuggerCommandLines::GetPanelLine ("disk", false));
            Assert::AreEqual (std::string ("/PANEL disk"),       DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("disk", true), CommandMode::Monitor));
        }


        //  A script run from the window runs its lines as the window does,
        //  so its PANEL and CODE lines reach the panes.
        TEST_METHOD (AScriptRunFromTheWindowRunsWindowCommands)
        {
            MachineRig            rig;
            Reply                 reply;
            DebuggerViewSnapshot  snapshot;



            rig.files.WriteAllText (L"C:\\Work\\panes.txt", "PANEL mmu\nCODE\n");

            reply    = rig.view.ExecuteWindowLine (rig.controller.GetSession(), "RUN \"C:\\Work\\panes.txt\"", CommandMode::AppleWin);
            snapshot = rig.view.Build (rig.controller.GetSession());

            for (const std::string & text : reply.text)
            {
                Assert::IsTrue (text.find ("needs the debugger window") == std::string::npos, L"no script line was sent away from the window");
            }

            Assert::AreEqual ((size_t) 1, snapshot.diagnostics.size(), L"the script's PANEL line opened the panel");
        }


        //  A script that ends inside an A block ends the block, so the next
        //  line typed is a command again.
        TEST_METHOD (AScriptEndingInsideAnAssemblyBlockEndsTheBlock)
        {
            MachineRig  rig;



            rig.files.WriteAllText (L"C:\\Work\\asm.txt", "A 300\nLDA #1\n");

            (void) rig.view.ExecuteWindowLine (rig.controller.GetSession(), "RUN \"C:\\Work\\asm.txt\"", CommandMode::AppleWin);

            Assert::IsFalse (rig.controller.GetSession().IsAssembling(), L"the script's end ended the block");
        }


        //  After a script line that starts a run that goes on, as G does in
        //  the window, the rest of the script waits and runs once the machine
        //  has stopped.
        TEST_METHOD (AScriptGoesOnAfterTheRunItStartedStops)
        {
            MachineRig                 rig;
            DebugSession             & session = rig.controller.GetSession();
            std::vector<std::string>   lines;
            StopEvent                  stop;



            rig.files.WriteAllText (L"C:\\Work\\go.txt", "G\nR PC 6000\n");

            (void) rig.view.ExecuteWindowLine (session, "RUN \"C:\\Work\\go.txt\"", CommandMode::AppleWin);

            Assert::IsTrue   (session.GetRunState() != RunState::Paused, L"G left the machine running");
            Assert::IsTrue   (session.HasPendingScript(), L"the rest of the script is waiting");
            Assert::AreNotEqual ((Word) 0x6000, rig.machine.GetCpu()->GetPC(), L"and hasn't run");

            stop.reason = StopReason::Step;
            stop.pc     = rig.machine.GetCpu()->GetPC();
            session.OnStopped (stop);

            lines = rig.view.RunPendingScript (session);

            Assert::IsFalse  (session.HasPendingScript(), L"the script was taken");
            Assert::AreEqual ((Word) 0x6000, rig.machine.GetCpu()->GetPC(), L"its next line ran");
            Assert::IsFalse  (lines.empty(), L"with its text for the console");
        }


        //  The menu's PANEL lines, in each dialect's words, open the panel
        //  in every dialect.
        TEST_METHOD (PanelLinesWorkInEveryDialect)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                MachineRig            rig;
                Reply                 reply;
                DebuggerViewSnapshot  snapshot;
                std::string           line = DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("mmu", true), mode);



                rig.controller.GetSession().ExecuteLine ("MODE " + CommandModeNames::GetUpperName (mode));
                reply    = rig.view.ExecuteWindowLine (rig.controller.GetSession(), line, mode);
                snapshot = rig.view.Build (rig.controller.GetSession());

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (line).c_str());
                Assert::AreEqual ((size_t) 1, snapshot.diagnostics.size(), TextEncoding::NarrowToWide (line).c_str());
            }
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


        //  The Panels menu sends its line in the session's mode; GSSquared
        //  and WinDbg lines open and close a panel as AppleWin's do.
        TEST_METHOD (PanelLinesWorkInEveryMode)
        {
            for (CommandMode mode : { CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Monitor })
            {
                MachineRig      rig;
                DebugSession  & session = rig.controller.GetSession();
                Reply           reply;



                reply = rig.view.ExecuteWindowLine (session, DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("mmu", true), mode), mode);
                Assert::IsTrue   (reply.status == CommandStatus::Ok, L"PANEL mmu");
                Assert::IsTrue   (rig.view.IsPanelOpen ("mmu"));

                reply = rig.view.ExecuteWindowLine (session, DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("mmu", false), mode), mode);
                Assert::IsTrue   (reply.status == CommandStatus::Ok, L"PANEL CLOSE mmu");
                Assert::IsFalse  (rig.view.IsPanelOpen ("mmu"));
            }
        }


        //  The Device menu's line opens its panel in every command mode.
        TEST_METHOD (ThePanelMenuLineOpensInEveryMode)
        {
            for (CommandMode mode : { CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Monitor })
            {
                MachineRig  rig;
                std::string line = DebuggerCommandLines::GetModeLine (DebuggerCommandLines::GetPanelLine ("mmu", true), mode);
                Reply       reply;



                reply = rig.view.ExecuteWindowLine (rig.controller.GetSession(), line, mode);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (line).c_str());
                Assert::IsTrue   (rig.view.IsPanelOpen ("mmu"));
            }
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


        //  A row's tip is over its label and its value.
        TEST_METHOD (APanelRowsTipIsOverItsLabelAndValue)
        {
            DiagnosticsSnapshot                           snapshot;
            std::vector<std::vector<DxuiListView::Cell>>  rows;



            snapshot.groups.push_back ({ "Switches", { IDiagnosticsProvider::MakeFlagRow ("RAMRD", true, "Reads of aux RAM.") } });
            rows = DiagnosticsPane::MakeRows (snapshot);

            Assert::AreEqual (std::wstring (L"Reads of aux RAM."), rows[1][0].tip);
            Assert::AreEqual (std::wstring (L"Reads of aux RAM."), rows[1][1].tip);
            Assert::AreEqual (std::wstring (L"ON"),                rows[1][1].text);
        }


        //  The graphic follows the payload's kind; only a change of kind asks
        //  the frame to lay out again.
        TEST_METHOD (APanelShowsTheGraphicItsPayloadAsksFor)
        {
            DxuiListView         list;
            MemoryMapBar         map;
            DiskHeadView         head;
            MeterBar             meters;
            SoundBoardView       board;
            DiagnosticsPane      pane ("fake", L"Fake", &list, &map, &head, &meters, &board);
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
