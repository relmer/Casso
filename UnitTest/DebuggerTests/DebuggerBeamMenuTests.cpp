#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerBeamMenuTests
//
//  The Debug menu's Run one frame, on F6, and Show beam on screen, which the
//  host keeps.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerBeamMenuTests
{
    class BeamMenuHost : public IDebuggerWindowHost
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

        bool  IsBeamOverlayOn  ()        override { return beamOverlay; }
        void  SetBeamOverlayOn (bool on) override { beamOverlay = on; }

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
        bool                      beamOverlay = false;
        FakeHostDialogs           dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BeamMenuWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BeamMenuWindow : public DebuggerWindow
    {
    public:
        BeamMenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BeamMenuRows
    //
    //  Finds a menu by its title and a row by its label.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BeamMenuRows
    {
    public:
        static const std::vector<DxuiPopupMenuItem> *  GetMenu (const std::vector<DxuiMenuBarItem> & items, const std::wstring & title)
        {
            for (const DxuiMenuBarItem & item : items)
            {
                if (item.label == title)
                {
                    return &item.submenu;
                }
            }

            return nullptr;
        }


        static const DxuiPopupMenuItem *  GetRow (const std::vector<DxuiPopupMenuItem> & menu, const std::wstring & label)
        {
            for (const DxuiPopupMenuItem & row : menu)
            {
                if (row.command != nullptr && row.command->label == label)
                {
                    return &row;
                }
            }

            return nullptr;
        }


        static const DxuiPopupMenuItem *  GetRow (const std::vector<DxuiMenuBarItem> & items, const std::wstring & title, const std::wstring & label)
        {
            const std::vector<DxuiPopupMenuItem>  * menu = GetMenu (items, title);



            return (menu != nullptr) ? GetRow (*menu, label) : nullptr;
        }
    };





    TEST_CLASS (DebuggerBeamMenuTests)
    {
    public:

        TEST_METHOD (RunOneFrameIsOnF6AndRunsWhenStopped)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            BeamMenuHost                           host;
            BeamMenuWindow                         window (theme, host);
            const DxuiPopupMenuItem              * row      = nullptr;
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.OnCreate();

            row = BeamMenuRows::GetRow (window.GetMenuBarItems(), L"&Debug", L"Run one frame");
            Assert::IsNotNull (row);
            Assert::AreEqual  (std::wstring (L"F6"), row->command->accelerator);

            Assert::IsFalse   (row->command->IsEnabled(), L"nothing to run from before a stop");

            snapshot->isPaused = true;
            window.SetSnapshotForTest (snapshot);
            Assert::IsTrue    (row->command->IsEnabled());

            row->command->dispatch();
            Assert::AreEqual  (1, host.actions);
        }


        TEST_METHOD (ShowBeamOnScreenTogglesTheHostsOverlay)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            BeamMenuHost               host;
            BeamMenuWindow             window (theme, host);
            const DxuiPopupMenuItem  * row    = nullptr;



            window.OnCreate();

            row = BeamMenuRows::GetRow (window.GetMenuBarItems(), L"&Debug", L"Show beam on screen");
            Assert::IsNotNull (row);
            Assert::IsFalse   (row->command->IsChecked());

            row->command->dispatch();
            Assert::IsTrue    (host.beamOverlay);
            Assert::IsTrue    (row->command->IsChecked());

            row->command->dispatch();
            Assert::IsFalse   (host.beamOverlay);
        }
    };
}