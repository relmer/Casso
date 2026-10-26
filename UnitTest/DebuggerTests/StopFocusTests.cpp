#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/StopFocus.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace StopFocusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StopHost
    //
    //  A host with no state; every query returns an empty value.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StopHost : public IDebuggerWindowHost
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

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StopWindow
    //
    //  The debugger window with its controls built and no HWND; a test runs
    //  and stops it by passing snapshots.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StopWindow : public DebuggerWindow
    {
    public:
        StopWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            DxuiDpiScaler  scaler;



            m_theme = &theme;
            m_host  = &host;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
            TakeSnapshot (MakeSnapshot (true));
        }

        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetConsolePane;
        using DebuggerWindow::GetPaneOfFocus;
        using DebuggerWindow::OnWindowFocusChanged;

        //  LDA #$41 at $0300 with the PC on it.
        static std::shared_ptr<const DebuggerViewSnapshot> MakeSnapshot (bool isPaused)
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;



            lda.address     = 0x0300;
            lda.bytes       = "A9 41";
            lda.instruction = "LDA #$41";
            lda.isCurrent   = true;

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = isPaused;
            snapshot->codeViews[0] = { lda };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            return snapshot;
        }

        void  RunAndStop()
        {
            TakeSnapshot (MakeSnapshot (false));
            TakeSnapshot (MakeSnapshot (true));
        }

        void  ClickRun()
        {
            RECT            run = {};
            DxuiMouseEvent  ev;



            Assert::IsTrue (GetCommandBar()->TryGetEntryRect (DebuggerCommands::kRun, run));

            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (run.left + run.right) / 2, (run.top + run.bottom) / 2 };
            ev.kind        = DxuiMouseEventKind::Down;
            (void) OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            (void) OnMouse (ev);
        }

        void  TypeInFocus (wchar_t ch)
        {
            DxuiKeyEvent  ev;



            ev.kind = DxuiKeyEventKind::Char;
            ev.vk   = (WPARAM) ch;
            (void) OnKey (ev);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StopFocusTests
    //
    //  At a stop, focus moves from the console to the disassembly unless the
    //  console received the last input.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StopFocusTests)
    {
    public:

        TEST_METHOD (OnlyAStopFromRunningMovesTheFocus)
        {
            StopFocus  rule;



            Assert::IsFalse (rule.OnSnapshot (true),  L"a stop with no run before it");
            Assert::IsFalse (rule.OnSnapshot (false), L"running");
            Assert::IsTrue  (rule.OnSnapshot (true),  L"the stop");
            Assert::IsFalse (rule.OnSnapshot (true),  L"stopped again with no run between");
        }


        TEST_METHOD (ConsoleInputKeepsTheFocusWhereItIs)
        {
            StopFocus  rule;



            rule.OnInput (true);
            (void) rule.OnSnapshot (false);
            Assert::IsFalse (rule.OnSnapshot (true), L"the console received the last input");

            rule.OnInput (false);
            (void) rule.OnSnapshot (false);
            Assert::IsTrue (rule.OnSnapshot (true), L"input elsewhere came after it");
        }


        TEST_METHOD (AStopAfterTheToolbarsRunMovesTheKeysToTheDisassembly)
        {
            CassoTheme  theme = CassoTheme::MakeSkeuomorphic();
            StopHost    host;
            StopWindow  window (theme, host);



            window.FocusControl (window.GetConsolePane().GetCommandLine().GetBox());
            window.ClickRun();
            window.RunAndStop();

            Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), window.GetPaneOfFocus());
        }


        TEST_METHOD (ACommandTypedInTheConsoleKeepsTheKeysThereAtAStop)
        {
            CassoTheme  theme = CassoTheme::MakeSkeuomorphic();
            StopHost    host;
            StopWindow  window (theme, host);



            window.FocusControl (window.GetConsolePane().GetCommandLine().GetBox());
            window.TypeInFocus (L'g');
            window.RunAndStop();

            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetPaneOfFocus());
        }


        TEST_METHOD (AnotherWindowTakingTheFocusMovesTheKeysToTheDisassemblyAtAStop)
        {
            CassoTheme  theme = CassoTheme::MakeSkeuomorphic();
            StopHost    host;
            StopWindow  window (theme, host);



            window.FocusControl (window.GetConsolePane().GetCommandLine().GetBox());
            window.TypeInFocus (L'g');
            window.OnWindowFocusChanged (false);
            window.RunAndStop();

            Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), window.GetPaneOfFocus());
        }
    };
}
