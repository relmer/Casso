#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewCascadeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CascadeHost
    //
    //  A host that does nothing; the tests read only the window's menus.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CascadeHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
        void  RunDebuggerAction        (const DebuggerAction &)                   override {}
        void  RunEmulatorCommand       (int)                                      override {}
        void  PauseDebugger            ()                                         override {}
        void  SetDebuggerCodeLines     (int, int)                                 override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop       (Word, int)                                override {}
        void  SetDebuggerFollowView    (int)                                      override {}
        void  CloseDebuggerCodeView    (int)                                      override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory       (int, const std::string &)                 override {}
        void  ScrollDebuggerCode       (int, int)                                 override {}
        void  OnDebuggerWindowClosed   ()                                         override {}
        void  SetDebuggerKeyScheme     (const std::string &)                      override {}
        void  SetDebuggerLayout        (const std::string &)                      override {}
        void  SetDebuggerOpenViews     (const std::string &)                      override {}
        void  SetDebuggerPlacement     (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                 override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs  dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CascadeWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CascadeWindow : public DebuggerWindow
    {
    public:
        CascadeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::SetSnapshotForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowViewCascadeTests
    //
    //  The View menu folds the windows that come in several instances, and
    //  the device panels, into a cascade each; the rest stay rows of their own.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowViewCascadeTests)
    {
    public:

        static const std::vector<DxuiPopupMenuItem> &  GetView (const CascadeWindow & window)
        {
            for (const DxuiMenuBarItem & item : window.GetMenuBarItems())
            {
                if (item.label == L"&View")
                {
                    return item.submenu;
                }
            }

            Assert::Fail (L"no View menu");
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


        static void  AssertCascade (const std::vector<DxuiPopupMenuItem> & view, const std::wstring & label, const std::vector<std::wstring> & rows)
        {
            const DxuiPopupMenuItem  * cascade = GetRow (view, label);



            Assert::IsNotNull (cascade, label.c_str());
            Assert::IsTrue    (cascade->kind == DxuiPopupMenuItem::Kind::Submenu, label.c_str());
            Assert::AreEqual  (rows.size(), cascade->children.size(), label.c_str());

            for (size_t i = 0; i < rows.size(); i++)
            {
                Assert::AreEqual (rows[i], cascade->children[i].command->label);
            }
        }


        TEST_METHOD (DisassemblyAndMemoryAreCascades)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            CascadeHost    host;
            CascadeWindow  window (theme, host);



            window.OnCreate();
            window.SetWindowMenus();

            AssertCascade (GetView (window), L"Disassembly", { L"Disassembly 1", L"Disassembly 2", L"Disassembly 3", L"Disassembly 4" });
            AssertCascade (GetView (window), L"Memory",      { L"Memory 1", L"Memory 2", L"Memory 3", L"Memory 4" });

            Assert::IsNull (GetRow (GetView (window), L"Memory 1"), L"Memory 1 is still a row of View itself");
        }


        TEST_METHOD (SingleWindowsStayRowsOfTheirOwn)
        {
            CassoTheme                 theme = CassoTheme::MakeSkeuomorphic();
            CascadeHost                host;
            CascadeWindow              window (theme, host);
            const DxuiPopupMenuItem  * row   = nullptr;



            window.OnCreate();
            window.SetWindowMenus();

            row = GetRow (GetView (window), L"Registers");

            Assert::IsNotNull (row);
            Assert::IsTrue    (row->kind == DxuiPopupMenuItem::Kind::Command);
        }


        TEST_METHOD (DevicePanelsAreACascade)
        {
            CassoTheme     theme    = CassoTheme::MakeSkeuomorphic();
            CascadeHost    host;
            CascadeWindow  window   (theme, host);
            auto           snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->panels = { { "mmu", "Memory map", true }, { "disk", "Disk II", false } };

            window.OnCreate();
            window.SetSnapshotForTest (snapshot);
            window.SetWindowMenus();

            AssertCascade (GetView (window), L"Device panels", { L"MMU", L"Disk II" });
        }
    };
}
