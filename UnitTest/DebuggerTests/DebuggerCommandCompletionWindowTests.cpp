#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCompletionWindowTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CompletionHost
    //
    //  A host that keeps nothing and answers every question with nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CompletionHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { lines.push_back (line); }
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

        std::vector<std::string>  lines;   // the lines run

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CompletionWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CompletionWindow : public DebuggerWindow
    {
    public:
        CompletionWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetConsolePane;
        using DebuggerWindow::GetPaneOfFocus;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::AppendConsole;

        void  Press (WPARAM vk, bool shift = false)
        {
            DxuiKeyEvent  ev = {};

            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = vk;
            ev.shift = shift;
            (void) OnKey (ev);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerCommandCompletionWindowTests
    //
    //  The command line's completion keys as the window routes them (FR-131).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerCommandCompletionWindowTests)
    {
    public:
        static void  Open (CompletionWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.FocusControl (window.GetConsolePane().GetCommandLine().GetBox());
        }



        TEST_METHOD (Tab_CompletesTheCommandWordAndCyclesTheMatches)
        {
            CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
            CompletionHost    host;
            CompletionWindow  window (theme, host);



            Open (window);
            window.GetConsolePane().GetCommandLine().GetBox()->SetText (L"bpc");
            window.GetConsolePane().GetCommandLine().GetBox()->SetSelection (3, 3);

            window.Press (VK_TAB);
            Assert::AreEqual (std::wstring (L"bpc"), window.GetConsolePane().GetCommandLine().GetBox()->GetText());

            window.Press (VK_TAB);
            Assert::AreEqual (std::wstring (L"bpchange"), window.GetConsolePane().GetCommandLine().GetBox()->GetText());

            window.Press (VK_TAB, true);
            Assert::AreEqual (std::wstring (L"bpc"), window.GetConsolePane().GetCommandLine().GetBox()->GetText());
        }



        TEST_METHOD (RightArrow_TakesTheGrayEarlierLine)
        {
            CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
            CompletionHost    host;
            CompletionWindow  window (theme, host);



            Open (window);
            window.GetConsolePane().GetCommandLine().GetBox()->SetText (L"bpm 400");
            window.Press (VK_RETURN);

            window.GetConsolePane().GetCommandLine().GetBox()->SetText (L"bp");
            window.GetConsolePane().GetCommandLine().GetBox()->SetSelection (2, 2);
            window.Press (VK_F8);
            Assert::AreEqual (std::wstring (L"bpm 400"), window.GetConsolePane().GetCommandLine().GetBox()->GetText(), L"F8 finds the earlier line");

            window.GetConsolePane().GetCommandLine().GetBox()->SetText (L"bp");
            window.GetConsolePane().GetCommandLine().GetBox()->SetSelection (2, 2);
            window.Press (VK_RIGHT);
            Assert::AreEqual (std::wstring (L"bpm 400"), window.GetConsolePane().GetCommandLine().GetBox()->GetText(), L"Right takes the gray line");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleWrapTests
    //
    //  A console line that wraps continues under its last field after a gap
    //  of two or more spaces, so a help row's description wraps under itself.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleWrapTests)
    {
    public:

        TEST_METHOD (AWrappedLineContinuesUnderItsLastField)
        {
            Assert::AreEqual (14, ConsoleOutput::GetWrapIndent ("    SYNTAX    description and more words"), L"under the description");
            Assert::AreEqual (30, ConsoleOutput::GetWrapIndent ("                              a description alone"), L"under a description on a row by itself");
            Assert::AreEqual (15, ConsoleOutput::GetWrapIndent ("0300: A9 00    LDA #$00"), L"under a listing's last column");
            Assert::AreEqual (0,  ConsoleOutput::GetWrapIndent ("plain text with single spaces"), L"no gap, no indent");
        }

        //  CLS and CLEAR, in either case and with a mode's prefix, clear the
        //  output here and are not run as commands; the line is in the history.
        TEST_METHOD (ClsClearsTheOutputAndIsNotRun)
        {
            for (const wchar_t * typed : { L"cls", L"CLEAR", L"  /cls ", L"!Clear" })
            {
                CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
                CompletionHost    host;
                CompletionWindow  window (theme, host);



                DebuggerCommandCompletionWindowTests::Open (window);
                window.AppendConsole ({ "LDA #$00", "STA $C030" });
                Assert::IsFalse (window.GetConsolePane().GetOutput().FormatSaved().empty(), (std::wstring (L"output before ") + typed).c_str());

                window.GetConsolePane().GetCommandLine().GetBox()->SetText (typed);
                window.Press (VK_RETURN);

                Assert::IsTrue (window.GetConsolePane().GetOutput().FormatSaved().empty(), (std::wstring (L"output after ") + typed).c_str());
                Assert::IsTrue   (host.lines.empty(), (std::wstring (L"not run ") + typed).c_str());
                Assert::AreEqual (std::wstring(), window.GetConsolePane().GetCommandLine().GetBox()->GetText(), typed);
            }
        }



        //  Ctrl+L clears the output; the line being typed is as it was.
        TEST_METHOD (CtrlLClearsTheOutput)
        {
            CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
            CompletionHost    host;
            CompletionWindow  window (theme, host);
            DxuiKeyEvent      ev    = {};



            DebuggerCommandCompletionWindowTests::Open (window);
            window.AppendConsole ({ "LDA #$00" });
            window.GetConsolePane().GetCommandLine().GetBox()->SetText (L"bp 300");

            ev.kind = DxuiKeyEventKind::Down;
            ev.vk   = 'L';
            ev.ctrl = true;
            (void) window.OnKey (ev);

            Assert::IsTrue   (window.GetConsolePane().GetOutput().FormatSaved().empty());
            Assert::AreEqual (std::wstring (L"bp 300"), window.GetConsolePane().GetCommandLine().GetBox()->GetText());
        }
    };
}
