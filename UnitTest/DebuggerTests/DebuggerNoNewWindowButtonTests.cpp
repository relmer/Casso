#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiTabGroup.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerNoNewWindowButtonTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NoPlusHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class NoPlusHost : public IDebuggerWindowHost
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
    //  NoPlusWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class NoPlusWindow : public DebuggerWindow
    {
    public:
        NoPlusWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::TakeSnapshot;

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
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerNoNewWindowButtonTests
    //
    //  New memory windows and disassembly views open from the View menu: the
    //  memory bar has no New memory window button, and no tab group has a
    //  "+" tab.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerNoNewWindowButtonTests)
    {
    public:

        TEST_METHOD (TheMemoryBarHasNoNewWindowButton)
        {
            MemoryBarCommands::Handlers      handlers;
            MemoryBarCommands                commands (handlers);
            std::vector<DxuiToolbar::Entry>  entries = commands.BuildEntries (nullptr);



            for (const DxuiToolbar::Entry & entry : entries)
            {
                Assert::IsNotNull (entry.command.get());
                Assert::IsTrue    (entry.command->label != L"New memory window", L"View > Memory opens memory windows");
            }
        }


        TEST_METHOD (NoTabGroupHasANewTab)
        {
            CassoTheme                            theme    = CassoTheme::MakeSkeuomorphic();
            NoPlusHost                            host;
            NoPlusWindow                          window (theme, host);
            DxuiDpiScaler                         scaler;
            DxuiDockSite                        * site     = nullptr;
            auto                                  snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::MemoryWindow    memory;



            scaler.SetDpi (96);
            window.OnCreate();
            snapshot->machine = "Apple2e";
            memory.id         = 1;
            snapshot->memoryWindows.push_back (memory);
            window.TakeSnapshot (snapshot);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            site = window.FindDockSite();
            Assert::IsNotNull (site);
            Assert::IsTrue    (site->GetGroupCount() > 0);

            for (size_t i = 0; i < site->GetGroupCount(); i++)
            {
                auto * group = site->GetGroup (i);

                if (group == nullptr)
                {
                    continue;
                }

                RECT  plus = group->GetNewTabRect();

                Assert::IsTrue (IsRectEmpty (&plus) != FALSE, L"View > Disassembly opens disassembly views");
            }
        }
    };
}
