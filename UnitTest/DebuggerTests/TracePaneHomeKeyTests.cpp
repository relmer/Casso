#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiListView.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceHomeHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TraceHomeHost : public IDebuggerWindowHost
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
    //  TraceHomeWindow
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TraceHomeWindow : public DebuggerWindow
    {
    public:
        TraceHomeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetPaneOfFocus;

        bool  Press (WPARAM vk)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, false, false };

            return OnKey (ev) || RouteMappedKey (ev);
        }

        DxuiListView *  GetTrace()
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (GetPaneOfControl (GetChild (i)) == DebuggerLayout::kTrace)
                {
                    return dynamic_cast<DxuiListView *> (GetChild (i));
                }
            }

            return nullptr;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TracePaneHomeKeyTests
    //
    //  Home in the trace pane goes to entry 0 and leaves the focus where it is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TracePaneHomeKeyTests)
    {
    public:
        TEST_METHOD (Home_InTheTracePane_StaysInThePane)
        {
            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            TraceHomeHost    host;
            TraceHomeWindow  window   (theme, host);
            DxuiDpiScaler    scaler;
            auto             snapshot = std::make_shared<DebuggerViewSnapshot>();
            DxuiListView   * trace    = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            snapshot->trace.isOn  = true;
            snapshot->trace.total = 200;
            snapshot->trace.first = 0;
            snapshot->trace.entries.resize (200);
            window.TakeSnapshot (snapshot);
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            trace = window.GetTrace();
            Assert::IsNotNull (trace);

            window.FocusControl (trace);
            Assert::IsTrue (window.GetFocused() == trace, L"the trace pane has the focus");

            trace->SetSelectedRow (150);
            window.Press (VK_HOME);

            Assert::IsTrue  (window.GetFocused() == trace, L"Home leaves the focus in the trace pane");
            Assert::AreEqual (0, trace->GetSelectedRow(), L"and goes to entry 0");
        }
    };
}
