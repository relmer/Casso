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
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::GetZoomSlider;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::UpdateStatusBar;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::HasTopLayer;
        using DebuggerWindow::kStatusReplay;
        using DebuggerWindow::kStatusBegin;
        using DebuggerWindow::kStatusBudget;
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


        TEST_METHOD (PressingTheZoomOpensASliderThatSetsTheTextSize)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);
            RECT                   popup  = {};
            RECT                   track  = {};



            window.Build();
            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());

            Assert::IsTrue (window.IsZoomPopupOpen());
            Assert::IsTrue (window.HasTopLayer());
            Assert::AreEqual (50.0f,  window.GetZoomSlider().GetMin());
            Assert::AreEqual (300.0f, window.GetZoomSlider().GetMax());
            Assert::AreEqual (10.0f,  window.GetZoomSlider().Step());

            popup = window.GetZoomPopupRect();
            Assert::IsTrue (popup.bottom <= window.GetStatusBar()->GetBounds().top, L"the popup opens above the bar");

            //  A press at the track's right end runs it to the largest size.
            track = window.GetZoomSlider().GetRect();
            window.Mouse (DxuiMouseEventKind::Down, POINT { track.right - 1, (track.top + track.bottom) / 2 });
            window.Mouse (DxuiMouseEventKind::Up,   POINT { track.right - 1, (track.top + track.bottom) / 2 });

            Assert::IsTrue (window.GetTextZoom() > 1.0f, L"the slider sets the panes' text size");
            Assert::AreEqual (DebuggerStatusText::GetZoomText (window.GetTextZoom()),
                              window.GetStatusBar()->GetField (window.kStatusZoom).text);
        }


        TEST_METHOD (APressElsewhereOrEscapeClosesThePopup)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);
            DxuiKeyEvent           escape = { DxuiKeyEventKind::Down, VK_ESCAPE, false, false, false, false };



            window.Build();

            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            Assert::IsFalse (window.IsZoomPopupOpen(), L"a second press on the zoom closes it");

            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            window.Mouse (DxuiMouseEventKind::Down, POINT { 400, 400 });
            Assert::IsFalse (window.IsZoomPopupOpen(), L"a press elsewhere closes it");

            window.Mouse (DxuiMouseEventKind::Down, window.GetZoomFieldCenter());
            window.OnKey (escape);
            Assert::IsFalse (window.IsZoomPopupOpen(), L"Escape closes it");
        }


        TEST_METHOD (ATextSizeFromTheKeysMovesTheZoomAndItsSlider)
        {
            CassoTheme             theme = CassoTheme::MakeSkeuomorphic();
            StatusBarDebuggerHost  host;
            StatusBarWindow        window (theme, host);



            window.Build();
            window.ApplyTextZoom (1.5f);

            Assert::AreEqual (std::wstring (L"150%"), window.GetStatusBar()->GetField (window.kStatusZoom).text);
            Assert::AreEqual (150.0f, window.GetZoomSlider().GetValue(), 0.01f);
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

            Assert::AreEqual (std::wstring (L"Begins at 2.0 s"),  window.GetStatusBar()->GetField (window.kStatusBegin).text);
            Assert::AreEqual (std::wstring (L"History 95% full"), window.GetStatusBar()->GetField (window.kStatusBudget).text);
            Assert::AreEqual (0.95f, window.GetStatusBar()->GetField (window.kStatusBudget).meter, 0.001f);
            Assert::AreEqual (theme.ErrorForeground(), window.GetStatusBar()->GetField (window.kStatusBudget).meterArgb,
                              L"with a twentieth left the meter is the error color");
            Assert::IsFalse (window.GetStatusBar()->GetField (window.kStatusReplay).text.empty());

            host.isReplaying = false;
            window.UpdateStatusBar();
            Assert::IsTrue (window.GetStatusBar()->GetField (window.kStatusReplay).text.empty());
        }
    };
}
