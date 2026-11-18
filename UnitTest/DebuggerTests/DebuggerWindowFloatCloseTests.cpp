#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFloatCloseTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FloatCloseHost
    //
    //  A host that keeps nothing but the memory windows it was told to close.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FloatCloseHost : public IDebuggerWindowHost
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
    //  FloatCloseWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FloatCloseWindow : public DebuggerWindow
    {
    public:
        FloatCloseWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_look.SetTheme (&theme);
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::Layout;
        using DebuggerWindow::CloseFloatingPane;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowFloatCloseTests
    //
    //  The close button on a floating pane's window closes the pane, which
    //  keeps its floating place to open in again.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowFloatCloseTests)
    {
    public:

        TEST_METHOD (ClosingAFloatingMemoryWindowClosesItAndKeepsItFloating)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FloatCloseHost    host;
            FloatCloseWindow  window (theme, host);
            DxuiDpiScaler     scaler;
            std::wstring      pane   = DebuggerLayout::GetMemoryPaneId (2);



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetDockSite()->EditPaneLayout().Float (pane, L"monitor", RECT { 100, 100, 500, 400 }), L"memory 2 is in the layout");

            window.CloseFloatingPane (pane);

            Assert::AreEqual ((size_t) 1, host.closedMemory.size(), L"the memory window closed");
            Assert::AreEqual (2, host.closedMemory[0]);
            Assert::IsTrue   (window.GetDockSite()->GetPaneLayout().IsFloating (pane), L"it opens floating again");
        }


        TEST_METHOD (ClosingAFloatingFixedPaneClosesItAndKeepsItFloating)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FloatCloseHost    host;
            FloatCloseWindow  window (theme, host);
            DxuiDpiScaler     scaler;
            std::wstring      pane   = DebuggerLayout::kRegisters;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetDockSite()->EditPaneLayout().Float (pane, L"monitor", RECT { 100, 100, 500, 400 }));

            window.CloseFloatingPane (pane);

            Assert::IsTrue (window.GetDockSite()->GetPaneLayout().IsFloating (pane), L"the View menu opens it floating again");
        }
    };
}
