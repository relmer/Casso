#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/KeystrokeInjector.h"
#include "EmuTests/MachineIdle.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/CallStackPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicPanesTests
//
//  The BASIC program pane's rows, the BASIC variables in the Watches pane
//  and the BASIC frames in the Call stack pane, from lines and values made
//  here and from a program typed into an enhanced //e and stopped at a
//  breakpoint.
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

        static DebuggerViewSnapshot::BasicState MakeValues()
        {
            DebuggerViewSnapshot::BasicState  state = MakeState ({}, 10, 0);
            BasicArray                        array;



            state.hasValues = true;
            state.variables = { { "A", BasicType::Real, 0, "1.5" }, { "S$", BasicType::String, 0, "HI" }, { "HI", BasicType::Real, 0, "7" } };
            array.name      = "X";
            array.sizes     = { 2, 3 };
            array.values    = { "0", "1", "2", "3", "4", "5" };
            state.arrays    = { array };
            return state;
        }

        //  Every variable, then every element, the first subscript varying
        //  fastest; a string in quotes.
        TEST_METHOD (EveryVariableAndElementIsListed)
        {
            std::vector<BasicWatches::Item>  items = BasicWatches::GetAllItems (MakeValues());



            Assert::AreEqual ((size_t) 9, items.size());
            Assert::AreEqual (std::string ("1.5"),    items[0].value);
            Assert::AreEqual (std::string ("\"HI\""), items[1].value);
            Assert::AreEqual (std::string ("X(0,0)"), items[3].name);
            Assert::AreEqual (std::string ("X(1,0)"), items[4].name, L"the first subscript varies fastest");
            Assert::AreEqual (std::string ("X(1,2)"), items[8].name);
        }

        //  A variable as Applesoft writes it, matched on its first two
        //  characters; an unset one reads 0 or "", and an element past the
        //  array's size is none.
        TEST_METHOD (AWatchReadsTheVariableApplesoftReads)
        {
            DebuggerViewSnapshot::BasicState  state = MakeValues();



            Assert::AreEqual (std::string ("7"),      BasicWatches::TryGetValue (state, "HIGH").value_or ("?"), L"HIGH is HI");
            Assert::AreEqual (std::string ("\"HI\""), BasicWatches::TryGetValue (state, "s$").value_or ("?"));
            Assert::AreEqual (std::string ("0"),      BasicWatches::TryGetValue (state, "N%").value_or ("?"));
            Assert::AreEqual (std::string ("\"\""),   BasicWatches::TryGetValue (state, "T$").value_or ("?"));
            Assert::AreEqual (std::string ("5"),      BasicWatches::TryGetValue (state, "X(1,2)").value_or ("?"));
            Assert::IsFalse  (BasicWatches::TryGetValue (state, "X(2,0)").has_value(), L"past the first subscript's size");
            Assert::IsFalse  (BasicWatches::TryGetValue (state, "3A").has_value());
        }

        //  $, % or subscripts always make a BASIC variable; a plain name does
        //  only when the stopped program has it.
        TEST_METHOD (APlainNameIsBasicOnlyWhenTheProgramHasIt)
        {
            std::optional<DebuggerViewSnapshot::BasicState>  state = MakeValues();



            Assert::IsTrue  (BasicWatches::IsBasicName (state, "N%"));
            Assert::IsTrue  (BasicWatches::IsBasicName (state, "X(1,2)"));
            Assert::IsTrue  (BasicWatches::IsBasicName (state, "A"));
            Assert::IsFalse (BasicWatches::IsBasicName (state, "COUT"), L"the program has no CO");
            Assert::IsFalse (BasicWatches::IsBasicName (state, "300"));

            state->line.reset();
            Assert::IsFalse (BasicWatches::IsBasicName (state, "A"), L"not stopped in the program");
            Assert::IsTrue  (BasicWatches::IsBasicName (state, "N%"));
        }

        //  The statement being run's simple variables, each once; a keyword,
        //  text in quotes and an array are not listed.
        TEST_METHOD (TheStatementsVariablesAreTheAutomaticWatches)
        {
            BasicLine                         line  = MakeLine (0x0801, 10, { "A", "=", "H", "I", "G", "H", "+", "A", ":", " PRINT ", "S", "$", ";", "\"", "B", "\"", ";", "X", "(", "1", ")" });
            DebuggerViewSnapshot::BasicState  state = MakeValues();
            std::vector<BasicWatches::Item>   items;



            state.lines       = { line };
            state.textPointer = (Word) (line.address + kBodyOffset);
            items             = BasicWatches::GetStatementItems (state);

            Assert::AreEqual ((size_t) 2, items.size(), L"A and HIGH");
            Assert::AreEqual (std::string ("HIGH"), items[1].name);
            Assert::AreEqual (std::string ("7"),    items[1].value);

            state.textPointer = (Word) (line.address + kBodyOffset + 9);
            items             = BasicWatches::GetStatementItems (state);

            Assert::AreEqual ((size_t) 1, items.size(), L"S$ only: B is in quotes and X is an array");
            Assert::AreEqual (std::string ("S$"), items[0].name);
        }

        //  The characters past the second are dimmed, up to the type's sign.
        TEST_METHOD (TheCharactersPastTheSecondAreDimmed)
        {
            Assert::IsTrue (BasicWatches::MakeNameCell ("HIGH$").dimRanges == std::vector<std::pair<int, int>> { { 2, 4 } });
            Assert::IsTrue (BasicWatches::MakeNameCell ("SCORE(3)").dimRanges == std::vector<std::pair<int, int>> { { 2, 5 } });
            Assert::IsTrue (BasicWatches::MakeNameCell ("HI").dimRanges.empty());
        }

        //  The GOSUB and FOR frames come first in the call stack, each on its
        //  line, and activating one goes to the line.
        TEST_METHOD (BasicFramesAreCallStackRows)
        {
            std::vector<BasicFrame>          frames = { { BasicFrameKind::Gosub, 0x01F0, 20 }, { BasicFrameKind::For, 0x01F7, 10, "I", "10", "2" } };
            std::vector<CallStackPane::Row>  rows   = CallStackPane::GetBasicRows (frames);



            Assert::AreEqual ((size_t) 2, rows.size());
            Assert::AreEqual (std::wstring (L"Line 20"),            rows[0].site);
            Assert::AreEqual (std::wstring (L"GOSUB"),              rows[0].routine);
            Assert::AreEqual (std::wstring (L"FOR I TO 10 STEP 2"), rows[1].routine);
            Assert::AreEqual ((Word) 10, rows[1].basicLine.value_or (0));
        }

        //  The program pane opened is written to the open-views text and read
        //  back, and the default layout has it as a tab of the disassembly;
        //  an old text's other BASIC panes are passed over. The BASIC watches
        //  are written and read back the same way.
        TEST_METHOD (TheOpenedPaneAndTheWatchesAreKept)
        {
            std::set<std::wstring>  opened = { DebuggerLayout::kBasicProgram, L"heatmap" };
            DxuiPaneLayout          layout = DebuggerLayout::MakeDefault();



            Assert::AreEqual (std::string (" open=basicprogram"), DebuggerLayout::FormatBasicOpened (opened));
            Assert::IsTrue   (DebuggerLayout::ParseBasicOpened ("code2=0300 open=basicstack bpcols=3 open=basicprogram") ==
                              std::set<std::wstring> ({ DebuggerLayout::kBasicProgram }));
            Assert::IsTrue   (layout.Contains (DebuggerLayout::kBasicProgram));

            Assert::AreEqual (std::string (" basicwatch=A$ basicwatch=X(1,2)"), DebuggerLayout::FormatBasicWatches ({ "A$", "X(1,2)" }));
            Assert::IsTrue   (DebuggerLayout::ParseBasicWatches ("open=basicprogram basicwatch=A$ bpcols=3 basicwatch=X(1,2)") ==
                              std::vector<std::string> ({ "A$", "X(1,2)" }));
        }
    };
}
