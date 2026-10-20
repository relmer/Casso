#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Config/GlobalUserPrefs.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyOptions.h"
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

            lookup.match = SourceMatch::Exact;
            lookup.text  = "; main\n        lda #$41\n        rts\n";
            return lookup;
        }

        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::string  options;
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

            rows = DisassemblyOptions::BuildRows (snapshot->codeViews[0], false, always);
            Assert::AreEqual ((size_t) 3, rows.size(), L"source off lists the instructions alone");
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
            Assert::IsFalse  (host.isHeatShown);
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
    };
}
