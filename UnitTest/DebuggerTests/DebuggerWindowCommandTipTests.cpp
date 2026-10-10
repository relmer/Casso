#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/DebuggerCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCommandTipTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipHost
    //
    //  A host that does nothing; the tests read only the window's own state.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
        void  RunDebuggerAction        (const DebuggerAction &)                   override {}
        void  RunEmulatorCommand       (int)                                      override {}
        void  PauseDebugger            ()                                         override {}
        void  SetDebuggerCodeLines     (int, int)                                 override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop       (Word, int)                                override {}
        void  SetDebuggerFollowView    (int)                                      override {}
        void  CloseDebuggerCodeView    (int)                                      override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory       (int, const std::string &)                 override {}
        void  ScrollDebuggerCode       (int, int)                                 override {}
        void  OnDebuggerWindowClosed   ()                                         override {}
        void  SetDebuggerKeyScheme     (const std::string &)                      override {}
        void  SetDebuggerLayout        (const std::string &)                      override {}
        void  SetDebuggerOpenViews     (const std::string &)                      override {}
        void  SetDebuggerPlacement     (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                 override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs  dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipWindow : public DebuggerWindow
    {
    public:
        TipWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::GetCommandBar;
        using DebuggerWindow::GetMenuBar;
        using DebuggerWindow::GetTooltip;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowCommandTipTests
    //
    //  A command bar button shows its tip when the pointer rests on it, and
    //  no tip is left over a menu that opens.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowCommandTipTests)
    {
    public:

        static constexpr int  kWidth  = 1400;
        static constexpr int  kHeight = 900;


        static void  Move (TipWindow & window, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = at;

            (void) window.OnMouse (ev);
        }


        static POINT  GetCenter (const RECT & rect)
        {
            return POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        TEST_METHOD (RestingOnBreakAsksForItsTip)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            TipHost        host;
            TipWindow      window (theme, host);
            DxuiDpiScaler  scaler;
            RECT           brk   = {};



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);

            Assert::IsTrue (window.GetCommandBar()->TryGetEntryRect (DebuggerCommands::kPause, brk));

            Move (window, GetCenter (brk));

            Assert::IsTrue  (window.GetTooltip().WantsTick(), L"no tip pending over Break");
        }


        TEST_METHOD (OpeningAMenuDropsThePendingTip)
        {
            CassoTheme       theme = CassoTheme::MakeSkeuomorphic();
            TipHost          host;
            TipWindow        window (theme, host);
            DxuiDpiScaler    scaler;
            RECT             brk   = {};
            RECT             file  = {};
            DxuiMouseEvent   press;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);

            Assert::IsTrue (window.GetCommandBar()->TryGetEntryRect (DebuggerCommands::kPause, brk));

            Move (window, GetCenter (brk));
            Assert::IsTrue (window.GetTooltip().WantsTick(), L"no tip pending over Break");

            file              = window.GetMenuBar()->GetMenuRect (0);
            press.kind        = DxuiMouseEventKind::Down;
            press.button      = DxuiMouseButton::Left;
            press.positionDip = GetCenter (file);

            (void) window.OnMouse (press);

            Assert::IsTrue  (window.GetMenuBar()->IsOpen(), L"File did not open");
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"a tip is still on its way over the open menu");
            Assert::IsFalse (window.GetTooltip().IsVisible());

            Move (window, GetCenter (brk));

            Assert::IsFalse (window.GetTooltip().WantsTick(), L"a tip started while the menu is open");
        }
    };
}
