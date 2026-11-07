#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerStatusBarTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StatusBarDebuggerHost
    //
    //  A host that keeps nothing, and reports a replay running when told to.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StatusBarDebuggerHost : public IDebuggerWindowHost
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

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }
        bool  IsReplayingHistory()                                               override { return isReplaying; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        bool  isReplaying = false;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StatusBarWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StatusBarWindow : public DebuggerWindow
    {
    public:
        StatusBarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::ApplyTextZoom;
        using DebuggerWindow::GetTextZoom;
        using DebuggerWindow::GetZoomResetRect;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::GetWindowZoom;
        using DebuggerWindow::GetTextZoomPosition;
        using DebuggerWindow::GetTextZoomAt;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::UpdateStatusBar;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::HasTopLayer;
        using DebuggerWindow::kStatusReplay;
        using DebuggerWindow::kStatusHistory;
        using DebuggerWindow::kStatusZoom;

        void  Build()
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
        }

        bool  Mouse (DxuiMouseEventKind kind, POINT point)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Move) ? DxuiMouseButton::None : DxuiMouseButton::Left;
            ev.positionDip = point;
            return OnMouse (ev);
        }

        static bool  GetTip (const StatusBarWindow & window, POINT point, DxuiTip & tip)
        {
            return static_cast<const IDxuiControl &> (window).TryGetTip (point, tip);
        }

        bool  Wheel (POINT point, float notches)
        {
            DxuiMouseEvent  ev;

            ev.kind        = DxuiMouseEventKind::Wheel;
            ev.wheelDelta  = notches;
            ev.positionDip = point;
            return OnMouse (ev);
        }

        POINT  GetZoomFieldCenter() const
        {
            RECT  r = GetStatusBar()->GetFieldRect (kStatusZoom);

            return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowStatusBarTests
    //
    //  The status bar along the window's bottom edge: its zoom field opens a
    //  slider that sets the panes' text size, its history parts follow the
    //  snapshot, and the replay note follows the host.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowStatusBarTests)
    {
    public:

        TEST_METHOD (TheBarLiesAlongTheBottomWithTheZoomAtTheRight)
        {
            CassoTheme             theme = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);



            window.Build();

            Assert::IsNotNull (window.GetStatusBar());
            Assert::AreEqual (840L,  window.GetStatusBar()->GetBounds().bottom);
            Assert::AreEqual (1100L, window.GetStatusBar()->GetFieldRect (window.kStatusZoom).right);
            Assert::AreEqual (std::wstring (L"100%"), window.GetStatusBar()->GetField (window.kStatusZoom).text);
        }


        //  The zoom panel is the heat map's: the pointer over the zoom field
        //  opens it above the bar, and a press or drag on its track sets the
        //  panes' text size, from 50% at the left to 300% at the right.
        TEST_METHOD (HoveringTheZoomOpensThePanelThatSetsTheTextSize)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);
            RECT                   track  = {};
            long                   middle = 0;



            window.Build();
            window.Mouse (DxuiMouseEventKind::Move, window.GetZoomFieldCenter());

            Assert::IsTrue (window.IsZoomPopupOpen(), L"the pointer over the zoom opens the panel");
            Assert::IsTrue (window.HasTopLayer());
            Assert::IsTrue (window.GetZoomPopupRect().bottom <= window.GetStatusBar()->GetBounds().top, L"the panel opens above the bar");

            track  = window.GetWindowZoom().GetTrackRect();
            middle = (track.top + track.bottom) / 2;

            window.Mouse (DxuiMouseEventKind::Down, POINT { track.right - 1, middle });
            Assert::AreEqual (3.0f, window.GetTextZoom(), 0.001f, L"the track's right end is the largest size");

            window.Mouse (DxuiMouseEventKind::Move, POINT { track.left - 50, middle });
            window.Mouse (DxuiMouseEventKind::Up,   POINT { track.left - 50, middle });
            Assert::AreEqual (0.5f, window.GetTextZoom(), 0.001f, L"dragged past the left end, the smallest");
            Assert::AreEqual (std::wstring (L"50%"), window.GetStatusBar()->GetField (window.kStatusZoom).text);
            Assert::IsTrue   (window.IsZoomPopupOpen(), L"open through the drag");
        }


        TEST_METHOD (MovingAwayAPressElsewhereOrEscapeClosesThePanel)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);
            DxuiKeyEvent           escape = { DxuiKeyEventKind::Down, VK_ESCAPE, false, false, false, false };



            window.Build();

            window.Mouse (DxuiMouseEventKind::Move, window.GetZoomFieldCenter());
            window.Mouse (DxuiMouseEventKind::Move, POINT { 400, 400 });
            Assert::IsFalse (window.IsZoomPopupOpen(), L"the pointer moved away");

            window.Mouse (DxuiMouseEventKind::Move, window.GetZoomFieldCenter());
            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            Assert::IsFalse (window.IsZoomPopupOpen(), L"a press on the zoom closes it");

            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            Assert::IsTrue  (window.IsZoomPopupOpen(), L"and opens it again");

            window.OnKey (escape);
            Assert::IsFalse (window.IsZoomPopupOpen(), L"Escape closes it");
        }


        TEST_METHOD (ATextSizeFromTheKeysMovesTheZoomAndItsThumb)
        {
            CassoTheme             theme = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);



            window.Build();
            window.ApplyTextZoom (1.5f);

            Assert::AreEqual (std::wstring (L"150%"), window.GetStatusBar()->GetField (window.kStatusZoom).text);
            Assert::AreEqual (0.0f, StatusBarWindow::GetTextZoomPosition (0.5f), 0.001f, L"the smallest at the left");
            Assert::AreEqual (1.0f, StatusBarWindow::GetTextZoomPosition (3.0f), 0.001f, L"the largest at the right");
            Assert::AreEqual (1.5f, StatusBarWindow::GetTextZoomAt (StatusBarWindow::GetTextZoomPosition (1.5f)), 0.001f, L"the thumb at 150%");
        }


        TEST_METHOD (TheWheelOverTheZoomFieldStepsTheTextSize)
        {
            CassoTheme             theme = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);



            window.Build();

            Assert::IsTrue (window.Wheel (window.GetZoomFieldCenter(), 1.0f));
            Assert::AreEqual (1.1f, window.GetTextZoom(), 0.001f);

            window.Wheel (window.GetZoomFieldCenter(), -1.0f);
            window.Wheel (window.GetZoomFieldCenter(), -1.0f);
            Assert::AreEqual (0.9f, window.GetTextZoom(), 0.001f);
        }


        TEST_METHOD (ResetSetsOneHundredPercentAndHasATip)
        {
            CassoTheme             theme = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);
            RECT                   reset = {};
            POINT                  at    = {};
            DxuiTip                tip;



            window.Build();
            window.ApplyTextZoom (2.0f);
            window.Mouse (DxuiMouseEventKind::Move, window.GetZoomFieldCenter());

            reset = window.GetZoomResetRect();
            at    = POINT { (reset.left + reset.right) / 2, (reset.top + reset.bottom) / 2 };

            window.Mouse (DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (StatusBarWindow::GetTip (window, at, tip));
            Assert::AreEqual (std::wstring (L"Reset to 100%"), tip.text);

            window.Mouse (DxuiMouseEventKind::Down, at);
            window.Mouse (DxuiMouseEventKind::Up,   at);

            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f);
            Assert::IsTrue   (window.IsZoomPopupOpen(), L"the panel stays open");
        }

        TEST_METHOD (TheHistoryPartsFollowTheSnapshotAndTheReplayFollowsTheHost)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost                  host;
            StatusBarWindow                        window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.Build();

            snapshot->history.isRecording = true;
            snapshot->history.hasHistory  = true;
            snapshot->history.beginCycle  = 1020484 * 2;
            snapshot->history.usedBytes   = 95;
            snapshot->history.budgetBytes = 100;
            window.SetSnapshotForTest (snapshot);
            host.isReplaying = true;

            window.UpdateStatusBar();

            const DxuiStatusBar::Field &  history = window.GetStatusBar()->GetField (window.kStatusHistory);

            Assert::AreEqual (std::wstring (L"History buffer remaining 5%"), history.text);
            Assert::AreEqual (0.95f, history.bar, 0.001f);
            Assert::AreEqual (DebuggerStatusText::kEmptyArgb, history.barFromArgb);
            Assert::AreEqual (DebuggerStatusText::GetBudgetColor (0.95f), history.barToArgb);
            Assert::IsFalse (window.GetStatusBar()->GetField (window.kStatusReplay).text.empty());

            host.isReplaying = false;
            window.UpdateStatusBar();
            Assert::IsTrue (window.GetStatusBar()->GetField (window.kStatusReplay).text.empty());

            //  Once the oldest history is being dropped, the bar goes and the
            //  section says where history begins.
            snapshot->history.isFull    = true;
            snapshot->history.usedBytes = 100;
            window.UpdateStatusBar();

            Assert::IsTrue (history.text.starts_with (L"History begins at Power + "), history.text.c_str());
            Assert::AreEqual (-1.0f, history.bar);

            snapshot->history.isRecording = false;
            window.UpdateStatusBar();

            Assert::AreEqual (std::wstring (L"History off"), history.text);
            Assert::AreEqual (-1.0f, history.bar);
        }
    };
}
