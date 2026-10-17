#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/ToolPanes.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerToolPanesTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that adds the echo of every action the window runs to commands.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class QuietDebuggerHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { commands.push_back (line); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { commands.push_back (action.echo); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  goTos;
        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };






    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ToolWindow
    //
    //  The debugger window with its controls built and no HWND; the step
    //  filters dialog returns what the test put in `filters`.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ToolWindow : public DebuggerWindow
    {
    public:
        ToolWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::EditStepFilters;
        using DebuggerWindow::OnMappedCommand;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetProfilerCommands;

        std::optional<std::vector<std::wstring>> RunStepFiltersDialog (const std::vector<std::wstring> & shown,
                                                                       std::function<bool (const std::wstring &, std::wstring &)> check) override
        {
            std::wstring  error;



            opened   = shown;
            accepted = check (L"COUT", error);
            rejected = !check (L"NOSUCH", error);
            return filters;
        }

        std::vector<std::wstring>  opened;
        std::vector<std::wstring>  filters;
        bool                       accepted = false;
        bool                       rejected = false;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ToolPanesTests
    //
    //  The Symbols, Bookmarks, Profiler and data ranges panes, the bookmark
    //  keys and Debug > Step filters... (FR-182 to FR-184, FR-189, FR-193).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ToolPanesTests)
    {
    public:

        static std::shared_ptr<DebuggerViewSnapshot::SymbolsState> MakeSymbols()
        {
            auto  symbols = std::make_shared<DebuggerViewSnapshot::SymbolsState>();



            symbols->tables  = { { "main", 1, true, SymbolTableId::Main }, { "user", 2, false, SymbolTableId::User } };
            symbols->symbols = { { "COUT", 0xFDED, SymbolTableId::Main }, { "LOOP", 0x0300, SymbolTableId::User }, { "WIDTH", 0x0028, SymbolTableId::User, true } };
            return symbols;
        }



        TEST_METHOD (EachTableIsAHeadingOverItsSymbols)
        {
            auto                                          symbols = MakeSymbols();
            std::vector<SymbolsPane::Row>                 rows    = SymbolsPane::GetRows (*symbols);
            std::vector<std::vector<DxuiListView::Cell>>  cells   = SymbolsPane::GetCells (*symbols, rows);



            Assert::AreEqual ((size_t) 5, rows.size());
            Assert::IsTrue   (rows[0].isHeading && rows[2].isHeading);
            Assert::AreEqual (std::wstring (L"main (1)"), cells[0][0].text);
            Assert::IsTrue   (cells[0][0].check.value_or (false), L"main is on");
            Assert::IsFalse  (cells[2][0].check.value_or (true), L"user is off");
            Assert::AreEqual (std::wstring (L"$0300"), cells[3][1].text);
            Assert::AreEqual (std::wstring (L"0028 (constant)"), cells[4][1].text, L"a constant is not an address");
        }



        TEST_METHOD (TheNextBookmarkGoesRoundFromTheLast)
        {
            std::vector<DebuggerViewSnapshot::WatchLine>  bookmarks = { { 0, 0x0400, "", true }, { 1, 0x0300, "", true } };



            Assert::AreEqual ((Word) 0x0400, BookmarksPane::GetNext (bookmarks, 0x0300, false).value_or (0));
            Assert::AreEqual ((Word) 0x0300, BookmarksPane::GetNext (bookmarks, 0x0400, false).value_or (0), L"round to the first");
            Assert::AreEqual ((Word) 0x0400, BookmarksPane::GetNext (bookmarks, 0x0300, true).value_or (0),  L"round to the last");
            Assert::IsFalse  (BookmarksPane::GetNext ({}, 0x0300, false).has_value());
        }



        TEST_METHOD (TheProfilerAndTheDataRangesListTheirRows)
        {
            ProfileData                                   profile;
            std::vector<std::vector<DxuiListView::Cell>>  cells;



            cells = ProfilerPane::GetCells (std::nullopt, false, false);
            Assert::AreEqual ((size_t) 1, cells.size());
            Assert::IsTrue   (cells[0][0].text.starts_with (L"Profiling is off."), L"how to start, while off");
            Assert::IsTrue   (ProfilerPane::GetCells (std::nullopt, false, true)[0][0].text.starts_with (L"Pause"), L"pause to read it, while on");

            profile.isOn         = true;
            profile.instructions = 10;
            profile.opcodes      = { { "LDA", "#", 4, 8 } };
            profile.addresses    = { { 0x0300, "LOOP", 20 } };

            cells = ProfilerPane::GetCells (profile, false, true);
            Assert::AreEqual ((size_t) 2, cells.size());
            Assert::AreEqual (std::wstring (L"LDA"), cells[1][0].text);
            Assert::AreEqual (std::wstring (L"$0300"), ProfilerPane::GetCells (profile, true, true)[1][0].text);

            cells = DataBlocksPane::GetCells ({ { "TABLE", 0x0800, 0x080F, DataBlockKind::Words } });
            Assert::AreEqual (std::wstring (L"$0800-$080F"), cells[0][1].text);
            Assert::AreEqual (std::wstring (L"Words"), cells[0][2].text);
        }



        //  The dialog opens with the filters as SKIP writes them; one taken
        //  out is removed, and each new one is added by its range or by its
        //  symbol, which must be one.
        TEST_METHOD (StepFiltersAddAndRemoveWhatTheDialogChanged)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->symbols     = MakeSymbols();
            snapshot->stepFilters = { { "", 0xF800, 0xF8FF }, { "LOOP", 0x0300, 0x0300 } };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            window.filters = { L"LOOP", L"FC00.FCFF", L"COUT" };
            window.EditStepFilters();

            Assert::IsTrue (window.opened == std::vector<std::wstring> { L"F800.F8FF", L"LOOP" });
            Assert::IsTrue (window.accepted && window.rejected, L"a symbol is taken, another name is not");
            Assert::IsTrue (host.commands == std::vector<std::string> { "SKIP -F800.F8FF", "SKIP FC00.FCFF", "SKIP COUT" });
        }



        //  Profile on the bar turns profiling on, and off once the snapshot
        //  has it on; the drop-down's label follows the view.
        TEST_METHOD (TheProfilerBarTurnsProfilingOnAndOff)
        {
            CassoTheme                    theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost             host;
            ToolWindow                    window   (theme, host);
            DxuiDpiScaler                 scaler;
            auto                          snapshot = std::make_shared<DebuggerViewSnapshot>();
            std::shared_ptr<DxuiCommand>  profile;
            std::shared_ptr<DxuiCommand>  view;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            for (const std::shared_ptr<DxuiCommand> & command : window.GetProfilerCommands())
            {
                if (command->label == L"Profile")   { profile = command; }
                if (command->label == L"By opcode") { view    = command; }
            }

            Assert::IsNotNull (profile.get());
            Assert::IsNotNull (view.get());
            Assert::IsFalse   (profile->isChecked());

            profile->dispatch();
            Assert::IsTrue (host.commands == std::vector<std::string> { "PROFILE ON" });

            snapshot              = std::make_shared<DebuggerViewSnapshot>();
            snapshot->isProfiling = true;
            window.TakeSnapshot (snapshot);

            Assert::IsTrue (profile->isChecked());
            profile->dispatch();
            Assert::IsTrue   (host.commands == std::vector<std::string> { "PROFILE ON", "PROFILE OFF" });
            Assert::AreEqual (std::wstring (L"By opcode"), view->labelText());
        }



        TEST_METHOD (TheBookmarkKeyAddsAtTheCursorAndTheToolPanesStartClosed)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->pc = 0x0300;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            (void) window.OnMappedCommand ((int) DebuggerKeySchemes::Action::ToggleBookmark);
            Assert::IsTrue (host.commands == std::vector<std::string> { "BMA 0300" }, L"no line selected, so the PC's");

            for (const std::wstring & pane : DebuggerLayout::GetToolPaneIds())
            {
                Assert::IsTrue  (DebuggerLayout::MakeDefault().Contains (pane), pane.c_str());
                Assert::IsFalse (window.IsPaneShown (pane), pane.c_str());
            }
        }
    };
}
