#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Debugger/Source/SourcePathList.h"
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
        std::string     GetDebuggerFocusedPane()          override { return focusedPane; }

        void  SetDebuggerFocusedPane (const std::string & pane) override { focusedPane = pane; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>  actions;
        std::string                  layout;
        std::string                  openViews;
        std::string                  focusedPane;
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
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::GetMemoryBox;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::OnWindowClose;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetRegisterList;
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
            auto                      rig    = std::make_unique<ControllerRig>();   // heap: the rig and the window overflow the frame (C6262)
            std::vector<std::string>  lines;



            host.openViews = "panel=mmu";

            window.OnCreate();
            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());

            Assert::AreEqual ((size_t) 1, host.actions.size(), L"the panel reopens through the window's own action");

            lines = rig->view.ExecuteAction (rig->controller.GetSession(), host.actions[0]);

            Assert::IsTrue (rig->view.IsPanelOpen ("mmu"));
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


        TEST_METHOD (TheConsoleCommandLineHasTheFocusWhenTheWindowOpens)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.GetFocused() == window.GetCommandBox(), L"the console's command line has the focus");
        }


        static void  OpenWindow (StartupWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }


        static std::string  MakeLayoutWithTraceInFront()
        {
            DxuiPaneLayout  saved = DebuggerLayout::Restore (std::wstring());



            Assert::IsTrue (saved.Activate (DebuggerLayout::kTrace), L"the trace pane shares the console's group");
            return SourcePathList::WideToUtf8 (saved.ToText());
        }


        static void  AssertPaneHasTheFocus (StartupWindow & window, const std::wstring & pane)
        {
            IDxuiControl  * focused = window.GetFocused();
            bool            isMain  = false;



            Assert::IsNotNull (focused, L"some control has the focus");

            isMain = dynamic_cast<DxuiListView *> (focused) != nullptr || dynamic_cast<DxuiHexView *> (focused) != nullptr || dynamic_cast<DxuiTextView *> (focused) != nullptr;

            Assert::AreEqual (pane, window.GetPaneOfControl (focused),                  L"the saved pane has the focus");
            Assert::AreEqual (pane, GetActive (window.GetPaneLayout().GetRoot(), pane), L"its tab is in front");
            Assert::IsTrue   (isMain,                                                   L"the pane's main control");
            Assert::IsTrue   (focused->IsVisible(),                                     L"a control that shows");
        }


        static void  AssertConsoleHasTheFocus (StartupWindow & window)
        {
            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), GetActive (window.GetPaneLayout().GetRoot(), DebuggerLayout::kConsole), L"the console tab is in front");
            Assert::IsTrue   (window.GetFocused() == window.GetCommandBox(), L"the console's command line has the focus");
            Assert::IsTrue   (window.GetCommandBox()->IsVisible(),          L"the command line shows");
        }


        TEST_METHOD (TheSavedListPaneHasTheFocusWhenTheWindowOpens)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            host.focusedPane = "registers";

            OpenWindow (window);

            AssertPaneHasTheFocus (window, DebuggerLayout::kRegisters);
        }


        TEST_METHOD (TheSavedPaneBehindAnotherTabComesForwardWithTheFocus)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            host.layout      = MakeLayoutWithTraceInFront();
            host.focusedPane = "watches";

            OpenWindow (window);

            AssertPaneHasTheFocus (window, DebuggerLayout::kWatches);
            Assert::AreEqual (std::wstring (DebuggerLayout::kTrace), GetActive (window.GetPaneLayout().GetRoot(), DebuggerLayout::kConsole), L"another group keeps its saved tab");
        }


        TEST_METHOD (TheSavedDisassemblyViewHasTheFocusWhenTheWindowOpens)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            host.focusedPane = "code";

            OpenWindow (window);

            AssertPaneHasTheFocus (window, DebuggerLayout::kCode);
        }


        TEST_METHOD (TheSavedConsoleComesForwardWithItsCommandLineFocused)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            host.layout      = MakeLayoutWithTraceInFront();
            host.focusedPane = "console";

            OpenWindow (window);

            AssertConsoleHasTheFocus (window);
        }


        TEST_METHOD (WithNothingSavedTheConsoleComesForwardWithItsCommandLineFocused)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            host.layout = MakeLayoutWithTraceInFront();

            OpenWindow (window);

            AssertConsoleHasTheFocus (window);
        }


        TEST_METHOD (ASavedPaneThatIsGoneFallsBackToTheConsole)
        {
            CassoTheme      theme         = CassoTheme::MakeSkeuomorphic();
            StartupHost     closedHost;
            StartupHost     unknownHost;
            auto            closedWindow  = std::make_unique<StartupWindow> (theme, closedHost);    // heap: two windows overflow the frame (C6262)
            auto            unknownWindow = std::make_unique<StartupWindow> (theme, unknownHost);



            closedHost.layout       = MakeLayoutWithTraceInFront();
            closedHost.focusedPane  = "memory3";
            unknownHost.layout      = MakeLayoutWithTraceInFront();
            unknownHost.focusedPane = "nosuchpane";

            OpenWindow (*closedWindow);
            OpenWindow (*unknownWindow);

            AssertConsoleHasTheFocus (*closedWindow);
            AssertConsoleHasTheFocus (*unknownWindow);
        }


        TEST_METHOD (ClosingTheWindowSavesThePaneWithTheFocus)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            StartupHost     host;
            StartupWindow   window (theme, host);



            OpenWindow (window);

            window.FocusControl (window.GetRegisterList());
            window.OnWindowClose();

            Assert::AreEqual (std::string ("registers"), host.focusedPane);
        }
    };
}
