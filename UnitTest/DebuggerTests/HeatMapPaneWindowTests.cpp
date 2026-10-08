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
        using DebuggerWindow::GetHeatViewBar;
        using DebuggerWindow::GetHeatViewCommands;
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



            Assert::IsTrue (window.GetHeatMapBar()->TryGetEntryRect (id, entry) || window.GetHeatViewBar()->TryGetEntryRect (id, entry),
                            L"the entry is on the bar or on the strip in the row of views");

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

            window.GetHeatMapFadeCommands()[4]->dispatch();
            Assert::AreEqual (std::string ("fade=30 view=code"), host.heatMapOptions, L"Fading for 30 s, kept");
        }



        //  Which accesses the map shows is a drop-down in its row of views,
        //  reading the view in force, with Blend beside it.
        TEST_METHOD (TheViewIsADropDownInTheRowOfViews)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            RECT            view   = {};
            RECT            blend  = {};



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            Assert::IsTrue   (window.GetHeatViewBar()->TryGetEntryRect (HeatMapBarCommands::kView,  view),  L"in the row of views");
            Assert::IsTrue   (window.GetHeatViewBar()->TryGetEntryRect (HeatMapBarCommands::kBlend, blend), L"with Blend");
            Assert::IsTrue   (blend.left >= view.right, L"Blend beside it");
            Assert::AreEqual (std::wstring (L"All"), window.GetHeatMapBarLabel (HeatMapBarCommands::kView));
            Assert::AreEqual ((size_t) HeatMapView::kModeCount, window.GetHeatViewCommands().size(), L"a row for each view");

            window.GetHeatViewCommands()[1]->dispatch();

            Assert::AreEqual ((int) HeatMapView::Mode::Code, (int) window.GetHeatMapView()->GetMode());
            Assert::AreEqual (std::wstring (L"Code"), window.GetHeatMapBarLabel (HeatMapBarCommands::kView));
            Assert::IsTrue   (host.heatMapOptions.find ("view=code") != std::string::npos, L"kept");
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



        //  How the map counts is one drop-down: fading at each fade time,
        //  then cumulative. The entry reads the choice in force.
        TEST_METHOD (HowTheMapCountsIsOneDropDownAndAChoiceIsKept)
        {
            CassoTheme                       theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost                      host;
            HeatMapWindow                    window (theme, host);
            HeatMapBarCommands               commands ({});
            std::vector<DxuiToolbar::Entry>  entries  = commands.BuildEntries (false);
            std::vector<std::wstring>        labels;
            std::wstring                     checked;



            BuildShown (window);

            Assert::IsTrue (std::ranges::any_of (entries, [] (const DxuiToolbar::Entry & entry)
            {
                return entry.command->id == HeatMapBarCommands::kMode && entry.kind == DxuiToolbar::Kind::DropDown;
            }), L"how the map counts is a drop-down");

            for (const std::shared_ptr<DxuiCommand> & row : window.GetHeatMapFadeCommands())
            {
                labels.push_back (row->label);

                if (row->IsChecked())
                {
                    checked = row->label;
                }
            }

            Assert::IsTrue   ((std::vector<std::wstring> { L"Fade (2 s)", L"Fade (5 s)", L"Fade (10 s)", L"Fade (20 s)", L"Fade (30 s)", L"Fade (60 s)", L"Cumulative" }) == labels,
                              L"the six times, in order, then cumulative");
            Assert::AreEqual (std::wstring (L"Fade (10 s)"), checked, L"the choice in force is checked");
            Assert::AreEqual (std::wstring (L"Fade (10 s)"), window.GetHeatMapBarLabel (HeatMapBarCommands::kMode));

            window.GetHeatMapFadeCommands()[4]->dispatch();

            Assert::AreEqual (30, window.GetHeatMapView()->GetOptions().fadeSeconds);
            Assert::AreEqual (std::string ("fade=30 view=all"), host.heatMapOptions, L"kept");
            Assert::AreEqual (std::wstring (L"Fade (30 s)"), window.GetHeatMapBarLabel (HeatMapBarCommands::kMode));
            Assert::IsTrue   (window.GetHeatMapFadeCommands()[4]->IsChecked(), L"the rows are built again with the new check");
        }



        //  Reset counts is on the bar only while the map counts totals, as
        //  there is nothing to reset while it fades.
        TEST_METHOD (ResetCountsIsOnTheBarOnlyWhileCumulative)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);
            RECT           entry  = {};



            BuildShown (window);

            Assert::IsFalse  (window.GetHeatMapBar()->TryGetEntryRect (HeatMapBarCommands::kResetCounts, entry), L"nothing to reset while fading");

            window.GetHeatMapFadeCommands().back()->dispatch();
            Relayout (window);

            Assert::IsTrue   (window.GetHeatMapView()->GetOptions().cumulative);
            Assert::AreEqual (std::wstring (L"Cumulative"), window.GetHeatMapBarLabel (HeatMapBarCommands::kMode));
            Assert::IsTrue   (window.GetHeatMapBar()->TryGetEntryRect (HeatMapBarCommands::kResetCounts, entry), L"Reset counts is on the bar");

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



        //  A click at a point, as the mouse makes it.
        static void ClickAt (HeatMapWindow & window, POINT at)
        {
            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Move, at));
            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Down, at));
            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Up,   at));
        }

        static POINT GetCenter (const RECT & rect)
        {
            return { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        //  The zoom is a widget in the map's bottom-right corner, not buttons
        //  on the bar: it shows the zoom as a percentage, its tip says how
        //  Ctrl+wheel zooms, and a click opens a slider from the smallest cell
        //  to the largest with a Reset.
        TEST_METHOD (TheZoomWidgetZoomsTheMap)
        {
            CassoTheme     theme       = CassoTheme::MakeSkeuomorphic();
            HeatMapHost    host;
            HeatMapWindow  window (theme, host);
            HeatMapView  * view        = nullptr;
            RECT           button      = {};
            RECT           track       = {};
            RECT           moved       = {};
            RECT           anchor      = {};
            DxuiDpiScaler  scaler;
            std::wstring   text;



            BuildShown (window);

            //  Tall enough for the pane to hold the slider above the button.
            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, 1400, 3000 }, scaler);

            view   = window.GetHeatMapView();
            button = view->GetZoomWidget().GetButtonRect();

            Assert::IsTrue   (button.right > button.left, L"the widget is placed");
            Assert::AreEqual (view->GetBounds().right  - HeatMapZoomWidget::kMarginDip - (view->HasVerticalScroll()   ? HeatMapView::kScrollbarDip : 0), button.right,
                              L"its margin in from the pane's right, inside a scrollbar that shows");
            Assert::AreEqual (view->GetBounds().bottom - HeatMapZoomWidget::kMarginDip - (view->HasHorizontalScroll() ? HeatMapView::kScrollbarDip : 0), button.bottom,
                              L"and its bottom");
            Assert::AreEqual (100, HeatMapZoomWidget::GetPercent (view->GetCellPx(), view->GetStartCellPx()), L"the starting zoom is 100%");

            Assert::IsTrue   (view->TryGetZoomTipAt (GetCenter (button), anchor, text));
            Assert::IsTrue   (text.find (L"Ctrl+wheel") != std::wstring::npos, text.c_str());

            (void) window.OnMouse (MakePress (DxuiMouseEventKind::Move, GetCenter (button)));
            Assert::IsTrue   (window.GetTooltip().WantsTick(), L"the pointer resting on the zoom asks for no tip");
            Assert::IsFalse  (view->GetHover().has_value(), L"a cell under the zoom is framed");

            ClickAt (window, GetCenter (button));
            Assert::IsTrue   (view->GetZoomWidget().IsOpen(), L"a click opens the slider");

            track = view->GetZoomWidget().GetTrackRect();
            ClickAt (window, POINT { track.right - 1, GetCenter (track).y });
            Assert::AreEqual ((int) HeatMapView::kMaxCellPx, view->GetCellPx(), L"the right end is the largest cell");

            moved = view->GetZoomWidget().GetTrackRect();
            Assert::IsTrue   (EqualRect (&track, &moved) != FALSE, L"the slider stays put as the zoom changes the map");

            ClickAt (window, POINT { track.left, GetCenter (track).y });
            Assert::AreEqual (1, view->GetCellPx(), L"the left end is the smallest");

            ClickAt (window, GetCenter (view->GetZoomWidget().GetResetRect()));
            Assert::AreEqual (view->GetStartCellPx(), view->GetCellPx(), L"Reset fits the map again");

            ClickAt (window, GetCenter (button));
            Assert::IsFalse  (view->GetZoomWidget().IsOpen(), L"a second click closes it");
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
            int             before = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 3);
            cell   = view->GetCellRect (target);
            zoom   = window.GetTextZoom();
            before = view->GetCellPx();

            wheel.kind        = DxuiMouseEventKind::Wheel;
            wheel.ctrl        = true;
            wheel.wheelDelta  = 1.0f;
            wheel.positionDip = { cell.left + 1, cell.top + 1 };

            Assert::IsTrue   (window.OnMouse (wheel));
            Assert::IsTrue   (view->GetCellPx() > before, L"the map zoomed");
            Assert::AreEqual (zoom, window.GetTextZoom(), L"and the text did not");
            Assert::IsTrue   (view->GetCellRect (target).top <= wheel.positionDip.y && wheel.positionDip.y <= view->GetCellRect (target).bottom, L"about the pointer");

            bounds            = view->GetBounds();
            wheel.positionDip = { bounds.left + 2, bounds.top + 2 };
            (void) window.OnMouse (wheel);
            Assert::IsTrue   (window.GetTextZoom() > zoom, L"off the map, Ctrl+wheel still sizes the text");
        }



        //  The tip waits for the machine to say what last wrote and read the
        //  cell, so it is never shown with one line and then grown; once they
        //  are in, it shows at once, with no dwell.
        TEST_METHOD (OverTheMapTheTipShowsWholeWithNoDwell)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost      host;
            HeatMapWindow    window (theme, host);
            HeatMapView    * view   = nullptr;
            RECT             cell   = {};
            DxuiMouseEvent   move;
            HeatAccessHover  hover;
            Word             target = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 3);
            cell   = view->GetCellRect (target);

            move.kind        = DxuiMouseEventKind::Move;
            move.positionDip = { cell.left + 1, cell.top + 1 };
            (void) window.OnMouse (move);

            Assert::IsFalse (window.GetTooltip().IsVisible(), L"not before its writer and reader are in");
            Assert::IsTrue  (view->GetHover().has_value(), L"but the cell is framed");

            hover.address = target;
            view->SetHoverAccess (std::make_shared<const HeatAccessHover> (hover));
            (void) window.OnMouse (move);

            Assert::IsTrue   (window.GetTooltip().IsVisible(), L"no dwell once they are");
            Assert::AreEqual (view->GetTipText (target), window.GetTooltip().GetText());
            Assert::IsTrue   (window.GetTooltip().GetText().starts_with (std::format (L"${:04X}  untouched\n", target)), L"the whole tip");
        }
    };
}
