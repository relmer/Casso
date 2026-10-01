#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewMenuTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewMenuHost
    //
    //  A host that keeps the memory windows it was told to open or close.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ViewMenuHost : public IDebuggerWindowHost
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
            if (address.has_value())
            {
                openedMemory.push_back (window);
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

        std::vector<int>  openedMemory;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewMenuWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ViewMenuWindow : public DebuggerWindow
    {
    public:
        ViewMenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::CloseFloatingPane;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowViewMenuTests
    //
    //  Every pane closes from its close button, and the View menu lists every
    //  debug window and shows a closed one where it last was.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowViewMenuTests)
    {
    public:

        TEST_METHOD (ClosingADockedFixedPaneHidesItInPlace)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            ViewMenuHost    host;
            ViewMenuWindow  window (theme, host);
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            window.ClosePane (DebuggerLayout::kRegisters);

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kRegisters), L"the registers closed");
            Assert::IsTrue  (window.GetPaneLayout().IsDocked (DebuggerLayout::kRegisters), L"the layout keeps its place");

            window.ShowPane (DebuggerLayout::kRegisters);

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kRegisters), L"the View menu shows it again");
        }


        TEST_METHOD (ClosingAFloatingFixedPaneKeepsItFloating)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            ViewMenuHost    host;
            ViewMenuWindow  window (theme, host);
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.EditPaneLayout().Float (DebuggerLayout::kWatches, L"monitor", RECT { 100, 100, 500, 400 }));

            window.CloseFloatingPane (DebuggerLayout::kWatches);

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kWatches), L"the watches closed");
            Assert::IsTrue  (window.GetPaneLayout().IsFloating (DebuggerLayout::kWatches), L"it opens floating again");

            window.ShowPane (DebuggerLayout::kWatches);

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kWatches));
            Assert::IsTrue (window.GetPaneLayout().IsFloating (DebuggerLayout::kWatches));
        }


        TEST_METHOD (ViewMenuListsEveryDebugWindow)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            ViewMenuHost               host;
            ViewMenuWindow             window (theme, host);
            std::vector<std::wstring>  panes;



            window.OnCreate();
            panes = window.GetViewMenuPanes();

            for (const wchar_t * pane : { DebuggerLayout::kCode,  DebuggerLayout::kRegisters, DebuggerLayout::kStack,
                                          DebuggerLayout::kCallStack, DebuggerLayout::kBreakpoints, DebuggerLayout::kWatches,
                                          DebuggerLayout::kTrace, DebuggerLayout::kConsole })
            {
                Assert::IsTrue (std::ranges::find (panes, std::wstring (pane)) != panes.end(), pane);
            }

            for (int memory = 1; memory <= DebuggerViewState::kMaxMemoryWindows; memory++)
            {
                Assert::IsTrue (std::ranges::find (panes, DebuggerLayout::GetMemoryPaneId (memory)) != panes.end());
            }
        }


        TEST_METHOD (ShowingAClosedMemoryWindowOpensIt)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            ViewMenuHost    host;
            ViewMenuWindow  window (theme, host);
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            window.ShowPane (DebuggerLayout::GetMemoryPaneId (3));

            Assert::AreEqual ((size_t) 1, host.openedMemory.size(), L"the memory window opened");
            Assert::AreEqual (3, host.openedMemory[0]);
        }
    };
}
