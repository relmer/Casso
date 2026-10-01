#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerPaneBarTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneBarHost
    //
    //  A host that keeps nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneBarHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  SetDebuggerMemoryWindow (int window, std::optional<Word> address) override
        {
            if (!address.has_value())
            {
                closedMemory.push_back (window);
            }
        }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<int>  closedMemory;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneBarWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneBarWindow : public DebuggerWindow
    {
    public:
        PaneBarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::GetMemoryBar;
        using DebuggerWindow::GetMemoryBox;
        using DebuggerWindow::GetBreakpointBar;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowPaneBarTests
    //
    //  A pane's toolbar belongs to the pane, so it goes with the pane into a
    //  floating window and takes that window's input there.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowPaneBarTests)
    {
    public:

        TEST_METHOD (BreakpointsBarIsOneOfThePanesControls)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            PaneBarHost    host;
            PaneBarWindow  window (theme, host);
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetPaneOfControl (window.GetBreakpointBar()) == DebuggerLayout::kBreakpoints, L"the bar moves with the breakpoints pane");
        }


        TEST_METHOD (MemoryBarBelongsToThePaneItSitsIn)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            PaneBarHost    host;
            PaneBarWindow  window (theme, host);
            DxuiDpiScaler  scaler;
            std::wstring   pane   = DebuggerLayout::GetMemoryPaneId (1);



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetMemoryBar()->IsVisible(), L"memory 1 is shown with its bar");
            Assert::IsTrue (window.GetPaneOfControl (window.GetMemoryBar()) == pane, L"the bar is memory 1's");
            Assert::IsTrue (window.GetPaneOfControl (window.GetMemoryBox()) == pane, L"and so is its Address box");
        }
    };
}