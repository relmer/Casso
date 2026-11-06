#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Core/TextEncoding.h"
#include "Devices/Acia6551.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Debugger/RoutineProfile.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/ProfilerPane.h"
#include "Ui/Debugger/Panes/CommGraph.h"
#include "Ui/Debugger/Panes/CommLogController.h"
#include "Ui/Debugger/Panes/CommLogPane.h"
#include "Ui/Debugger/Panes/CommSignalGraph.h"
#include "Ui/Debugger/Panes/DevicePanelController.h"
#include "Ui/Debugger/Panes/EventLogControllers.h"
#include "Ui/Debugger/Panes/InputLogPane.h"
#include "Ui/Debugger/Panes/IoLogPane.h"

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

        FakeHostDialogs &  GetFakeDialogs() { return m_dialogs; }

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
        using DebuggerWindow::GetProfilerPane;
        using DebuggerWindow::GetPaneHostForTest;
        using DebuggerWindow::ShowPane;

        //  The name typed in a text dialog, Add symbol's among them.
        std::optional<std::wstring> RunTextDialog (const std::wstring &, const std::wstring &, const std::wstring &, TextDialog::CheckFn check) override
        {
            std::wstring  error;



            return check (typed, error) ? std::optional<std::wstring> (typed) : std::nullopt;
        }

        std::wstring  typed;

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



        //  The user's tables are in one pane and the machine's in the other,
        //  each a heading over its symbols while it is open.
        TEST_METHOD (EachTableIsAHeadingOverItsSymbols)
        {
            auto                                          symbols = MakeSymbols();
            std::set<std::string>                         open    = { "main", "user" };
            std::vector<SymbolsPane::Row>                 user    = SymbolsPane::GetRows (*symbols, SymbolsPane::Kind::User,    open);
            std::vector<SymbolsPane::Row>                 machine = SymbolsPane::GetRows (*symbols, SymbolsPane::Kind::Machine, open);
            std::vector<std::vector<DxuiListView::Cell>>  cells   = SymbolsPane::GetCells (*symbols, user, open);



            Assert::AreEqual ((size_t) 3, user.size(), L"the user table's heading and symbols");
            Assert::AreEqual ((size_t) 2, machine.size(), L"main's heading and its symbol");
            Assert::AreEqual (std::string ("main"), machine[0].table);
            Assert::IsTrue   (user[0].isHeading);
            Assert::AreEqual (std::format (L"{} user (2)", s_kpszTriangleDown), cells[0][0].text);
            Assert::IsFalse  (cells[0][0].check.value_or (true), L"user is off");
            Assert::AreEqual (std::wstring (L"$0300"), cells[1][1].text);
            Assert::AreEqual (std::wstring (L"0028 (constant)"), cells[2][1].text, L"a constant is not an address");
        }


        //  A heading starts closed, with no symbols under it; a table loaded
        //  from a file has the file's name.
        TEST_METHOD (AHeadingStartsClosedAndHasItsFilesName)
        {
            auto                                          symbols = std::make_shared<DebuggerViewSnapshot::SymbolsState> (*MakeSymbols());
            std::vector<SymbolsPane::Row>                 rows;
            std::vector<std::vector<DxuiListView::Cell>>  cells;



            symbols->tables[1].origins = { "demo.dbg" };
            rows  = SymbolsPane::GetRows (*symbols, SymbolsPane::Kind::User, {});
            cells = SymbolsPane::GetCells (*symbols, rows, {});

            Assert::AreEqual ((size_t) 1, rows.size(), L"only the heading");
            Assert::AreEqual (std::format (L"{} demo.dbg (2)", s_kpszTriangleRight), cells[0][0].text);
        }



        //  AppleWin's spare tables are listed once something is in one; the
        //  User table always is.
        TEST_METHOD (AnEmptySpareTableIsNotListed)
        {
            auto                           symbols = std::make_shared<DebuggerViewSnapshot::SymbolsState> (*MakeSymbols());
            std::vector<SymbolsPane::Row>  rows;



            symbols->tables.push_back ({ "user2", 0, true, SymbolTableId::User2 });
            symbols->tables.push_back ({ "src",   0, true, SymbolTableId::Src   });
            symbols->tables.push_back ({ "src2",  3, true, SymbolTableId::Src2  });

            rows = SymbolsPane::GetRows (*symbols, SymbolsPane::Kind::User, {});

            Assert::AreEqual ((size_t) 2, rows.size(), L"user, and src2, which has symbols");
            Assert::AreEqual (std::string ("user"), rows[0].table);
            Assert::AreEqual (std::string ("src2"), rows[1].table);
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



            cells = ProfilerPane::GetCells (std::nullopt, ProfilerPane::View::Opcodes, false);
            Assert::AreEqual ((size_t) 1, cells.size());
            Assert::IsTrue   (cells[0][0].text.starts_with (L"Choose a period"), L"how to build one");
            Assert::IsTrue   (ProfilerPane::GetCells (std::nullopt, ProfilerPane::View::Opcodes, true)[0][0].text.starts_with (L"Pause"), L"pause to read it, while on");

            profile.isOn         = true;
            profile.instructions = 10;
            profile.cycles       = 2040968;
            profile.opcodes      = { { "LDA", "#", 4, 8 }, { "RTS", "[No Operand]", 1, 6 } };
            profile.addresses    = { { 0x0300, "LOOP", 20 } };

            //  The totals are a line of their own, with the span the cycles
            //  cover, and an opcode with no operand has a blank mode.
            cells = ProfilerPane::GetCells (profile, ProfilerPane::View::Opcodes, true);
            Assert::AreEqual ((size_t) 2, cells.size());
            Assert::AreEqual (std::wstring (L"LDA"), cells[0][0].text);
            Assert::AreEqual (std::wstring(), cells[1][1].text);
            Assert::AreEqual (std::wstring (L"$0300"), ProfilerPane::GetCells (profile, ProfilerPane::View::Addresses, true)[0][0].text);
            Assert::IsTrue   (ProfilerPane::GetTotalsText (profile).starts_with (L"2.0 s of running: 10 instructions"), ProfilerPane::GetTotalsText (profile).c_str());
            Assert::AreEqual (std::wstring(), ProfilerPane::GetTotalsText (std::nullopt));

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



        //  A choice in the Show profile drop-down runs PROFILE HISTORY for its
        //  period; the drop-down reads the period, and the progress bar is up while
        //  the snapshot has the build under way. The view drop-down's label
        //  follows the view.
        //  Open on a stopped machine with history, the pane builds the chosen
        //  period's profile at a stop, with no period chosen again. A step, or
        //  steps that add up to less than a frame, don't build it again.
        TEST_METHOD (TheOpenPaneBuildsTheProfileAtEachStop)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = std::make_shared<DebuggerViewSnapshot>();



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ShowPane (DebuggerLayout::kProfiler);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            snapshot->isPaused           = true;
            snapshot->history.hasHistory = true;
            snapshot->history.cycle      = 100;
            window.TakeSnapshot (snapshot);
            Assert::IsTrue (host.commands == std::vector<std::string> { "PROFILE HISTORY 5" }, L"the default period, the last 5 seconds, built at the stop");

            window.TakeSnapshot (snapshot);
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"once for the stop");

            snapshot                = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            snapshot->history.cycle = 200;
            window.TakeSnapshot (snapshot);
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"not again after a step");

            snapshot                = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            snapshot->history.cycle = 100 + ProfilerPane::kRebuildCycles;
            window.TakeSnapshot (snapshot);
            Assert::AreEqual ((size_t) 2, host.commands.size(), L"and again after a frame's run");

            snapshot                = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            snapshot->history.cycle = 100;
            window.TakeSnapshot (snapshot);
            Assert::AreEqual ((size_t) 3, host.commands.size(), L"and after a frame's step back");
        }



        TEST_METHOD (TheProfilerPeriodDefaultsToFiveSecondsAndReadsTheSetting)
        {
            Assert::AreEqual ((size_t) 0, ProfilerPane::GetPeriodIndex (""),    L"no setting is the last 5 seconds");
            Assert::AreEqual ((size_t) 0, ProfilerPane::GetPeriodIndex ("7"),   L"an unknown setting is the last 5 seconds");
            Assert::AreEqual ("HISTORY 5", ProfilerPane::GetPeriods()[0].words);
            Assert::AreEqual ((size_t) 2, ProfilerPane::GetPeriodIndex ("60"));
            Assert::AreEqual ("HISTORY ALL", ProfilerPane::GetPeriods()[ProfilerPane::GetPeriodIndex ("ALL")].words);
        }



        TEST_METHOD (ShowProfileBuildsTheProfileOfAPeriodFromHistory)
        {
            CassoTheme                    theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost             host;
            ToolWindow                    window   (theme, host);
            DxuiDpiScaler                 scaler;
            auto                          snapshot = std::make_shared<DebuggerViewSnapshot>();
            std::shared_ptr<DxuiCommand>  period;
            std::shared_ptr<DxuiCommand>  view;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            for (const std::shared_ptr<DxuiCommand> & command : window.GetProfilerPane().GetCommands())
            {
                if (command->label == L"Last 5 s")  { period = command; }
                if (command->label == L"Routines")  { view   = command; }
                Assert::AreNotEqual (std::wstring (L"Profile"), command->label, L"a Profile switch");
            }

            Assert::IsNotNull (period.get());
            Assert::IsNotNull (view.get(), L"the pane opens on Routines");
            Assert::IsTrue    (window.GetProfilerPane().GetView() == ProfilerPane::View::Routines);

            window.GetProfilerPane().ShowPeriod (1);
            Assert::IsTrue   (host.commands == std::vector<std::string> { "PROFILE HISTORY 10" });
            Assert::AreEqual (std::wstring (L"Last 10 s"), period->labelText());

            snapshot                  = std::make_shared<DebuggerViewSnapshot>();
            snapshot->profileProgress = 0.5;
            window.TakeSnapshot (snapshot);
            Assert::IsTrue (window.GetProfilerPane().IsBuilding(), L"no progress bar while it builds");

            snapshot = std::make_shared<DebuggerViewSnapshot>();
            window.TakeSnapshot (snapshot);
            Assert::IsFalse (window.GetProfilerPane().IsBuilding(), L"the progress bar outlasted the build");

            window.GetProfilerPane().SetView (ProfilerPane::View::Tree);
            Assert::AreEqual (std::wstring (L"Call tree"), view->labelText());
        }


        //  The wheel over the timeline zooms it, and once the wheel stops the
        //  view's profile is built from history; a drag selects frames, whose
        //  profile is the list's. Back and forward move through the states
        //  with no build, and a profile already taken goes nowhere.
        TEST_METHOD (TheProfilerZoomsAndSelectsWithinAProfileFromHistory)
        {
            constexpr int64_t   kFrame   = (int64_t) RoutineProfile::kCyclesPerFrame;
            CassoTheme          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            ToolWindow          window   (theme, host);
            DxuiDpiScaler       scaler;
            auto                snapshot = std::make_shared<DebuggerViewSnapshot>();
            IDxuiControl      * timeline = nullptr;
            DxuiMouseEvent      ev;
            RECT                bounds   = {};
            std::string         command;
            uint64_t            from     = 0;
            uint64_t            to       = 0;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ShowPane (DebuggerLayout::kProfiler);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            ProfilerPane & pane = window.GetProfilerPane();

            for (IDxuiControl * control : pane.GetControls())
            {
                if (control != nullptr && control->GetAccessibleName() == L"Profile timeline")
                {
                    timeline = control;
                }
            }

            Assert::IsNotNull (timeline);
            bounds = timeline->GetBounds();

            //  The period's profile, from history.
            snapshot->isPaused                   = true;
            snapshot->history.hasHistory         = true;
            snapshot->history.cycle              = 600 * kFrame;
            snapshot->profile                    = ProfileData();
            snapshot->profile->historyRange      = CycleRange (0, 600 * kFrame);
            snapshot->profile->historyBuild      = 1;
            window.TakeSnapshot (snapshot);
            host.commands.clear();

            ev.kind        = DxuiMouseEventKind::Wheel;
            ev.positionDip = POINT { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };
            ev.wheelDelta  = 1.0f;
            Assert::IsTrue  (pane.RouteEarlyMouse (ev), L"the wheel over the timeline, not the text size");
            Assert::IsTrue  (pane.GetZoom().GetState().view.has_value(), L"zoomed in");
            Assert::IsTrue  (host.commands.empty(), L"no build while the wheel turns");
            Assert::IsFalse (pane.Tick ((int64_t) GetTickCount64() + 1000));

            Assert::AreEqual ((size_t) 1, host.commands.size(), L"the view's profile, once the wheel stopped");
            command = host.commands[0];
            Assert::IsTrue (sscanf_s (command.c_str(), "PROFILE HISTORY CYCLES %llu %llu", &from, &to) == 2, TextEncoding::NarrowToWide (command).c_str());
            Assert::IsTrue (CycleRange (from, to) == CycleRange (pane.GetZoom().GetState().view->first, pane.GetZoom().GetState().view->second));

            snapshot                        = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            snapshot->profile->historyRange = CycleRange (from, to);
            snapshot->profile->historyBuild = 2;
            window.TakeSnapshot (snapshot);
            Assert::IsTrue (pane.GetProfile()->historyRange == CycleRange (from, to), L"the zoomed view's profile is the list's");

            //  A drag selects frames.
            ev             = DxuiMouseEvent();
            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { bounds.left + 20, bounds.top + 2 };
            Assert::IsTrue (pane.RouteContentMouse (ev));
            ev.kind          = DxuiMouseEventKind::Move;
            ev.positionDip.x = bounds.left + 120;
            Assert::IsTrue (pane.RouteContentMouse (ev));
            ev.kind = DxuiMouseEventKind::Up;
            Assert::IsTrue (pane.RouteContentMouse (ev));
            Assert::IsTrue (pane.GetZoom().GetState().selection.has_value(), L"the drag's frames are selected");
            Assert::AreEqual ((size_t) 2, host.commands.size(), L"the selection's profile is built");

            snapshot                        = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            snapshot->profile->historyRange = CycleRange (pane.GetZoom().GetState().selection->first, pane.GetZoom().GetState().selection->second);
            snapshot->profile->historyBuild = 3;
            window.TakeSnapshot (snapshot);
            Assert::IsTrue (pane.GetProfile()->historyRange == snapshot->profile->historyRange, L"the selection's profile is the list's");

            //  A move back or forward doesn't start a build.
            Assert::IsTrue (pane.GoBack());
            Assert::IsFalse (pane.GetZoom().GetState().selection.has_value());
            Assert::IsTrue (pane.GetProfile()->historyRange == CycleRange (from, to), L"the view's profile, as it was built");
            Assert::IsTrue (pane.GoBack());
            Assert::IsTrue (pane.GetProfile()->historyRange == CycleRange (0, 600 * kFrame), L"the whole profile");
            Assert::IsTrue (pane.GoForward() && pane.GoForward());
            Assert::IsTrue (pane.GetZoom().GetState().selection.has_value());
            Assert::AreEqual ((size_t) 2, host.commands.size(), L"no build to move back and forward");

            //  The selection's build, in the next snapshot, is taken once.
            snapshot = std::make_shared<DebuggerViewSnapshot> (*snapshot);
            window.TakeSnapshot (snapshot);
            Assert::IsTrue (pane.GetZoom().CanGoBack(), L"the stack is the same after the same build again");
        }


        //  The view drop-down lists the views most used first.
        TEST_METHOD (TheProfilerViewsAreListedMostUsedFirst)
        {
            std::vector<std::wstring>  labels;
            int                        view   = 0;



            for (view = 0; view < ProfilerPane::kViewCount; view++)
            {
                labels.push_back (ProfilerPane::GetViewLabel ((ProfilerPane::View) view));
            }

            Assert::IsTrue (labels == std::vector<std::wstring> { L"Routines", L"Call tree", L"Flame graph", L"Addresses", L"Opcodes" });
        }



        //  A profile of Top level calling DRAW, which calls PLOT twice as hot
        //  as FILL.
        static ProfileData MakeRoutineProfile()
        {
            ProfileData  profile;



            profile.isOn         = true;
            profile.instructions = 50;
            profile.cycles       = 100;
            profile.routines     = { { 0x6000, RoutineKind::Call, "PLOT", 40, 40, 8 }, { 0x6100, RoutineKind::Call, "FILL", 20, 20, 2 },
                                     { 0, RoutineKind::Top, "", 30, 100, 0 }, { 0x6200, RoutineKind::Call, "DRAW", 10, 70, 1 } };
            profile.tree         = { { 2, 0, 30, 100, 0 }, { 3, 1, 10, 70, 1 }, { 0, 2, 40, 40, 8 }, { 1, 2, 20, 20, 2 } };
            profile.frames       = { { 9, { { 2, 1000 }, { 0, 3000 } }, 30 } };
            return profile;
        }



        TEST_METHOD (TheRoutinesAndTheTreeListEachRoutineAndGoToIt)
        {
            ProfileData                                   profile = MakeRoutineProfile();
            std::vector<std::vector<DxuiListView::Cell>>  cells;



            cells = ProfilerPane::GetCells (profile, ProfilerPane::View::Routines, true);
            Assert::AreEqual ((size_t) 4, cells.size());
            Assert::AreEqual (std::wstring (L"PLOT"),  cells[0][0].text);
            Assert::AreEqual (std::wstring (L"40.0%"), cells[0][2].text);
            Assert::AreEqual (std::wstring (L"5"),     cells[0][6].text, L"40 cycles over 8 calls");
            Assert::AreEqual (std::wstring (L"-"),     cells[2][6].text, L"Top level is never called");
            Assert::AreEqual (FlameGraphView::GetRoutineColor (0), cells[0][0].swatch, L"PLOT's swatch is its timeline color");
            Assert::AreEqual (FlameGraphView::GetRoutineColor (2), cells[2][0].swatch, L"and Top level's");

            cells = ProfilerPane::GetCells (profile, ProfilerPane::View::Tree, true);
            Assert::AreEqual (std::wstring (L"Top level"),  cells[0][0].text);
            Assert::AreEqual (std::wstring (L"    PLOT"),   cells[2][0].text, L"two levels in");
            Assert::AreEqual (std::wstring (L"70.0%"),      cells[1][3].text);
            Assert::AreEqual (FlameGraphView::GetRoutineColor (2), cells[0][0].swatch, L"each node in its routine's color");
            Assert::AreEqual (FlameGraphView::GetRoutineColor (0), cells[2][0].swatch);

            Assert::AreEqual ((Word) 0x6000, ProfilerPane::GetRowAddress (profile, ProfilerPane::View::Routines, 0).value_or (0));
            Assert::AreEqual ((Word) 0x6200, ProfilerPane::GetRowAddress (profile, ProfilerPane::View::Tree, 1).value_or (0));
            Assert::IsFalse  (ProfilerPane::GetRowAddress (profile, ProfilerPane::View::Tree, 0).has_value(), L"Top level has no code to go to");
            Assert::IsFalse  (ProfilerPane::GetRowAddress (profile, ProfilerPane::View::Routines, 9).has_value(), L"past the last row");
        }



        //  Each bar is as wide as its total's share of its caller's; zoomed,
        //  the routine spans the width under its callers.
        //  Each view opens sorted the way it is most read: the routines by
        //  self, the call tree by total, most first; any column sorts both
        //  ways, by number or by name.
        TEST_METHOD (TheProfilerListSortsByAnyColumn)
        {
            using View = ProfilerPane::View;

            ProfileData  profile = MakeRoutineProfile();
            auto         sort    = ProfilerPane::GetDefaultSort (View::Routines);



            Assert::AreEqual (1, sort.column, L"Self");
            Assert::IsTrue   (sort.descending);
            Assert::AreEqual (2, ProfilerPane::GetDefaultSort (View::Tree).column, L"the call tree by Total");
            Assert::IsTrue   (ProfilerPane::GetOrder (profile, View::Routines, 3, true)  == std::vector<size_t> { 2, 3, 0, 1 }, L"Top level, DRAW, PLOT, FILL");
            Assert::IsTrue   (ProfilerPane::GetOrder (profile, View::Routines, 1, true)  == std::vector<size_t> { 0, 2, 1, 3 }, L"by self");
            Assert::IsTrue   (ProfilerPane::GetOrder (profile, View::Routines, 1, false) == std::vector<size_t> { 3, 1, 2, 0 }, L"by self, least first");
            Assert::IsTrue   (ProfilerPane::GetOrder (profile, View::Routines, 0, false) == std::vector<size_t> { 3, 1, 0, 2 }, L"by name, ignoring case");

            Assert::AreEqual (3, ProfilerPane::GetDefaultSort (View::Opcodes).column,   L"Cycles");
            Assert::AreEqual (2, ProfilerPane::GetDefaultSort (View::Addresses).column, L"Cycles");
        }


        //  The call tree sorts each routine's callees among themselves, so
        //  every routine stays under its caller.
        TEST_METHOD (TheCallTreeSortsCalleesUnderTheirCaller)
        {
            using View = ProfilerPane::View;

            ProfileData  profile = MakeRoutineProfile();



            Assert::IsTrue (ProfilerPane::GetOrder (profile, View::Tree, 2, true)  == std::vector<size_t> { 0, 1, 2, 3 }, L"by total: PLOT before FILL");
            Assert::IsTrue (ProfilerPane::GetOrder (profile, View::Tree, 1, false) == std::vector<size_t> { 0, 1, 3, 2 }, L"by self, least first: FILL before PLOT, both under DRAW");
        }


        //  A routine's row has Add symbol here..., which adds a symbol at the
        //  routine's address; Top level has no address, so its row doesn't
        //  have the item.
        TEST_METHOD (AProfilerRowsMenuAddsASymbolAtItsRoutine)
        {
            CassoTheme                     theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost              host;
            ToolWindow                     window   (theme, host);
            DxuiDpiScaler                  scaler;
            auto                           snapshot = std::make_shared<DebuggerViewSnapshot>();
            PaneMenuItems                  top;
            PaneMenuItems                  plot;
            const std::function<void()>  * add      = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            snapshot->profile = MakeRoutineProfile();
            window.TakeSnapshot (snapshot);

            //  Sorted by self: PLOT, then Top level.
            window.GetProfilerPane().AddMenuItems (window.GetProfilerPane().GetList(), 1, top);
            window.GetProfilerPane().AddMenuItems (window.GetProfilerPane().GetList(), 0, plot);

            for (const auto & [label, run] : plot)
            {
                add = (label == L"Add symbol here...") ? &run : add;
            }

            Assert::IsTrue    (std::ranges::none_of (top, [] (const auto & item) { return item.first == L"Add symbol here..."; }), L"Top level has no address");
            Assert::IsNotNull (add, L"PLOT's row has it");

            window.typed = L"DRAWLINE";
            (*add)();

            Assert::IsFalse (host.commands.empty());
            Assert::AreEqual (std::string ("SYM DRAWLINE = 6000"), host.commands.back());
        }


        //  A click on a bar selects its routine and moves the disassembly to
        //  it; a double-click zooms to it.
        TEST_METHOD (AClickOnAFlameBarSelectsItsRoutineAndADoubleClickZooms)
        {
            FlameGraphView                  flame;
            DxuiDpiScaler                   scaler;
            DxuiMouseEvent                  click;
            std::optional<Word>             activated;
            std::optional<size_t>           selected;
            std::optional<size_t>           zoomed;
            const FlameGraphView::Bar     * plot     = nullptr;



            scaler.SetDpi (96);
            flame.SetData       (MakeRoutineProfile());
            flame.Layout        (RECT { 0, 0, 1000, 200 }, scaler);
            flame.SetOnActivate ([&] (Word address)    { activated = address; });
            flame.SetOnSelect   ([&] (size_t routine)  { selected  = routine; });
            flame.SetOnZoom     ([&] (size_t node)     { zoomed    = node; });

            for (const FlameGraphView::Bar & bar : flame.GetBars())
            {
                plot = (bar.node == 2) ? &bar : plot;
            }

            Assert::IsNotNull (plot, L"PLOT's bar");

            click.kind        = DxuiMouseEventKind::Up;
            click.button      = DxuiMouseButton::Left;
            click.positionDip = POINT { (LONG) (plot->x + plot->width / 2.0f), (LONG) (plot->row * FlameGraphView::kRowDip + FlameGraphView::kRowDip / 2) };

            Assert::IsTrue   (flame.OnMouse (click));
            Assert::AreEqual ((size_t) 2,      flame.GetSelected().value_or (99), L"PLOT's node is selected");
            Assert::AreEqual ((size_t) 0,      selected.value_or (99),            L"PLOT is routine 0");
            Assert::AreEqual ((Word) 0x6000,   activated.value_or (0),            L"the disassembly goes to PLOT");
            Assert::AreEqual ((size_t) 0,      flame.GetZoom(),                   L"a click does not zoom");
            Assert::IsFalse  (zoomed.has_value());

            Assert::IsTrue   (flame.OnMouse (click), L"the second click of a double-click");
            Assert::AreEqual ((size_t) 2,      flame.GetZoom(),       L"a double-click zooms to PLOT");
            Assert::AreEqual ((size_t) 2,      zoomed.value_or (99));
        }


        TEST_METHOD (TheFlameGraphLaysOutEachRoutineUnderItsCaller)
        {
            ProfileData                         profile = MakeRoutineProfile();
            std::vector<FlameGraphView::Bar>    bars    = FlameGraphView::LayoutBars (profile.tree, 0, 0.0f, 1000.0f);
            auto                                find    = [] (const std::vector<FlameGraphView::Bar> & all, size_t node)
            {
                return *std::ranges::find_if (all, [node] (const FlameGraphView::Bar & bar) { return bar.node == node; });
            };



            Assert::AreEqual ((size_t) 4, bars.size());
            Assert::AreEqual (700.0f, find (bars, 1).width, 0.01f, L"DRAW: 70 of 100");
            Assert::AreEqual (1,      find (bars, 1).row);
            Assert::AreEqual (400.0f, find (bars, 2).width, 0.01f, L"PLOT: 40 of 100");
            Assert::AreEqual (400.0f, find (bars, 3).x,     0.01f, L"FILL after PLOT");

            bars = FlameGraphView::LayoutBars (profile.tree, 1, 0.0f, 1000.0f);
            Assert::AreEqual (1000.0f, find (bars, 0).width, 0.01f, L"Top level above, full width");
            Assert::AreEqual (1000.0f, find (bars, 1).width, 0.01f, L"DRAW zoomed");
            Assert::AreEqual (571.43f, find (bars, 2).width, 0.01f, L"PLOT: 40 of DRAW's 70");
            Assert::AreEqual (2,       find (bars, 2).row);
        }



        //  A new profile's tree has another node order: the zoom stays on the
        //  routine it was on, under the same callers, wherever its node is
        //  now, and goes back to Top level when the routine is gone.
        TEST_METHOD (TheZoomFollowsItsRoutineIntoANewProfile)
        {
            ProfileData     profile  = MakeRoutineProfile();
            ProfileData     reordered;
            FlameGraphView  view;



            view.SetData (profile);
            view.SetZoom (2);

            reordered = profile;
            std::swap (reordered.tree[2], reordered.tree[3]);
            view.SetData (reordered);

            Assert::AreEqual ((size_t) 3, view.GetZoom(), L"the same routine, now node 3");

            reordered.tree.resize (2);
            view.SetData (reordered);

            Assert::AreEqual ((size_t) 0, view.GetZoom(), L"gone, so Top level");
        }



        //  The timeline spans a range of cycles: by default the frames', the
        //  newest at the right, at least RoutineProfile::kFrameCount frames
        //  wide; zoomed into a range, each frame in it is wider.
        TEST_METHOD (TheTimelineSpansARangeOfCycles)
        {
            constexpr int64_t     kFrame   = (int64_t) RoutineProfile::kCyclesPerFrame;
            ProfileTimeline       timeline;
            ProfileData           profile;
            ProfileFrameEntry     frame;
            DxuiDpiScaler         scaler;



            frame.shares = { { 0, 1000 } };
            frame.index  = 1000;
            profile.frames.push_back (frame);
            frame.index  = 1001;
            profile.frames.push_back (frame);

            scaler.SetDpi (96);
            timeline.SetData (profile);
            timeline.Layout  (RECT { 0, 0, 600, 40 }, scaler);

            Assert::AreEqual (1002 * kFrame,                                         timeline.GetView().second, L"the newest frame's end at the right");
            Assert::AreEqual ((1002 - (int64_t) RoutineProfile::kFrameCount) * kFrame, timeline.GetView().first,  L"a frame's slot wide each");
            Assert::AreEqual ((size_t) 1, timeline.FindFrameAt (POINT { 599, 20 }).value_or (99), L"the newest at the right edge");
            Assert::IsFalse  (timeline.FindFrameAt (POINT { 10, 20 }).has_value(), L"nothing before the first frame");

            timeline.SetView (1001 * kFrame, 1002 * kFrame);
            Assert::AreEqual ((size_t) 1, timeline.FindFrameAt (POINT { 10,  20 }).value_or (99), L"zoomed in, the second frame fills the timeline");
            Assert::AreEqual ((size_t) 1, timeline.FindFrameAt (POINT { 590, 20 }).value_or (99));
            Assert::AreEqual (1001 * kFrame + kFrame / 2, timeline.GetCycleAt (300), L"the middle is the middle of the frame");

            timeline.ResetView();
            Assert::AreEqual (1002 * kFrame, timeline.GetView().second);
        }


        //  With the pointer over the timeline, a line is drawn down it at the
        //  pointer, as the history timeline draws one; with the pointer
        //  gone, none is.
        TEST_METHOD (ALineIsDrawnAtThePointerOverTheTimeline)
        {
            ProfileTimeline       timeline;
            ProfileData           profile   = MakeRoutineProfile();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            auto                  findLine  = [&]
            {
                std::optional<float>  x;

                for (const RecordedPaintCall & call : painter.Calls())
                {
                    x = (call.argb == theme.PlayheadHoverLine()) ? std::optional<float> (call.x) : x;
                }

                return x;
            };



            scaler.SetDpi (96);
            timeline.SetData (profile);
            timeline.Layout  (RECT { 0, 0, 600, 40 }, scaler);

            Assert::IsTrue  (timeline.SetPointer (250), L"the line appears");
            timeline.Paint  (painter, text, theme);
            Assert::AreEqual (250.0f, findLine().value_or (-1.0f), L"at the pointer");

            Assert::IsFalse (timeline.SetPointer (250), L"nothing changed");
            Assert::IsTrue  (timeline.SetPointer (std::nullopt), L"the line goes");

            painter.Reset();
            timeline.Paint  (painter, text, theme);
            Assert::IsFalse (findLine().has_value());
        }


        //  The clicked bar is drawn in marching dashes, light and dark so one
        //  of them reads on any bar, and the clicked routine's other bars in a
        //  ring of the focus accent; the dashes march with time.
        TEST_METHOD (TheClickedBarMarchesAndItsRoutinesOtherBarsAreOutlined)
        {
            ProfileData                  profile  = MakeRoutineProfile();
            FlameGraphView               flame;
            DxuiDpiScaler                scaler;
            DxuiMouseEvent               click;
            MockDxuiPainter              painter;
            MockDxuiTextRenderer         text;
            MockDxuiTheme                theme;
            const FlameGraphView::Bar  * first    = nullptr;
            const FlameGraphView::Bar  * second   = nullptr;
            int                          light    = 0;
            int                          dark     = 0;
            int                          outlined = 0;



            //  PLOT is called from DRAW and from FILL.
            profile.tree = { { 2, 0, 30, 100, 0 }, { 3, 1, 10, 50, 1 }, { 0, 2, 40, 40, 4 }, { 1, 1, 10, 20, 2 }, { 0, 2, 10, 10, 4 } };

            scaler.SetDpi (96);
            flame.SetData (profile);
            flame.Layout  (RECT { 0, 0, 1000, 200 }, scaler);

            for (const FlameGraphView::Bar & bar : flame.GetBars())
            {
                first  = (bar.node == 2) ? &bar : first;
                second = (bar.node == 4) ? &bar : second;
            }

            Assert::IsNotNull (first);
            Assert::IsNotNull (second);

            click.kind        = DxuiMouseEventKind::Up;
            click.button      = DxuiMouseButton::Left;
            click.positionDip = POINT { (LONG) (first->x + first->width / 2.0f), (LONG) (first->row * FlameGraphView::kRowDip + FlameGraphView::kRowDip / 2) };

            Assert::IsTrue (flame.OnMouse (click));
            Assert::IsTrue (flame.Animate (1000), L"the dashes march while a bar is clicked");

            flame.Paint (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                light    += (call.kind == RecordedPaintKind::DrawLine && call.argb == DxuiMarquee::kLightArgb) ? 1 : 0;
                dark     += (call.kind == RecordedPaintKind::DrawLine && call.argb == DxuiMarquee::kDarkArgb)  ? 1 : 0;
                outlined += (call.kind == RecordedPaintKind::OutlineRect && std::abs (call.x - second->x) < 6.0f) ? 1 : 0;
            }

            Assert::IsTrue   (light > 0 && dark > 0, L"dashes in both colors");
            Assert::AreEqual (2, outlined, L"PLOT's other bar has a ring and a line inside it");
            Assert::IsTrue   (std::ranges::any_of (painter.Calls(), [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::OutlineRect && call.argb == FlameGraphView::kOtherRingArgb; }),
                              L"the ring is white");
            Assert::AreNotEqual (DxuiMarquee::GetPhasePx (4.0f, 0), DxuiMarquee::GetPhasePx (4.0f, DxuiMarquee::kStepMs), L"a step later the dashes have moved");
        }


        //  A routine's tip starts with its name, its address and its symbol's
        //  description, on a flame bar, a list row and a timeline line alike.
        TEST_METHOD (ARoutinesTipHasItsAddressAndDescription)
        {
            ProfileRoutineEntry  named;
            ProfileRoutineEntry  bare;
            ProfileRoutineEntry  top;



            named.kind        = RoutineKind::Call;
            named.address     = 0xDB5C;
            named.symbol      = "OUTDO";
            named.description = "Applesoft: print the character in A";
            bare.kind         = RoutineKind::Call;
            bare.address      = 0x0300;

            Assert::AreEqual (std::string ("OUTDO at $DB5C\nApplesoft: print the character in A"), named.GetTipHead());
            Assert::AreEqual (std::string ("$0300"), bare.GetTipHead(), L"an address alone is the name");
            Assert::AreEqual (std::string ("Top level"), top.GetTipHead());
            Assert::AreEqual (std::string ("OUTDO ($DB5C, Applesoft: print the character in A)"), ProfileTimeline::GetTimelineName (named));
            Assert::AreEqual (std::string ("$0300"), ProfileTimeline::GetTimelineName (bare));
        }


        //  A selected range is framed in the theme's text color, unlike every
        //  routine color, and the bars on either side of it are dimmed.
        TEST_METHOD (TheSelectionIsFramedInTheTextColorAndTheRestDimmed)
        {
            constexpr int64_t     kFrame   = (int64_t) RoutineProfile::kCyclesPerFrame;
            ProfileTimeline       timeline;
            ProfileData           profile  = MakeRoutineProfile();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            float                 from     = 0.0f;
            int                   edges    = 0;
            int                   dimmed   = 0;



            scaler.SetDpi (96);
            timeline.SetData (profile);
            timeline.Layout  (RECT { 0, 0, 600, 40 }, scaler);
            timeline.SetView (0, 10 * kFrame);
            timeline.SetSelection (ProfileTimeline::Range { 4 * kFrame, 6 * kFrame });
            timeline.Paint   (painter, text, theme);

            from = 240.0f;

            for (const RecordedPaintCall & call : painter.Calls())
            {
                edges  += (call.argb == theme.Foreground() && std::abs (call.x - from) < 0.5f) ? 1 : 0;
                dimmed += ((call.argb & 0x00FFFFFF) == (theme.Background() & 0x00FFFFFF) && call.argb != theme.Background()) ? 1 : 0;
            }

            for (size_t i = 0; i < ProfileTimeline::kColoredRoutines; i++)
            {
                Assert::AreNotEqual (theme.Foreground(), FlameGraphView::GetRoutineColor (i));
            }

            Assert::IsTrue   (edges >= 3, L"the left edge, top and bottom start at the selection");
            Assert::AreEqual (2, dimmed, L"a scrim on each side");
        }


        //  A bar that covers several frames, once the frames are joined into
        //  wider bars, is as tall as its cycles are a share of those frames'.
        TEST_METHOD (ABarOfSeveralFramesIsScaledByThem)
        {
            static constexpr uint64_t  kFrames = 4;

            ProfileTimeline       timeline;
            ProfileData           profile;
            ProfileFrameEntry     frame;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            float                 tall     = -1.0f;



            frame.count  = kFrames;
            frame.shares = { { 0, 2 * RoutineProfile::kCyclesPerFrame } };
            profile.frames.push_back (frame);

            scaler.SetDpi (96);
            timeline.SetData (profile);
            timeline.Layout  (RECT { 0, 0, 600, 100 }, scaler);
            timeline.Paint   (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                tall = (call.argb == FlameGraphView::GetRoutineColor (0)) ? call.height : tall;
            }

            Assert::AreEqual (50.0f, tall, 0.5f, L"two frames' cycles in a bar of four frames");
        }



        TEST_METHOD (AFramesTipListsItsRoutinesByCycles)
        {
            ProfileData            profile  = MakeRoutineProfile();
            std::vector<uint32_t>  swatches;
            std::wstring           tip      = ProfileTimeline::GetFrameTip (profile.frames[0], profile.routines, 0xFF808080, swatches);
            std::wstring  head    = std::format (L"Frame 9: {} cycles\nPLOT ($6000): {} cycles (74.4%)\nTop level: {} cycles", DebuggerStatusText::FormatCount (4030),
                                                 DebuggerStatusText::FormatCount (3000), DebuggerStatusText::FormatCount (1000));



            //  The counts have the locale's thousands separators.
            Assert::IsTrue (tip.starts_with (head), tip.c_str());
            Assert::IsTrue (tip.ends_with   (L"Other routines: 30 cycles"), tip.c_str());

            //  A swatch in the bar's color before each routine, none before
            //  the frame's line.
            Assert::IsTrue (swatches == std::vector<uint32_t> { 0, FlameGraphView::GetRoutineColor (0), FlameGraphView::GetRoutineColor (2), 0xFF808080 });
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



        //  The labels of a pane's row menu, none for a list not the pane's.
        static std::vector<std::wstring> GetMenuLabels (IDebuggerPane & pane, DxuiListView * list, int row)
        {
            PaneMenuItems              items;
            std::vector<std::wstring>  labels;



            if (!pane.AddMenuItems (list, row, items))
            {
                return { L"none" };
            }

            for (const auto & [label, chosen] : items)
            {
                labels.push_back (label);
            }

            return labels;
        }



        //  The input log's guest and host input are chosen on the Source
        //  column's Filter cascade, and the heading has a funnel while any of
        //  it is left out; another column has no filter.
        TEST_METHOD (TheInputLogsSourceColumnHasTheFilterCascade)
        {
            CassoTheme                                 theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                          host;
            ToolWindow                                 window (theme, host);
            InputLogController                         log    (window.GetPaneHostForTest());
            std::vector<std::shared_ptr<DxuiCommand>>  commands;
            std::vector<DxuiPopupMenuItem>             items;
            DxuiListView                             * list   = nullptr;



            window.OnCreate();
            log.CreateControls();
            log.Configure();
            list = log.GetLists().front();

            items = log.GetColumnFilterItems (list, InputLogPane::kSource, commands);
            Assert::AreEqual ((size_t) 6, items.size(), L"three guest inputs, a separator and two host inputs");
            Assert::AreEqual ((size_t) 5, commands.size());
            Assert::AreEqual (std::wstring (L"Guest keyboard"), commands[0]->label);
            Assert::IsTrue   (log.GetColumnFilterItems (list, InputLogPane::kMeaning, commands).empty(), L"the meaning has a filter");

            log.ApplyLogDeltas ({}, false);
            Assert::IsFalse  (list->IsColumnFiltered (InputLogPane::kSource), L"a funnel with every input shown");

            commands[0]->dispatch();
            Assert::IsTrue   (list->IsColumnFiltered (InputLogPane::kSource), L"no funnel with the guest's keyboard left out");
        }


        //  FR-198: the sound log's Clear and Save run SOUNDLOG, so the pane
        //  and the console agree on what the log holds; its Source column has
        //  the sources' filter. The video log has no filter.
        TEST_METHOD (TheIoLogsClearAndSaveRunTheLogCommand)
        {
            CassoTheme                                 theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                          host;
            ToolWindow                                 window (theme, host);
            IoLogController                            sound  (window.GetPaneHostForTest(), DebugLogKind::Sound);
            IoLogController                            video  (window.GetPaneHostForTest(), DebugLogKind::Video);
            std::vector<std::shared_ptr<DxuiCommand>>  commands;



            window.OnCreate();

            for (IoLogController * log : { &sound, &video })
            {
                log->CreateControls();
                log->Configure();
            }

            sound.Clear();
            Assert::IsFalse  (host.commands.empty(), L"Clear runs a command");
            Assert::AreEqual (std::string ("SOUNDLOG CLEAR"), host.commands.back());

            host.GetFakeDialogs().path = L"C:\\Logs\\Sound log.txt";
            sound.Save();
            Assert::AreEqual (std::string ("SOUNDLOG SAVE \"C:\\Logs\\Sound log.txt\""), host.commands.back(), L"the file chosen, quoted");
            Assert::AreEqual (std::wstring (L"SoundLog.txt"), host.GetFakeDialogs().lastSpec.defaultFileName);

            video.Clear();
            Assert::AreEqual (std::string ("VIDEOLOG CLEAR"), host.commands.back());

            Assert::AreEqual ((size_t) 4, sound.GetColumnFilterItems (sound.GetLists().front(), IoLogPane::kSource, commands).size(), L"speaker, 6522, AY and SSI-263");
            Assert::IsTrue   (video.GetColumnFilterItems (video.GetLists().front(), IoLogPane::kSource, commands).empty());
        }


        //  The serial log's list has a row for each byte sent and received,
        //  not for the settings and line rows, which are the graph's. The
        //  graph has a lane for each line, and a lane turned off makes it a
        //  lane shorter.
        TEST_METHOD (TheSerialLogListsBytesAndGraphsTheirBits)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            CommLogController  serial   (window.GetPaneHostForTest(), DebugLogKind::Serial);
            DebugLogDeltas     deltas;
            IoLogDelta         delta;
            int                height   = 0;
            auto               row      = [] (IoEventKind kind, uint64_t cycle, Byte value)
            {
                IoLogRow  each;

                each.cycle         = cycle;
                each.event.kind    = kind;
                each.event.cycle   = cycle;
                each.event.value   = value;
                each.event.control = 0x1E;
                each.event.command = 0x0B;
                each.event.lines   = IoEventLog::kLineDtr;
                each.event.chip    = Acia6551::kCommPortBuiltIn | 1;
                return each;
            };



            window.OnCreate();
            serial.CreateControls();
            serial.Configure();

            delta.newer = { row (IoEventKind::SerialSettings, 900, 0), row (IoEventKind::SerialSent, 1'000, 'A'), row (IoEventKind::SerialReceived, 9'000, 'B') };
            deltas.serial.push_back (delta);
            serial.ApplyLogDeltas (deltas, false);

            Assert::AreEqual ((ptrdiff_t) 2, std::ranges::count_if (serial.GetModel().GetView(), [] (int index) { return index >= 0; }), L"the two bytes, not the settings");
            Assert::IsTrue   (serial.GetModel().GetText (false).find (L"Sent") != std::wstring::npos);
            Assert::IsTrue   (serial.GetModel().GetText (false).find (L"41") != std::wstring::npos, L"the byte in hex");
            Assert::IsTrue   (serial.GetModel().GetText (false).find (L"Port 1") != std::wstring::npos, L"the //c's first port");

            height = serial.GetGraph().GetHeightDip();
            Assert::AreEqual (8 * CommSignalGraph::kLaneDip + CommSignalGraph::kAxisDip, height, L"a lane for each of the eight lines");

            serial.ShowLane (CommGraph::kIrq, false);
            Assert::AreEqual (height - CommSignalGraph::kLaneDip, serial.GetGraph().GetHeightDip(), L"one lane fewer");
            Assert::IsTrue   (serial.GetModel().GetOptionsText().starts_with ("lanes=11111110"), L"and the choice is in the options");
        }


        //  A connection opening or closing is a row, with the far end's
        //  address as its text, and a byte received with errors lists them.
        TEST_METHOD (TheSerialLogListsConnectionsAndReceiveErrors)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                         host;
            ToolWindow                                window   (theme, host);
            CommLogController                         serial   (window.GetPaneHostForTest(), DebugLogKind::Serial);
            DebugLogDeltas                            deltas;
            IoLogDelta                                delta;
            IoLogRow                                  opened;
            IoLogRow                                  received;
            std::string                               peer     = "bbs.example.org:23";
            std::wstring                              text;



            opened.cycle        = 100;
            opened.event.kind   = IoEventKind::SerialConnection;
            opened.event.value  = 1;
            opened.data         = std::make_shared<const std::vector<Byte>> (peer.begin(), peer.end());

            received.cycle       = 200;
            received.event.kind  = IoEventKind::SerialReceived;
            received.event.value = 'C';
            received.event.reg   = 0x02;

            window.OnCreate();
            serial.CreateControls();
            serial.Configure();

            delta.newer = { opened, received };
            deltas.serial.push_back (delta);
            serial.ApplyLogDeltas (deltas, false);

            text = serial.GetModel().GetText (false);
            Assert::AreEqual ((ptrdiff_t) 2, std::ranges::count_if (serial.GetModel().GetView(), [] (int index) { return index >= 0; }));
            Assert::IsTrue   (text.find (L"Connected") != std::wstring::npos);
            Assert::IsTrue   (text.find (L"bbs.example.org:23") != std::wstring::npos, L"the far end's address");
            Assert::IsTrue   (text.find (L"Received, framing error") != std::wstring::npos);
        }


        //  Taken back in history, the graph ends at the last row before the
        //  machine's position, not at the newest row the log has.
        TEST_METHOD (BehindLiveTheGraphEndsAtThePosition)
        {
            CassoTheme            theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost     host;
            ToolWindow            window   (theme, host);
            CommLogController     serial   (window.GetPaneHostForTest(), DebugLogKind::Serial);
            DebugLogDeltas        deltas;
            IoLogDelta            delta;
            DebuggerViewSnapshot  snapshot;
            auto                  row      = [] (uint64_t position, uint64_t cycle, Byte value)
            {
                IoLogRow  each;

                each.position      = position;
                each.cycle         = cycle;
                each.event.kind    = IoEventKind::SerialSent;
                each.event.cycle   = cycle;
                each.event.value   = value;
                each.event.control = 0x1E;
                each.event.command = 0x0B;
                return each;
            };



            window.OnCreate();
            serial.CreateControls();
            serial.Configure();

            delta.newer = { row (10, 1'000, 'A'), row (500, 50'000, 'B'), row (900, 90'000, 'C') };
            deltas.serial.push_back (delta);
            serial.ApplyLogDeltas (deltas, false);

            Assert::IsTrue (serial.GetGraph().GetLast() > 90'000, L"live, the graph ends just after the newest row");

            snapshot.history.isBehindLive = true;
            snapshot.history.position     = 600;
            serial.Apply (snapshot);
            serial.ApplyLogDeltas ({}, false);

            Assert::IsTrue (serial.GetGraph().GetLast() > 50'000,  L"just after the last row before the position");
            Assert::IsTrue (serial.GetGraph().GetLast() < 60'000,  L"and not at the newest row");
        }


        //  The serial ports' and the Uthernet II's panels open their logs.
        TEST_METHOD (TheSerialAndNetworkPanelsOpenTheirLogs)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            PaneMenuItems      items;



            for (const char * id : { "serial-card", "serial-port-1" })
            {
                DevicePanelController  panel (window.GetPaneHostForTest(), id, L"Serial");

                items.clear();
                panel.AddContentMenuItems ({}, items);

                Assert::AreEqual ((size_t) 1, items.size());
                Assert::AreEqual (std::wstring (L"Open serial log"), items[0].first);
            }

            {
                DevicePanelController  panel (window.GetPaneHostForTest(), "uthernet", L"Uthernet II");

                items.clear();
                panel.AddContentMenuItems ({}, items);

                Assert::AreEqual ((size_t) 1, items.size());
                Assert::AreEqual (std::wstring (L"Open network log"), items[0].first);
            }
        }


        //  The network log lists each send and receive with its length, and a
        //  lane for each socket.
        TEST_METHOD (TheNetworkLogListsSendsAndReceives)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            ToolWindow         window   (theme, host);
            CommLogController  network  (window.GetPaneHostForTest(), DebugLogKind::Network);
            DebugLogDeltas     deltas;
            IoLogDelta         delta;
            IoLogRow           sent;



            window.OnCreate();
            network.CreateControls();
            network.Configure();

            sent.cycle         = 1'000;
            sent.event.kind    = IoEventKind::SocketSent;
            sent.event.cycle   = 1'000;
            sent.event.chip    = 1;
            sent.event.length  = 3;
            sent.data          = std::make_shared<const std::vector<Byte>> (std::vector<Byte> { 'G', 'E', 'T' });

            delta.newer = { sent };
            deltas.network.push_back (delta);
            network.ApplyLogDeltas (deltas, false);

            Assert::IsTrue   (network.GetModel().GetText (false).find (L"Socket 1\tSent\t3\tGET\t47 45 54") != std::wstring::npos, network.GetModel().GetText (false).c_str());
            Assert::AreEqual (4 * CommSignalGraph::kLaneDip + CommSignalGraph::kAxisDip, network.GetGraph().GetHeightDip(), L"a lane for each socket");
        }


        //  A symbol's row has Go to and Remove over its table's commands; a
        //  table's heading has only the table's; another pane's list has none.
        TEST_METHOD (TheSymbolsRowMenuActsOnTheSymbolAndItsTable)
        {
            CassoTheme            theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost     host;
            ToolWindow            window  (theme, host);
            SymbolsPane           symbols (window.GetPaneHostForTest(), SymbolsPane::Kind::Machine);
            BookmarksPane         other   (window.GetPaneHostForTest());
            DebuggerViewSnapshot  snapshot;
            PaneMenuItems         items;



            window.OnCreate();
            symbols.CreateControls();
            other.CreateControls();
            symbols.Configure();

            snapshot.symbols = MakeSymbols();
            symbols.Apply (snapshot);
            symbols.ToggleTable ("main");

            Assert::IsTrue (GetMenuLabels (symbols, symbols.GetList(), 1) == std::vector<std::wstring> { L"Go to", L"Remove", L"Clear table", L"Save table...", L"Copy" });
            Assert::IsTrue (GetMenuLabels (symbols, symbols.GetList(), 0) == std::vector<std::wstring> { L"Clear table", L"Save table...", L"Copy" });
            Assert::IsTrue (GetMenuLabels (symbols, other.GetList(), 1)   == std::vector<std::wstring> { L"none" });

            (void) symbols.AddMenuItems (symbols.GetList(), 1, items);
            items[1].second();

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands[0].find ("COUT") != std::string::npos, TextEncoding::NarrowToWide (host.commands[0]).c_str());
        }


        //  A user table loaded from a file unloads it, which clears the
        //  table.
        TEST_METHOD (AUserTableLoadedFromAFileUnloadsIt)
        {
            CassoTheme            theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost     host;
            ToolWindow            window  (theme, host);
            SymbolsPane           symbols (window.GetPaneHostForTest(), SymbolsPane::Kind::User);
            DebuggerViewSnapshot  snapshot;
            PaneMenuItems         items;
            auto                  state   = std::make_shared<DebuggerViewSnapshot::SymbolsState> (*MakeSymbols());



            window.OnCreate();
            symbols.CreateControls();
            symbols.Configure();

            state->tables[1].origins = { "demo.dbg" };
            snapshot.symbols         = state;
            symbols.Apply (snapshot);

            Assert::IsTrue (GetMenuLabels (symbols, symbols.GetList(), 0) == std::vector<std::wstring> { L"Unload demo.dbg", L"Save table...", L"Copy" });

            (void) symbols.AddMenuItems (symbols.GetList(), 0, items);
            items[0].second();

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands[0].find ("CLEAR") != std::string::npos, TextEncoding::NarrowToWide (host.commands[0]).c_str());
        }
    };
}
