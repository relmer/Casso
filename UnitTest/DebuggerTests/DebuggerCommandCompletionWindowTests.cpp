#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

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
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::GetPaneOfFocus;
        using DebuggerWindow::OnKey;

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
            window.FocusControl (window.GetCommandBox());
        }



        TEST_METHOD (Tab_CompletesTheCommandWordAndCyclesTheMatches)
        {
            CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
            CompletionHost    host;
            CompletionWindow  window (theme, host);



            Open (window);
            window.GetCommandBox()->SetText (L"bpc");
            window.GetCommandBox()->SetSelection (3, 3);

            window.Press (VK_TAB);
            Assert::AreEqual (std::wstring (L"bpc"), window.GetCommandBox()->GetText());

            window.Press (VK_TAB);
            Assert::AreEqual (std::wstring (L"bpchange"), window.GetCommandBox()->GetText());

            window.Press (VK_TAB, true);
            Assert::AreEqual (std::wstring (L"bpc"), window.GetCommandBox()->GetText());
        }



        TEST_METHOD (RightArrow_TakesTheGrayEarlierLine)
        {
            CassoTheme        theme = CassoTheme::MakeSkeuomorphic();
            CompletionHost    host;
            CompletionWindow  window (theme, host);



            Open (window);
            window.GetCommandBox()->SetText (L"bpm 400");
            window.Press (VK_RETURN);

            window.GetCommandBox()->SetText (L"bp");
            window.GetCommandBox()->SetSelection (2, 2);
            window.Press (VK_F8);
            Assert::AreEqual (std::wstring (L"bpm 400"), window.GetCommandBox()->GetText(), L"F8 finds the earlier line");

            window.GetCommandBox()->SetText (L"bp");
            window.GetCommandBox()->SetSelection (2, 2);
            window.Press (VK_RIGHT);
            Assert::AreEqual (std::wstring (L"bpm 400"), window.GetCommandBox()->GetText(), L"Right takes the gray line");
        }
    };
}