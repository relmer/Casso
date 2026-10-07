#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Debugger/Source/SourcePathList.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HeatMapPaneWindowTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatMapHost
    //
    //  A host that keeps every change to the heat map's recording it is told.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HeatMapHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int window, std::optional<Word> address) override { memoryMoves.emplace_back (window, address); }
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  SetDebuggerHeatMapShown (bool shown) override { recording.push_back (shown); }

        std::string  GetDebuggerHeatMapOptions()                           override { return heatMapOptions; }
        void         SetDebuggerHeatMapOptions (const std::string & text)  override { heatMapOptions = text; optionsSent.push_back (text); }
        void         ResetDebuggerHeatMap      ()                          override { resets++; }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return layout; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<bool>                                  recording;
        std::vector<std::pair<int, std::optional<Word>>>   memoryMoves;
        std::vector<std::string>                           optionsSent;
        std::string                                        heatMapOptions;
        std::string                                        layout;
        int                                                resets         = 0;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatMapWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HeatMapWindow : public DebuggerWindow
    {
    public:
        HeatMapWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::SyncHeatMapRecording;
        using DebuggerWindow::IsHeatMapRecording;
        using DebuggerWindow::GetHeatMapView;
        using DebuggerWindow::GetHeatMapBar;
        using DebuggerWindow::GetHeatMapFadeCommands;
        using DebuggerWindow::GetHeatMapBarLabel;
        using DebuggerWindow::IsHeatMapBarEnabled;
        using DebuggerWindow::IsHeatMapBarChecked;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::GetTextZoom;
        using DebuggerWindow::OnMouse;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatMapPaneWindowTests
    //
    //  The default layout holds the heat map open, as a tab behind the first
    //  memory window, and the host records for it only while it is open and
    //  in front. A layout saved before that keeps it closed. Its options are
    //  kept by the host and sent on as the window opens, and a click on a
    //  cell shows the address in memory.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HeatMapPaneWindowTests)
    {
    public:

        static void Build (HeatMapWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }

        static void Relayout (HeatMapWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.SyncHeatMapRecording();
        }

        static DxuiMouseEvent MakePress (DxuiMouseEventKind kind, POINT point)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = point;
            return ev;
        }

        //  A click on an entry of the heat map's bar, as the mouse makes it.
        static void ClickBarEntry (HeatMapWindow & window, int id)
        {
            RECT   entry = {};
            POINT  at    = {};



            Assert::IsTrue (window.GetHeatMapBar()->TryGetEntryRect (id, entry), L"the entry is on the bar");

            at = { (entry.left + entry.right) / 2, (entry.top + entry.bottom) / 2 };

            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Move, at));
            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Down, at));
            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Up,   at));
        }

        static void BuildShown (HeatMapWindow & window)
        {
            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);
        }



        TEST_METHOD (TheDefaultLayoutTabsTheHeatMapWithTheMemoryWindows)
        {
            DxuiPaneLayout             layout = DebuggerLayout::Restore (L"");
            std::vector<std::wstring>  group;



            Assert::IsTrue (layout.Contains (DebuggerLayout::kHeatMap));

            group = layout.GetGroup (DebuggerLayout::GetMemoryPaneId (1));
            Assert::IsTrue (std::ranges::find (group, std::wstring (DebuggerLayout::kHeatMap)) != group.end(), L"a tab beside Memory 1");

            group = layout.GetGroup (DebuggerLayout::kConsole);
            Assert::IsTrue (std::ranges::find (group, std::wstring (DebuggerLayout::kHeatMap)) == group.end(), L"no longer beside the console");
        }



        TEST_METHOD (TheDefaultLayoutHoldsThePaneOpenBehindMemoryAndRecordsNothingUntilInFront)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost                host;
            HeatMapWindow              window (theme, host);
            std::vector<std::wstring>  panes;



            Build (window);
            window.SyncHeatMapRecording();
            panes = window.GetViewMenuPanes();

            Assert::IsTrue  (window.IsPaneShown (DebuggerLayout::kHeatMap), L"open by default");
            Assert::IsTrue  (host.recording.empty(), L"behind Memory 1 it records nothing");
            Assert::IsTrue  (std::ranges::find (panes, std::wstring (DebuggerLayout::kHeatMap)) != panes.end(), L"the View menu lists it");

            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            Assert::AreEqual ((size_t) 1, host.recording.size());
            Assert::IsTrue   (host.recording.back(), L"in front, it records");
        }



        TEST_METHOD (ALayoutSavedBeforeKeepsTheHeatMapClosed)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);



            host.layout = SourcePathList::WideToUtf8 (DebuggerLayout::MakeDefault().ToText());
            Build (window);

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kHeatMap), L"a saved layout with no heat map options predates it");

            window.ShowPane (DebuggerLayout::kHeatMap);
            Assert::IsTrue  (window.IsPaneShown (DebuggerLayout::kHeatMap), L"the View menu still opens it");
        }



        TEST_METHOD (ALayoutSavedSinceShowsTheHeatMap)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);



            host.layout         = SourcePathList::WideToUtf8 (DebuggerLayout::MakeDefault().ToText());
            host.heatMapOptions = "fade=10 view=all";
            Build (window);

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kHeatMap));
        }



        TEST_METHOD (ClosingThePaneStopsTheRecording)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);
            window.ClosePane (DebuggerLayout::kHeatMap);
            Relayout (window);

            Assert::IsFalse  (window.IsPaneShown (DebuggerLayout::kHeatMap));
            Assert::AreEqual ((size_t) 2, host.recording.size());
            Assert::IsFalse  (host.recording.back(), L"closed, it stops");
        }



        TEST_METHOD (APaneBehindAnotherTabDoesNotRecord)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);
            window.ShowPane (DebuggerLayout::GetMemoryPaneId (1));
            Relayout (window);

            Assert::IsTrue  (window.IsPaneShown (DebuggerLayout::kHeatMap), L"still open, behind Memory 1");
            Assert::IsFalse (window.IsHeatMapRecording());
        }



        TEST_METHOD (TheOptionsKeptAreSentOnAsTheWindowOpensAndEachChangeIsKept)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);



            host.heatMapOptions = "fade=30 view=code cumulative";
            Build (window);

            Assert::IsTrue   (window.GetHeatMapView()->GetOptions().cumulative);
            Assert::AreEqual (30, window.GetHeatMapView()->GetOptions().fadeSeconds);
            Assert::AreEqual ((size_t) 1, host.optionsSent.size(), L"sent once as the window opens");
            Assert::AreEqual (std::string ("fade=30 view=code cumulative"), host.optionsSent.back());

            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            ClickBarEntry (window, HeatMapBarCommands::kFading);
            Assert::AreEqual (std::string ("fade=30 view=code"), host.heatMapOptions, L"Fading, kept");
        }



        TEST_METHOD (AClickOnAModeInTheWindowReachesThePane)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            RECT            bounds = {};



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            bounds = window.GetHeatMapView()->GetBounds();

            Assert::IsTrue   (window.OnMouse (MakePress (DxuiMouseEventKind::Down, { bounds.left + HeatMapView::kGutterDip + HeatMapView::kTabDip + 5, bounds.top + HeatMapView::kBarDip / 2 })));
            Assert::AreEqual ((int) HeatMapView::Mode::Code, (int) window.GetHeatMapView()->GetMode());
        }



        TEST_METHOD (AClickOnACellShowsItsAddressInMemoryAndBringsMemoryForward)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            HeatMapView   * view   = nullptr;
            RECT            cell   = {};
            POINT           at     = {};
            Word            target = 0;



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            //  On a memory row's start, which is where the window reports it is.
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 0x10);
            cell   = view->GetCellRect (target);
            at     = { cell.left + 1, cell.top + 1 };

            host.memoryMoves.clear();
            window.OnMouse (MakePress (DxuiMouseEventKind::Down, at));
            window.OnMouse (MakePress (DxuiMouseEventKind::Up,   at));
            Relayout (window);

            Assert::IsFalse  (host.memoryMoves.empty(), L"a memory window was moved");
            Assert::AreEqual (1, host.memoryMoves.back().first);
            Assert::AreEqual (target, host.memoryMoves.back().second.value_or (0));
            Assert::IsFalse  (view->IsVisible(), L"Memory 1 came forward over the heat map it shares a group with");
        }



        TEST_METHOD (TheFadeIsADropDownOfItsTimesAndAChoiceIsKept)
        {
            CassoTheme                       theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost                      host;
            HeatMapWindow                    window (theme, host);
            HeatMapBarCommands               commands ({});
            std::vector<DxuiToolbar::Entry>  entries  = commands.BuildEntries();
            std::vector<std::wstring>        labels;
            std::wstring                     checked;



            BuildShown (window);

            Assert::IsTrue (std::ranges::any_of (entries, [] (const DxuiToolbar::Entry & entry)
            {
                return entry.command->id == HeatMapBarCommands::kFade && entry.kind == DxuiToolbar::Kind::DropDown;
            }), L"the fade time is a drop-down, not a link that cycles");

            for (const std::shared_ptr<DxuiCommand> & row : window.GetHeatMapFadeCommands())
            {
                labels.push_back (row->label);

                if (row->IsChecked())
                {
                    checked = row->label;
                }
            }

            Assert::IsTrue   ((std::vector<std::wstring> { L"2 s", L"5 s", L"10 s", L"20 s", L"30 s", L"60 s" }) == labels, L"the six times, in order");
            Assert::AreEqual (std::wstring (L"10 s"), checked, L"the time in force is checked");
            Assert::AreEqual (std::wstring (L"Fade: 10 s"), window.GetHeatMapBarLabel (HeatMapBarCommands::kFade));

            window.GetHeatMapFadeCommands()[4]->dispatch();

            Assert::AreEqual (30, window.GetHeatMapView()->GetOptions().fadeSeconds);
            Assert::AreEqual (std::string ("fade=30 view=all"), host.heatMapOptions, L"kept");
            Assert::AreEqual (std::wstring (L"Fade: 30 s"), window.GetHeatMapBarLabel (HeatMapBarCommands::kFade));
            Assert::IsTrue   (window.GetHeatMapFadeCommands()[4]->IsChecked(), L"the rows are built again with the new check");
        }



        TEST_METHOD (FadingCumulativeAndResetCountsAreOnTheBar)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);



            BuildShown (window);

            Assert::IsTrue  (window.IsHeatMapBarChecked (HeatMapBarCommands::kFading));
            Assert::IsFalse (window.IsHeatMapBarEnabled (HeatMapBarCommands::kResetCounts), L"nothing to reset while fading");

            ClickBarEntry (window, HeatMapBarCommands::kCumulative);
            Assert::IsTrue  (window.GetHeatMapView()->GetOptions().cumulative);
            Assert::IsTrue  (window.IsHeatMapBarChecked (HeatMapBarCommands::kCumulative));
            Assert::IsFalse (window.IsHeatMapBarEnabled (HeatMapBarCommands::kFade), L"the fade counts for nothing while cumulative");
            Assert::IsTrue  (window.IsHeatMapBarEnabled (HeatMapBarCommands::kResetCounts));

            ClickBarEntry (window, HeatMapBarCommands::kResetCounts);
            Assert::AreEqual (1, host.resets);
        }



        TEST_METHOD (BlendOnTheBarTurnsOnAndOffAndIsKept)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);
            DxuiDpiScaler  scaler;



            BuildShown (window);
            Assert::IsFalse (window.IsHeatMapBarChecked (HeatMapBarCommands::kBlend), L"off by default");

            //  Wide enough for the bar to hold Blend rather than its "..." menu.
            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, 3000, 900 }, scaler);

            ClickBarEntry (window, HeatMapBarCommands::kBlend);
            Assert::IsTrue   (window.GetHeatMapView()->GetOptions().blend, L"on");
            Assert::IsTrue   (window.IsHeatMapBarChecked (HeatMapBarCommands::kBlend), L"checked");
            Assert::AreEqual (std::string ("fade=10 view=all blend"), host.heatMapOptions, L"kept");

            ClickBarEntry (window, HeatMapBarCommands::kBlend);
            Assert::IsFalse  (window.GetHeatMapView()->GetOptions().blend);
        }



        TEST_METHOD (TheZoomButtonsZoomTheMap)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);
            HeatMapView  * view   = nullptr;



            BuildShown (window);
            view = window.GetHeatMapView();

            ClickBarEntry (window, HeatMapBarCommands::kZoomIn);
            Assert::IsTrue   (view->GetCellPx() > 3, L"Zoom in");

            ClickBarEntry (window, HeatMapBarCommands::kZoomOut);
            ClickBarEntry (window, HeatMapBarCommands::kZoomOut);
            Assert::IsTrue   (view->GetCellPx() < 3, L"Zoom out");

            ClickBarEntry (window, HeatMapBarCommands::kResetZoom);
            Assert::AreEqual (3, view->GetCellPx(), L"Reset zoom");
        }



        TEST_METHOD (CtrlWheelOverTheMapZoomsTheMapAndElsewhereTheText)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            HeatMapView   * view   = nullptr;
            DxuiMouseEvent  wheel;
            float           zoom   = 0.0f;
            RECT            cell   = {};
            RECT            bounds = {};
            Word            target = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 3);
            cell   = view->GetCellRect (target);
            zoom   = window.GetTextZoom();

            wheel.kind        = DxuiMouseEventKind::Wheel;
            wheel.ctrl        = true;
            wheel.wheelDelta  = 1.0f;
            wheel.positionDip = { cell.left + 1, cell.top + 1 };

            Assert::IsTrue   (window.OnMouse (wheel));
            Assert::IsTrue   (view->GetCellPx() > 3, L"the map zoomed");
            Assert::AreEqual (zoom, window.GetTextZoom(), L"and the text did not");
            Assert::IsTrue   (view->GetCellRect (target).top <= wheel.positionDip.y && wheel.positionDip.y <= view->GetCellRect (target).bottom, L"about the pointer");

            bounds            = view->GetBounds();
            wheel.positionDip = { bounds.left + 2, bounds.top + 2 };
            (void) window.OnMouse (wheel);
            Assert::IsTrue   (window.GetTextZoom() > zoom, L"off the map, Ctrl+wheel still sizes the text");
        }



        TEST_METHOD (OverTheMapTheTipShowsAtOnce)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            HeatMapView   * view   = nullptr;
            RECT            cell   = {};
            DxuiMouseEvent  move;
            Word            target = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 3);
            cell   = view->GetCellRect (target);

            move.kind        = DxuiMouseEventKind::Move;
            move.positionDip = { cell.left + 1, cell.top + 1 };
            (void) window.OnMouse (move);

            Assert::IsTrue   (window.GetTooltip().IsVisible(), L"no dwell");
            Assert::AreEqual (std::format (L"${:04X}  untouched", target), window.GetTooltip().GetText());
            Assert::IsTrue   (view->GetHover().has_value(), L"and the cell is framed");
        }
    };
}