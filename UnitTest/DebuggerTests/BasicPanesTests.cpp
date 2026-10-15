#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/KeystrokeInjector.h"
#include "EmuTests/MachineIdle.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/BasicPanes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicPanesTests
//
//  The BASIC program, variables and stack panes' rows, from lines and
//  values made here and from a program typed into an enhanced //e and
//  stopped at a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BasicPanesTests)
    {
    public:

        static constexpr Word  kBodyOffset = 4;

        //  A line at address whose pieces are as ApplesoftProgram reads them:
        //  a keyword with a space either side, any other byte alone.
        static BasicLine MakeLine (Word address, Word number, std::initializer_list<const char *> pieces)
        {
            BasicLine  line;



            line.address = address;
            line.number  = number;

            for (const char * piece : pieces)
            {
                line.pieces.push_back (piece);
                line.text += piece;
            }

            return line;
        }

        static DebuggerViewSnapshot::BasicState MakeState (std::vector<BasicLine> lines, std::optional<Word> current, Word textPointer)
        {
            DebuggerViewSnapshot::BasicState  state;



            state.isApplesoft = true;
            state.hasProgram  = true;
            state.lines       = std::move (lines);
            state.line        = current;
            state.textPointer = textPointer;
            return state;
        }

        static DebuggerViewSnapshot::BreakpointLine MakeLineBreakpoint (int id, Word line, bool enabled)
        {
            DebuggerViewSnapshot::BreakpointLine  bp;



            bp.id           = id;
            bp.enabled      = enabled;
            bp.info.id      = id;
            bp.info.kind    = BreakpointKind::BasicLine;
            bp.info.address = line;
            bp.info.enabled = enabled;
            return bp;
        }


        //  A statement runs to its colon, which is part of it; a colon in
        //  quotes or after REM ends nothing.
        TEST_METHOD (StatementsEndAtColonsOutsideQuotesAndRemarks)
        {
            BasicLine  line = MakeLine (0x0801, 10, { "A", "=", "1", ":", " PRINT ", "\"", "X", ":", "Y", "\"", ":", " REM ", "A", ":", "B" });



            Assert::IsTrue (BasicProgramPane::GetStatement (line, 0)  == std::pair<int, int> (0, 4));
            Assert::IsTrue (BasicProgramPane::GetStatement (line, 3)  == std::pair<int, int> (0, 4));
            Assert::IsTrue (BasicProgramPane::GetStatement (line, 4)  == std::pair<int, int> (4, 11));
            Assert::IsTrue (BasicProgramPane::GetStatement (line, 7)  == std::pair<int, int> (4, 11), L"the colon in quotes");
            Assert::IsTrue (BasicProgramPane::GetStatement (line, 11) == std::pair<int, int> (11, 15), L"a REM runs to the end");
            Assert::IsTrue (BasicProgramPane::GetStatement (line, 14) == std::pair<int, int> (11, 15));
        }

        //  The line being run has the PC's row color; its statement is in
        //  full color and the rest muted, with the PC's arrow on the row the
        //  statement starts on; a line breakpoint's dot is on its first row.
        TEST_METHOD (TheStatementBeingRunIsMarked)
        {
            BasicLine                                  first  = MakeLine (0x0801, 10, { "A", "=", "1" });
            BasicLine                                  second = MakeLine (0x0809, 20, { "B", "=", "2", ":", "C", "=", "3" });
            BasicProgramPane::Style                       style;
            std::vector<BasicProgramPane::Row>            rows;
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            DebuggerViewSnapshot::BasicState              state;



            style.pcIcon          = std::make_shared<DxuiIconImage>();
            style.enabledIcon     = std::make_shared<DxuiIconImage>();
            style.pcOnEnabledIcon = std::make_shared<DxuiIconImage>();
            style.pcRowArgb       = 0xFF112233;

            //  TXTPTR + 1 on C, the second statement's first piece.
            state = MakeState ({ first, second }, 20, (Word) (second.address + kBodyOffset + 4));
            rows  = BasicProgramPane::GetRows (state.lines);
            cells = BasicProgramPane::GetCells (state, rows, { MakeLineBreakpoint (3, 20, true) }, style);

            Assert::AreEqual ((size_t) 2, cells.size());
            Assert::AreEqual (std::wstring (L" 10 A=1"),     cells[0][1].text);
            Assert::AreEqual (std::wstring (L" 20 B=2:C=3"), cells[1][1].text);
            Assert::AreEqual (0u, cells[0][1].background, L"line 10 is not being run");
            Assert::IsTrue   (cells[0][0].icon == nullptr);
            Assert::AreEqual (style.pcRowArgb, cells[1][1].background);
            Assert::IsTrue   (cells[1][0].icon == style.pcOnEnabledIcon, L"the arrow over the breakpoint's dot");
            Assert::AreEqual ((size_t) 4, cells[1][1].dimRanges.size(), L"B, =, 2 and the colon muted");
            Assert::IsTrue   (cells[1][1].dimRanges.front() == std::pair<int, int> (4, 5));
            Assert::IsTrue   (cells[1][1].dimRanges.back()  == std::pair<int, int> (7, 8));
        }

        //  A line LIST continues on a second row has its pieces' places,
        //  and the arrow goes to the row the statement starts on.
        TEST_METHOD (TheArrowIsOnTheRowTheStatementStartsOn)
        {
            std::vector<const char *>                     pieces;
            BasicLine                                     line;
            BasicProgramPane::Style                       style;
            std::vector<BasicProgramPane::Row>            rows;
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            int                                           wrapped = 0;



            line.address = 0x0801;
            line.number  = 100;

            //  "A=1:" repeated: each statement four pieces, the row wrapping
            //  once it is 33 columns long.
            for (int i = 0; i < 12; i++)
            {
                for (const char * piece : { "A", "=", "1", ":" })
                {
                    line.pieces.push_back (piece);
                }
            }

            rows = BasicProgramPane::GetRows ({ line });

            Assert::AreEqual ((size_t) 2, rows.size(), L"one continuation row");
            Assert::AreEqual (std::wstring (5, L' '), rows[1].text.substr (0, 5));
            Assert::IsFalse  (rows[1].isFirst);

            wrapped      = rows[1].pieces.front().first;
            style.pcIcon = std::make_shared<DxuiIconImage>();
            cells        = BasicProgramPane::GetCells (MakeState ({ line }, 100, (Word) (line.address + kBodyOffset + 4 * ((wrapped + 3) / 4))), rows, {}, style);

            Assert::IsTrue (cells[0][0].icon == nullptr);
            Assert::IsTrue (cells[1][0].icon == style.pcIcon, L"the statement starts on the second row");
        }

        //  The rows are LIST's, row for row, from a program typed into the
        //  machine; stopped at line 30's breakpoint, the snapshot has line
        //  30 and the pointer on its first statement.
        TEST_METHOD (TheRowsAreWhatListPrints)
        {
            MachineHandlerRig<BasicHandlers>    rig { "Apple2eEnhanced", TestMachine::Slots::Empty };
            BreakpointHandlers                  breakpoints;
            std::vector<BasicLine>              lines;
            std::vector<std::string>            listed;
            std::vector<BasicProgramPane::Row>  rows;
            DebuggerViewSnapshot::BasicState    state;



            rig.session.AddHandler (&breakpoints);
            MachineIdle::RunUntilIdle (rig.machine, 20'000'000);

            for (const char * text : { "10 DIM X(2): X(1) = 5: S$ = \"A:B\"", "20 GOSUB 100: REM CALL IT", "30 FOR I = 1 TO 2: NEXT I: END",
                                       "100 PRINT \"A VERY LONG STRING THAT WRAPS PAST THE COLUMN\": RETURN" })
            {
                GuestSession::TypeAndCollect (rig.machine, text);
            }

            Assert::AreEqual ((size_t) 3, KeystrokeInjector::InjectString (rig.machine, "RUN"));
            rig.RunOk ("BUDGET 5000000");
            rig.RunOk ("BPBASIC 30");
            rig.machine.GetRefs().keyboard->PressKey (KeystrokeInjector::kAppleReturn);
            rig.RunOk ("G");

            state = DebuggerViewState::ReadBasic (rig.session, true);

            Assert::IsTrue   (state.isApplesoft && state.hasProgram);
            Assert::AreEqual ((Word) 30, state.line.value_or (0));
            Assert::AreEqual ((Word) (state.lines[2].address + kBodyOffset), state.textPointer, L"on line 30's first piece");
            Assert::IsTrue   (state.hasValues);
            Assert::IsTrue   (state.frames.empty(), L"the GOSUB has returned; the FOR has not run");

            rows   = BasicProgramPane::GetRows (state.lines);
            listed = ApplesoftProgram::GetListRows (state.lines);

            Assert::AreEqual (listed.size(), rows.size());

            for (size_t i = 0; i < rows.size(); i++)
            {
                Assert::AreEqual (TextEncoding::NarrowToWide (listed[i]), rows[i].text);
            }

            Assert::IsTrue (DebuggerViewState::ReadBasic (rig.session, false).variables.empty(), L"no values while running");

            //  Run to END and the prompt: CURLIN has line 30 for CONT, and no
            //  line is being run.
            rig.RunOk ("BPC *");
            rig.RunOk ("G");
            Assert::IsTrue (rig.LastStop().reason == StopReason::Budget);
            Assert::IsFalse (DebuggerViewState::ReadBasic (rig.session, true).line.has_value(), L"nothing marked at the prompt");
        }

        TEST_METHOD (VariablesAndElementsAreListed)
        {
            DebuggerViewSnapshot::BasicState               state = MakeState ({}, std::nullopt, 0);
            BasicArray                                     array;
            std::vector<std::vector<DxuiListView::Cell>>   cells;



            state.hasValues = true;
            state.variables = { { "A", BasicType::Real, 0, "1.5" }, { "S$", BasicType::String, 0, "HI" } };
            array.name      = "X";
            array.sizes     = { 2, 3 };
            array.values    = { "0", "1", "2", "3", "4", "5" };
            state.arrays    = { array };

            cells = BasicVariablesPane::GetCells (state);

            Assert::AreEqual ((size_t) 8, cells.size());
            Assert::AreEqual (std::wstring (L"1.5"),      cells[0][1].text);
            Assert::AreEqual (std::wstring (L"\"HI\""),   cells[1][1].text);
            Assert::AreEqual (std::wstring (L"X(0,0)"),   cells[2][0].text);
            Assert::AreEqual (std::wstring (L"X(1,0)"),   cells[3][0].text, L"the first subscript varies fastest");
            Assert::AreEqual (std::wstring (L"X(1,2)"),   cells[7][0].text);

            state.hasValues = false;
            cells           = BasicVariablesPane::GetCells (state);
            Assert::AreEqual ((size_t) 1, cells.size());
            Assert::IsTrue   (cells[0][0].spansRow && cells[0][0].dim, L"one note while running");
        }

        TEST_METHOD (FramesAreListedNewestFirst)
        {
            DebuggerViewSnapshot::BasicState               state = MakeState ({}, std::nullopt, 0);
            std::vector<std::vector<DxuiListView::Cell>>   cells;



            state.hasValues = true;
            state.frames    = { { BasicFrameKind::Gosub, 0x01F0, 20 }, { BasicFrameKind::For, 0x01F7, 10, "I", "10", "2" } };
            cells           = BasicStackPane::GetCells (state);

            Assert::AreEqual (std::wstring (L"GOSUB"),             cells[0][0].text);
            Assert::AreEqual (std::wstring (L"20"),                cells[0][1].text);
            Assert::AreEqual (std::wstring (L"FOR I TO 10 STEP 2"), cells[1][0].text);

            state.frames.clear();
            Assert::AreEqual (std::wstring (L"No GOSUB or FOR is active."), BasicStackPane::GetCells (state)[0][0].text);

            state.isApplesoft = false;
            Assert::AreEqual (std::wstring (L"Applesoft is not in memory."), BasicStackPane::GetCells (state)[0][0].text);
        }

        //  The panes opened are written to the open-views text and read back;
        //  the default layout has each, as a tab of its neighbor.
        TEST_METHOD (TheOpenedPanesAreKept)
        {
            std::set<std::wstring>  opened = { DebuggerLayout::kBasicProgram, DebuggerLayout::kBasicStack, L"heatmap" };
            DxuiPaneLayout          layout = DebuggerLayout::MakeDefault();



            Assert::AreEqual (std::string (" open=basicprogram open=basicstack"), DebuggerLayout::FormatBasicOpened (opened));
            Assert::IsTrue   (DebuggerLayout::ParseBasicOpened ("code2=0300 open=basicstack bpcols=3 open=basicprogram") ==
                              std::set<std::wstring> ({ DebuggerLayout::kBasicProgram, DebuggerLayout::kBasicStack }));

            for (const wchar_t * pane : { DebuggerLayout::kBasicProgram, DebuggerLayout::kBasicVariables, DebuggerLayout::kBasicStack })
            {
                Assert::IsTrue (layout.Contains (pane), pane);
            }
        }
    };
}
