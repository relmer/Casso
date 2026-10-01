#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerUndoBarTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndoBarHost
    //
    //  A host that keeps nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class UndoBarHost : public IDebuggerWindowHost
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
    //  UndoBarWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class UndoBarWindow : public DebuggerWindow
    {
    public:
        UndoBarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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
        using DebuggerWindow::GetUndoBar;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowUndoBarTests
    //
    //  The registers, stack and watch panes each have a bar of Undo and Redo
    //  over their list, which belongs to the pane, so it goes with the pane into
    //  a floating window. Both buttons are disabled with nothing to act on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowUndoBarTests)
    {
    public:

        TEST_METHOD (RegistersStackAndWatchesEachHaveUndoAndRedo)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            UndoBarHost    host;
            UndoBarWindow  window (theme, host);
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            for (const wchar_t * pane : { DebuggerLayout::kRegisters, DebuggerLayout::kStack, DebuggerLayout::kWatches })
            {
                DxuiToolbar  * bar  = window.GetUndoBar (pane);
                RECT           rect = {};

                Assert::IsNotNull (bar, pane);
                Assert::IsTrue    (window.GetPaneOfControl (bar) == pane, pane);
                Assert::IsTrue    (bar->IsVisible(), pane);
                Assert::IsTrue    (bar->TryGetEntryRect (UndoBarCommands::kUndo, rect), pane);
                Assert::IsTrue    (bar->TryGetEntryRect (UndoBarCommands::kRedo, rect), pane);
            }
        }
    };
}
