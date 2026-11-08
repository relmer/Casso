#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"
#include "TestHelpers.h"
#include "WindowConsoleRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace WindowCursorCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CursorRig
    //
    //  The window's console over a real //e holding a program at $0300 whose
    //  instructions have every length and every kind of operand:
    //
    //      0300  LDA #$41       0312  LDA ($12),Y
    //      0302  STA $0400      0314  NOP
    //      0305  STA $0400,X    0315  JMP $0300
    //      0308  BEQ $030C      0318  ASL A
    //      030A  LDA $12        0319  LDA $12,X
    //      030C  JSR $FDED      031B  RTS
    //      030F  JMP ($03F0)
    //
    //  NOPs fill $0200 up to it and from its end to $0500, so a disassembly
    //  view following the PC at $0300 has ten one-byte lines above it and
    //  its top line is $02F6.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CursorRig : public WindowConsoleRig
    {
    public:
        static constexpr Byte  kNop       = 0xEA;
        static constexpr Word  kProgram   = 0x0300;
        static constexpr Word  kFollowTop = 0x02F6;
        static constexpr int   kMiddle    = DebuggerViewState::kCodeLines / 2;



        explicit CursorRig (const std::string & machineId = "Apple2e") :
            WindowConsoleRig (machineId)
        {
            Fill (0x0200, 0x0500, kNop);
            Poke (kProgram, { 0xA9, 0x41,             // LDA #$41
                              0x8D, 0x00, 0x04,       // STA $0400
                              0x9D, 0x00, 0x04,       // STA $0400,X
                              0xF0, 0x02,             // BEQ $030C
                              0xA5, 0x12,             // LDA $12
                              0x20, 0xED, 0xFD,       // JSR $FDED
                              0x6C, 0xF0, 0x03,       // JMP ($03F0)
                              0xB1, 0x12,             // LDA ($12),Y
                              0xEA,                   // NOP
                              0x4C, 0x00, 0x03,       // JMP $0300
                              0x0A,                   // ASL A
                              0xB5, 0x12,             // LDA $12,X
                              0x60 });                // RTS
            SetPc (kProgram);
        }



        //  A reply to a cursor command that moved the pane: Ok, one line, and
        //  the line it answers, as the console echoes it.
        static void AssertMoved (const Reply & reply, const std::string & line, Word top)
        {
            AssertReply (reply, CommandStatus::Ok, "", "", { std::format ("The code pane is at ${:04X}.", top) }, line);
            Assert::AreEqual (line, reply.command, Widen (line).c_str());
            Assert::IsTrue   (reply.verb == DebugVerb::None, Widen (line).c_str());
        }



        //  The first view's top line once the window builds it.
        Word GetTop()
        {
            DebuggerViewSnapshot  snapshot = Build();



            Assert::IsFalse (snapshot.codeViews[0].empty(), L"the first view has lines");
            return snapshot.codeViews[0][0].address;
        }



        //  A view following a PC ten NOPs below `top`, so its first build puts
        //  `top` on its top line. The bytes above `top` are the test's to set
        //  once it has, since the view holds its anchor while the PC is on it.
        void PlaceTop (Word top)
        {
            static constexpr int  kAbove = 40;
            static constexpr int  kBelow = 12;



            Fill ((Word) (top - kAbove), (uint32_t) top + kBelow, kNop);
            SetPc ((Word) (top + kMiddle));
            Assert::AreEqual (top, GetTop(), L"the pane's top line is where the test put it");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UnreadableStack
    //
    //  A target whose page 1 cannot be read. No real machine refuses a read of
    //  its stack, so this is the only way to reach RET's error.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class UnreadableStack : public MockDebugTarget
    {
    public:
        bool TryPeek (Word address, Byte & value) const override
        {
            static constexpr Word  kStackPage = 0x01;



            return (address >> 8) != kStackPage && MockDebugTarget::TryPeek (address, value);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FollowPcCommandTests
    //
    //  `.` makes the view that follows the PC follow it again with the PC in
    //  its middle, exactly as the Follow PC control does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (FollowPcCommandTests)
    {
    public:

        TEST_METHOD (TheDotBringsAMovedPaneBackToThePc)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Build();
            CursorRig::AssertMoved (rig.Type ("PAGEDOWN4K"), "PAGEDOWN4K", 0x12F6);

            snapshot = rig.Build();
            Assert::AreEqual (-1, CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"moved away, the PC is off the pane");

            CursorRig::AssertReply (rig.Type ("."), CommandStatus::Ok, "", "", { "The code pane follows the PC." }, ".");

            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kFollowTop,  snapshot.codeViews[0][0].address);
            Assert::AreEqual (CursorRig::kMiddle,     CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"the PC in the middle");
            Assert::AreEqual ((size_t) DebuggerViewState::kCodeLines, snapshot.codeViews[0].size());
        }



        //  The PC moved on screen, so the anchor the view had before it was
        //  moved no longer centers it. `.` centers it anyway, where clearing the
        //  pin alone went back to that stale anchor with the PC a row low.
        TEST_METHOD (TheDotCentersThePcWhereverItMovedOnScreen)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Build();
            rig.Step();
            Assert::AreEqual ((Word) 0x0302, rig.GetRegisters().pc, L"one step, still on the pane");

            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kMiddle + 1, CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"the pane held still and the marker moved");

            rig.Type ("V");
            rig.Build();
            rig.Type (".");

            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"the PC is brought to the middle");
            Assert::AreEqual ((Word) 0x02F7, snapshot.codeViews[0][0].address);
        }



        //  The Follow PC control goes through ShowPcIn; `.` lands the pane in
        //  the same place from the same state.
        TEST_METHOD (TheDotAndTheFollowPcControlAgree)
        {
            CursorRig  typed;
            CursorRig  clicked;



            for (CursorRig * rig : { &typed, &clicked })
            {
                rig->Build();
                rig->Step();
                rig->Build();
                rig->Type ("V");
                rig->Build();
            }

            typed.Type (".");
            clicked.view.ShowPcIn (0);

            Assert::IsTrue (CursorRig::GetAddresses (typed.Build().codeViews[0]) == CursorRig::GetAddresses (clicked.Build().codeViews[0]));
        }



        //  Leading blanks, Monitor's `/`, and Casso mode; the reply keeps the
        //  line as it was typed.
        TEST_METHOD (EveryFormOfTheDot)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", " ." },
                                               std::pair<const char *, const char *> { "MONITOR",  "/." },
                                               std::pair<const char *, const char *> { "MONITOR",  "  / ." },
                                               std::pair<const char *, const char *> { "CASSO",    "." } })
            {
                CursorRig             rig;
                DebuggerViewSnapshot  snapshot;



                rig.Type (std::string ("MODE ") + mode);
                rig.Build();
                rig.Type (std::string (mode) == "MONITOR" ? "/PAGEDOWN4K" : "PAGEDOWN4K");
                rig.Build();

                CursorRig::AssertReply (rig.Type (line), CommandStatus::Ok, "", "", { "The code pane follows the PC." }, line);

                snapshot = rig.Build();
                Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetCurrentRow (snapshot.codeViews[0]), CursorRig::Widen (line).c_str());
            }
        }



        //  A move queued ahead of `.` -- the wheel, or Show in disassembly --
        //  does not undo it at the next build.
        TEST_METHOD (TheDotWinsOverAMoveStillWaitingForABuild)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Build();
            rig.view.CenterCodeOn (0x0450);
            rig.view.ScrollCode   (4);
            rig.Type (".");

            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kFollowTop, snapshot.codeViews[0][0].address);
            Assert::AreEqual (CursorRig::kMiddle,    CursorRig::GetCurrentRow (snapshot.codeViews[0]));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LineCommandTests
    //
    //  V and ^ move the view that follows the PC one instruction, from the
    //  line on its top, which in a following view is ten lines above the PC.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LineCommandTests)
    {
    public:

        //  A following view shows the PC in its middle. V moves it one line,
        //  where starting from the PC jumped it half a pane.
        TEST_METHOD (TheFirstVFromAFollowingPaneMovesItOneLine)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before = rig.Build();
            DebuggerViewSnapshot  after;
            std::vector<Word>     shifted;



            Assert::AreEqual (CursorRig::kFollowTop, before.codeViews[0][0].address);
            Assert::AreEqual (CursorRig::kMiddle,    CursorRig::GetCurrentRow (before.codeViews[0]));

            CursorRig::AssertMoved (rig.Type ("V"), "V", before.codeViews[0][1].address);

            after   = rig.Build();
            shifted = CursorRig::GetAddresses (before.codeViews[0]);
            shifted.erase (shifted.begin());
            shifted.push_back (0x0318);

            Assert::IsTrue   (shifted == CursorRig::GetAddresses (after.codeViews[0]), L"every row moved up one");
            Assert::AreEqual (CursorRig::kMiddle - 1, CursorRig::GetCurrentRow (after.codeViews[0]), L"and the PC with them");
        }



        //  Each V moves by the length of the instruction on the top line.
        TEST_METHOD (VStepsByEachInstructionsLength)
        {
            static constexpr Word  kTops[] = { 0x0300, 0x0302, 0x0305, 0x0308, 0x030A, 0x030C, 0x030F,
                                               0x0312, 0x0314, 0x0315, 0x0318, 0x0319, 0x031B, 0x031C };
            CursorRig              rig;



            rig.Build();

            for (int i = 0; i < 9; i++)
            {
                rig.Type ("V");
            }

            for (Word top : kTops)
            {
                CursorRig::AssertMoved (rig.Type ("V"), "V", top);
                Assert::AreEqual (top, rig.GetTop(), std::format (L"the pane at ${:04X}", top).c_str());
            }
        }



        //  Any case, an argument ignored, Monitor's `/` with or without blanks,
        //  and Casso mode: each moves one line.
        TEST_METHOD (EveryFormOfV)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "v" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "V 5" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "  V" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/V" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/v" },
                                               std::pair<const char *, const char *> { "MONITOR",  "  /V" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/ V" },
                                               std::pair<const char *, const char *> { "CASSO",    "V" } })
            {
                CursorRig  rig;



                rig.Type (std::string ("MODE ") + mode);
                rig.Build();

                CursorRig::AssertMoved (rig.Type (line), line, 0x02F7);
                Assert::AreEqual ((Word) 0x02F7, rig.GetTop(), CursorRig::Widen (line).c_str());
            }
        }



        //  ^ takes the longest instruction that ends where the top line
        //  starts: a three-byte one over a one-byte one that also fits.
        TEST_METHOD (CaretTakesTheLongestInstructionThatEndsAtTheTop)
        {
            CursorRig  rig;



            rig.PlaceTop (0x0600);
            rig.Poke (0x05FD, { 0x4C, 0xEA, 0xEA });    // JMP $EAEA, and a NOP at $05FF

            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x05FD);
            Assert::AreEqual ((Word) 0x05FD, rig.GetTop());

            CursorRig::AssertMoved (rig.Type ("V"), "V", 0x0600);
            Assert::AreEqual ((Word) 0x0600, rig.GetTop(), L"V undoes it");
        }



        TEST_METHOD (CaretTakesATwoByteInstruction)
        {
            CursorRig  rig;



            rig.PlaceTop (0x0600);
            rig.Poke (0x05FD, { 0xEA, 0xA9, 0xEA });    // NOP, LDA #$EA

            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x05FE);
            Assert::AreEqual ((Word) 0x05FE, rig.GetTop());

            rig.Type ("V");
            Assert::AreEqual ((Word) 0x0600, rig.GetTop(), L"V undoes it");
        }



        TEST_METHOD (CaretTakesAOneByteInstruction)
        {
            CursorRig  rig;



            rig.PlaceTop (0x0600);

            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x05FF);
            Assert::AreEqual ((Word) 0x05FF, rig.GetTop());

            rig.Type ("V");
            Assert::AreEqual ((Word) 0x0600, rig.GetTop(), L"V undoes it");
        }



        //  No instruction ends at the top line: ^ steps back one byte, and V
        //  from there moves by that byte's own instruction, past where it was.
        TEST_METHOD (CaretStepsBackOneByteWhereNothingFits)
        {
            CursorRig  rig;



            rig.PlaceTop (0x0600);
            rig.Poke (0x05FD, { 0xEA, 0xEA, 0xA9 });    // NOP, NOP, and LDA # over the top line

            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x05FF);
            Assert::AreEqual ((Word) 0x05FF, rig.GetTop());

            CursorRig::AssertMoved (rig.Type ("V"), "V", 0x0601);
            Assert::AreEqual ((Word) 0x0601, rig.GetTop(), L"V is not ^ undone here");
        }



        //  The wheel walks forward from three bytes up and takes the first
        //  alignment that reaches the top line; ^ takes the longest fit. Over
        //  LDA #$A9 / NOP the two land a byte apart.
        TEST_METHOD (CaretAndTheWheelCanLandOnDifferentAlignments)
        {
            CursorRig  caret;
            CursorRig  wheel;



            for (CursorRig * rig : { &caret, &wheel })
            {
                rig->PlaceTop (0x0600);
                rig->Poke (0x05FD, { 0xA9, 0xA9, 0xEA });
            }

            caret.Type ("^");
            wheel.view.ScrollCode (-1);

            Assert::AreEqual ((Word) 0x05FE, caret.GetTop(), L"^: LDA #$EA at $05FE ends at the top line");
            Assert::AreEqual ((Word) 0x05FF, wheel.GetTop(), L"the wheel: LDA #$A9 at $05FD, then the NOP");
        }



        TEST_METHOD (EveryFormOfTheCaret)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "^" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/^" },
                                               std::pair<const char *, const char *> { "CASSO",    " ^" } })
            {
                CursorRig  rig;



                rig.Type (std::string ("MODE ") + mode);
                rig.Build();

                CursorRig::AssertMoved (rig.Type (line), line, 0x02F5);
                Assert::AreEqual ((Word) 0x02F5, rig.GetTop(), CursorRig::Widen (line).c_str());
            }
        }



        //  ^ from $0000 goes round to the top of memory. The //e's IRQ vector
        //  is $FA40, so $FFFF holds $FA, a one-byte instruction on the 6502,
        //  and ^ lands on it. The view there holds that one line, never one
        //  wrapped past $FFFF, and V goes round again.
        TEST_METHOD (CaretAndVWrapThroughTheAddressSpace)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Fill (0x0000, 0x0040, CursorRig::kNop);
            rig.SetPc (0x0000);
            Assert::AreEqual ((Word) 0x0000, rig.GetTop(), L"a PC this low sits on the top line");

            CursorRig::AssertMoved (rig.Type ("^"), "^", 0xFFFF);

            snapshot = rig.Build();
            Assert::AreEqual ((size_t) 1,     snapshot.codeViews[0].size(), L"one line, and none wrapped past $FFFF");
            Assert::AreEqual ((Word) 0xFFFF,  snapshot.codeViews[0][0].address);
            Assert::AreEqual (std::string ("FA"),  snapshot.codeViews[0][0].bytes);

            CursorRig::AssertMoved (rig.Type ("V"), "V", 0x0000);
            Assert::AreEqual ((Word) 0x0000, rig.GetTop());

            CursorRig::AssertMoved (rig.Type ("PAGEUP256"), "PAGEUP256", 0xFF00);
            Assert::AreEqual ((size_t) DebuggerViewState::kCodeLines, rig.Build().codeViews[0].size(), L"a page below the top fills the pane");
        }



        //  A move queued ahead of V -- the wheel, or Follow PC -- is dropped:
        //  the pane goes where the reply says.
        TEST_METHOD (VWinsOverAMoveStillWaitingForABuild)
        {
            CursorRig  wheel;
            CursorRig  follow;



            wheel.Build();
            wheel.view.ScrollCode (5);
            CursorRig::AssertMoved (wheel.Type ("V"), "V", 0x02F7);
            Assert::AreEqual ((Word) 0x02F7, wheel.GetTop(), L"not five lines more");

            follow.Build();
            follow.Type ("V");
            follow.Build();
            follow.view.ShowPcIn (0);
            CursorRig::AssertMoved (follow.Type ("V"), "V", 0x02F8);
            Assert::AreEqual ((Word) 0x02F8, follow.GetTop(), L"not re-centered");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PageCommandTests
    //
    //  PAGEUP and PAGEDN move as many instructions as the view has lines; the
    //  numbered forms a fixed distance through the whole address space.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PageCommandTests)
    {
    public:

        //  A page of the default twenty lines: PAGEDN puts the line after the
        //  last one shown on top, and PAGEUP brings the first back.
        TEST_METHOD (APageIsTheLinesThePaneShows)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before = rig.Build();
            DebuggerViewSnapshot  after;



            Assert::AreEqual ((Word) 0x0315, before.codeViews[0].back().address, L"the last of twenty lines");

            CursorRig::AssertMoved (rig.Type ("PAGEDN"), "PAGEDN", 0x0318);

            after = rig.Build();
            Assert::AreEqual ((Word) 0x0318, after.codeViews[0][0].address);

            CursorRig::AssertMoved (rig.Type ("PAGEUP"), "PAGEUP", CursorRig::kFollowTop);
            Assert::AreEqual (CursorRig::kFollowTop, rig.GetTop());
        }



        //  The window measures the pane and says how many lines it holds; a page
        //  is that many, not the default.
        TEST_METHOD (APageFollowsThePanesMeasuredHeight)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before;



            rig.view.SetCodeLines (7);
            before = rig.Build();

            Assert::AreEqual ((size_t) 7,      before.codeViews[0].size());
            Assert::AreEqual ((Word) 0x02FD,   before.codeViews[0][0].address);
            Assert::AreEqual ((Word) 0x0308,   before.codeViews[0][6].address);

            CursorRig::AssertMoved (rig.Type ("PAGEDN"), "PAGEDN", 0x030A);
            Assert::AreEqual ((Word) 0x030A, rig.GetTop());

            CursorRig::AssertMoved (rig.Type ("PAGEUP"), "PAGEUP", 0x02FD);
            Assert::AreEqual ((Word) 0x02FD, rig.GetTop(), L"seven instructions back through LDA #, STA and BEQ");
        }



        TEST_METHOD (EveryFormOfAPage)
        {
            for (const auto & [mode, line, top] : { std::tuple<const char *, const char *, Word> { "APPLEWIN", "pagedn",  0x0318 },
                                                    std::tuple<const char *, const char *, Word> { "MONITOR",  "/PAGEDN", 0x0318 },
                                                    std::tuple<const char *, const char *, Word> { "CASSO",    "PageUp",  0x02E2 },
                                                    std::tuple<const char *, const char *, Word> { "MONITOR",  "/pageup", 0x02E2 } })
            {
                CursorRig  rig;



                rig.Type (std::string ("MODE ") + mode);
                rig.Build();

                CursorRig::AssertMoved (rig.Type (line), line, top);
                Assert::AreEqual (top, rig.GetTop(), CursorRig::Widen (line).c_str());
            }
        }



        //  $100 and $1000 either way from the top line, round the top and the
        //  bottom of memory.
        TEST_METHOD (TheNumberedPagesMoveAFixedDistanceAndWrap)
        {
            static const std::pair<const char *, Word>  kMoves[] =
            {
                { "PAGEDOWN256", 0x03F6 },
                { "PAGEUP256",   0x02F6 },
                { "PAGEDOWN4K",  0x12F6 },
                { "PAGEUP4K",    0x02F6 },
                { "PAGEUP4K",    0xF2F6 },
                { "PAGEDOWN256", 0xF3F6 },
                { "PAGEDOWN256", 0xF4F6 },
                { "PAGEDOWN256", 0xF5F6 },
                { "PAGEDOWN256", 0xF6F6 },
                { "PAGEDOWN256", 0xF7F6 },
                { "PAGEDOWN256", 0xF8F6 },
                { "PAGEDOWN4K",  0x08F6 },
                { "pageup4k",    0xF8F6 },
                { "pagedown256", 0xF9F6 },
            };
            CursorRig  rig;



            rig.Build();

            for (const auto & [line, top] : kMoves)
            {
                CursorRig::AssertMoved (rig.Type (line), line, top);
                Assert::AreEqual (top, rig.GetTop(), CursorRig::Widen (line).c_str());
            }
        }



        //  The view is built from the address the command left: that many rows
        //  from it, no PC marked when the PC is not among them, and the first
        //  view's copy the same.
        TEST_METHOD (TheWindowShowsThePaneFromTheNewTop)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetCodeLines (12);
            Assert::AreEqual ((Word) 0x02FA, rig.GetTop(), L"twelve lines put the PC six down");

            rig.Type ("PAGEDOWN4K");
            snapshot = rig.Build();

            Assert::AreEqual ((size_t) 12,     snapshot.codeViews[0].size());
            Assert::AreEqual ((Word) 0x12FA,   snapshot.codeViews[0][0].address);
            Assert::AreEqual (-1,              CursorRig::GetCurrentRow (snapshot.codeViews[0]));
            Assert::IsTrue   (CursorRig::GetAddresses (snapshot.code) == CursorRig::GetAddresses (snapshot.codeViews[0]));
            Assert::AreEqual ((Word) 0x12FA,   *rig.view.GetCodeAddress (0));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReturnCommandTests
    //
    //  RET goes to where an RTS would return: one past the word on the
    //  stack, which JSR pushed as the address of its own last byte. A
    //  navigation, so the address is in the middle of the pane.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReturnCommandTests)
    {
    public:

        //  A JSR stepped for real, so the pushed word is the core's.
        TEST_METHOD (RetGoesPastTheJsrAStepPushed)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Fill (0x05C0, 0x0720, CursorRig::kNop);
            rig.Poke (0x0600, { 0x20, 0x00, 0x07 });    // JSR $0700
            rig.SetPc (0x0600);
            rig.Build();
            rig.Step();

            Assert::AreEqual ((Word) 0x0700, rig.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0xFD,   rig.GetRegisters().sp);
            Assert::AreEqual ((Byte) 0x06,   rig.Peek (0x01FF), L"the high byte of $0602");
            Assert::AreEqual ((Byte) 0x02,   rig.Peek (0x01FE), L"the low byte");

            CursorRig::AssertMoved (rig.Type ("RET"), "RET", 0x0603);

            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetRowOf (snapshot.codeViews[0], 0x0603), L"in the middle, never on an edge");
            Assert::AreEqual (-1,                 CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"the PC is in the subroutine");
        }



        TEST_METHOD (EveryFormOfRet)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "ret" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "RET 1234" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/RET" },
                                               std::pair<const char *, const char *> { "CASSO",    "RET" } })
            {
                CursorRig  rig;



                rig.SetPc (0x0300, 0xFD);
                rig.Poke (0x01FE, { 0x0F, 0x04 });
                rig.Type (std::string ("MODE ") + mode);

                CursorRig::AssertMoved (rig.Type (line), line, 0x0410);
                Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetRowOf (rig.Build().codeViews[0], 0x0410), CursorRig::Widen (line).c_str());
            }
        }



        //  The stack pointer at $FE and $FF reads its word round page 1, and a
        //  word of $FFFF returns to $0000.
        TEST_METHOD (RetReadsTheStackRoundPageOne)
        {
            CursorRig  high;
            CursorRig  top;



            high.SetPc (0x0300, 0xFF);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) high.Type ("PUSH 34").status);
            Assert::AreEqual ((Byte) 0xFE, high.GetRegisters().sp);
            high.Type ("MEB 100 12");
            CursorRig::AssertMoved (high.Type ("RET"), "RET", 0x1235);

            top.SetPc (0x0300, 0xFF);
            top.Type ("MEB 100 FF FF");
            CursorRig::AssertMoved (top.Type ("RET"), "RET", 0x0000);
        }



        //  A stack the target cannot read. A real machine's page 1 is always
        //  readable, so only a target that refuses the peek reaches this; the
        //  pane stays where it was.
        TEST_METHOD (AStackThatCannotBeReadIsAnError)
        {
            TestCpu                    cpu;
            UnreadableStack            target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;
            DebuggerViewState          view;
            std::vector<Word>          before;
            Reply                      reply;



            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();
            target.registers.sp   = 0xFD;
            handlers.Attach (session);

            before = WindowConsoleRig::GetAddresses (view.Build (session).codeViews[0]);
            reply  = view.ExecuteWindowLine (session, "RET", session.GetMode());

            WindowConsoleRig::AssertReply (reply, CommandStatus::Error, "no address", "The stack at $01FE cannot be read.",
                                           { "Error: no address", "       The stack at $01FE cannot be read." }, "RET");
            Assert::IsTrue (before == WindowConsoleRig::GetAddresses (view.Build (session).codeViews[0]), L"the pane did not move");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OperandCommandTests
    //
    //  -> goes to the address the instruction's operand refers to: the PC's
    //  instruction while the view follows it, and the top line's once it was
    //  moved. A navigation, so the address is in the middle of the pane.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OperandCommandTests)
    {
    public:

        TEST_METHOD (TheArrowFollowsEachKindOfOperand)
        {
            static const std::tuple<Word, Word, const wchar_t *>  kCases[] =
            {
                { 0x0302, 0x0400, L"STA $0400"   },
                { 0x0305, 0x0400, L"STA $0400,X" },
                { 0x0308, 0x030C, L"BEQ $030C"   },
                { 0x030C, 0xFDED, L"JSR $FDED"   },
                { 0x030F, 0x03F0, L"JMP ($03F0): the pointer" },
                { 0x0315, 0x0300, L"JMP $0300"   },
            };



            for (const auto & [pc, target, what] : kCases)
            {
                CursorRig             rig;
                DebuggerViewSnapshot  snapshot;



                rig.SetPc (pc);
                rig.Build();

                CursorRig::AssertMoved (rig.Type ("->"), "->", target);

                snapshot = rig.Build();
                Assert::IsTrue (CursorRig::GetRowOf (snapshot.codeViews[0], target) > 0, what);

                if (target < 0xC000)
                {
                    Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetRowOf (snapshot.codeViews[0], target), what);
                }
            }
        }



        //  Immediate, zero-page and implied operands, and the accumulator:
        //  nothing to go to, and the pane stays.
        TEST_METHOD (AnOperandWithNoAddressIsAnError)
        {
            static const std::pair<Word, const char *>  kCases[] =
            {
                { 0x0300, "LDA #$41"    },
                { 0x030A, "LDA $12"     },
                { 0x0312, "LDA ($12),Y" },
                { 0x0314, "NOP"         },
                { 0x0318, "ASL A"       },
                { 0x0319, "LDA $12,X"   },
            };



            for (const auto & [pc, what] : kCases)
            {
                CursorRig          rig;
                std::vector<Word>  before;
                std::string        detail = std::format ("The instruction at ${:04X} has no address operand.", pc);



                rig.SetPc (pc);
                before = CursorRig::GetAddresses (rig.Build().codeViews[0]);

                CursorRig::AssertReply (rig.Type ("->"), CommandStatus::Error, "no address", detail,
                                        { "Error: no address", "       " + detail }, what);
                Assert::IsTrue (before == CursorRig::GetAddresses (rig.Build().codeViews[0]), CursorRig::Widen (what).c_str());
            }
        }



        //  A view moved off the PC reads its top line, not the PC's.
        TEST_METHOD (AMovedPaneReadsItsTopLine)
        {
            CursorRig  rig;



            rig.Build();
            rig.Type ("V");

            CursorRig::AssertReply (rig.Type ("->"), CommandStatus::Error, "no address", "The instruction at $02F7 has no address operand.",
                                    { "Error: no address", "       The instruction at $02F7 has no address operand." }, "a NOP on top");

            for (int i = 0; i < 10; i++)
            {
                rig.Type ("V");
            }

            Assert::AreEqual ((Word) 0x0302, rig.GetTop(), L"STA $0400 on top, the PC's LDA # above it");
            CursorRig::AssertMoved (rig.Type ("->"), "->", 0x0400);
        }



        //  A 65C02 bit branch: its operand starts with the zero-page byte, but
        //  the branch goes where the disassembler says.
        TEST_METHOD (TheArrowFollowsA65C02BitBranch)
        {
            CursorRig  rig ("Apple2eEnhanced");



            rig.Poke (0x0300, { 0x0F, 0x12, 0x05,       // BBR0 $12,$0308
                                0xEA, 0xEA, 0xEA, 0xEA, 0xEA });
            rig.Build();

            CursorRig::AssertMoved (rig.Type ("->"), "->", 0x0308);
            Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetRowOf (rig.Build().codeViews[0], 0x0308));

            rig.Poke (0x0310, { 0x7C, 0x34, 0x12 });    // JMP ($1234,X)
            rig.SetPc (0x0310);
            rig.Build();
            CursorRig::AssertMoved (rig.Type ("->"), "->", 0x1234);
        }



        //  A table of words is a table of addresses: -> takes the first. A
        //  line of bytes has none, and text that happens to read "$1234" is
        //  text.
        TEST_METHOD (TheArrowOnDataLines)
        {
            CursorRig  words;
            CursorRig  text;
            CursorRig  bytes;



            words.Poke (0x1100, { 0x00, 0x20, 0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A });
            Assert::AreEqual ((int) CommandStatus::Ok, (int) words.Type ("DW 1100:1107").status);
            words.SetPc (0x1100);
            CursorRig::AssertMoved (words.Type ("->"), "->", 0x2000);

            text.Poke (0x1200, { 0xA4, 0xB1, 0xB2, 0xB3, 0xB4 });     // "$1234"
            Assert::AreEqual ((int) CommandStatus::Ok, (int) text.Type ("ASC 1200:1204").status);
            text.SetPc (0x1200);
            CursorRig::AssertReply (text.Type ("->"), CommandStatus::Error, "no address", "The instruction at $1200 has no address operand.",
                                    { "Error: no address", "       The instruction at $1200 has no address operand." }, "ASC");

            bytes.Poke (0x1300, { 0x00, 0x20, 0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A });
            Assert::AreEqual ((int) CommandStatus::Ok, (int) bytes.Type ("DB 1300:1307").status);
            bytes.SetPc (0x1300);
            Assert::AreEqual ((int) CommandStatus::Error, (int) bytes.Type ("->").status, L"DB");
        }



        TEST_METHOD (EveryFormOfTheArrow)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "MONITOR",  "/->" },
                                               std::pair<const char *, const char *> { "MONITOR",  " / ->" },
                                               std::pair<const char *, const char *> { "CASSO",    "->" } })
            {
                CursorRig  rig;



                rig.SetPc (0x0302);
                rig.Type (std::string ("MODE ") + mode);

                CursorRig::AssertMoved (rig.Type (line), line, 0x0400);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DataBlockCursorTests
    //
    //  In a data block a line is the block's, eight bytes of DB laid from its
    //  first byte, and the cursor commands move by those lines.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DataBlockCursorTests)
    {
    public:

        //  A block of DB at $1000 to $103F, with the PC on its first byte and
        //  NOPs above it.
        static void MakeBlock (CursorRig & rig)
        {
            rig.Fill (0x0FC0, 0x1000, CursorRig::kNop);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("DB 1000:103F").status);
            rig.SetPc (0x1000);
        }



        //  The pane holds as many lines as it has room for, data lines being
        //  longer than the three bytes an instruction takes at most.
        TEST_METHOD (APaneOverADataBlockFillsEveryLine)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            MakeBlock (rig);
            snapshot = rig.Build();

            Assert::AreEqual ((size_t) DebuggerViewState::kCodeLines, snapshot.codeViews[0].size());

            for (int line = 0; line < 8; line++)
            {
                Assert::AreEqual ((Word) (0x1000 + line * 8), snapshot.codeViews[0][(size_t) (CursorRig::kMiddle + line)].address);
            }
        }



        TEST_METHOD (VAndCaretMoveByTheBlocksLines)
        {
            CursorRig  rig;



            MakeBlock (rig);
            rig.Build();

            for (int i = 0; i < 10; i++)
            {
                rig.Type ("V");
            }

            Assert::AreEqual ((Word) 0x1000, rig.GetTop());
            CursorRig::AssertMoved (rig.Type ("V"), "V", 0x1008);
            CursorRig::AssertMoved (rig.Type ("V"), "V", 0x1010);
            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x1008);
            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x1000);
            CursorRig::AssertMoved (rig.Type ("^"), "^", 0x0FFF);
        }



        //  From the first line past the block, ^ goes to the block's last line
        //  and PAGEUP back five of them.
        TEST_METHOD (UpFromPastTheBlockLandsOnItsLines)
        {
            CursorRig  rig;



            MakeBlock (rig);
            rig.view.SetCodeLines (5);
            rig.Build();

            for (int i = 0; i < DebuggerViewState::kCodeLines && rig.GetTop() < 0x1040; i++)
            {
                rig.Type ("V");
            }

            Assert::AreEqual ((Word) 0x1040, rig.GetTop());
            CursorRig::AssertMoved (rig.Type ("^"),      "^",      0x1038);
            CursorRig::AssertMoved (rig.Type ("PAGEUP"), "PAGEUP", 0x1010);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CursorViewTests
    //
    //  Which view the cursor commands move: the one following the PC.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CursorViewTests)
    {
    public:

        //  CODE2 brings the second view forward without it following the PC,
        //  so V still moves the first.
        TEST_METHOD (CodeTwoDoesNotTakeTheCursor)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before;
            DebuggerViewSnapshot  after;



            rig.Type ("CODE2");
            before = rig.Build();
            rig.Type ("V");
            after  = rig.Build();

            Assert::AreEqual (0, after.followView);
            Assert::AreEqual (before.codeViews[0][1].address, after.codeViews[0][0].address, L"the first view moved");
            Assert::IsTrue   (CursorRig::GetAddresses (before.codeViews[1]) == CursorRig::GetAddresses (after.codeViews[1]), L"the second did not");
        }



        //  The view given the PC -- the window's Follow the PC on its tab --
        //  follows from where it stands while the PC is on its lines. It used
        //  to keep an anchor it had never set, and went to $0000.
        TEST_METHOD (AViewTakingThePcFollowsFromWhereItStands)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before;
            DebuggerViewSnapshot  after;



            rig.Type ("CODE2");
            before = rig.Build();
            Assert::IsTrue (CursorRig::GetRowOf (before.codeViews[1], 0x0300) >= 0, L"the second view shows the PC");

            rig.view.SetFollowView (1);
            after = rig.Build();

            Assert::IsTrue (CursorRig::GetAddresses (before.codeViews[1]) == CursorRig::GetAddresses (after.codeViews[1]), L"the second view held still");
            Assert::IsTrue (CursorRig::GetAddresses (before.codeViews[0]) == CursorRig::GetAddresses (after.codeViews[0]), L"and so did the first");
        }



        //  With the second view following the PC, every cursor command moves it
        //  and leaves the first.
        TEST_METHOD (TheCursorMovesTheViewThatFollowsThePc)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  before;
            DebuggerViewSnapshot  after;



            rig.Type ("CODE2");
            rig.Build();
            rig.view.SetFollowView (1);
            before = rig.Build();

            Assert::AreEqual (1, before.followView);
            CursorRig::AssertMoved (rig.Type ("V"), "V", before.codeViews[1][1].address);

            after = rig.Build();
            Assert::AreEqual (before.codeViews[1][1].address, after.codeViews[1][0].address, L"the second view moved");
            Assert::IsTrue   (CursorRig::GetAddresses (before.codeViews[0]) == CursorRig::GetAddresses (after.codeViews[0]), L"the first did not");
            Assert::IsTrue   (CursorRig::GetAddresses (before.code)         == CursorRig::GetAddresses (after.code),         L"nor its copy");

            rig.Type ("PAGEDOWN256");
            Assert::AreEqual ((Word) (after.codeViews[1][0].address + 0x0100), rig.Build().codeViews[1][0].address);

            rig.Type (".");
            after = rig.Build();
            Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetCurrentRow (after.codeViews[1]), L". brings the PC back in the second view");
            Assert::IsTrue   (CursorRig::GetAddresses (before.codeViews[0]) == CursorRig::GetAddresses (after.codeViews[0]));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CursorAcrossRunsTests
    //
    //  A pane a cursor command moved holds while the PC stays where it was.
    //  Once the PC moves, by a step or a run, the view following it follows
    //  again from where it stands.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CursorAcrossRunsTests)
    {
    public:

        //  NOPs from $0300 to $04FF and a JMP back to $0301, so a run never
        //  comes back to $0300, where it started.
        static void MakeLoop (CursorRig & rig)
        {
            rig.Fill (0x0300, 0x0500, CursorRig::kNop);
            rig.Poke (0x0500, { 0x4C, 0x01, 0x03 });
            rig.SetPc (0x0300);
        }



        TEST_METHOD (APaneMovedByVHoldsWhileAStepStaysOnIt)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            rig.Build();
            rig.Type ("V");
            Assert::AreEqual ((Word) 0x02F7, rig.GetTop());

            rig.Step();
            snapshot = rig.Build();
            Assert::AreEqual ((Word) 0x02F7, snapshot.codeViews[0][0].address, L"the PC moved to a line the pane shows");
            Assert::AreEqual (CursorRig::GetRowOf (snapshot.codeViews[0], 0x0302), CursorRig::GetCurrentRow (snapshot.codeViews[0]));

            //  STA, STA,X, BEQ not taken and LDA $12, all on the pane's lines.
            for (int i = 0; i < 4; i++)
            {
                rig.Step();
            }

            snapshot = rig.Build();
            Assert::AreEqual ((Word) 0x030C, rig.GetRegisters().pc);
            Assert::AreEqual ((Word) 0x02F7, snapshot.codeViews[0][0].address, L"still where V put it");

            //  Then a jump off them.
            rig.Fill (0x1FC0, 0x2040, CursorRig::kNop);
            rig.Poke (0x030C, { 0x4C, 0x00, 0x20 });    // JMP $2000
            rig.Step();

            Assert::AreEqual ((Word) 0x2000, rig.GetRegisters().pc);
            snapshot = rig.Build();
            Assert::AreEqual (CursorRig::kMiddle, CursorRig::GetCurrentRow (snapshot.codeViews[0]), L"off its lines, the pane follows the PC");
        }



        //  G runs the machine through the emulator's frames; a pause stops it
        //  away from where it started, and the pane moved by PAGEDOWN256 lets
        //  go and shows the PC.
        TEST_METHOD (APaneMovedByAPageFollowsAgainAfterARun)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  snapshot;



            MakeLoop (rig);
            rig.Build();
            rig.Type ("PAGEDOWN256");
            Assert::AreEqual ((Word) 0x03F6, rig.GetTop());

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("G").status);
            rig.RunFrames (3);
            rig.controller.RequestPause();
            rig.RunFrames (1);
            Assert::IsFalse (rig.controller.IsRunInProgress(), L"the pause ended the run");

            snapshot = rig.Build();
            Assert::AreNotEqual ((Word) 0x0300, rig.GetRegisters().pc);
            Assert::AreEqual   (CursorRig::GetRowOf (snapshot.codeViews[0], rig.GetRegisters().pc), CursorRig::GetCurrentRow (snapshot.codeViews[0]));
            Assert::IsTrue     (CursorRig::GetCurrentRow (snapshot.codeViews[0]) >= 0, L"the PC is on the pane");
        }



        //  During a run the window builds every frame. V moves the pane for the
        //  frame it was typed in; the next frame the PC has moved, and the view
        //  follows it again.
        TEST_METHOD (APaneMovedDuringARunFollowsAtTheNextFrame)
        {
            CursorRig             rig;
            DebuggerViewSnapshot  running;
            DebuggerViewSnapshot  moved;
            DebuggerViewSnapshot  next;



            MakeLoop (rig);
            rig.Type ("G");
            rig.RunFrames (1);

            running = rig.Build (false);
            Assert::IsTrue (CursorRig::GetCurrentRow (running.codeViews[0]) >= 0, L"following the running PC");

            CursorRig::AssertMoved (rig.Type ("V"), "V", running.codeViews[0][1].address);
            moved = rig.Build (false);
            Assert::AreEqual (running.codeViews[0][1].address, moved.codeViews[0][0].address, L"the frame it was typed in");

            rig.RunFrames (1);
            next = rig.Build (false);
            Assert::AreNotEqual (moved.pc, next.pc);
            Assert::IsTrue (CursorRig::GetCurrentRow (next.codeViews[0]) >= 0, L"the next frame shows the PC again");
            Assert::IsTrue (rig.controller.IsRunInProgress(), L"and V never stopped the run");

            rig.controller.RequestPause();
            rig.RunFrames (1);
        }
    };
}
