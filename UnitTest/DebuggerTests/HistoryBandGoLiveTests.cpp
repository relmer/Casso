#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/HistoryBand.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HistoryBandGoLiveTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GoLiveHost
    //
    //  A host that keeps each line the window runs, in order, and counts the
    //  pauses it is sent.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class GoLiveHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { lines.push_back (line); }
        void  RunDebuggerAction       (const DebuggerAction & action)            override { lines.push_back (action.echo); }
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

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { lines.push_back (line); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs           dialogs;
        std::vector<std::string>  lines;
        int                       pauses = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GoLiveWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class GoLiveWindow : public DebuggerWindow
    {
    public:
        GoLiveWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::MakePendingSeek;
        using DebuggerWindow::GetCodeList;

        //  The band across the top of the first disassembly view: the one
        //  over its rows, rather than the registers pane's.
        HistoryBand *  GetCodeBand()
        {
            HistoryBand  * found = nullptr;
            RECT           rows  = GetCodeList (0)->GetBounds();



            for (size_t i = 0; i < GetChildCount() && found == nullptr; i++)
            {
                HistoryBand  * band = dynamic_cast<HistoryBand *> (GetChild (i));
                RECT           area = (band != nullptr) ? band->GetBounds() : RECT {};

                if (band != nullptr && area.left < rows.right && rows.left < area.right && area.bottom <= rows.top)
                {
                    found = band;
                }
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryBandGoLiveTests
    //
    //  A press on a history band's Go live link, routed as the window routes
    //  a click. History moves only under a stopped machine, so while the
    //  machine runs on behind live, the link stops it first and goes live and
    //  runs on once it has, as the timeline's live end does; stopped, it goes
    //  live at once and stays stopped.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HistoryBandGoLiveTests)
    {
    public:

        static std::shared_ptr<DebuggerViewSnapshot> MakeBehindLive (bool isPaused)
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->isPaused                   = isPaused;
            snapshot->history.isRecording        = true;
            snapshot->history.isBehindLive       = true;
            snapshot->history.instructionsBehind = 3;
            snapshot->history.cyclesBehind       = 9;

            return snapshot;
        }


        //  Opens the window behind live, places the band's link with a paint,
        //  and presses it as a click does.
        static void  OpenAndPressGoLive (GoLiveWindow & window, bool isPaused)
        {
            DxuiDpiScaler             scaler;
            MockDxuiPainter           painter;
            MockDxuiTextRenderer      text;
            MockDxuiTheme             theme;
            HistoryBand             * band  = nullptr;
            const RecordedTextCall  * link  = nullptr;
            RECT                      area  = {};
            DxuiMouseEvent            press;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeBehindLive (isPaused));
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            band = window.GetCodeBand();

            Assert::IsNotNull (band, L"the disassembly view has a history band");
            Assert::IsTrue    (band->IsVisible(), L"shown behind live");

            band->Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (link == nullptr && call.kind == RecordedTextKind::DrawString && call.text == HistoryBand::kGoLiveText)
                {
                    link = &call;
                }
            }

            Assert::IsNotNull (link, L"the link is drawn");

            area = band->GetBounds();

            press.kind        = DxuiMouseEventKind::Down;
            press.button      = DxuiMouseButton::Left;
            press.positionDip = POINT { (LONG) (link->x + link->width) - 4, (area.top + area.bottom) / 2 };

            Assert::IsTrue (window.OnMouse (press), L"the press is taken");
        }



        TEST_METHOD (RunningBehindLive_TheLinkStopsTheMachineThenGoesLiveAndRunsOn)
        {
            CassoTheme    theme  = CassoTheme::MakeSkeuomorphic();
            GoLiveHost    host;
            GoLiveWindow  window (theme, host);



            OpenAndPressGoLive (window, false);

            Assert::AreEqual (1, host.pauses, L"the running machine is stopped first");
            Assert::IsTrue   (host.lines.empty(), L"and nothing goes live while it runs");

            window.MakePendingSeek();
            Assert::IsTrue (host.lines.empty(), L"nor before the stop arrives");

            window.TakeSnapshot (MakeBehindLive (true));
            window.MakePendingSeek();

            Assert::AreEqual ((size_t) 2,           host.lines.size(), L"once stopped, the go live and the run");
            Assert::AreEqual (std::string ("LIVE"), host.lines[0]);
            Assert::AreEqual (std::string ("G"),    host.lines[1], L"and it runs on, as it was");
            Assert::AreEqual (1,                    host.pauses);

            window.MakePendingSeek();
            Assert::AreEqual ((size_t) 2, host.lines.size(), L"made once");
        }



        TEST_METHOD (StoppedBehindLive_TheLinkGoesLiveAndStaysStopped)
        {
            CassoTheme    theme  = CassoTheme::MakeSkeuomorphic();
            GoLiveHost    host;
            GoLiveWindow  window (theme, host);



            OpenAndPressGoLive (window, true);
            window.MakePendingSeek();

            Assert::AreEqual (0,                    host.pauses, L"nothing to stop");
            Assert::AreEqual ((size_t) 1,           host.lines.size(), L"the go live alone");
            Assert::AreEqual (std::string ("LIVE"), host.lines[0]);
        }
    };
}
