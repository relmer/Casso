#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Dxui/MockDxuiPainter.h"
#include "Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/ReplayProgressDialog.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/DebuggerCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ReplayProgressStatusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReplayProgressHost
    //
    //  A host that keeps nothing, reports the replay progress it is given,
    //  and counts the pauses and stop requests the window sends it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ReplayProgressHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override { pauses++; }
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
        bool  IsReplayingHistory()                                               override { return progress.isReplaying; }

        ReplayProgress  GetReplayProgress() override { return progress; }
        void            StopReplay()        override { stops++; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        ReplayProgress  progress;
        int             pauses = 0;
        int             stops  = 0;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReplayProgressWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ReplayProgressWindow : public DebuggerWindow
    {
    public:
        ReplayProgressWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::OnMappedCommand;
        using DebuggerWindow::IsCommandBarEntryEnabled;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::GetCommandBar;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::UpdateStatusBar;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::kStatusReplay;

        void  Build()
        {
            DxuiDpiScaler                          scaler;
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);

            //  A replay runs with the machine stopped.
            snapshot->isPaused = true;
            SetSnapshotForTest (snapshot);
        }

        bool  PressEscape()
        {
            DxuiKeyEvent  escape = { DxuiKeyEventKind::Down, VK_ESCAPE, false, false, false, false };

            return OnKey (escape);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReplayProgressStatusTests
    //
    //  A long replay shows its progress in the status bar and can be stopped
    //  with Pause or Escape, which reach the host directly rather than queue
    //  behind the replay.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReplayProgressStatusTests)
    {
    public:

        TEST_METHOD (AShortReplayShowsOnlyTheNote)
        {
            ReplayProgress  progress;



            Assert::AreEqual (std::wstring(), DebuggerStatusText::GetReplayText (progress));

            progress.isReplaying = true;
            progress.elapsedMs   = DebuggerStatusText::kProgressDelayMs - 1;
            progress.fraction    = 0.42f;

            Assert::AreEqual (DebuggerStatusText::GetReplayText (true), DebuggerStatusText::GetReplayText (progress));
        }


        TEST_METHOD (ALongReplayAddsItsProgressAndHowToStopIt)
        {
            ReplayProgress  progress;
            std::wstring    text;



            progress.isReplaying = true;
            progress.elapsedMs   = DebuggerStatusText::kProgressDelayMs;
            progress.fraction    = 0.42f;

            text = DebuggerStatusText::GetReplayText (progress);

            Assert::IsTrue (text.starts_with (L"Replaying history"), text.c_str());
            Assert::IsTrue (text.find (L"42%")    != std::wstring::npos, text.c_str());
            Assert::IsTrue (text.find (L"Escape") != std::wstring::npos, text.c_str());
            Assert::IsTrue (text.find (L"Pause")  != std::wstring::npos, text.c_str());
        }


        TEST_METHOD (ALongReplayWithNoProgressOffersNoStop)
        {
            ReplayProgress  progress;



            progress.isReplaying = true;
            progress.elapsedMs   = DebuggerStatusText::kProgressDelayMs * 10;

            Assert::AreEqual (DebuggerStatusText::GetReplayText (true), DebuggerStatusText::GetReplayText (progress));
        }


        TEST_METHOD (TheStatusBarShowsTheHostsProgress)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            ReplayProgressHost    host;
            ReplayProgressWindow  window (theme, host);



            window.Build();

            host.progress.isReplaying = true;
            host.progress.elapsedMs   = DebuggerStatusText::kProgressDelayMs * 2;
            host.progress.fraction    = 0.5f;
            window.UpdateStatusBar();

            Assert::AreEqual (DebuggerStatusText::GetReplayText (host.progress),
                              window.GetStatusBar()->GetField (window.kStatusReplay).text);
            Assert::IsTrue (window.GetStatusBar()->GetField (window.kStatusReplay).text.find (L"50%") != std::wstring::npos);
        }


        TEST_METHOD (EscapeStopsAReplayAndOnlyAReplay)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            ReplayProgressHost    host;
            ReplayProgressWindow  window (theme, host);



            window.Build();

            window.PressEscape();
            Assert::AreEqual (0, host.stops, L"no replay, nothing to stop");

            host.progress.isReplaying = true;

            Assert::IsTrue (window.PressEscape(), L"the window takes the key");
            Assert::AreEqual (1, host.stops, L"Escape stops the replay");
        }


        TEST_METHOD (PauseStopsAReplayAndIsOfferedDuringOne)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            ReplayProgressHost    host;
            ReplayProgressWindow  window (theme, host);



            window.Build();

            Assert::IsFalse (window.IsCommandBarEntryEnabled (DebuggerCommands::kPause), L"a stopped machine has nothing to pause");

            host.progress.isReplaying = true;

            Assert::IsTrue (window.IsCommandBarEntryEnabled (DebuggerCommands::kPause), L"a replay can be stopped");

            window.OnMappedCommand ((int) DebuggerKeySchemes::Action::Pause);

            Assert::AreEqual (1, host.stops, L"Pause stops the replay");
        }


        TEST_METHOD (AStoppedCommandIsPutInWords)
        {
            HistoryStatus  status;



            status.isRecording  = true;
            status.isBehindLive = true;
            status.outcome      = ReverseOutcome::Stopped;

            Assert::IsTrue (HistoryBand::GetOutcomeText (ReverseOutcome::Stopped).find (L"last position") != std::wstring::npos);
            Assert::AreEqual (std::wstring (L"Stopped"), HistoryBand::GetShortText (status));
        }

        TEST_METHOD (APressOfThePauseButtonStopsAReplay)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            ReplayProgressHost    host;
            ReplayProgressWindow  window (theme, host);
            RECT                  pause = {};
            DxuiMouseEvent        ev;



            window.Build();
            host.progress.isReplaying = true;

            Assert::IsTrue (window.GetCommandBar()->TryGetEntryRect (DebuggerCommands::kRunOrPause, pause));

            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (pause.left + pause.right) / 2, (pause.top + pause.bottom) / 2 };

            for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Move, DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
            {
                ev.kind = kind;
                window.OnMouse (ev);
            }

            Assert::AreEqual (1, host.stops, L"the button stops the replay");
        }


        //  The progress dialog is due once a search that reports its progress
        //  has run past the delay, and not for a short one or one that reports
        //  none, which can't be stopped.
        TEST_METHOD (TheProgressDialogIsDueForALongSearch)
        {
            constexpr uint64_t  kLong = ReplayProgressDialog::kShowAfterMs;



            Assert::IsTrue  (ReplayProgressDialog::IsDue ({ true,  kLong,     0.25f }));
            Assert::IsFalse (ReplayProgressDialog::IsDue ({ true,  kLong - 1, 0.25f }), L"a short search");
            Assert::IsFalse (ReplayProgressDialog::IsDue ({ true,  kLong,     ReplayControl::kNoProgress }), L"no progress to report");
            Assert::IsFalse (ReplayProgressDialog::IsDue ({ false, kLong,     0.25f }), L"no search");

            Assert::AreEqual (std::wstring (L"Running backward to the previous breakpoint: 25%"), ReplayProgressDialog::GetText ({ true, kLong, 0.25f }, false));
            Assert::AreEqual (std::wstring (L"Stopping..."),                                      ReplayProgressDialog::GetText ({ true, kLong, 0.25f }, true));
        }


        //  At a few percent the fill is the whole capsule cut off by a clip at
        //  the fraction, so it has the capsule's rounded end and is inside the
        //  outline.
        TEST_METHOD (AShortFillStaysInsideTheCapsule)
        {
            static constexpr LONG   kWidth    = 400;
            static constexpr float  kFraction = 0.02f;

            ReplayProgressPanel   panel;
            CassoTheme            theme;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            bool                  isFound = false;



            scaler.SetDpi (96);
            panel.Layout      (RECT { 0, 0, kWidth, 80 }, scaler);
            panel.SetProgress (L"x", kFraction);
            panel.Paint       (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRoundedRect && call.argb == theme.Accent() && call.isClipped)
                {
                    isFound = true;

                    Assert::AreEqual ((float) kWidth, call.width, L"the fill is drawn as the whole capsule");
                    Assert::IsTrue   (call.isClipped, L"and clipped");
                    Assert::AreEqual ((LONG) (kWidth * kFraction), call.clip.right - call.clip.left, L"at the fraction");
                }
            }

            Assert::IsTrue (isFound, L"the fill was drawn");
        }


        //  Stop, the Close button and Escape each request the stop; the search
        //  is stopped once however many of them are pressed.
        TEST_METHOD (EveryWayOfClosingTheDialogStopsTheSearchOnce)
        {
            ReplayProgressDialog  dialog;
            int                   stops = 0;



            dialog.SetStop ([&stops] { stops++; });

            dialog.RequestStop();
            dialog.RequestStop();

            Assert::AreEqual (1, stops);
            Assert::IsTrue   (dialog.IsStopping());
        }
    };
}
