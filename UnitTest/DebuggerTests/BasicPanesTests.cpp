#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/SymbolHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/KeystrokeInjector.h"
#include "EmuTests/MachineIdle.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/HeatStrip.h"
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

        static BasicProgramPane::Style MakeStyle()
        {
            BasicProgramPane::Style  style;



            style.textArgb         = 0xFFD0D0D0u;
            style.syntax.mnemonic  = 0xFF4080FFu;
            style.syntax.address   = 0xFF80C080u;
            style.syntax.string    = 0xFFC08040u;
            style.syntax.comment   = 0xFF60A060u;
            return style;
        }

        //  Where a piece starts in a row's text.
        static int GetOffset (const BasicProgramPane::Row & row, int piece)
        {
            auto  found = std::ranges::find_if (row.pieces, [piece] (const std::pair<int, int> & p) { return p.first == piece; });



            return (found != row.pieces.end()) ? found->second : -1;
        }

        //  A character's ink from a cell's color ranges, zero for none.
        static uint32_t GetInk (const DxuiListView::Cell & cell, int at)
        {
            for (const auto & [first, last, argb] : cell.colorRanges)
            {
                if (at >= first && at < last)
                {
                    return argb;
                }
            }

            return 0;
        }

        static DebuggerViewSnapshot::HeatColumnLevels MakeHeat()
        {
            DebuggerViewSnapshot::HeatColumnLevels  heat;



            heat.execute.assign        (0x10000, 0);
            heat.read.assign           (0x10000, 0);
            heat.write.assign          (0x10000, 0);
            heat.statement.assign      (0x10000, 0);
            heat.statementCount.assign (0x10000, 0);
            return heat;
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


        //  A line breakpoint whose line the program doesn't have is marked,
        //  with "(not in the program)" after its name; one on a line the
        //  program has is not.
        TEST_METHOD (ALineBreakpointOnALineNotInTheProgramIsMarked)
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.basic = MakeState ({ MakeLine (0x0801, 10, { " PRINT ", "A" }) }, std::nullopt, 0x0801);
            snapshot.breakpoints.push_back (MakeLineBreakpoint (0, 10,   true));
            snapshot.breakpoints.push_back (MakeLineBreakpoint (1, 2630, true));

            DebuggerViewState::MarkAbsentBasicLines (snapshot);

            Assert::IsFalse (snapshot.breakpoints[0].isAbsent);
            Assert::IsTrue  (snapshot.breakpoints[1].isAbsent);
            Assert::AreEqual (std::string ("Line 2630 (not in the program)"), BreakpointColumns::GetCells (snapshot, snapshot.breakpoints[1])[(size_t) BreakpointColumns::Column::Name]);
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

        //  With heat on, a compound line's Heat cell has its hottest
        //  statement's heat, and each statement is tinted in place by how
        //  often BASIC began it, from the count at its first token, its text
        //  in the ink that reads on its tint in place of the syntax colors. A
        //  statement that hasn't run is in its syntax colors, and each
        //  statement's tip has its count.
        TEST_METHOD (ACompoundLinesStatementsAreTintedByHowOftenEachRan)
        {
            BasicLine                                     line    = MakeLine (0x0801, 10, { "A", "=", "1", ":", " PRINT ", "A" });
            DebuggerViewSnapshot::BasicState              state   = MakeState ({ line }, 0, 0);
            std::vector<BasicProgramPane::Row>            rows    = BasicProgramPane::GetRows (state.lines);
            DebuggerViewSnapshot::HeatColumnLevels        heat    = MakeHeat();
            HeatMapView::Palette                          palette;
            BasicProgramPane::Style                       style   = MakeStyle();
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            uint32_t                                      tint    = HeatStrip::GetScaleColor (HeatStrip::Kind::Run, 255, palette);
            int                                           first   = GetOffset (rows[0], 0);
            int                                           second  = GetOffset (rows[0], 4);



            heat.statement[0x0805]      = 255;
            heat.statementCount[0x0805] = 3;

            cells = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette);

            const DxuiListView::Cell  & listing = cells[0][BasicProgramPane::kListingColumn];

            Assert::AreEqual ((size_t) 1, listing.backgroundRanges.size(), L"the cold statement has no tint");
            Assert::IsTrue   (listing.backgroundRanges[0] == std::tuple<int, int, uint32_t> (first, second, tint), L"the first statement, its colon included");
            Assert::AreEqual (DxuiColor::ChooseInkFor (tint, 0xFF101010u, 0xFFF8F8F8u), GetInk (listing, first), L"the ink for the tint, not a syntax color");
            Assert::AreEqual (style.syntax.mnemonic, GetInk (listing, second + 1), L"the cold statement's keyword is in its syntax color");
            Assert::IsTrue   (std::get<2> (listing.rangeTips[0]).find (L"3 times") != std::wstring::npos, std::get<2> (listing.rangeTips[0]).c_str());
            Assert::IsTrue   (std::get<2> (listing.rangeTips[1]).find (L"hasn't run") != std::wstring::npos, std::get<2> (listing.rangeTips[1]).c_str());
            Assert::AreEqual (tint, cells[0][BasicProgramPane::kHeatColumn].background, L"the Heat cell has the hottest statement's heat");
        }

        //  A compound line whose statements all have the same heat would be
        //  one color from end to end, so it is drawn as a plain line is: its
        //  heat in the Heat column alone, its text in its syntax colors.
        TEST_METHOD (ACompoundLineOfOneHeatIsNotSegmented)
        {
            BasicLine                                     line    = MakeLine (0x0801, 10, { "A", "=", "1", ":", " PRINT ", "A" });
            DebuggerViewSnapshot::BasicState              state   = MakeState ({ line }, 0, 0);
            std::vector<BasicProgramPane::Row>            rows    = BasicProgramPane::GetRows (state.lines);
            DebuggerViewSnapshot::HeatColumnLevels        heat    = MakeHeat();
            HeatMapView::Palette                          palette;
            BasicProgramPane::Style                       style   = MakeStyle();
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            std::vector<std::vector<DxuiListView::Cell>>  plain   = BasicProgramPane::GetCells (state, rows, {}, style);



            heat.statement[0x0805]      = 200;
            heat.statementCount[0x0805] = 4;
            heat.statement[0x0809]      = 200;
            heat.statementCount[0x0809] = 5;

            cells = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette);

            Assert::AreEqual (HeatStrip::GetScaleColor (HeatStrip::Kind::Run, 200, palette), cells[0][BasicProgramPane::kHeatColumn].background, L"the heat in the column");
            Assert::IsTrue   (cells[0][BasicProgramPane::kListingColumn].backgroundRanges.empty(), L"no segments");
            Assert::IsTrue   (cells[0][BasicProgramPane::kListingColumn].colorRanges == plain[0][BasicProgramPane::kListingColumn].colorRanges, L"the syntax colors");
        }

        //  A line of one statement has its heat in the Heat column alone:
        //  its text is in its syntax colors.
        TEST_METHOD (APlainLinesHeatIsInTheHeatColumnAlone)
        {
            BasicLine                                     line    = MakeLine (0x0801, 10, { " PRINT ", "A" });
            DebuggerViewSnapshot::BasicState              state   = MakeState ({ line }, 0, 0);
            std::vector<BasicProgramPane::Row>            rows    = BasicProgramPane::GetRows (state.lines);
            DebuggerViewSnapshot::HeatColumnLevels        heat    = MakeHeat();
            HeatMapView::Palette                          palette;
            BasicProgramPane::Style                       style   = MakeStyle();
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            std::vector<std::vector<DxuiListView::Cell>>  plain   = BasicProgramPane::GetCells (state, rows, {}, style);



            heat.statement[0x0805]      = 255;
            heat.statementCount[0x0805] = 7;

            cells = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette);

            Assert::AreEqual (HeatStrip::GetScaleColor (HeatStrip::Kind::Run, 255, palette), cells[0][BasicProgramPane::kHeatColumn].background);
            Assert::IsTrue   (cells[0][BasicProgramPane::kHeatColumn].tip.find (L"7 times") != std::wstring::npos, cells[0][BasicProgramPane::kHeatColumn].tip.c_str());
            Assert::IsTrue   (cells[0][BasicProgramPane::kListingColumn].backgroundRanges.empty(), L"no tint in the listing");
            Assert::IsTrue   (cells[0][BasicProgramPane::kListingColumn].colorRanges == plain[0][BasicProgramPane::kListingColumn].colorRanges, L"the syntax colors");
        }

        //  A statement after a colon is counted at its first token, not
        //  at the line's: with only the second statement begun, the second
        //  is tinted and the first is in its syntax colors. A read of the
        //  first statement's token is not a run of it.
        TEST_METHOD (AStatementAfterAColonIsCountedAtItsOwnFirstToken)
        {
            BasicLine                                     line    = MakeLine (0x0801, 10, { "A", "=", "1", ":", " PRINT ", "A" });
            DebuggerViewSnapshot::BasicState              state   = MakeState ({ line }, 0, 0);
            std::vector<BasicProgramPane::Row>            rows    = BasicProgramPane::GetRows (state.lines);
            DebuggerViewSnapshot::HeatColumnLevels        heat    = MakeHeat();
            HeatMapView::Palette                          palette;
            BasicProgramPane::Style                       style   = MakeStyle();
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            uint32_t                                      tint    = HeatStrip::GetScaleColor (HeatStrip::Kind::Run, 255, palette);
            int                                           first   = GetOffset (rows[0], 0);
            int                                           second  = GetOffset (rows[0], 4);



            heat.read[0x0805]           = 255;
            heat.statement[0x0809]      = 255;
            heat.statementCount[0x0809] = 6;

            cells = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette);

            const DxuiListView::Cell  & listing = cells[0][BasicProgramPane::kListingColumn];

            Assert::AreEqual ((size_t) 1, listing.backgroundRanges.size(), L"only the statement that ran is tinted");
            Assert::AreEqual (second, std::get<0> (listing.backgroundRanges[0]), L"the tint starts at the second statement");
            Assert::AreEqual (tint, std::get<2> (listing.backgroundRanges[0]));
            Assert::AreNotEqual (DxuiColor::ChooseInkFor (tint, 0xFF101010u, 0xFFF8F8F8u), GetInk (listing, first), L"the first statement isn't in the tint's ink");
            Assert::IsTrue   (std::get<2> (listing.rangeTips[0]).find (L"hasn't run") != std::wstring::npos, std::get<2> (listing.rangeTips[0]).c_str());
            Assert::IsTrue   (std::get<2> (listing.rangeTips[1]).find (L"6 times") != std::wstring::npos, std::get<2> (listing.rangeTips[1]).c_str());
            Assert::AreEqual (tint, cells[0][BasicProgramPane::kHeatColumn].background, L"the Heat cell has the second statement's heat");
        }

        //  A line the pointer is over fades from its tint to its syntax
        //  colors: halfway, the tint is half as opaque and the ink between
        //  the two; all the way, the line is as it is with heat off.
        TEST_METHOD (AHoveredLineFadesToItsSyntaxColors)
        {
            BasicLine                                     line    = MakeLine (0x0801, 10, { " PRINT ", "A", ":", " END " });
            DebuggerViewSnapshot::BasicState              state   = MakeState ({ line }, 0, 0);
            std::vector<BasicProgramPane::Row>            rows    = BasicProgramPane::GetRows (state.lines);
            DebuggerViewSnapshot::HeatColumnLevels        heat    = MakeHeat();
            HeatMapView::Palette                          palette;
            BasicProgramPane::Style                       style   = MakeStyle();
            std::map<size_t, float>                       reveals;
            std::vector<std::vector<DxuiListView::Cell>>  cells;
            std::vector<std::vector<DxuiListView::Cell>>  plain   = BasicProgramPane::GetCells (state, rows, {}, style);
            int                                           first   = GetOffset (rows[0], 0);



            heat.statement[0x0805]      = 255;
            heat.statementCount[0x0805] = 1;

            reveals[0] = 0.5f;
            cells      = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette, &reveals);

            Assert::AreEqual (0x80u, std::get<2> (cells[0][BasicProgramPane::kListingColumn].backgroundRanges.at (0)) >> 24, L"half as opaque");
            Assert::AreNotEqual (style.syntax.mnemonic, GetInk (cells[0][BasicProgramPane::kListingColumn], first), L"not yet the syntax color");

            reveals[0] = 1.0f;
            cells      = BasicProgramPane::GetCells (state, rows, {}, style, &heat, &palette, &reveals);

            Assert::IsTrue   (cells[0][BasicProgramPane::kListingColumn].backgroundRanges.empty(), L"no tint");
            Assert::AreEqual (GetInk (plain[0][BasicProgramPane::kListingColumn], first), GetInk (cells[0][BasicProgramPane::kListingColumn], first), L"the syntax color");
        }



        //  The pane fades the line under the pointer over its fade time, to
        //  its syntax colors, and back once the pointer is elsewhere.
        TEST_METHOD (TheLineUnderThePointerFadesAndFadesBack)
        {
            BasicLine                                                       line    = MakeLine (0x0801, 10, { " PRINT ", "A", ":", " END " });
            DxuiListView                                                    list;
            BasicProgramPane                                                pane    (&list, nullptr);
            std::shared_ptr<DebuggerViewSnapshot::HeatColumnLevels>         heat    = std::make_shared<DebuggerViewSnapshot::HeatColumnLevels> (MakeHeat());
            auto                                                            alpha   = [&list] ()
            {
                const DxuiListView::Cell  & cell = list.GetCellsOfRow (0)[BasicProgramPane::kListingColumn];



                return cell.backgroundRanges.empty() ? 0u : std::get<2> (cell.backgroundRanges[0]) >> 24;
            };



            heat->statement[0x0805]      = 255;
            heat->statementCount[0x0805] = 1;

            pane.Configure (16);
            pane.SetStyle  (MakeStyle());
            pane.SetHeat   (heat, HeatMapView::Palette());
            pane.Apply     (MakeState ({ line }, std::nullopt, 0), {});

            Assert::AreEqual (0xFFu, alpha(), L"tinted while the pointer is elsewhere");

            list.SetHoveredRow (0);
            (void) pane.Tick (1000);
            Assert::IsTrue   (pane.Tick (1075), L"fading");
            Assert::AreEqual (0x80u, alpha(), L"halfway");
            (void) pane.Tick (1200);
            Assert::AreEqual (0u, alpha(), L"the syntax colors alone");
            Assert::IsFalse  (pane.Tick (1300), L"done fading");

            list.SetHoveredRow (-1);
            (void) pane.Tick (1375);
            Assert::AreEqual (0x80u, alpha(), L"back halfway");
            (void) pane.Tick (1500);
            Assert::AreEqual (0xFFu, alpha(), L"tinted again");
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
            Assert::AreEqual (std::wstring (L" 10 A=1"),     cells[0][BasicProgramPane::kListingColumn].text);
            Assert::AreEqual (std::wstring (L" 20 B=2:C=3"), cells[1][BasicProgramPane::kListingColumn].text);
            Assert::AreEqual (0u, cells[0][BasicProgramPane::kListingColumn].background, L"line 10 is not being run");
            Assert::IsTrue   (cells[0][0].icon == nullptr);
            Assert::AreEqual (style.pcRowArgb, cells[1][BasicProgramPane::kListingColumn].background);
            Assert::IsTrue   (cells[1][0].icon == style.pcOnEnabledIcon, L"the arrow over the breakpoint's dot");
            Assert::AreEqual ((size_t) 4, cells[1][BasicProgramPane::kListingColumn].dimRanges.size(), L"B, =, 2 and the colon muted");
            Assert::IsTrue   (cells[1][BasicProgramPane::kListingColumn].dimRanges.front() == std::pair<int, int> (4, 5));
            Assert::IsTrue   (cells[1][BasicProgramPane::kListingColumn].dimRanges.back()  == std::pair<int, int> (7, 8));
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
        //  While ProDOS banks language card RAM over Applesoft, the program
        //  is listed from memory, with no values read.
        TEST_METHOD (TheProgramListsWhileRamIsBankedOverApplesoft)
        {
            MachineHandlerRig<BasicHandlers>  rig { "Apple2eEnhanced", TestMachine::Slots::Empty };
            DebuggerViewSnapshot::BasicState  state;



            MachineIdle::RunUntilIdle (rig.machine, 20'000'000);
            GuestSession::TypeAndCollect (rig.machine, "10 PRINT 1");

            rig.machine.GetMemoryBus().ReadByte  (0xC083);
            rig.machine.GetMemoryBus().ReadByte  (0xC083);
            rig.machine.GetMemoryBus().WriteByte (0xD81D, 0x00);

            state = DebuggerViewState::ReadBasic (rig.session, true);

            Assert::IsFalse  (BasicStopMonitor::IsApplesoftVisible (rig.session), L"RAM is banked over the ROM");
            Assert::IsTrue   (state.isApplesoft && state.hasProgram);
            Assert::AreEqual ((size_t) 1, state.lines.size());
            Assert::IsFalse  (state.hasValues);
        }



        //  After a run ends its values are in memory; at the prompt they are
        //  read, running or not.
        TEST_METHOD (TheValuesAreReadAtThePromptAfterARun)
        {
            MachineHandlerRig<BasicHandlers>  rig { "Apple2eEnhanced", TestMachine::Slots::Empty };
            DebuggerViewSnapshot::BasicState  state;



            MachineIdle::RunUntilIdle (rig.machine, 20'000'000);
            GuestSession::TypeAndCollect (rig.machine, "10 A = 5");
            GuestSession::TypeAndCollect (rig.machine, "RUN");

            state = DebuggerViewState::ReadBasic (rig.session, false);

            Assert::IsFalse  (state.line.has_value(), L"at the prompt");
            Assert::IsTrue   (state.hasValues);
            Assert::AreEqual ((size_t) 1, state.variables.size());
        }



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

        //  Wrapped at a width, a row is cut before the piece that would pass
        //  it, the next row indented as LIST indents one, and a row has at
        //  least one piece.
        TEST_METHOD (TheListingWrapsAtTheWidthGiven)
        {
            auto                                     count = [] (const std::wstring & text) { return (float) text.size(); };
            std::vector<BasicLine>                   lines = { MakeLine (0x0801, 10, { " PRINT ", "\"", "A", "B", "C", "D", "\"" }) };
            std::vector<BasicProgramPane::Row>       rows  = BasicProgramPane::GetRows (lines, 12.0f, count);



            Assert::AreEqual (std::wstring (L" 10  PRINT \""), rows[0].text, L"the row before the piece past the width");
            Assert::IsTrue   (rows.size() > 1);
            Assert::IsFalse  (rows[1].isFirst);
            Assert::IsTrue   (rows[1].text.starts_with (std::wstring (ApplesoftProgram::kListIndent, L' ')));

            rows = BasicProgramPane::GetRows (lines, 1000.0f, count);
            Assert::AreEqual ((size_t) 1, rows.size(), L"a wide pane has the line on one row");

            rows = BasicProgramPane::GetRows ({ MakeLine (0x0801, 10, { " PRINT " }) }, 1.0f, count);
            Assert::AreEqual ((size_t) 1, rows.size(), L"a piece wider than the width is alone on a row");
        }

        //  A line's values are its variables' at this stop; on the line the
        //  machine stopped at before, a changed one is before -> after.
        TEST_METHOD (ALinesValuesShowWhatItChanged)
        {
            DebuggerViewSnapshot::BasicState  now    = MakeValues();
            DebuggerViewSnapshot::BasicState  before = MakeValues();
            BasicLine                         line   = MakeLine (0x0801, 10, { "A", " = ", "A", " + ", "1", ":", " PRINT ", "S", "$" });



            before.variables[0].value = "0.5";

            Assert::AreEqual (std::wstring (L"A=1.5  S$=\"HI\""), BasicProgramPane::GetValuesText (line, now, nullptr));
            Assert::AreEqual (std::wstring (L"A=0.5 ") + s_kpszRightArrow + L" 1.5  S$=\"HI\"", BasicProgramPane::GetValuesText (line, now, &before));
            Assert::AreEqual (std::wstring (L"A\t1.5\nS$\t\"HI\""), BasicProgramPane::GetValuesTip (line, now));
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

        //  A watch's text is read by the session as FR-241 has it: a symbol
        //  first; then a name with $, % or subscripts, or a plain name the
        //  stopped program has, as a BASIC variable; BASIC and a name always.
        //  WL lists each with its value, and the project script has each as
        //  BASIC and the name.
        TEST_METHOD (TheSessionReadsAWatchAsABasicVariable)
        {
            MachineHandlerRig<BasicHandlers>  rig { "Apple2eEnhanced", TestMachine::Slots::Empty };
            BreakpointHandlers                breakpoints;
            WatchHandlers                     watches;
            SymbolHandlers                    symbols;
            std::vector<std::string>          listed;



            rig.session.AddHandler (&breakpoints);
            rig.session.AddHandler (&watches);
            rig.session.AddHandler (&symbols);
            MachineIdle::RunUntilIdle (rig.machine, 20'000'000);

            for (const char * text : { "10 HIGH = 7: S$ = \"AB\": DIM X(2): X(1) = 5: SCUD = 3", "20 END" })
            {
                GuestSession::TypeAndCollect (rig.machine, text);
            }

            Assert::AreEqual ((size_t) 3, KeystrokeInjector::InjectString (rig.machine, "RUN"));
            rig.RunOk ("BUDGET 5000000");
            rig.RunOk ("BPBASIC 20");
            rig.machine.GetRefs().keyboard->PressKey (KeystrokeInjector::kAppleReturn);
            rig.RunOk ("G");

            rig.RunOk ("SYM SC = 300");
            rig.RunOk ("W HI");
            rig.RunOk ("W S$");
            rig.RunOk ("W X(1)");
            rig.RunOk ("W SC");
            rig.RunOk ("W BASIC Q");
            listed = rig.RunOk ("WL").text;

            Assert::AreEqual ((size_t) 5, listed.size());
            Assert::AreEqual (std::string ("#0 HI = 7"),        listed[0], L"a plain name the program has");
            Assert::AreEqual (std::string ("#1 S$ = \"AB\""),   listed[1]);
            Assert::AreEqual (std::string ("#2 X(1) = 5"),      listed[2]);
            Assert::IsTrue   (listed[3].starts_with ("#3 $0300"), L"the symbol SC comes before SCUD's SC");
            Assert::AreEqual (std::string ("#4 Q = 0"),         listed[4], L"BASIC makes any name one");

            Assert::IsTrue (WatchHandlers::MakeScript (rig.session, WatchListKind::Watch).find ("WA BASIC S$\n") != std::string::npos);
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

        //  After a step from I = I + 1 to GOTO 10, I is among the automatic
        //  watches, as Visual Studio's Autos list what the line before used:
        //  the statement just run's variables follow the next one's, marked
        //  as the previous statement's, and a value that differs from the
        //  last stop's is marked changed.
        TEST_METHOD (TheStatementJustRunKeepsItsVariablesAmongTheAutos)
        {
            BasicLine                           increment = MakeLine (0x0810, 20, { "I", "=", "I", "+", "1" });
            BasicLine                           jump      = MakeLine (0x0820, 30, { " GOTO ", "1", "0" });
            DebuggerViewSnapshot::BasicState    state     = MakeState ({ increment, jump }, 20, (Word) (increment.address + kBodyOffset));
            BasicAutoWatches                    autos;
            std::vector<BasicAutoWatches::Row>  rows;



            state.hasValues = true;
            state.variables = { { "I", BasicType::Real, 0, "0" } };
            rows            = autos.Update (state, 100);

            Assert::AreEqual ((size_t) 1, rows.size());
            Assert::AreEqual (std::string ("I"), rows[0].item.name);
            Assert::IsFalse  (rows[0].isPrevious || rows[0].isChanged, L"the statement being run's, as it was");

            state.line        = 30;
            state.textPointer = (Word) (jump.address + kBodyOffset);
            state.variables   = { { "I", BasicType::Real, 0, "1" } };
            rows              = autos.Update (state, 200);

            Assert::AreEqual ((size_t) 1, rows.size(), L"GOTO 10 has no variable, and I stays");
            Assert::AreEqual (std::string ("1"), rows[0].item.value);
            Assert::IsTrue   (rows[0].isPrevious, L"the statement just run's");
            Assert::IsTrue   (rows[0].isChanged,  L"it changed since the last stop");

            rows = autos.Update (state, 200);
            Assert::IsTrue (rows.size() == 1 && rows[0].isPrevious && rows[0].isChanged, L"the same stop again has the same rows");
        }

        //  The characters past the second are dimmed, up to the type's sign.
        TEST_METHOD (TheCharactersPastTheSecondAreDimmed)
        {
            Assert::IsTrue (BasicWatches::MakeNameCell ("HIGH$").dimRanges == std::vector<std::pair<int, int>> { { 2, 4 } });
            Assert::IsTrue (BasicWatches::MakeNameCell ("SCORE(3)").dimRanges == std::vector<std::pair<int, int>> { { 2, 5 } });
            Assert::IsTrue (BasicWatches::MakeNameCell ("HI").dimRanges.empty());
        }

        //  (BASIC) follows a variable's name only where another row has that
        //  label, as the I flag and a variable I do.
        TEST_METHOD (ANameIsMarkedBasicOnlyWhereItIsADuplicate)
        {
            Assert::AreEqual (std::wstring (L"I (BASIC)"), BasicWatches::MakeNameCell ("I", { "I", "Z" }).text);
            Assert::AreEqual (std::wstring (L"J"),         BasicWatches::MakeNameCell ("J", { "I", "Z" }).text);
            Assert::AreEqual (std::wstring (L"I"),         BasicWatches::MakeNameCell ("I").text);
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
        //  an old text's other BASIC panes are passed over.
        TEST_METHOD (TheOpenedPaneIsWrittenAndReadBack)
        {
            std::set<std::wstring>  opened = { DebuggerLayout::kBasicProgram, L"heatmap" };
            DxuiPaneLayout          layout = DebuggerLayout::MakeDefault();



            Assert::AreEqual (std::string (" open=basicprogram"), DebuggerLayout::FormatBasicOpened (opened));
            Assert::IsTrue   (DebuggerLayout::ParseBasicOpened ("code2=0300 open=basicstack bpcols=3 open=basicprogram") ==
                              std::set<std::wstring> ({ DebuggerLayout::kBasicProgram }));
            Assert::IsTrue   (layout.Contains (DebuggerLayout::kBasicProgram));
        }
    };
}
