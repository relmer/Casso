#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTabFocusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TabFocusHost
    //
    //  A host that keeps nothing and answers every question with nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TabFocusHost : public IDebuggerWindowHost
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
    //  TabFocusWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TabFocusWindow : public DebuggerWindow
    {
    public:
        TabFocusWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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

        DxuiDockSite * FindDockSite()
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (auto * site = dynamic_cast<DxuiDockSite *> (GetChild (i)))
                {
                    return site;
                }
            }

            return nullptr;
        }

        void  Click (POINT pointDip)
        {
            DxuiMouseEvent  ev = {};

            ev.positionDip = pointDip;
            ev.button      = DxuiMouseButton::Left;
            ev.kind        = DxuiMouseEventKind::Down;
            (void) OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            (void) OnMouse (ev);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowTabFocusTests
    //
    //  A press on a pane's tab gives that pane the focus, as a press inside it
    //  does, so its group draws the focus outline.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowTabFocusTests)
    {
    public:

        TEST_METHOD (APressOnAnotherPanesTabFocusesThatPane)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            TabFocusHost     host;
            TabFocusWindow   window (theme, host);
            DxuiDpiScaler    scaler;
            DxuiDockSite   * site   = nullptr;
            int              tried  = 0;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.FocusControl (window.GetCommandBox());

            site = window.FindDockSite();
            Assert::IsNotNull (site, L"the dock site is one of the window's controls");
            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetPaneOfFocus(), L"the console starts with the focus");

            for (int y = 0; y < 900 && tried < 3; y += 3)
            {
                for (int x = 0; x < 1400 && tried < 3; x += 6)
                {
                    RECT          tab  = {};
                    std::wstring  tip;
                    std::wstring  pane = site->GetTabAt (POINT { x, y }, tab, tip);

                    if (pane.empty() || pane == window.GetPaneOfFocus())
                    {
                        continue;
                    }

                    window.Click (POINT { (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2 });
                    window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

                    Assert::AreEqual (pane, window.GetPaneOfFocus(), std::format (L"a press on the {} tab", pane).c_str());
                    tried++;
                }
            }

            Assert::IsTrue (tried > 0, L"the layout shows at least one other pane's tab");
        }
    };
}
