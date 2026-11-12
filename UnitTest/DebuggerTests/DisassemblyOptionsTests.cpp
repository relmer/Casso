#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Config/GlobalUserPrefs.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DisassemblyOptionsTests
{
    using Option = DisassemblyOptions::Option;



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OptionsHost
    //
    //  A host that keeps the viewing options the window saves and finds one
    //  source file, main.a65, whose second line is LDA #$41.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class OptionsHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                     override {}
        void  PauseDebugger            ()                                        override {}
        void  SetDebuggerCodeLines     (int, int)                                override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                override {}
        void  SetDebuggerCodeTop       (Word, int)                               override {}
        void  SetDebuggerFollowView    (int)                                     override {}
        void  CloseDebuggerCodeView    (int)                                     override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                 override {}
        void  GoToDebuggerMemory       (int, const std::string &)                override {}
        void  ScrollDebuggerCode       (int, int)                                override {}
        void  OnDebuggerWindowClosed   ()                                        override {}
        void  SetDebuggerKeyScheme     (const std::string &)                     override {}
        void  SetDebuggerLayout        (const std::string &)                     override {}
        void  SetDebuggerOpenViews     (const std::string &)                     override {}
        void  SetDebuggerPlacement     (const RECT &)                            override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}
        void  SetDebuggerDisassemblyOptions (const std::string & text)           override { options = text; }
        void  SetDebuggerHeatColumnsShown   (bool shown)                         override { isHeatShown = shown; }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }
        std::string     GetDebuggerDisassemblyOptions()   override { return options; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;

            lookup.match = sourceText.empty() ? SourceMatch::NotFound : SourceMatch::Exact;
            lookup.text  = sourceText;
            return lookup;
        }

        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::string  options;
        std::string  sourceText  = "; main\n        lda #$41\n        rts\n";
        bool         isHeatShown = false;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OptionsWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class OptionsWindow : public DebuggerWindow
    {
    public:
        OptionsWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::HasTopLayer;
        using DebuggerWindow::GetCodeGroup;
        using DebuggerWindow::GetCodeLineOfRow;
        using DebuggerWindow::GetCodeGutter;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DisassemblyOptionsTests
    //
    //  The disassembly views' viewing options, as Visual Studio's: address,
    //  code bytes, source, symbol names and line numbers, saved as a setting;
    //  the Options sheet's operand values, pointers, branch arrow and address
    //  colon; source rows above their code; and the gray breakpoint drawn in
    //  the gutter under the pointer.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DisassemblyOptionsTests)
    {
    public:
        //  LDA #$41 at $0300 from line 2, RTS at $0302 from line 3, and a data
        //  byte at $0303 with no line. The PC is at $0300.
        static std::shared_ptr<const DebuggerViewSnapshot> MakeSnapshot (bool withDebugFile)
        {
            auto                                  snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine        lda;
            DebuggerViewSnapshot::CodeLine        rts;
            DebuggerViewSnapshot::CodeLine        data;
            DebuggerViewSnapshot::SourceState     source;



            lda.address      = 0x0300;
            lda.bytes        = "A9 41";
            lda.instruction  = "LDA #$41";
            lda.isCurrent    = true;
            lda.sourceFileId = withDebugFile ? 0 : -1;
            lda.sourceLine   = withDebugFile ? 2 : 0;

            rts.address      = 0x0302;
            rts.bytes        = "60";
            rts.instruction  = "RTS";
            rts.sourceFileId = withDebugFile ? 0 : -1;
            rts.sourceLine   = withDebugFile ? 3 : 0;

            data.address     = 0x0303;
            data.bytes       = "FF";
            data.instruction = "DB $FF";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts, data };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            if (withDebugFile)
            {
                source.debugFilePath = L"C:\\Work\\main.dbg";
                source.programKey    = "key";
                source.files         = { { 0, "main.a65", 45, 0, "", 0 } };
                snapshot->source     = source;
            }

            return snapshot;
        }



        //  The middle of a row, in window coordinates, at 96 DPI.
        static int GetRowMiddle (const DxuiListView & list, int row)
        {
            return list.GetBounds().top + list.GetHeaderHeightPx() + (row - list.GetTopRow()) * list.GetRowHeightDip() + list.GetRowHeightDip() / 2;
        }



        TEST_METHOD (TheTextRoundTripsAndEmptyGivesTheDefaults)
        {
            DisassemblyOptions  options;
            DisassemblyOptions  none;



            Assert::IsTrue  (DisassemblyOptions::FromText ("") == DisassemblyOptions(), L"empty is the defaults");
            Assert::IsTrue  (options.IsOn (Option::Addresses));
            Assert::IsTrue  (options.IsOn (Option::Source));
            Assert::IsTrue  (options.IsOn (Option::LineNumbers));

            options.Toggle (Option::CodeBytes);
            options.Toggle (Option::LineNumbers);
            Assert::AreEqual (std::string ("address -bytes source symbols -lines values pointers arrow -colon -heat dim=code"), options.ToText());
            Assert::IsTrue   (DisassemblyOptions::FromText (options.ToText()) == options);

            for (int i = 0; i < DisassemblyOptions::kOptionCount; i++)
            {
                none.Set ((Option) i, false);
            }

            Assert::IsTrue (DisassemblyOptions::FromText (none.ToText()) == none, L"every option off survives the round trip");
        }



        //  The kind of row to dim is saved as dim= and its key, and a text
        //  without it dims the disassembly.
        TEST_METHOD (TheDimChoiceIsSaved)
        {
            DisassemblyOptions  options;



            Assert::IsTrue (options.GetDim() == DisassemblyOptions::Dim::Code, L"the default");
            Assert::IsTrue (DisassemblyOptions::FromText ("address source").GetDim() == DisassemblyOptions::Dim::Code, L"an earlier text");

            options.SetDim (DisassemblyOptions::Dim::Source);
            Assert::IsTrue (DisassemblyOptions::FromText (options.ToText()).GetDim() == DisassemblyOptions::Dim::Source);

            options.SetDim (DisassemblyOptions::Dim::None);
            Assert::IsTrue (DisassemblyOptions::FromText (options.ToText()) == options, L"the whole set survives the round trip");
        }



        //  A text saved before the last four options existed lists the options
        //  turned on: one of the first five it does not list is off, and the
        //  last four take their defaults.
        TEST_METHOD (AnEarlierTextKeepsItsOptionsAndDefaultsTheNewOnes)
        {
            DisassemblyOptions  options = DisassemblyOptions::FromText ("address source symbols");



            Assert::IsTrue  (options.IsOn  (Option::Addresses));
            Assert::IsFalse (options.IsOn  (Option::CodeBytes),     L"not listed, so off");
            Assert::IsFalse (options.IsOn  (Option::LineNumbers),   L"not listed, so off");
            Assert::IsTrue  (options.IsOn  (Option::OperandValues), L"the default");
            Assert::IsTrue  (options.IsOn  (Option::Pointers),      L"the default");
            Assert::IsTrue  (options.IsOn  (Option::BranchArrow),   L"the default");
            Assert::IsFalse (options.IsOn  (Option::AddressColon),  L"the default");

            options = DisassemblyOptions::FromText ("none");

            Assert::IsFalse (options.IsOn  (Option::Addresses),     L"the earlier form's every-option-off text");
            Assert::IsTrue  (options.IsOn  (Option::OperandValues), L"the default");
        }



        //  With a source row above each instruction, only half of the lines
        //  have their instruction's row in a view of a given height.
        TEST_METHOD (TheShownLinesAreThoseWhoseInstructionRowFits)
        {
            std::vector<DisassemblyOptions::Row>  rows;



            for (int line = 0; line < 10; line++)
            {
                rows.push_back ({ -1, 0, line + 1 });
                rows.push_back ({ line, -1, 0 });
            }

            Assert::AreEqual (5,  DisassemblyOptions::CountShownLines (rows, 10));
            Assert::AreEqual (4,  DisassemblyOptions::CountShownLines (rows, 9), L"a source row alone at the bottom doesn't count its instruction");
            Assert::AreEqual (10, DisassemblyOptions::CountShownLines (rows, 40));
        }

        TEST_METHOD (ASourceRowGoesAboveTheFirstInstructionOfEachLine)
        {
            std::shared_ptr<const DebuggerViewSnapshot>  snapshot = MakeSnapshot (true);
            std::vector<DisassemblyOptions::Row>         rows;
            auto                                         always   = [] (int, int) { return true; };



            rows = DisassemblyOptions::BuildRows (snapshot->codeViews[0], true, always);

            Assert::AreEqual ((size_t) 5, rows.size(), L"two source rows and three instructions");
            Assert::AreEqual (-1, rows[0].codeLine);
            Assert::AreEqual (2,  rows[0].sourceLine);
            Assert::AreEqual (0,  rows[1].codeLine);
            Assert::AreEqual (3,  rows[2].sourceLine);
            Assert::AreEqual (1,  rows[3].codeLine);
            Assert::AreEqual (2,  rows[4].codeLine, L"the data byte has no line, so no row above it");
            Assert::AreEqual (3,  DisassemblyOptions::GetRowOfLine (rows, 1));
            Assert::AreEqual (-1, DisassemblyOptions::GetLineOfRow (rows, 2));
            Assert::AreEqual (1,  DisassemblyOptions::GetLineAtOrBelow (rows, 2), L"a source row's menu is its instruction's");
            Assert::AreEqual (0,  DisassemblyOptions::GetLineAtOrBelow (rows, 0));
            Assert::AreEqual (2,  DisassemblyOptions::GetLineAtOrBelow (rows, 4), L"an instruction row is its own");
            Assert::AreEqual (-1, DisassemblyOptions::GetLineAtOrBelow (rows, 5), L"past the last row");

            rows = DisassemblyOptions::BuildRows (snapshot->codeViews[0], false, always);
            Assert::AreEqual ((size_t) 3, rows.size(), L"source off lists the instructions alone");
        }



        //  A source row's heat is its instructions' heat: the span runs from
        //  the first instruction under the row to the end of the last one
        //  from the same line.
        TEST_METHOD (ASourceRowSpansItsLinesInstructions)
        {
            std::vector<DebuggerViewSnapshot::CodeLine>  lines (4);
            std::vector<DisassemblyOptions::Row>         rows;
            auto                                         always = [] (int, int) { return true; };
            Word                                         first  = 0;
            size_t                                       count  = 0;



            lines[0] = { .address = 0x6000, .bytes = "A9 41" };
            lines[1] = { .address = 0x6002, .bytes = "20 ED FD" };
            lines[2] = { .address = 0x6005, .bytes = "60" };
            lines[3] = { .address = 0x6006, .bytes = "00" };

            for (size_t i = 0; i < 3; i++)
            {
                lines[i].sourceFileId = 0;
                lines[i].sourceLine   = (i < 2) ? 4 : 5;
            }

            rows = DisassemblyOptions::BuildRows (lines, true, always);

            Assert::IsTrue   (DisassemblyCells::TryGetSourceRowSpan (rows, 0, lines, first, count));
            Assert::AreEqual ((Word) 0x6000, first);
            Assert::AreEqual ((size_t) 5, count, L"both instructions from line 4");

            Assert::IsTrue   (DisassemblyCells::TryGetSourceRowSpan (rows, 3, lines, first, count));
            Assert::AreEqual ((Word) 0x6005, first);
            Assert::AreEqual ((size_t) 1, count, L"line 5's instruction alone");

            Assert::IsFalse  (DisassemblyCells::TryGetSourceRowSpan (rows, 1, lines, first, count), L"an instruction's row");
            Assert::IsFalse  (DisassemblyCells::TryGetSourceRowSpan (rows, rows.size(), lines, first, count), L"no row");
        }



        //  A label alone on its line produces no instruction, so it goes above
        //  the source line of the instruction it labels, back to the line of
        //  the instruction before it and no further.
        TEST_METHOD (LabelLinesGoAboveTheLineTheyLabel)
        {
            std::vector<DebuggerViewSnapshot::CodeLine>  lines (3);
            std::vector<DisassemblyOptions::Row>         rows;
            auto                                         always  = [] (int, int) { return true; };
            auto                                         isLabel = [] (int, int line) { return line == 2 || (line >= 4 && line <= 7); };
            std::vector<int>                             shown;
            std::vector<int>                             expected = { 4, 5, 6, 7, -1, 8, -1, 9, -1 };



            lines[0] = { .address = 0x6000, .bytes = "A9 41" };
            lines[1] = { .address = 0x6002, .bytes = "60" };
            lines[2] = { .address = 0x6003, .bytes = "00" };

            lines[0].sourceFileId = 0;
            lines[0].sourceLine   = 7;
            lines[1].sourceFileId = 0;
            lines[1].sourceLine   = 8;
            lines[2].sourceFileId = 0;
            lines[2].sourceLine   = 9;

            rows = DisassemblyOptions::BuildRows (lines, true, always, isLabel);

            for (const DisassemblyOptions::Row & each : rows)
            {
                shown.push_back (each.codeLine < 0 ? each.sourceLine : -1);
            }

            Assert::IsTrue (expected == shown, L"lines 4 to 6 above line 7, not line 2 past the code on line 3, and not line 7 again above line 8");

            rows = DisassemblyOptions::BuildRows (lines, true, always);

            Assert::AreEqual ((size_t) 6, rows.size(), L"no label lines without the check");
        }



        //  With source lines on, the PC's marker goes on the source row its
        //  instruction came from; an instruction with no source row has it.
        TEST_METHOD (ThePcsSourceRowIsTheOneAboveItsInstruction)
        {
            std::shared_ptr<const DebuggerViewSnapshot>   snapshot = MakeSnapshot (true);
            std::vector<DebuggerViewSnapshot::CodeLine>   lines    = snapshot->codeViews[0];
            std::vector<DisassemblyOptions::Row>          rows;
            auto                                          always   = [] (int, int) { return true; };



            rows = DisassemblyOptions::BuildRows (lines, true, always);

            for (size_t i = 0; i < lines.size(); i++)
            {
                lines[i].isCurrent = i == 1;
            }

            Assert::AreEqual (2, DisassemblyGroup::FindPcSourceRow (lines, rows), L"the second instruction's source row");

            for (size_t i = 0; i < lines.size(); i++)
            {
                lines[i].isCurrent = i == 2;
            }

            Assert::AreEqual (-1, DisassemblyGroup::FindPcSourceRow (lines, rows), L"the data byte has no source row");
        }



        TEST_METHOD (SymbolNamesOffShowsTheOperandsAddress)
        {
            DebuggerViewSnapshot::CodeLine  line;



            line.instruction   = "STA COUT";
            line.shownOperand  = "COUT";
            line.memoryOperand = "$FDED";

            Assert::AreEqual (std::string ("STA COUT"),  DisassemblyOptions::GetInstructionText (line, true));
            Assert::AreEqual (std::string ("STA $FDED"), DisassemblyOptions::GetInstructionText (line, false));
        }



        TEST_METHOD (AnOptionHidesItsColumnAndIsSaved)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);



            scaler.SetDpi (96);
            host.options = "address bytes symbols";

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (false), 0);

            Assert::IsTrue  (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kAddressColumn), L"the saved options are read");
            Assert::IsTrue  (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kBytesColumn));

            window.GetCodeGroup().ToggleOption (Option::Addresses);

            Assert::IsFalse  (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kAddressColumn), L"no address column");
            Assert::AreEqual (std::string ("-address bytes -source symbols -lines values pointers arrow -colon -heat dim=code"), host.options, L"the choice is saved");
            Assert::IsNotNull (window.GetCodeGroup().GetView (0).GetBar(), L"each view has its options bar");
        }



        //  With Show heat on, the heat column is visible with a block or a
        //  letter per byte of each line, and the heat map is on.
        TEST_METHOD (ShowHeatAddsAStripPerByteAndRecordsTheHeat)
        {
            CassoTheme     theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window   (theme, host);
            auto           snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot (false));
            auto           levels   = std::make_shared<DebuggerViewSnapshot::HeatColumnLevels>();



            levels->execute.assign (0x10000, 0);
            levels->read.assign    (0x10000, 0);
            levels->write.assign   (0x10000, 0);
            levels->execute[0x0300] = 200;
            levels->read[0x0301]    = 100;
            levels->write[0x0302]   = 50;
            snapshot->heatColumns   = levels;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kHeatColumn), L"off by default");

            window.GetCodeGroup().ToggleOption (Option::Heat);

            Assert::IsTrue   (host.isHeatShown, L"the heat map is on");
            Assert::IsTrue   (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kHeatColumn));
            Assert::AreEqual (std::wstring (L"  "), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kHeatColumn].text, L"LDA #$41: a fill a byte");
            Assert::AreNotEqual (0u, window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kHeatColumn].background, L"run");
            Assert::AreNotEqual (window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kHeatColumn].background,
                                 window.GetCodeList (0)->GetCellsOfRow (1)[DisassemblyCells::kHeatColumn].background, L"RTS: written, in another color");

            window.GetCodeGroup().ToggleOption (Option::Heat);
            Assert::IsFalse  (window.GetCodeList (0)->IsColumnVisible (DisassemblyCells::kHeatColumn), L"the column goes; the map is on for the memory windows' heat");
        }



        TEST_METHOD (SourceIsOnlyOfferedWithADebugFile)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (false), 0);

            Assert::IsFalse  (window.GetCodeGroup().IsOptionEnabled (Option::Source));
            Assert::IsFalse  (window.GetCodeGroup().IsOptionEnabled (Option::LineNumbers));
            Assert::AreEqual (3, window.GetCodeList (0)->GetRowCount(), L"no debug file, no source rows");

            window.GetCodeGroup().ToggleOption (Option::Source);
            Assert::IsTrue   (window.GetCodeGroup().GetOptions().IsOn (Option::Source), L"a disabled option does not switch");
        }



        TEST_METHOD (TheSourceLineSitsAboveItsCodeWithItsNumber)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);
            std::wstring   text;



            scaler.SetDpi (96);
            host.options = "address bytes source symbols";

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (true), 0);

            Assert::AreEqual (5,  window.GetCodeList (0)->GetRowCount());
            Assert::AreEqual (-1, window.GetCodeLineOfRow (0, 0), L"the first row is source");
            Assert::AreEqual (0,  window.GetCodeLineOfRow (0, 1));

            //  The line has no label, so its opcode starts in the instruction
            //  column, aligned with the disassembly's.
            for (const DxuiListView::Cell & cell : window.GetCodeList (0)->GetCellsOfRow (0))
            {
                text += cell.text;
            }

            Assert::AreEqual (std::wstring (L"lda #$41"), text);
            Assert::AreEqual (std::wstring (L"lda #$41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text);

            window.GetCodeGroup().ToggleOption (Option::LineNumbers);

            Assert::AreEqual (std::wstring (L"2"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kAddressColumn].text, L"the line number is in the address column");
            Assert::AreEqual (std::wstring (L"lda #$41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text);
        }



        //  A source not found is looked up again at the next snapshot, and a
        //  debug file loaded again at the same path with another program key
        //  has its source read again.
        TEST_METHOD (TheSourceRowsFollowASourceFoundLaterAndANewProgram)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);
            auto           first = MakeSnapshot (true);
            auto           again = std::make_shared<DebuggerViewSnapshot> (*first);
            auto           other = std::make_shared<DebuggerViewSnapshot> (*first);



            scaler.SetDpi (96);
            host.options    = "address bytes source symbols";
            host.sourceText = std::string();

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (first, 0);
            Assert::AreEqual (0, window.GetCodeLineOfRow (0, 0), L"no source found, no source row");

            host.sourceText = "; main\n        lda #$41\n        rts\n";
            window.ApplyCodeSnapshot (again, 0);
            Assert::AreEqual (-1, window.GetCodeLineOfRow (0, 0), L"found at the next snapshot");
            Assert::AreEqual (std::wstring (L"lda #$41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text);

            host.sourceText          = "; main\n        ldx #$41\n        rts\n";
            other->source->programKey = "reassembled";
            window.ApplyCodeSnapshot (other, 0);
            Assert::AreEqual (std::wstring (L"ldx #$41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text, L"the new program's source");
        }



        //  A source line's label is in the label column and its opcode and
        //  operand start in the instruction column, so the branch arrow, which
        //  is drawn left of that column, is never over the source's text.
        TEST_METHOD (ASourceLinesLabelAndOpcodeAreInTheirColumns)
        {
            DxuiListView                     list;
            std::vector<DxuiListView::Cell>  cells (DisassemblyCells::kColumnCount);
            DebuggerTextColors::Set          colors = DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);



            DisassemblyCells::Configure     (list);
            DisassemblyCells::FillSourceRow (cells, list, 11, L"LOOP    LDA (PTR),Y", true, colors, 0xFF808080);

            Assert::AreEqual (std::wstring (L"11"),          cells[DisassemblyCells::kAddressColumn].text);
            Assert::AreEqual (std::wstring (L"LOOP"),        cells[DisassemblyCells::kLabelColumn].text);
            Assert::AreEqual (std::wstring (L"LDA (PTR),Y"), cells[DisassemblyCells::kInstructionColumn].text);
            Assert::IsTrue   (cells[DisassemblyCells::kInstructionColumn].spansRow, L"the rest of the line runs to the row's end");
        }


        //  A source row represents the first instruction its line produced:
        //  a press in its gutter acts on that instruction, the instruction's
        //  row is inert under it, and only a line with one instruction in
        //  view has the instruction's operand and result.
        TEST_METHOD (ASourceRowRepresentsItsLinesFirstInstruction)
        {
            std::vector<DebuggerViewSnapshot::CodeLine>  lines (4);
            std::vector<DisassemblyOptions::Row>         rows;
            auto                                         always = [] (int, int) { return true; };



            lines[0] = { .address = 0x6000, .bytes = "A9 41" };
            lines[1] = { .address = 0x6002, .bytes = "20 ED FD" };
            lines[2] = { .address = 0x6005, .bytes = "60" };
            lines[3] = { .address = 0x6006, .bytes = "00" };

            for (size_t i = 0; i < 3; i++)
            {
                lines[i].sourceFileId = 0;
                lines[i].sourceLine   = (i < 2) ? 4 : 5;
            }

            //  Rows: line 4, $6000, $6002, line 5, $6005, $6006.
            rows = DisassemblyOptions::BuildRows (lines, true, always);

            Assert::AreEqual (0,  DisassemblyOptions::GetFrontedLine (rows, 0));
            Assert::AreEqual (-1, DisassemblyOptions::GetFrontedLine (rows, 1), L"an instruction's row");
            Assert::IsTrue   (DisassemblyOptions::IsFronted (rows, 1));
            Assert::IsFalse  (DisassemblyOptions::IsFronted (rows, 2), L"the second instruction of line 4");
            Assert::IsFalse  (DisassemblyOptions::IsFronted (rows, 5), L"the byte with no line");

            Assert::AreEqual (0, DisassemblyOptions::GetFrontRowOfLine (rows, 0));
            Assert::AreEqual (2, DisassemblyOptions::GetFrontRowOfLine (rows, 1));
            Assert::AreEqual (3, DisassemblyOptions::GetFrontRowOfLine (rows, 2));
            Assert::AreEqual (5, DisassemblyOptions::GetFrontRowOfLine (rows, 3));

            Assert::AreEqual (0,  DisassemblyOptions::GetGutterLineOfRow (rows, 0));
            Assert::AreEqual (-1, DisassemblyOptions::GetGutterLineOfRow (rows, 1), L"inert under its source row");
            Assert::AreEqual (1,  DisassemblyOptions::GetGutterLineOfRow (rows, 2));
            Assert::AreEqual (2,  DisassemblyOptions::GetGutterLineOfRow (rows, 3));
            Assert::AreEqual (-1, DisassemblyOptions::GetGutterLineOfRow (rows, 4));
            Assert::AreEqual (3,  DisassemblyOptions::GetGutterLineOfRow (rows, 5));

            Assert::IsFalse (DisassemblyOptions::IsSingleInstruction (rows, 0), L"line 4 produced two");
            Assert::IsTrue  (DisassemblyOptions::IsSingleInstruction (rows, 3));
            Assert::IsFalse (DisassemblyOptions::IsSingleInstruction (rows, 4), L"not a source row");

            lines[1].isCurrent = true;
            Assert::AreEqual (-1, DisassemblyGroup::FindPcSourceRow (lines, rows), L"the PC past a line's first instruction is marked on the instruction's row");

            lines[1].isCurrent = false;
            lines[0].isCurrent = true;
            Assert::AreEqual (0, DisassemblyGroup::FindPcSourceRow (lines, rows));
        }



        //  With source on, the source row has its instruction's breakpoint,
        //  PC marker, row color, operand and result, and the instruction's
        //  row under it doesn't have them.
        TEST_METHOD (TheSourceRowHasItsInstructionsMarks)
        {
            CassoTheme                              theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                             host;
            DxuiDpiScaler                           scaler;
            OptionsWindow                           window (theme, host);
            auto                                    snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot (true));
            DxuiListView                          * list     = nullptr;



            snapshot->codeViews[0][0].hasBreakpoint = true;
            snapshot->codeViews[0][0].isEnabled     = true;
            snapshot->codeViews[0][0].effect        = "A=$41";
            snapshot->code                          = snapshot->codeViews[0];

            scaler.SetDpi (96);
            host.options = "address bytes source symbols";

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            list = window.GetCodeList (0);

            Assert::IsNotNull (list->GetCellsOfRow (0)[DisassemblyCells::kGutterColumn].icon.get(), L"the PC's marker over the breakpoint");
            Assert::IsNull    (list->GetCellsOfRow (1)[DisassemblyCells::kGutterColumn].icon.get(), L"none on the instruction's row");
            Assert::AreNotEqual (0u, list->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].background, L"the PC's row color");
            Assert::AreEqual  (0u, list->GetCellsOfRow (1)[DisassemblyCells::kInstructionColumn].background);

            Assert::AreEqual (std::wstring (L"lda #$41"),      list->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text);
            Assert::AreEqual (std::wstring (L"Result: A=$41"), list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text);
            Assert::AreEqual (std::wstring(),                  list->GetCellsOfRow (1)[DisassemblyCells::kOperandColumn].text);

            Assert::AreEqual (0,  window.GetCodeGroup().GetGutterLineOfRow (0, 0));
            Assert::AreEqual (-1, window.GetCodeGroup().GetGutterLineOfRow (0, 1));
        }



        //  The gray breakpoint goes on the source row under the pointer, and
        //  the instruction's row under it doesn't get one.
        TEST_METHOD (TheGutterShowsAGrayBreakpointOnTheSourceRow)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            OptionsHost      host;
            DxuiDpiScaler    scaler;
            OptionsWindow    window (theme, host);
            DxuiListView   * list   = nullptr;
            RECT             box    = {};



            scaler.SetDpi (96);
            host.options = "address bytes source symbols";

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (true), 0);

            list = window.GetCodeList (0);
            box  = list->GetBounds();

            window.GetCodeGutter().Hover (POINT { box.left + 2, GetRowMiddle (*list, 2) });
            Assert::IsNotNull (list->GetCellsOfRow (2).front().icon.get(), L"the gray dot is on the RTS line");
            Assert::IsNull    (list->GetCellsOfRow (3).front().icon.get());

            window.GetCodeGutter().Hover (POINT { box.left + 2, GetRowMiddle (*list, 3) });
            Assert::IsNull (list->GetCellsOfRow (2).front().icon.get(), L"none over the RTS instruction's row");
            Assert::IsNull (list->GetCellsOfRow (3).front().icon.get());
        }



        //  With source on, the PC's branch arrow runs from the branch's source
        //  row to the target's.
        TEST_METHOD (TheBranchArrowRunsBetweenSourceRows)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot (true));
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            BranchArrow::Input              input;
            Word                            goesTo   = 0;
            bool                            isTaken  = false;
            DxuiListView                  * list     = nullptr;
            float                           half     = 0.0f;



            snapshot->codeViews[0][0].bytes         = "D0 00";
            snapshot->codeViews[0][0].instruction   = "BNE $0302";
            snapshot->codeViews[0][0].target        = Word (0x0302);
            snapshot->codeViews[0][0].isTargetTaken = true;
            snapshot->code                          = snapshot->codeViews[0];

            scaler.SetDpi (96);
            host.options    = "address bytes source symbols";
            host.sourceText = "; main\n        bne done\ndone    rts\n";

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            list = window.GetCodeList (0);
            half = (float) list->GetRowHeightDip() / 2.0f;
            list->Paint (painter, text, theme);

            Assert::IsTrue (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken));
            Assert::IsTrue (input.sourceY.has_value() && input.targetY.has_value());
            Assert::IsTrue (std::abs (*input.sourceY - (float) GetRowMiddle (*list, 0)) < half, L"from the BNE's source row");
            Assert::IsTrue (std::abs (*input.targetY - (float) GetRowMiddle (*list, 2)) < half, L"to the RTS's source row");
        }



        //  With an operand cell, the code stops at the comment, the operand
        //  is in its column, and the comment follows it.
        TEST_METHOD (ASourceRowsOperandIsInItsColumnBeforeTheComment)
        {
            DxuiListView                     list;
            std::vector<DxuiListView::Cell>  cells (DisassemblyCells::kColumnCount);
            DebuggerTextColors::Set          colors = DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);
            DxuiListView::Cell               operand;



            operand.text = L"=$1234";
            operand.argb = 0xFF0000FF;

            DisassemblyCells::Configure     (list);
            DisassemblyCells::FillSourceRow (cells, list, 11, L"LOOP    LDA (PTR),Y   ; next byte", false, colors, 0xFF808080, &operand);

            Assert::AreEqual (std::wstring (L"LOOP"),                cells[DisassemblyCells::kLabelColumn].text);
            Assert::AreEqual (std::wstring (L"LDA (PTR),Y"),         cells[DisassemblyCells::kInstructionColumn].text);
            Assert::IsFalse  (cells[DisassemblyCells::kInstructionColumn].spansRow, L"the code is fitted like an instruction");
            Assert::AreEqual (std::wstring (L"=$1234  ; next byte"), cells[DisassemblyCells::kOperandColumn].text);
            Assert::IsTrue   (cells[DisassemblyCells::kOperandColumn].spansRow, L"the comment runs to the row's end");

            //  An instruction column dragged narrower than the code: the line
            //  runs on across the row with the operand after it.
            cells.assign (DisassemblyCells::kColumnCount, DxuiListView::Cell());
            list.SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, list.GetFittedWidthPx (4));
            DisassemblyCells::FillSourceRow (cells, list, 11, L"LOOP    LDA (PTR),Y   ; next byte", false, colors, 0xFF808080, &operand);

            Assert::AreEqual (std::wstring (L"LDA (PTR),Y   ; next byte  =$1234"), cells[DisassemblyCells::kInstructionColumn].text);
            Assert::IsTrue   (cells[DisassemblyCells::kInstructionColumn].spansRow);
            Assert::AreEqual (std::wstring(), cells[DisassemblyCells::kOperandColumn].text);
        }


        TEST_METHOD (TheGutterShowsAGrayBreakpointOnlyOnAnInstructionUnderThePointer)
        {
            CassoTheme                             theme  = CassoTheme::MakeSkeuomorphic();
            OptionsHost                            host;
            DxuiDpiScaler                          scaler;
            OptionsWindow                          window (theme, host);
            DxuiListView                         * list   = nullptr;
            RECT                                   box    = {};
            std::shared_ptr<const DxuiIconImage>   arrow;



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (false), 0);

            list  = window.GetCodeList (0);
            box   = list->GetBounds();
            arrow = list->GetCellsOfRow (0).front().icon;

            Assert::IsNull    (list->GetCellsOfRow (1).front().icon.get(), L"nothing before the pointer comes");
            Assert::IsNotNull (arrow.get(), L"but the PC's arrow, in the same margin");

            window.GetCodeGutter().Hover (POINT { box.left + 2, GetRowMiddle (*list, 1) });
            Assert::IsNotNull (list->GetCellsOfRow (1).front().icon.get(), L"the RTS row shows the gray dot");
            Assert::IsTrue    (list->GetCellsOfRow (0).front().icon == arrow, L"the PC's line keeps its arrow alone");

            window.GetCodeGutter().Hover (POINT { box.left + 2, GetRowMiddle (*list, 2) });
            Assert::IsNull (list->GetCellsOfRow (2).front().icon.get(), L"a data byte cannot take a breakpoint");
            Assert::IsNull (list->GetCellsOfRow (1).front().icon.get(), L"the dot left the row the pointer left");

            window.GetCodeGutter().Hover (POINT { box.left + 200, GetRowMiddle (*list, 2) });
            Assert::IsNull (list->GetCellsOfRow (2).front().icon.get(), L"none away from the gutter");
        }



        TEST_METHOD (ThePreferenceRoundTrips)
        {
            InMemoryFileSystem  fs;
            GlobalUserPrefs     saved;
            GlobalUserPrefs     loaded;
            HRESULT             hr     = S_OK;



            saved.debugger.disassemblyOptions = "address source";

            hr = saved.Save (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = loaded.Load (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::AreEqual (std::string ("address source"), loaded.debugger.disassemblyOptions);
        }



        //  LDA ($3E),Y at $0300 with the PC on it, its annotation in parts: Y,
        //  the pointer, and the operand's address and value.
        static std::shared_ptr<const DebuggerViewSnapshot> MakeIndirectSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;



            lda.address                      = 0x0300;
            lda.bytes                        = "B1 3E";
            lda.instruction                  = "LDA ($3E),Y";
            lda.isCurrent                    = true;
            lda.annotationParts.registers    = "Y=05";
            lda.annotationParts.pointer      = "($003E)=$1230";
            lda.annotationParts.operandValue = "$1235=41";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            return snapshot;
        }



        TEST_METHOD (OperandValuesAndPointersChooseThePartsOfTheAnnotation)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeIndirectSnapshot(), 0);

            Assert::AreEqual (std::wstring (L"Y=05 ($003E)=$1230 $1235=41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text, L"the default: both");

            window.GetCodeGroup().ToggleOption (Option::Pointers);
            Assert::AreEqual (std::wstring (L"Y=05 $1235=41"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text);

            window.GetCodeGroup().ToggleOption (Option::OperandValues);
            Assert::AreEqual (std::wstring (L"Y=05"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text);
        }



        TEST_METHOD (AddressColonFollowsEachAddress)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (false), 0);

            Assert::AreEqual (std::wstring (L"0300"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kAddressColumn].text, L"the default: no colon");

            window.GetCodeGroup().ToggleOption (Option::AddressColon);
            Assert::AreEqual (std::wstring (L"0300:"), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kAddressColumn].text);
            Assert::AreEqual (std::wstring (L"0302:"), window.GetCodeList (0)->GetCellsOfRow (1)[DisassemblyCells::kAddressColumn].text);
        }



        TEST_METHOD (BranchArrowOffDrawsNoArrow)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DebuggerViewSnapshot::CodeLine  rts;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            BranchArrow::Input              input;
            Word                            goesTo   = 0;
            bool                            isTaken  = false;



            bne.address       = 0x0300;
            bne.bytes         = "D0 01";
            bne.instruction   = "BNE $0303";
            bne.isCurrent     = true;
            bne.target        = Word (0x0303);
            bne.isTargetTaken = true;

            rts.address       = 0x0303;
            rts.bytes         = "60";
            rts.instruction   = "RTS";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue  (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken), L"the default: the arrow is drawn");
            Assert::AreEqual ((int) 0x0303, (int) goesTo);

            window.GetCodeGroup().ToggleOption (Option::BranchArrow);
            Assert::IsFalse (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken));
        }


        //  After a step that took a branch, the branch's arrow runs from its
        //  line to the PC's.
        TEST_METHOD (AStepThroughATakenBranchDrawsItsArrowToThePc)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DebuggerViewSnapshot::CodeLine  rts;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            BranchArrow::Input              input;
            Word                            goesTo   = 0;
            bool                            isTaken  = false;



            bne.address       = 0x0300;
            bne.bytes         = "D0 01";
            bne.instruction   = "BNE $0303";
            bne.isBranchedFrom = true;
            bne.target        = Word (0x0303);

            rts.address       = 0x0303;
            rts.bytes         = "60";
            rts.instruction   = "RTS";
            rts.isCurrent     = true;

            snapshot->pc           = 0x0303;
            snapshot->branchedFrom  = Word (0x0300);
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue   (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken, true), L"the stepped branch's arrow");
            Assert::AreEqual ((int) 0x0303, (int) goesTo);
            Assert::IsTrue   (input.sourceY.has_value() && input.targetY.has_value() && *input.sourceY < *input.targetY);
            Assert::IsFalse  (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken), L"the PC's RTS has none");
        }


        //  A stepped branch whose line is not among the view's lines, above
        //  them, runs in from the top edge to the PC.
        TEST_METHOD (AStepFromABranchOutOfViewRunsInFromTheEdge)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  nop;
            DebuggerViewSnapshot::CodeLine  rts;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            BranchArrow::Input              input;
            Word                            goesTo   = 0;
            bool                            isTaken  = false;



            nop.address     = 0x0400;
            nop.bytes       = "EA";
            nop.instruction = "NOP";

            rts.address     = 0x0401;
            rts.bytes       = "60";
            rts.instruction = "RTS";
            rts.isCurrent   = true;

            snapshot->pc           = 0x0401;
            snapshot->branchedFrom  = Word (0x0300);
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { nop, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue   (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken, true), L"drawn though $0300 is not in view");
            Assert::AreEqual ((int) 0x0401, (int) goesTo);
            Assert::IsFalse  (input.sourceY.has_value(), L"no source row: it runs in from the edge");
            Assert::IsTrue   (input.isTargetBelow,        L"from above, $0300 being before the lines");
            Assert::IsTrue   (window.HasTopLayer());
        }

        //  The PC's arrow, with its line scrolled above the view's lines and its
        //  target below them, runs across the view from edge to edge; with
        //  both ends above the lines, none of it crosses them.
        TEST_METHOD (ThePcsArrowWithThePcOutOfViewRunsAcrossTheView)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  nop;
            DebuggerViewSnapshot::CodeLine  rts;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            BranchArrow::Input              input;
            Word                            goesTo   = 0;
            bool                            isTaken  = false;



            nop.address     = 0x0400;
            nop.bytes       = "EA";
            nop.instruction = "NOP";

            rts.address     = 0x0401;
            rts.bytes       = "60";
            rts.instruction = "RTS";

            snapshot->pc              = 0x0300;
            snapshot->pcTarget        = Word (0x0500);
            snapshot->isPcTargetTaken = true;
            snapshot->isPaused        = true;
            snapshot->codeViews[0]    = { nop, rts };
            snapshot->code            = snapshot->codeViews[0];
            snapshot->codeOpen[0]     = true;

            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue   (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken), L"drawn though the PC's line is not in view");
            Assert::AreEqual ((int) 0x0500, (int) goesTo);
            Assert::IsTrue   (isTaken);
            Assert::IsFalse  (input.sourceY.has_value(), L"it runs in from the top edge");
            Assert::IsFalse  (input.targetY.has_value(), L"and on to the bottom edge");
            Assert::IsTrue   (input.isTargetBelow);

            snapshot->pcTarget = Word (0x0200);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            input = BranchArrow::Input();
            Assert::IsFalse  (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken), L"from above the lines to above them");
        }


        //  The same through a real step: with the snapshot the view state
        //  builds after a taken BPL applied, the window draws the BPL's arrow.
        TEST_METHOD (ARealStepThroughABranchDrawsItsArrowInTheWindow)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            OptionsHost                     host;
            DxuiDpiScaler                   scaler;
            OptionsWindow                   window (theme, host);
            ControllerRig                          rig;
            Cpu6502Registers                       r        = rig.controller.GetSession().GetTarget().GetRegisters();
            MockDxuiPainter                        painter;
            MockDxuiTextRenderer                   text;
            BranchArrow::Input                     input;
            Word                                   goesTo   = 0;
            bool                                   isTaken  = false;
            std::shared_ptr<DebuggerViewSnapshot>  snapshot;



            rig.machine.GetMemoryBus().WriteByte (0x0300, 0xEA);    // NOP
            rig.machine.GetMemoryBus().WriteByte (0x0301, 0x10);    // BPL $0300
            rig.machine.GetMemoryBus().WriteByte (0x0302, 0xFD);

            r.pc = 0x0301;
            r.p  = 0x26;
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            (void) rig.view.Build (rig.controller.GetSession());
            rig.machine.StepOne();
            snapshot = std::make_shared<DebuggerViewSnapshot> (rig.view.Build (rig.controller.GetSession()));

            Assert::AreEqual ((int) 0x0300, (int) snapshot->pc);

            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue   (window.GetCodeGroup().GetBranchArrow (0, input, goesTo, isTaken, true), L"the BPL's arrow, after the step took it");
            Assert::AreEqual ((int) 0x0300, (int) goesTo);
            Assert::IsTrue   (window.HasTopLayer(), L"and the layer it is drawn on is painted, though the PC's NOP has no arrow");
        }


        //  The operand and result column fills the width after the columns
        //  ahead of it. A long annotation, even one from rows replaced since,
        //  is cut off at the pane's edge rather than widening the column past
        //  it, so the pane doesn't scroll sideways.
        TEST_METHOD (TheOperandColumnFitsThePaneWithoutScrolling)
        {
            constexpr LONG                                kPaneWidth = 700;
            CassoTheme                                    theme      = CassoTheme::MakeSkeuomorphic();
            DxuiDpiScaler                                 scaler;
            DxuiListView                                  list;
            MockDxuiPainter                               painter;
            MockDxuiTextRenderer                          text;
            std::vector<std::vector<DxuiListView::Cell>>  rows;
            int                                           right      = 0;



            scaler.SetDpi (96);
            DebuggerWindow::MakeDense   (&list);
            DisassemblyCells::Configure (list);
            list.Layout (RECT { 0, 0, kPaneWidth, 400 }, scaler);

            for (const std::wstring & operand : { std::wstring (200, L'X'), std::wstring (L"A=41") })
            {
                rows.assign (1, std::vector<DxuiListView::Cell> (DisassemblyCells::kColumnCount));

                rows[0][DisassemblyCells::kAddressColumn].text     = L"6000";
                rows[0][DisassemblyCells::kInstructionColumn].text = L"LDA $C000";
                rows[0][DisassemblyCells::kOperandColumn].text     = operand;

                list.SetRows (std::move (rows));
                list.Paint   (painter, text, theme);

                Assert::IsFalse  (list.IsHorzScrollbarVisible(), operand.c_str());
                Assert::AreEqual (0, list.GetMaxLeftPx(), operand.c_str());
            }

            right = list.GetPaneLeadPx();

            for (size_t c = 0; c < DisassemblyCells::kColumnCount; c++)
            {
                right += list.GetColumnEffectiveWidthPx (c);
            }

            Assert::AreEqual ((int) kPaneWidth, right, L"the last column runs to the pane's edge");
        }
    };
}
