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
#include "Ui/Debugger/OpenViewsText.h"
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
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeViewTests
    //
    //  Where the disassembly views start, follow the PC and scroll.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeViewTests)
    {
    public:
        TEST_METHOD (ANavigationPutsTheAddressInTheMiddle)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            int                   row = -1;



            rig.view.SetCodeLines (20);
            rig.view.CenterCodeOn (0x0800);
            snapshot = rig.view.Build (rig.controller.GetSession());

            for (int i = 0; i < (int) snapshot.code.size(); i++)
            {
                row = (snapshot.code[(size_t) i].address == 0x0800) ? i : row;
            }

            Assert::AreEqual (10, row, L"in the middle, never on an edge");

            rig.view.CenterCodeOn (0x0002);
            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsTrue (snapshot.code.front().address <= 0x0002, L"at the bottom of memory, as near the middle as it can be");
        }


        TEST_METHOD (OneDisassemblyViewFollowsThePcAndTheRestStayPut)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  first;
            DebuggerViewSnapshot  moved;
            Cpu6502Registers      r        = rig.controller.GetSession().GetTarget().GetRegisters();
            auto                  contains = [] (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, Word address)
            {
                return std::any_of (lines.begin(), lines.end(), [address] (const DebuggerViewSnapshot::CodeLine & line) { return line.address == address; });
            };



            rig.view.SetCodeLines (20, 0);
            rig.view.SetCodeLines (20, 1);
            rig.view.OpenCodeView (1, 0x0800);
            first = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (first.codeOpen[1]);
            Assert::IsTrue   (contains (first.codeViews[1], 0x0800), L"a new view opens where it was asked to");
            Assert::IsTrue   (contains (first.codeViews[0], 0x0300), L"the first still shows the PC");
            Assert::AreEqual (0, first.followView);

            rig.view.SetFollowView (1);
            r.pc = 0x0900;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            moved = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (contains (moved.codeViews[1], 0x0900), L"the view following the PC goes to it");
            Assert::AreEqual (first.codeViews[0].front().address, moved.codeViews[0].front().address, L"the view that gave it up stays put");

            rig.view.CloseCodeView (1);
            Assert::AreEqual (0, rig.view.GetFollowView(), L"closing the follower hands the PC back to the first");
        }


        //  Paused, the code pane scrolls through the line list: up over code
        //  the CPU fetched, it lands on the fetched start where a guessed
        //  alignment from three bytes a line would land inside the LDA; up
        //  over a marked range, on the range's data lines.
        TEST_METHOD (ScrollingUpLandsOnFetchedStartsAndDataLines)
        {
            std::unique_ptr<MachineRig>   held = std::make_unique<MachineRig>();
            MachineRig                  & rig  = *held;
            MemoryBus                   & bus  = rig.machine.GetMemoryBus();
            DebuggerViewSnapshot          shown;



            //  $2000: NOP, then LDA $EAEA whose operand is NOPs; the CPU ran
            //  the NOP at $2002, so that is where an instruction starts.
            for (Word at = 0x1F00; at < 0x2100; at++)
            {
                bus.WriteByte (at, 0xEA);
            }

            bus.WriteByte (0x2001, 0xAD);
            bus.NoteFetch (0x2002);
            bus.NoteFetch (0x2003);

            rig.view.SetCodeLines   (10);
            rig.view.SetCodeAddress (0x2003);
            shown = rig.view.Build (rig.controller.GetSession());

            rig.view.ScrollCode (-1);
            shown = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((int) 0x2002, (int) shown.code[0].address, L"the fetched NOP, not the LDA's operand");

            rig.Run ("DB 2100:210F");
            rig.view.SetCodeAddress (0x2110);
            shown = rig.view.Build (rig.controller.GetSession());

            rig.view.ScrollCode (-1);
            shown = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((int) 0x2108, (int) shown.code[0].address, L"the range's second line of eight bytes");
        }


        //  A jump into the middle of an instruction, as the BIT skip does:
        //  the instruction before the PC is cut short, so the PC starts a row.
        TEST_METHOD (ThePcStartsARowInsideAnInstruction)
        {
            static constexpr Byte  kCode[] = { 0x4C, 0x04, 0x60, 0x2C, 0xA9, 0x01, 0x60 };   // JMP $6004, BIT $01A9, RTS

            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          shown;
            bool                          isPcRow  = false;
            bool                          isCutRow = false;



            for (size_t i = 0; i < sizeof (kCode); i++)
            {
                rig.machine.GetMemoryBus().WriteByte ((Word) (0x6000 + i), kCode[i]);
            }

            rig.Run ("R PC 6004");
            rig.view.SetCodeLines   (10);
            rig.view.SetCodeAddress (0x6000);
            shown = rig.view.Build (rig.controller.GetSession());

            for (const DebuggerViewSnapshot::CodeLine & line : shown.code)
            {
                isPcRow  = isPcRow  || line.address == 0x6004;
                isCutRow = isCutRow || (line.address == 0x6003 && line.bytes == "2C");
            }

            Assert::IsTrue (isPcRow,  L"a row starts at the PC");
            Assert::IsTrue (isCutRow, L"the BIT before it is its first byte alone");
        }


        TEST_METHOD (TheWheelScrollsTheCodePaneThroughMemory)
        {
            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          first;
            DebuggerViewSnapshot          down;
            DebuggerViewSnapshot          up;
            size_t                        lastHeld = 0;



            rig.view.SetCodeLines (20);
            first = rig.view.Build (rig.controller.GetSession());

            rig.view.ScrollCode (3);
            down = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual (first.code[3].address, down.code[0].address, L"three instructions down");

            rig.view.ScrollCode (-3);
            up = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual (first.code[0].address, up.code[0].address, L"and back");

            for (int i = 0; i < 40; i++)
            {
                rig.view.ScrollCode (-20);
                up = rig.view.Build (rig.controller.GetSession());
            }

            Assert::AreEqual ((Word) 0x0000, up.code[0].address, L"up past everything stops at $0000");

            //  And down: from near the top of memory, the wheel alone reaches
            //  the line holding $FFFF, and goes no further.
            rig.view.SetCodeAddress (0xFF00);

            for (int i = 0; i < 40; i++)
            {
                rig.view.ScrollCode (20);
                down = rig.view.Build (rig.controller.GetSession());
            }

            lastHeld = down.code.back().address + (down.code.back().bytes.size() + 1) / 3 - 1;

            Assert::IsTrue (lastHeld >= 0xFFFF, std::format (L"the last line, at ${:04X}, holds $FFFF", down.code.back().address).c_str());
            Assert::IsTrue (down.code[0].address >= 0xFF00, L"down past everything stops at the top of memory, without wrapping to $0000");
            Assert::AreEqual ((size_t) rig.view.GetCodeLines(), down.code.size(), L"with every line of the pane filled");

            //  A view taken straight to the vectors lists them too.
            rig.view.CenterCodeOn (0xFFFC);
            down = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (down.code.empty(), L"the lines around the reset vector");
        }

        TEST_METHOD (AScrolledFollowingViewFollowsAgainOnceThePcMoves)
        {
            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          scrolled;
            DebuggerViewSnapshot          stepped;
            DebuggerViewSnapshot          left;
            Cpu6502Registers              r        = rig.controller.GetSession().GetTarget().GetRegisters();



            rig.view.SetCodeLines (20);
            (void) rig.view.Build (rig.controller.GetSession());
            rig.view.ScrollCode (2);
            scrolled = rig.view.Build (rig.controller.GetSession());

            r.pc = scrolled.code[5].address;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            stepped = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual (scrolled.code[0].address, stepped.code[0].address, L"a PC still on the lines moves the marker, not the view");
            Assert::IsTrue   (stepped.code[5].isCurrent);

            r.pc = 0x0900;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            left = rig.view.Build (rig.controller.GetSession());
            Assert::IsTrue (std::any_of (left.code.begin(), left.code.end(), [] (const DebuggerViewSnapshot::CodeLine & line) { return line.isCurrent; }),
                            L"a PC off the lines brings the view to it");
        }

        //  While the machine runs, a view the user scrolled isn't moved to the
        //  PC until kScrollHoldMs pass with no more scrolling.
        TEST_METHOD (AViewScrolledWhileRunningFollowsThePcOnlyAfterAPause)
        {
            static uint64_t               s_now    = 1000;
            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          scrolled;
            DebuggerViewSnapshot          moved;
            Cpu6502Registers              r        = rig.controller.GetSession().GetTarget().GetRegisters();
            auto                          isAtPc   = [] (const DebuggerViewSnapshot & snapshot)
            {
                return std::any_of (snapshot.code.begin(), snapshot.code.end(), [] (const DebuggerViewSnapshot::CodeLine & line) { return line.isCurrent; });
            };



            s_now = 1000;
            rig.view.SetClock ([] { return s_now; });
            rig.view.SetCodeLines (20);
            (void) rig.view.Build (rig.controller.GetSession(), false);
            rig.view.ScrollCode (40);
            scrolled = rig.view.Build (rig.controller.GetSession(), false);

            r.pc = 0x0900;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            s_now += DebuggerViewState::kScrollHoldMs - 1;
            moved  = rig.view.Build (rig.controller.GetSession(), false);
            Assert::AreEqual (scrolled.code[0].address, moved.code[0].address, L"the view isn't moved while the scroll is recent");
            Assert::IsFalse  (isAtPc (moved));

            s_now += 1;
            moved  = rig.view.Build (rig.controller.GetSession(), false);
            Assert::IsTrue (isAtPc (moved), L"once the time passes, the view follows the PC");
            Assert::IsTrue (GetPcIndex (moved) >= 5 && GetPcIndex (moved) <= 15, L"with the PC in its middle, not on an edge row");
        }

        //  The index of the PC's line in the snapshot's code, or -1.
        static int GetPcIndex (const DebuggerViewSnapshot & snapshot)
        {
            for (size_t i = 0; i < snapshot.code.size(); i++)
            {
                if (snapshot.code[i].isCurrent)
                {
                    return (int) i;
                }
            }

            return -1;
        }

        //  A view of more lines than fit, as with source rows above them, moves
        //  when the PC is on a line past the ones in view, and puts the PC in
        //  the middle of those.
        TEST_METHOD (APcPastTheLinesInViewIsBroughtIntoTheirMiddle)
        {
            std::unique_ptr<MachineRig>   held   = std::make_unique<MachineRig>();
            MachineRig                  & rig    = *held;
            DebuggerViewSnapshot          before;
            DebuggerViewSnapshot          after;
            Cpu6502Registers              r      = rig.controller.GetSession().GetTarget().GetRegisters();



            rig.view.SetCodeLines (20);
            rig.view.SetCodeShown (6);
            before = rig.view.Build (rig.controller.GetSession());

            r.pc = before.code[10].address;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            after = rig.view.Build (rig.controller.GetSession());

            Assert::AreNotEqual (before.code[0].address, after.code[0].address, L"line 10 is past the 6 in view, so the view moves");
            Assert::AreEqual    (3, GetPcIndex (after), L"to put the PC in the middle of the 6");

            r.pc = after.code[4].address;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            before = after;
            after  = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (before.code[0].address, after.code[0].address, L"a PC on a line in view doesn't move the view");
        }

        //  While a view's menu is open, a running machine doesn't move it; once
        //  it closes, the view isn't moved for the scroll pause, then follows the PC.
        TEST_METHOD (AViewWhoseMenuIsOpenIsntMovedWhileRunning)
        {
            static uint64_t               s_now    = 1000;
            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          before;
            DebuggerViewSnapshot          after;
            Cpu6502Registers              r        = rig.controller.GetSession().GetTarget().GetRegisters();



            s_now = 1000;
            rig.view.SetClock ([] { return s_now; });
            rig.view.SetCodeLines (20);
            before = rig.view.Build (rig.controller.GetSession(), false);

            rig.view.HoldCode (true);
            r.pc = 0x0900;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            s_now += DebuggerViewState::kScrollHoldMs * 3;
            after  = rig.view.Build (rig.controller.GetSession(), false);
            Assert::AreEqual (before.code[0].address, after.code[0].address, L"not moved while the menu is open");
            Assert::AreEqual (-1, GetPcIndex (after));

            rig.view.HoldCode (false);
            s_now += DebuggerViewState::kScrollHoldMs - 1;
            after  = rig.view.Build (rig.controller.GetSession(), false);
            Assert::AreEqual (before.code[0].address, after.code[0].address, L"nor for the pause after it closes");

            s_now += 1;
            after  = rig.view.Build (rig.controller.GetSession(), false);
            Assert::IsTrue (GetPcIndex (after) >= 5 && GetPcIndex (after) <= 15, L"then the PC, in the middle");
        }


        TEST_METHOD (TheCodePaneHoldsStillWhileThePcIsOnALineItShows)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  first  = rig.view.Build (rig.controller.GetSession());
            Cpu6502Registers      r      = rig.controller.GetSession().GetTarget().GetRegisters();
            DebuggerViewSnapshot  second;



            //  One instruction on, still among the lines already shown.
            r.pc = 0x0302;
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            second = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (first.code[0].address, second.code[0].address,
                              L"the pane does not move while the PC is on it");

            for (const DebuggerViewSnapshot::CodeLine & line : second.code)
            {
                Assert::AreEqual (line.address == 0x0302, line.isCurrent, L"only the marker moved");
            }
        }


        TEST_METHOD (TheCodePaneMovesWhenThePcLeavesIt)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  first   = rig.view.Build (rig.controller.GetSession());
            Cpu6502Registers      r       = rig.controller.GetSession().GetTarget().GetRegisters();
            DebuggerViewSnapshot  second;
            int                   current = -1;



            r.pc = 0x0800;
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            second = rig.view.Build (rig.controller.GetSession());

            for (int i = 0; i < (int) second.code.size(); i++)
            {
                current = second.code[(size_t) i].isCurrent ? i : current;
            }

            Assert::AreNotEqual (first.code[0].address, second.code[0].address, L"it followed");
            Assert::IsTrue      (current > 0, L"and the PC came back with code above it, not at the top");
        }


        TEST_METHOD (TheCodePaneFillsTheLinesItIsGiven)
        {
            MachineRig  rig;



            rig.view.SetCodeLines (30);
            Assert::AreEqual ((size_t) 30, rig.view.Build (rig.controller.GetSession()).code.size());

            rig.view.SetCodeLines (8);
            Assert::AreEqual ((size_t) 8, rig.view.Build (rig.controller.GetSession()).code.size());
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
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OpenViewsTextTests
    //
    //  The open views as the text the preferences keep.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OpenViewsTextTests)
    {
    public:
        TEST_METHOD (TheOpenViewsSurviveARestartAsText)
        {
            DebuggerViewSnapshot      snapshot;
            OpenViewsText::OpenViews  views;
            std::string               text;



            //  Disassembly 3 open around $E000, following the PC; memory 2 at
            //  $0300; the MMU panel open and the clock closed.
            snapshot.codeOpen[2]  = true;
            snapshot.codeViews[2] = { { 0xDFFC }, { 0xE000 }, { 0xE002 } };
            snapshot.followView   = 2;
            snapshot.memoryWindows = { { 1, 0x0000 }, { 2, 0x0300 } };
            snapshot.panels        = { { "mmu", "MMU", true }, { "clock", "Clock", false } };

            text = OpenViewsText::FormatOpenViews (snapshot);
            Assert::AreEqual (std::string ("code3=DFFC follow=3 memory2=0300 panel=mmu"), text,
                              L"the top line, which the view reopens on; the first views are never written");

            views = OpenViewsText::ParseOpenViews (text);
            Assert::IsTrue   (views.code[2] == std::optional<Word> (0xDFFC));
            Assert::IsFalse  (views.code[1].has_value());
            Assert::AreEqual (2, views.follow);
            Assert::IsTrue   (views.memory[1] == std::optional<Word> (0x0300));
            Assert::AreEqual ((size_t) 1, views.panels.size());
            Assert::AreEqual (std::string ("mmu"), views.panels[0]);

            //  What a later build might write is passed over, not fatal.
            views = OpenViewsText::ParseOpenViews ("code2=0400 hexdump9=FFFF code9=0000 memory2=zz panel=disk");
            Assert::IsTrue   (views.code[1] == std::optional<Word> (0x0400));
            Assert::IsFalse  (views.memory[1].has_value(), L"not hex");
            Assert::AreEqual (std::string ("disk"), views.panels.at (0));

            Assert::AreEqual (std::string(), OpenViewsText::FormatOpenViews (DebuggerViewSnapshot {}), L"nothing optional open");
        }
    };
}
