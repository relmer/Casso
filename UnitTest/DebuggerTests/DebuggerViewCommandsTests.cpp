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
    TEST_CLASS (WindowCommandTests)
    {
    public:

        static Reply RunInWindow (MachineRig & rig, const std::string & line, CommandMode mode = CommandMode::AppleWin)
        {
            return rig.view.ExecuteWindowLine (rig.controller.GetSession(), line, mode);
        }



        TEST_METHOD (ConsoleEchoTakesThePromptOfTheModeTheLineWasTypedIn)
        {
            MachineRig                rig;
            std::vector<std::string>  lines;



            lines = rig.view.ExecuteConsoleLine (rig.controller.GetSession(), "MODE MONITOR");

            Assert::IsFalse  (lines.empty());
            Assert::AreEqual (std::string (">MODE MONITOR"), lines[0], L"typed in AppleWin mode");

            lines = rig.view.ExecuteConsoleLine (rig.controller.GetSession(), "/MODE APPLEWIN");

            Assert::IsFalse  (lines.empty());
            Assert::AreEqual (std::string ("*/MODE APPLEWIN"), lines[0], L"typed in Monitor mode");
        }


        //  Focusing the source or disassembly pane sends SRC in the words of
        //  the session's mode; the bare AppleWin line is unknown elsewhere.
        TEST_METHOD (TheSourceStepLineRunsInEveryMode)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                auto  rig = std::make_unique<MachineRig>();   // heap: two rigs overflow the frame (C6262)



                for (const char * line : { "SRC ON", "SRC OFF" })
                {
                    Reply  reply = RunInWindow (*rig, DebuggerViewState::GetModeLine (line, mode), mode);

                    Assert::IsTrue (reply.status != CommandStatus::Unknown, L"the mode reads the line");
                }
            }

            {
                auto  rig = std::make_unique<MachineRig>();

                Assert::IsTrue (RunInWindow (*rig, "SRC ON", CommandMode::WinDbg).status == CommandStatus::Unknown, L"not a WinDbg line");
            }
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



        //  The row of the code pane that shows `address`, or -1.
        static int GetRowOf (const DebuggerViewSnapshot & snapshot, Word address)
        {
            for (size_t row = 0; row < snapshot.code.size(); row++)
            {
                if (snapshot.code[row].address == address)
                {
                    return (int) row;
                }
            }

            return -1;
        }



        //  RET and -> are navigations: the target lands on the middle line,
        //  with what leads to it above.
        TEST_METHOD (RetAndArrowGoToTheAddressesTheyRead)
        {
            MachineRig        rig;
            DebugSession    & session = rig.controller.GetSession();
            Cpu6502Registers  r       = session.GetTarget().GetRegisters();
            Reply             reply;
            constexpr Byte    kNop    = 0xEA;



            r.sp = 0xFD;
            session.GetTarget().SetRegisters (r);
            rig.machine.GetMemoryBus().WriteByte (0x01FE, 0x34);
            rig.machine.GetMemoryBus().WriteByte (0x01FF, 0x12);

            //  NOPs above both targets, so the lines above them are certain.
            for (Word address = 0x1200; address < 0x1235; address++)
            {
                rig.machine.GetMemoryBus().WriteByte (address, kNop);
            }

            for (Word address = 0x03E0; address < 0x0400; address++)
            {
                rig.machine.GetMemoryBus().WriteByte (address, kNop);
            }

            RunInWindow (rig, "RET");
            Assert::AreEqual (DebuggerViewState::kCodeLines / 2, GetRowOf (rig.view.Build (session), 0x1235),
                              L"one past the pushed address, on the middle line");

            rig.view.SetCodeAddress (0x0302);
            RunInWindow (rig, "->");
            Assert::AreEqual (DebuggerViewState::kCodeLines / 2, GetRowOf (rig.view.Build (session), 0x0400),
                              L"STA $0400's address, on the middle line");

            rig.view.SetCodeAddress (0x0300);
            reply = RunInWindow (rig, "->");
            Assert::IsTrue   (reply.status == CommandStatus::Error, L"LDA #$41 has no address");
            Assert::AreEqual ((Word) 0x0300, rig.view.GetCodeAddress().value_or (0), L"and the pane stays");
        }



        //  With nothing readable on the stack there is no return address, and
        //  RET says so rather than moving the pane.
        TEST_METHOD (RetWithAnUnreadableStackIsAnError)
        {
            TestCpu                    cpu;
            UnreadableStackTarget      target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;
            DebuggerViewState          view;
            Reply                      reply;



            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();
            handlers.Attach (session);
            view.SetCodeAddress (0x0300);

            reply = view.ExecuteWindowLine (session, "RET", CommandMode::AppleWin);

            Assert::IsTrue   (reply.status == CommandStatus::Error, L"no return address");
            Assert::AreEqual ((Word) 0x0300, view.GetCodeAddress().value_or (0), L"and the pane stays");
        }



        TEST_METHOD (ArrowGoesToABitBranchTarget)
        {
            MachineRig  rig ("Apple2eEnhanced");
            Reply       reply;



            //  BBR0 $12,+$10 at $0310: the target is $0310 + 3 + $10.
            rig.machine.GetMemoryBus().WriteByte (0x0310, 0x0F);
            rig.machine.GetMemoryBus().WriteByte (0x0311, 0x12);
            rig.machine.GetMemoryBus().WriteByte (0x0312, 0x10);

            rig.view.SetCodeAddress (0x0310);
            reply = RunInWindow (rig, "->");
            Assert::IsTrue   (reply.status == CommandStatus::Ok, L"BBR0 has a branch target");
            Assert::IsTrue   (GetRowOf (rig.view.Build (rig.controller.GetSession()), 0x0323) >= 0);
        }



        //  A following pane shows the PC in its middle; one V moves the lines
        //  on screen up by one, not the PC to the top.
        TEST_METHOD (CursorMovesStartFromTheLinesAFollowingPaneShows)
        {
            MachineRig            rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;
            constexpr Byte        kNop     = 0xEA;



            //  NOPs above the PC, so the lines above it are certain.
            for (Word address = 0x0200; address < 0x0300; address++)
            {
                rig.machine.GetMemoryBus().WriteByte (address, kNop);
            }

            snapshot = rig.view.Build (session);
            Assert::IsTrue   (snapshot.code.size() > 2);
            Assert::IsTrue   (snapshot.code[0].address != 0x0300, L"the PC is not on the top line");

            RunInWindow (rig, "V");
            Assert::AreEqual (snapshot.code[1].address, rig.view.GetCodeAddress().value_or (0), L"V moves one line");

            RunInWindow (rig, "^");
            Assert::AreEqual (snapshot.code[0].address, rig.view.GetCodeAddress().value_or (0), L"^ moves it back");
        }



        //  A page is as many lines as the pane has.
        TEST_METHOD (APageIsThePanesOwnLineCount)
        {
            MachineRig            rig;
            DebugSession        & session = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;
            constexpr int         kLines  = 3;



            rig.view.SetCodeLines   (kLines + 1);
            rig.view.SetCodeAddress (0x0300);
            snapshot = rig.view.Build (session);
            Assert::AreEqual ((size_t) (kLines + 1), snapshot.code.size());

            rig.view.SetCodeLines   (kLines);
            rig.view.SetCodeAddress (0x0300);
            RunInWindow (rig, "PAGEDN");
            Assert::AreEqual (snapshot.code[kLines].address, rig.view.GetCodeAddress().value_or (0), L"PAGEDN");

            RunInWindow (rig, "PAGEUP");
            Assert::AreEqual ((Word) 0x0300, rig.view.GetCodeAddress().value_or (0), L"PAGEUP");
        }



        //  Moving down stops at the page whose last line holds $FFFF, as the
        //  wheel does, rather than wrapping to $0000.
        TEST_METHOD (CursorMovesStopAtTheTopOfMemory)
        {
            MachineRig            rig;
            DebugSession        & session = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;



            rig.view.SetCodeAddress (0xFFFE);
            RunInWindow (rig, "V");
            snapshot = rig.view.Build (session);

            Assert::IsTrue   (rig.view.GetCodeAddress().value_or (0) > 0xFF00, L"no wrap to $0000");
            Assert::AreEqual ((size_t) DebuggerViewState::kCodeLines, snapshot.code.size(), L"every line filled");

            rig.view.SetCodeAddress (0x0000);
            RunInWindow (rig, "PAGEUP");
            Assert::AreEqual ((Word) 0x0000, rig.view.GetCodeAddress().value_or (0xFFFF), L"no wrap below $0000");
        }



        TEST_METHOD (MiniMemoryCommandsMoveTheMemoryPane)
        {
            MachineRig  rig;



            Assert::IsTrue   (RunInWindow (rig, "MD1 1000").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x1000, rig.view.GetMemoryAddress());

            Assert::IsTrue   (RunInWindow (rig, "/MT1 $2000", CommandMode::Monitor).status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryAddress(), L"a / line in Monitor mode");

            Assert::IsTrue   (RunInWindow (rig, "MD1 PC+2").status == CommandStatus::Ok, L"an expression");
            Assert::AreEqual ((Word) (rig.controller.GetSession().GetTarget().GetRegisters().pc + 2), rig.view.GetMemoryAddress());

            Assert::IsTrue   (RunInWindow (rig, "MD2 3000").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x3000, rig.view.GetMemoryWindowAddress (2).value_or (0), L"the 2 forms move the second window");
            Assert::AreNotEqual ((Word) 0x3000, rig.view.GetMemoryAddress(), L"and leave the first");

            Assert::IsTrue   (RunInWindow (rig, "MD1 2000").status == CommandStatus::Ok);
            Assert::IsTrue   (RunInWindow (rig, "MA1").status == CommandStatus::Error, L"no address");
            Assert::IsTrue   (RunInWindow (rig, "MA1 XYZ").status == CommandStatus::Error, L"not an expression");
            Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryAddress(), L"a bad address leaves the pane");
        }


        TEST_METHOD (MiniMemoryCommandsTakeAddressExpressions)
        {
            MachineRig  rig;



            Assert::IsTrue   (RunInWindow (rig, "MD1 300+10").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x0310, rig.view.GetMemoryAddress(), L"an expression");

            Assert::IsTrue   (RunInWindow (rig, "MA1 PC").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x0300, rig.view.GetMemoryAddress(), L"a register");

            Assert::IsTrue   (RunInWindow (rig, "MD1 300 + 10").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x0310, rig.view.GetMemoryAddress(), L"an expression with spaces, read whole");
        }


        //  The console's MD and the memory address box take the same text to
        //  the same place.
        TEST_METHOD (TheConsoleAndTheAddressBoxResolveAlike)
        {
            MachineRig            rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r        = session.GetTarget().GetRegisters();



            rig.machine.GetMemoryBus().WriteByte (0x0028, 0x00);
            rig.machine.GetMemoryBus().WriteByte (0x0029, 0x05);
            r.y = 0x03;
            session.GetTarget().SetRegisters (r);

            rig.view.RequestGoTo (session, 1, "(basl),y");
            snapshot = rig.view.Build (session);

            Assert::IsTrue   (snapshot.goTo.has_value());
            Assert::IsTrue   (snapshot.goTo->address == std::optional<Word> (0x0503), L"the box: BASL is a ROM symbol, read in any case");

            Assert::IsTrue   (RunInWindow (rig, "MD1 (basl),y").status == CommandStatus::Ok, L"the console takes the same text");
            Assert::AreEqual ((Word) 0x0503, rig.view.GetMemoryAddress());
        }


        TEST_METHOD (ACursorCommandDropsAPendingScrollOrCenter)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.ScrollCode (5);
            Assert::IsTrue (RunInWindow (rig, "V").status == CommandStatus::Ok);
            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((Word) 0x0302, snapshot.codeViews[0].front().address, L"a queued wheel scroll");

            rig.view.CenterCodeOn (0x1000);
            RunInWindow (rig, "V");
            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((Word) 0x0305, snapshot.codeViews[0].front().address, L"a queued navigation");
        }


        TEST_METHOD (APaneOnADataBlockFillsItsLines)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            Assert::IsTrue   (rig.Run ("DB 1000:10FF").status == CommandStatus::Ok);
            rig.view.SetCodeAddress (0x1000);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) DebuggerViewState::kCodeLines, snapshot.codeViews[0].size());
            Assert::AreEqual ((Word) 0x1008, snapshot.codeViews[0][1].address, L"eight bytes a line");
        }


        TEST_METHOD (UpStepsBackOneDataLine)
        {
            MachineRig  rig;



            Assert::IsTrue   (rig.Run ("DB 1000:10FF").status == CommandStatus::Ok);

            rig.view.SetCodeAddress (0x1010);
            RunInWindow (rig, "^");
            Assert::AreEqual ((Word) 0x1008, rig.view.GetCodeAddress().value_or (0), L"one DB line back");

            rig.view.SetCodeAddress (0x1100);
            RunInWindow (rig, "^");
            Assert::AreEqual ((Word) 0x10F8, rig.view.GetCodeAddress().value_or (0), L"onto the block's last line");
        }



        //  The memory pane places its view only once, then follows its own
        //  scrolling; a new address reaches it as a Go to, as the Go to box
        //  does, or the pane scrolls straight back.
        TEST_METHOD (MiniMemoryCommandsAndData2GoToTheirAddress)
        {
            MachineRig            rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot = rig.view.Build (session);
            uint32_t              serial   = snapshot.goTo.has_value() ? snapshot.goTo->serial : 0;



            RunInWindow (rig, "MD1 2000");
            snapshot = rig.view.Build (session);

            Assert::IsTrue   (snapshot.goTo.has_value(), L"MD1 asks the pane to go there");
            Assert::AreEqual (1, snapshot.goTo->window);
            Assert::AreEqual ((Word) 0x2000, snapshot.goTo->address.value_or (0));
            Assert::IsTrue   (snapshot.goTo->serial != serial, L"a new Go to");

            serial = snapshot.goTo->serial;
            rig.view.OpenMemoryWindow  (2, 0x1000);
            rig.view.CloseMemoryWindow (2);
            RunInWindow (rig, "DATA2");
            snapshot = rig.view.Build (session);

            Assert::AreEqual (2, snapshot.goTo->window, L"a reopened window 2 goes to its address");
            Assert::AreEqual ((Word) 0x2000, snapshot.goTo->address.value_or (0));
            Assert::IsTrue   (snapshot.goTo->serial != serial);
        }



        //  The 2 forms move the second memory window, as DATA2 numbers it.
        TEST_METHOD (MiniMemoryTwoFormsMoveTheSecondMemoryWindow)
        {
            MachineRig  rig;



            for (const char * line : { "MD2 2000", "MA2 2000", "MT2 2000", "M2 2000" })
            {
                rig.view.CloseMemoryWindow (2);
                rig.view.SetMemoryAddress (0x0000);

                Assert::IsTrue   (RunInWindow (rig, line).status == CommandStatus::Ok);
                Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryWindowAddress (2).value_or (0), L"the second window moves");
                Assert::AreEqual ((Word) 0x0000, rig.view.GetMemoryAddress(), L"the first stays");
            }
        }



        //  `.` returns the pane to the PC as the Follow PC control does, a
        //  pending scroll included.
        TEST_METHOD (DotFollowsThePcAsTheFollowControlDoes)
        {
            std::unique_ptr<MachineRig>   held   = std::make_unique<MachineRig>();
            MachineRig                  & rig    = *held;
            DebuggerViewState             other;
            DebuggerViewSnapshot          dot;
            DebuggerViewSnapshot          follow;



            rig.view.SetCodeLines (20, 0);
            other.SetCodeLines (20, 0);

            rig.view.ScrollCode (5);
            RunInWindow (rig, ".");
            dot = rig.view.Build (rig.controller.GetSession());

            other.ShowPcIn (0);
            follow = other.Build (rig.controller.GetSession());

            Assert::IsFalse  (dot.code.empty());
            Assert::AreEqual (follow.code.front().address, dot.code.front().address);
        }



        //  The rig's Run is the console's path, so window-only names answer
        //  as the window does.
        TEST_METHOD (RigRunTakesTheConsolePath)
        {
            MachineRig  rig;
            Reply       reply = rig.Run ("WIN");



            Assert::IsTrue   (reply.status == CommandStatus::Ok);
            Assert::AreEqual (std::string ("This window shows every pane at once."), reply.text.at (0));
        }



        //  Line assembly takes the lines typed, not the panes' own reads.
        TEST_METHOD (PanesReadTheMachineWhileAssembling)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetCodeLines (20, 0);
            rig.Run ("A 300");
            Assert::IsTrue   (rig.controller.GetSession().IsAssembling());

            snapshot = rig.view.Build (rig.controller.GetSession(), true);

            Assert::IsTrue   (rig.controller.GetSession().IsAssembling(), L"still assembling");
            Assert::AreEqual ((Word) 0x0300, snapshot.pc);
            Assert::IsFalse  (snapshot.registers.empty());
            Assert::IsFalse  (snapshot.memory.empty());
            Assert::AreEqual ((Word) 0x0300, LineAt (snapshot, 0x0300).address, L"the code pane shows the PC");
        }



        //  A bare U or D continues from the user's last listing, not from
        //  where the panes last read.
        TEST_METHOD (PaneReadsLeaveBareUAndDContinuing)
        {
            MachineRig  rig;
            Reply       reply;



            rig.view.SetCodeLines (20, 0);
            rig.view.SetMemoryAddress (0x2000);

            reply = rig.Run ("D 1000");
            Assert::IsTrue   (std::holds_alternative<MemoryData> (reply.data));
            reply = rig.Run ("U 300");
            Assert::IsTrue   (std::holds_alternative<DisassemblyData> (reply.data));

            Word  next = (Word) (std::get<DisassemblyData> (reply.data).lines.back().instruction.address
                               + std::get<DisassemblyData> (reply.data).lines.back().instruction.bytes.size());

            (void) rig.view.Build (rig.controller.GetSession(), true);
            RunInWindow (rig, "V");

            reply = rig.Run ("U");
            Assert::AreEqual (next, std::get<DisassemblyData> (reply.data).lines.front().instruction.address, L"U continues");

            reply = rig.Run ("D");
            Assert::AreEqual ((Word) 0x1040, std::get<MemoryData> (reply.data).rows.front().address, L"D continues");
        }



        //  CODE, DATA and CONSOLE bring a pane forward; the second disassembly
        //  and memory window open when they are not.
        //  A Monitor line of several commands prints each command's output
        //  once, a failure's error included.
        TEST_METHOD (AMonitorLineOfSeveralCommandsPrintsEachErrorOnce)
        {
            MachineRig                rig;
            Reply                     reply = rig.Run ("300 C0FF:00", CommandMode::Monitor);
            std::vector<std::string>  text;
            size_t                    errors = 0;



            text = reply.text;

            for (const std::string & line : text)
            {
                errors += line.starts_with ("Error:") ? 1 : 0;
            }

            Assert::IsTrue   (reply.status != CommandStatus::Ok, L"the deposit to I/O failed");
            Assert::AreEqual ((size_t) 1, errors,                L"its error is printed once");
            Assert::IsTrue   (text.front().starts_with ("0300"), L"the examine before it printed");
        }



        TEST_METHOD (WindowNamesBringAPaneForward)
        {
            MachineRig            rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot = rig.view.Build (session);
            uint32_t              serial   = snapshot.showPaneSerial;



            Assert::IsTrue   (RunInWindow (rig, "CONSOLE").status == CommandStatus::Ok);
            snapshot = rig.view.Build (session);
            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), snapshot.showPane);
            Assert::AreEqual (serial + 1, snapshot.showPaneSerial);

            Assert::IsFalse  (rig.view.IsCodeViewOpen (1));
            RunInWindow (rig, "code2");
            snapshot = rig.view.Build (session);
            Assert::AreEqual (DebuggerLayout::GetCodePaneId (1), snapshot.showPane);
            Assert::IsTrue   (rig.view.IsCodeViewOpen (1), L"CODE2 opens the second disassembly");

            RunInWindow (rig, "DATA2");
            snapshot = rig.view.Build (session);
            Assert::AreEqual (DebuggerLayout::GetMemoryPaneId (2), snapshot.showPane);
            Assert::IsTrue   (rig.view.GetMemoryWindowAddress (2).has_value(), L"DATA2 opens the second memory window");

            RunInWindow (rig, "DATA2");
            Assert::AreEqual (serial + 4, rig.view.Build (session).showPaneSerial, L"the same pane asked again is asked again");

            RunInWindow (rig, "/CODE", CommandMode::Monitor);
            Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), rig.view.Build (session).showPane, L"a / line in Monitor mode");
        }



        TEST_METHOD (LayoutViewAndAppearanceNamesReplyWithoutChangingThePanes)
        {
            MachineRig  rig;



            Assert::IsTrue (RunInWindow (rig, "WIN").status     == CommandStatus::Ok,           L"every pane is shown");
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
}
