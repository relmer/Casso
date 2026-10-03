#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
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
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  SetDebuggerHeatMapShown (bool shown) override { recording.push_back (shown); }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<bool>  recording;

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
    //  The heat map pane is closed until the View menu opens it, and the host
    //  records for it only while it is open and in front.
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



        TEST_METHOD (ThePaneIsClosedAndNothingRecordsUntilTheViewMenuShowsIt)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost                host;
            HeatMapWindow              window (theme, host);
            std::vector<std::wstring>  panes;



            Build (window);
            window.SyncHeatMapRecording();
            panes = window.GetViewMenuPanes();

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kHeatMap));
            Assert::IsTrue  (host.recording.empty(), L"a closed pane records nothing");
            Assert::IsTrue  (std::ranges::find (panes, std::wstring (DebuggerLayout::kHeatMap)) != panes.end(), L"the View menu lists it");

            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            Assert::IsTrue   (window.IsPaneShown (DebuggerLayout::kHeatMap));
            Assert::AreEqual ((size_t) 1, host.recording.size());
            Assert::IsTrue   (host.recording.back(), L"shown, it records");
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
            window.ShowPane (DebuggerLayout::kConsole);
            Relayout (window);

            Assert::IsTrue  (window.IsPaneShown (DebuggerLayout::kHeatMap), L"still open, behind the console");
            Assert::IsFalse (window.IsHeatMapRecording());
        }



        TEST_METHOD (AClickOnAModeInTheWindowReachesThePane)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            HeatMapHost     host;
            HeatMapWindow   window (theme, host);
            DxuiMouseEvent  click;
            RECT            bounds = {};



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            Relayout (window);

            bounds            = window.GetHeatMapView()->GetBounds();
            click.kind        = DxuiMouseEventKind::Down;
            click.button      = DxuiMouseButton::Left;
            click.positionDip = { bounds.left + HeatMapView::kGutterDip + HeatMapView::kTabDip + 5, bounds.top + HeatMapView::kBarDip / 2 };

            Assert::IsTrue   (window.OnMouse (click));
            Assert::AreEqual ((int) HeatMapView::Mode::Code, (int) window.GetHeatMapView()->GetMode());
        }



        TEST_METHOD (TheDefaultLayoutTabsTheHeatMapWithTheConsole)
        {
            DxuiPaneLayout             layout = DebuggerLayout::Restore (L"");
            std::vector<std::wstring>  group;



            Assert::IsTrue (layout.Contains (DebuggerLayout::kHeatMap));

            group = layout.GetGroup (DebuggerLayout::kConsole);
            Assert::IsTrue (std::ranges::find (group, std::wstring (DebuggerLayout::kHeatMap)) != group.end());
        }
    };
}
