#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Config/GlobalUserPrefs.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyOptions.h"

#include "CppUnitTest.h"

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
        using DebuggerWindow::GetCodeBar;
        using DebuggerWindow::ToggleCodeOption;
        using DebuggerWindow::IsCodeOptionEnabled;
        using DebuggerWindow::GetCodeOptions;
        using DebuggerWindow::GetCodeLineOfRow;
        using DebuggerWindow::HoverGutter;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DisassemblyOptionsTests
    //
    //  The disassembly views' viewing options, as Visual Studio's: address,
    //  code bytes, source, symbol names and line numbers, saved as a setting;
    //  source rows above their code; and the gray breakpoint the gutter shows
    //  under the pointer.
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
            Assert::IsFalse (options.IsOn (Option::LineNumbers));

            options.Toggle (Option::CodeBytes);
            options.Toggle (Option::LineNumbers);
            Assert::AreEqual (std::string ("address source symbols lines"), options.ToText());
            Assert::IsTrue   (DisassemblyOptions::FromText (options.ToText()) == options);

            for (int i = 0; i < DisassemblyOptions::kOptionCount; i++)
            {
                none.Set ((Option) i, false);
            }

            Assert::IsTrue (DisassemblyOptions::FromText (none.ToText()) == none, L"every option off survives the round trip");
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

            Assert::IsTrue  (window.GetCodeList (0)->IsColumnVisible (2), L"the saved options are read");
            Assert::IsTrue  (window.GetCodeList (0)->IsColumnVisible (3));

            window.ToggleCodeOption (Option::Addresses);

            Assert::IsFalse  (window.GetCodeList (0)->IsColumnVisible (2), L"no address column");
            Assert::AreEqual (std::string ("bytes symbols"), host.options, L"the choice is saved");
            Assert::IsNotNull (window.GetCodeBar (0), L"each view has its options bar");
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

            Assert::IsFalse  (window.IsCodeOptionEnabled (Option::Source));
            Assert::IsFalse  (window.IsCodeOptionEnabled (Option::LineNumbers));
            Assert::AreEqual (3, window.GetCodeList (0)->GetRowCount(), L"no debug file, no source rows");

            window.ToggleCodeOption (Option::Source);
            Assert::IsTrue   (window.GetCodeOptions().IsOn (Option::Source), L"a disabled option does not switch");
        }



        TEST_METHOD (TheSourceLineSitsAboveItsCodeWithItsNumber)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);
            std::wstring   text;



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (true), 0);

            Assert::AreEqual (5,  window.GetCodeList (0)->GetRowCount());
            Assert::AreEqual (-1, window.GetCodeLineOfRow (0, 0), L"the first row is source");
            Assert::AreEqual (0,  window.GetCodeLineOfRow (0, 1));

            for (const DxuiListView::Cell & cell : window.GetCodeList (0)->GetCellsOfRow (0))
            {
                text += cell.text;
            }

            Assert::AreEqual (std::wstring (L"        lda #$41"), text);

            window.ToggleCodeOption (Option::LineNumbers);
            text.clear();

            for (const DxuiListView::Cell & cell : window.GetCodeList (0)->GetCellsOfRow (0))
            {
                text += cell.text;
            }

            Assert::AreEqual (std::wstring (L"    2          lda #$41"), text, L"the line number leads");
        }



        TEST_METHOD (TheGutterShowsAGrayBreakpointOnlyOnAnInstructionUnderThePointer)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            OptionsHost    host;
            DxuiDpiScaler  scaler;
            OptionsWindow  window (theme, host);
            DxuiListView * list  = nullptr;
            RECT           box   = {};



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (false), 0);

            list = window.GetCodeList (0);
            box  = list->GetBounds();

            Assert::IsNull (list->GetCellsOfRow (1).front().icon.get(), L"nothing before the pointer comes");

            window.HoverGutter (POINT { box.left + 2, GetRowMiddle (*list, 1) });
            Assert::IsNotNull (list->GetCellsOfRow (1).front().icon.get(), L"the RTS row shows the gray dot");
            Assert::IsNull    (list->GetCellsOfRow (0).front().icon.get());

            window.HoverGutter (POINT { box.left + 2, GetRowMiddle (*list, 2) });
            Assert::IsNull (list->GetCellsOfRow (2).front().icon.get(), L"a data byte cannot take a breakpoint");
            Assert::IsNull (list->GetCellsOfRow (1).front().icon.get(), L"the dot left the row the pointer left");

            window.HoverGutter (POINT { box.left + 200, GetRowMiddle (*list, 2) });
            Assert::IsNull (list->GetCellsOfRow (2).front().icon.get(), L"none away from the gutter");
        }



        TEST_METHOD (ThePreferenceRoundTrips)
        {
            InMemoryFileSystem  fs;
            GlobalUserPrefs     saved;
            GlobalUserPrefs     loaded;
            HRESULT             hr     = S_OK;



            saved.debuggerDisassemblyOptions = "address source";

            hr = saved.Save (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = loaded.Load (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::AreEqual (std::string ("address source"), loaded.debuggerDisassemblyOptions);
        }
    };
}
