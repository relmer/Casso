#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerStartupStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StartupHost
    //
    //  A host that offers a saved layout and saved open views, and keeps the
    //  actions the window sent it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StartupHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
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

        void  RunDebuggerAction (const DebuggerAction & action) override { actions.push_back (action); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return layout; }
        std::string     GetDebuggerOpenViews()            override { return openViews; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>  actions;
        std::string                  layout;
        std::string                  openViews;
        FakeHostDialogs              dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StartupWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StartupWindow : public DebuggerWindow
    {
    public:
        StartupWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::ClosePane;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowStartupStateTests
    //
    //  What the window puts back when it opens: each tab group shows the tab
    //  it showed when it closed, the panels it reopens write nothing to the
    //  console, and a pane turned on from the View menu opens docked.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowStartupStateTests)
    {
    public:

        static std::wstring  GetActive (const DxuiPaneLayoutNode * node, const std::wstring & pane)
        {
            std::wstring  found;



            if (node == nullptr)
            {
                return {};
            }

            if (node->kind == DxuiPaneLayoutNode::Kind::Split)
            {
                found = GetActive (node->first.get(), pane);
                return found.empty() ? GetActive (node->second.get(), pane) : found;
            }

            if (std::find (node->panes.begin(), node->panes.end(), pane) == node->panes.end())
            {
                return {};
            }

            return node->panes[(size_t) node->active];
        }


        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshotWithPanel (const std::string & id)
        {
            auto                 snapshot = std::make_shared<DebuggerViewSnapshot>();
            DiagnosticsSnapshot  diagnostics;



            diagnostics.id     = id;
            diagnostics.device = id;

            snapshot->diagnostics = { diagnostics };
            snapshot->panels      = { { id, id, true } };
            return snapshot;
        }


        TEST_METHOD (APanelReopenedAtStartupLeavesItsGroupOnTheSavedTab)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);
            DxuiDpiScaler   scaler;
            std::wstring    mmu    = DebuggerLayout::GetDiagnosticsPaneId ("mmu");
            std::wstring    before;



            host.openViews = "panel=mmu";

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            before = GetActive (window.GetPaneLayout().GetRoot(), mmu);
            Assert::AreNotEqual (mmu, before, L"the saved layout shows another tab of the panel's group");

            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());
            window.TakeSnapshot (MakeSnapshotWithPanel ("mmu"));

            Assert::AreEqual (before, GetActive (window.GetPaneLayout().GetRoot(), mmu), L"the reopened panel does not come forward");
        }


        TEST_METHOD (ACodeViewOpenAtStartupLeavesItsGroupOnTheSavedTab)
        {
            CassoTheme      theme    = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window   (theme, host);
            DxuiDpiScaler   scaler;
            auto            snapshot = std::make_shared<DebuggerViewSnapshot>();



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            snapshot->codeOpen[1] = true;
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (std::wstring (DebuggerLayout::kCode), GetActive (window.GetPaneLayout().GetRoot(), DebuggerLayout::kCode));
        }


        TEST_METHOD (APanelReopenedAtStartupWritesNothingToTheConsole)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost               host;
            StartupWindow             window (theme, host);
            ControllerRig             rig;
            std::vector<std::string>  lines;



            host.openViews = "panel=mmu";

            window.OnCreate();
            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());

            Assert::AreEqual ((size_t) 1, host.actions.size(), L"the panel reopens through the window's own action");

            lines = rig.view.ExecuteAction (rig.controller.GetSession(), host.actions[0]);

            Assert::IsTrue (rig.view.IsPanelOpen ("mmu"));
            Assert::IsTrue (lines.empty(), L"no echo and no reply");
        }


        TEST_METHOD (TheDebugMenuEndsWithTheMachineRestarts)
        {
            CassoTheme                              theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost                             host;
            StartupWindow                           window (theme, host);
            const std::vector<DxuiPopupMenuItem>  * debug  = nullptr;
            std::vector<std::wstring>               labels;



            window.OnCreate();
            window.SetWindowMenus();

            for (const DxuiMenuBarItem & item : window.GetMenuBarItems())
            {
                debug = (item.label == L"&Debug") ? &item.submenu : debug;
            }

            Assert::IsNotNull (debug);

            for (const DxuiPopupMenuItem & row : *debug)
            {
                labels.push_back ((row.command != nullptr) ? row.command->label : std::wstring (L"-"));
            }

            Assert::IsTrue   (labels.size() >= 4);
            Assert::AreEqual (std::wstring (L"Restart under debugger"), labels[labels.size() - 1]);
            Assert::AreEqual (std::wstring (L"Power cycle"),            labels[labels.size() - 2]);
            Assert::AreEqual (std::wstring (L"Reset"),                  labels[labels.size() - 3]);
            Assert::AreEqual (std::wstring (L"-"),                      labels[labels.size() - 4], L"a separator sets them apart");
        }


        TEST_METHOD (APaneTurnedOnFromTheViewMenuOpensDocked)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.EditPaneLayout().AutoHide (DebuggerLayout::kRegisters, DxuiDockSide::Right));
            window.ClosePane (DebuggerLayout::kRegisters);

            window.ShowPane (DebuggerLayout::kRegisters);

            Assert::IsTrue  (window.GetPaneLayout().IsDocked     (DebuggerLayout::kRegisters));
            Assert::IsFalse (window.GetPaneLayout().IsAutoHidden (DebuggerLayout::kRegisters));
        }
    };
}
