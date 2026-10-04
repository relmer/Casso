#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Config/GlobalUserPrefs.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerStatusBarBeamZoomTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BeamZoomHost
    //
    //  A host that keeps the text size and the beam mark the window sets, as
    //  the preferences and the screen do.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BeamZoomHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                     override {}
        void  PauseDebugger            ()                                        override {}
        void  SetDebuggerCodeLines     (int, int)                                override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                override {}
        void  SetDebuggerCodeTop       (Word, int)                               override {}
        void  SetDebuggerFollowView    (int)                                     override {}
        void  CloseDebuggerCodeView    (int)                                     override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                 override {}
        void  GoToDebuggerMemory       (int, const std::string &)                override {}
        void  ScrollDebuggerCode       (int, int)                                override {}
        void  OnDebuggerWindowClosed   ()                                        override {}
        void  SetDebuggerKeyScheme     (const std::string &)                     override {}
        void  SetDebuggerLayout        (const std::string &)                     override {}
        void  SetDebuggerOpenViews     (const std::string &)                     override {}
        void  SetDebuggerPlacement     (const RECT &)                            override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        bool  IsBeamOverlayOn()          override { return beamOverlay; }
        void  SetBeamOverlayOn (bool on) override { beamOverlay = on; }

        int   GetDebuggerTextZoomPercent()             override { return zoomPercent; }
        void  SetDebuggerTextZoomPercent (int percent) override { zoomPercent = percent; saves++; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        bool  beamOverlay = false;
        int   zoomPercent = 100;
        int   saves       = 0;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BeamZoomWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BeamZoomWindow : public DebuggerWindow
    {
    public:
        BeamZoomWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::ApplyTextZoom;
        using DebuggerWindow::GetTextZoom;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::UpdateStatusBar;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::kStatusBeam;
        using DebuggerWindow::kStatusBudget;
        using DebuggerWindow::kStatusZoom;

        void  Build()
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
        }

        void  Press (POINT point)
        {
            DxuiMouseEvent  ev;

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = point;
            OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            OnMouse (ev);
        }

        POINT  GetBeamFieldCenter() const
        {
            RECT  r = GetStatusBar()->GetFieldRect (kStatusBeam);

            return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerStatusBarBeamZoomTests
    //
    //  The status bar shows where the beam is and turns its mark on the screen
    //  on and off; the history meter takes fixed colors, green empty to blue
    //  full; and the text size is kept in the preferences.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerStatusBarBeamZoomTests)
    {
    public:

        using Beam = DebuggerViewSnapshot::BeamState;


        //  Every channel within one step: a blend's rounding is not the point.
        static bool  AreChannelsNear (uint32_t a, uint32_t b)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                int  delta = (int) ((a >> shift) & 0xFFu) - (int) ((b >> shift) & 0xFFu);

                if (delta > 1 || delta < -1)
                {
                    return false;
                }
            }

            return true;
        }


        TEST_METHOD (BeamText_IsDecimalThenHex)
        {
            Assert::AreEqual (std::wstring (L"Scanline:cycle 192:1 ($0C0:01)"), DebuggerStatusText::GetBeamText (Beam { 192, 1 }));
            Assert::AreEqual (std::wstring (L"Scanline:cycle 261:64 ($105:40)"), DebuggerStatusText::GetBeamText (Beam { 261, 64 }));
            Assert::AreEqual (std::wstring(), DebuggerStatusText::GetBeamText (std::nullopt));
        }


        TEST_METHOD (BudgetColor_IsGreenEmptyBlendingToBlueFull)
        {
            uint32_t  half = DebuggerStatusText::GetBudgetColor (0.5f);



            Assert::AreEqual (DebuggerStatusText::kEmptyArgb, DebuggerStatusText::GetBudgetColor (0.0f));
            Assert::IsTrue   (AreChannelsNear (DebuggerStatusText::kFullArgb, DebuggerStatusText::GetBudgetColor (1.0f)));
            Assert::AreEqual (DxuiColor::Lerp (DebuggerStatusText::kEmptyArgb, DebuggerStatusText::kFullArgb, 0.5f), half);

            //  Green at empty, blue at full, and never red on the way.
            Assert::IsTrue (((DebuggerStatusText::kEmptyArgb >> 8) & 0xFFu) > (DebuggerStatusText::kEmptyArgb & 0xFFu));
            Assert::IsTrue ((DebuggerStatusText::kFullArgb & 0xFFu) > ((DebuggerStatusText::kFullArgb >> 8) & 0xFFu));
            Assert::IsTrue (((half >> 16) & 0xFFu) < 0x80u);
        }


        TEST_METHOD (TheMeterIgnoresTheTheme)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            BeamZoomHost                           host;
            BeamZoomWindow                         window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.Build();

            snapshot->history.isRecording = true;
            snapshot->history.usedBytes   = 100;
            snapshot->history.budgetBytes = 100;
            window.SetSnapshotForTest (snapshot);
            window.UpdateStatusBar();

            Assert::IsTrue (AreChannelsNear (DebuggerStatusText::kFullArgb, window.GetStatusBar()->GetField (window.kStatusBudget).meterArgb));
        }


        TEST_METHOD (TheBarShowsTheBeamAndAPressTogglesItsMark)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            BeamZoomHost                           host;
            BeamZoomWindow                         window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.Build();

            snapshot->beam = Beam { 192, 1 };
            window.SetSnapshotForTest (snapshot);
            window.UpdateStatusBar();

            Assert::AreEqual (std::wstring (L"Scanline:cycle 192:1 ($0C0:01)"), window.GetStatusBar()->GetField (window.kStatusBeam).text);

            window.Press (window.GetBeamFieldCenter());
            Assert::IsTrue (host.beamOverlay, L"a press turns the mark on");

            window.Press (window.GetBeamFieldCenter());
            Assert::IsFalse (host.beamOverlay, L"a second press turns it off");
        }


        TEST_METHOD (TheTextSizeIsSavedAndRestored)
        {
            CassoTheme      theme = CassoTheme::MakeSkeuomorphic();
            BeamZoomHost    host;



            host.zoomPercent = 130;

            {
                BeamZoomWindow  window (theme, host);

                window.Build();
                Assert::AreEqual (1.3f, window.GetTextZoom(), 0.001f, L"the saved size is restored at startup");
                Assert::AreEqual (std::wstring (L"130%"), window.GetStatusBar()->GetField (window.kStatusZoom).text);

                window.ApplyTextZoom (1.5f);
                Assert::AreEqual (150, host.zoomPercent, L"a change is saved");
            }
        }


        TEST_METHOD (ThePreferenceRoundTripsAndIsClamped)
        {
            InMemoryFileSystem  fs;
            GlobalUserPrefs     saved;
            GlobalUserPrefs     loaded;
            GlobalUserPrefs     clamped;
            HRESULT             hr     = S_OK;



            Assert::AreEqual (100, saved.debuggerTextZoomPercent, L"100% until the user changes it");

            saved.debuggerTextZoomPercent = 170;
            hr = saved.Save (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = loaded.Load (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::AreEqual (170, loaded.debuggerTextZoomPercent);

            saved.debuggerTextZoomPercent = 5000;
            hr = saved.Save (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = clamped.Load (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::AreEqual (300, clamped.debuggerTextZoomPercent);
        }
    };
}
