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
            RECT            bounds = {};
            int             top    = 0;



            host.heatMapOptions = "fade=30 view=code cumulative";
            Build (window);

            Assert::IsTrue   (window.GetHeatMapView()->GetOptions().cumulative);
            Assert::AreEqual (30, window.GetHeatMapView()->GetOptions().fadeSeconds);
            Assert::AreEqual ((size_t) 1, host.optionsSent.size(), L"sent once as the window opens");
            Assert::AreEqual (std::string ("fade=30 view=code cumulative"), host.optionsSent.back());

            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);
            bounds = window.GetHeatMapView()->GetBounds();
            top    = bounds.top + HeatMapView::kBarDip + HeatMapView::kBarDip / 2;

            Assert::IsTrue   (window.OnMouse (MakePress (DxuiMouseEventKind::Down, { bounds.left + HeatMapView::kGutterDip + 5, top })));
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
            RECT            map    = {};
            POINT           at     = {};
            int             pitch  = 0;



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            view  = window.GetHeatMapView();
            map   = view->GetMapRect();
            pitch = view->GetCellPx() + HeatMapView::kStreetPx;
            at    = { map.left + 0x10 * pitch + 1, map.top + 0x05 * pitch + 1 };

            host.memoryMoves.clear();
            window.OnMouse (MakePress (DxuiMouseEventKind::Down, at));
            window.OnMouse (MakePress (DxuiMouseEventKind::Up,   at));
            Relayout (window);

            Assert::IsFalse  (host.memoryMoves.empty(), L"a memory window was moved");
            Assert::AreEqual (1, host.memoryMoves.back().first);
            Assert::AreEqual ((Word) 0x0510, host.memoryMoves.back().second.value_or (0));
            Assert::IsFalse  (view->IsVisible(), L"Memory 1 came forward over the heat map it shares a group with");
        }
    };
}