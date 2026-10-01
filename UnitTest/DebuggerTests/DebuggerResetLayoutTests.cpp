#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerResetLayoutTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ResetHost
    //
    //  A host that keeps the lines, actions and emulator commands the window
    //  sent it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ResetHost : public IDebuggerWindowHost
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

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  inModeLines;
        std::vector<CommandMode>  inModeModes;
        std::vector<int>          emulatorCommands;
        int                       actions = 0;
        FakeHostDialogs           dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ResetWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ResetWindow : public DebuggerWindow
    {
    public:
        ResetWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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
    //  DebuggerResetLayoutTests
    //
    //  Window > Reset window layout puts every pane back, closed ones too.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerResetLayoutTests)
    {
    public:

        TEST_METHOD (ResetWindowLayoutReopensAClosedPane)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            ResetHost                  host;
            ResetWindow                window (theme, host);
            DxuiDpiScaler              scaler;
            const DxuiPopupMenuItem  * reset = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            window.ClosePane (DebuggerLayout::kWatches);
            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kWatches));

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
            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kWatches));
        }
    };
}
