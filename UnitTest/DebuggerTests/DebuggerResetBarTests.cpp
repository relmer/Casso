#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerResetBarTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BarResetHost
    //
    //  A host that keeps the lines, actions and emulator commands the window
    //  sent it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BarResetHost : public IDebuggerWindowHost
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
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) override
        {
            inModeLines.push_back (line);
            inModeModes.push_back (mode);
        }

        void  RunDebuggerAction (const DebuggerAction &) override { actions++; }

        void  RunEmulatorCommand (int commandId) override { emulatorCommands.push_back (commandId); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }
        std::string     GetDebuggerCommandBarDock()       override { return savedDock; }

        void  SetDebuggerCommandBarDock (const std::string & dock) override { savedDock = dock; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  inModeLines;
        std::vector<CommandMode>  inModeModes;
        std::vector<int>          emulatorCommands;
        int                       actions = 0;
        FakeHostDialogs           dialogs;
        std::string               savedDock = "left 120";
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BarResetWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BarResetWindow : public DebuggerWindow
    {
    public:
        BarResetWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyKeyScheme;
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::GetCommandBar;
        using DebuggerWindow::GetConsoleBar;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::kDialectEntry;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::IsPaneShown;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerResetBarTests
    //
    //  Window > Reset window layout puts the command bar back in its default
    //  place: the top band, at its start, under the menu bar.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerResetBarTests)
    {
    public:

        TEST_METHOD (ResetWindowLayoutDocksTheCommandBarAtTheTop)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            BarResetHost               host;
            BarResetWindow             window (theme, host);
            DxuiDpiScaler              scaler;
            const DxuiPopupMenuItem  * reset  = nullptr;
            RECT                       bar    = {};



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetCommandBar()->IsVertical(), L"the saved place is down the left");

            for (const DxuiMenuBarItem & menu : window.GetMenuBarItems())
            {
                for (const DxuiPopupMenuItem & row : menu.submenu)
                {
                    if (row.command != nullptr && row.command->label == L"Reset window layout")
                    {
                        reset = &row;
                    }
                }
            }

            Assert::IsNotNull (reset);
            reset->command->dispatch();

            bar = window.GetCommandBar()->GetBounds();

            Assert::AreEqual (std::string ("top 0"), host.savedDock);
            Assert::IsFalse  (window.GetCommandBar()->IsVertical(), L"across the top");
            Assert::IsTrue   (bar.top < 120, L"right under the menu bar");
        }
    };
}