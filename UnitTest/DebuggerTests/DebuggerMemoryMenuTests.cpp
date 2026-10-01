#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiTabGroup.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerMemoryMenuTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryMenuHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemoryMenuHost : public IDebuggerWindowHost
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
    //  MemoryMenuWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemoryMenuWindow : public DebuggerWindow
    {
    public:
        MemoryMenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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
    //  DebuggerMemoryMenuTests
    //
    //  The memory windows open from the View menu's Memory cascade; a memory
    //  tab group has no "+" tab.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerMemoryMenuTests)
    {
    public:

        static bool IsMemory (IDxuiControl * content)
        {
            return content != nullptr && content->GetAccessibleName().starts_with (L"Memory ");
        }


        TEST_METHOD (ViewMenuListsTheMemoryWindowsAsACascade)
        {
            CassoTheme                              theme   = CassoTheme::MakeSkeuomorphic();
            MemoryMenuHost                          host;
            MemoryMenuWindow                        window (theme, host);
            DxuiDpiScaler                           scaler;
            const std::vector<DxuiPopupMenuItem>  * view    = nullptr;
            const DxuiPopupMenuItem               * cascade = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            for (const DxuiMenuBarItem & item : window.GetMenuBarItems())
            {
                if (item.label == L"&View")
                {
                    view = &item.submenu;
                }
            }

            Assert::IsNotNull (view);

            for (const DxuiPopupMenuItem & row : *view)
            {
                Assert::IsFalse (row.command != nullptr && row.command->label.starts_with (L"Memory ") , L"no memory window at the top level");

                if (row.kind == DxuiPopupMenuItem::Kind::Submenu && row.command != nullptr && row.command->label == L"Memory")
                {
                    cascade = &row;
                }
            }

            Assert::IsNotNull (cascade, L"a Memory cascade");
            Assert::AreEqual  ((size_t) DebuggerViewState::kMaxMemoryWindows, cascade->children.size());
            Assert::AreEqual  (std::wstring (L"Memory 1"), cascade->children[0].command->label);
            Assert::AreEqual  (std::wstring (L"Memory 4"), cascade->children[3].command->label);
        }


        TEST_METHOD (AMemoryTabGroupHasNoNewTab)
        {
            CassoTheme                            theme    = CassoTheme::MakeSkeuomorphic();
            MemoryMenuHost                        host;
            MemoryMenuWindow                      window (theme, host);
            DxuiDpiScaler                         scaler;
            DxuiDockSite                        * site     = nullptr;
            int                                   found    = 0;
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

            for (size_t i = 0; i < site->GetGroupCount(); i++)
            {
                auto * group = site->GetGroup (i);

                if (group == nullptr || group->GetTabCount() == 0 || !IsMemory (group->GetContent (0)))
                {
                    continue;
                }

                RECT  plus = group->GetNewTabRect();

                found++;
                Assert::IsTrue (IsRectEmpty (&plus) != FALSE, L"no + tab after the memory tabs");
            }

            Assert::IsTrue (found > 0, L"a memory tab group was checked");
        }
    };
}