#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerBreakpointPaneContentTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BarHost
    //
    //  A host that keeps the actions and AppleWin lines the window sends.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BarHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) override { lines.push_back ({ line, mode }); }
        void  RunDebuggerAction        (const DebuggerAction & action)            override { actions.push_back (action); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>                       actions;
        std::vector<std::pair<std::string, CommandMode>>  lines;
        FakeHostDialogs                                   dialogs;
    };






    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ContentWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ContentWindow : public DebuggerWindow
    {
    public:
        ContentWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::GetBreakpointsPane;
        using DebuggerWindow::GetPaneContentForTest;
        using DebuggerWindow::TakeSnapshot;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerBreakpointPaneContentTests
    //
    //  The breakpoints pane, docked or floating, is placed as its frame: the
    //  toolbar holds its height at the top and the list fills the rest.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerBreakpointPaneContentTests)
    {
    public:
        TEST_METHOD (TheContentKeepsTheBarAtTheTopAndTheListBelow)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            DxuiDpiScaler    scaler;
            IDxuiControl   * content = nullptr;
            RECT             list    = {};
            RECT             bar     = {};



            window.OnCreate();

            content = window.GetPaneContentForTest (DebuggerLayout::kBreakpoints);
            Assert::IsNotNull (content);

            content->Layout (RECT { 0, 0, 400, 300 }, scaler);
            list = window.GetBreakpointsPane().GetList()->GetBounds();

            Assert::IsTrue (window.GetBreakpointsPane().GetList()->IsVisible());
            Assert::AreEqual (300L, list.bottom);
            Assert::IsTrue (list.top > 0 && list.top < 100);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BreakpointGroupPaneTests
    //
    //  Breakpoint groups in the pane: the breakpoints in no group first with
    //  no heading, then each group under a heading with its name and count;
    //  a row's Add to breakpoint group and a heading's menu each run BPGROUP
    //  as one step on the pane's undo list.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BreakpointGroupPaneTests)
    {
    public:

        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot()
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();

            for (int id = 1; id <= 3; id++)
            {
                DebuggerViewSnapshot::BreakpointLine  bp;

                bp.id           = id;
                bp.address      = (Word) (0x0300 + id);
                bp.info.id      = id;
                bp.info.address = bp.address;
                bp.info.last    = bp.address;
                bp.info.group   = (id == 2) ? std::string() : std::string ("Loop");
                snapshot->breakpoints.push_back (bp);
            }

            return snapshot;
        }

        static const DebuggerAction &  GetStepAction (const BarHost & host)
        {
            Assert::IsFalse (host.actions.empty(), L"an action ran");
            Assert::IsTrue  (host.actions.back().breakpointStep.has_value(), L"as a step on the pane's undo list");
            Assert::AreEqual ((size_t) 1, host.actions.back().breakpointStep->actions.size());

            return host.actions.back().breakpointStep->actions[0];
        }


        TEST_METHOD (UngroupedRowsComeFirstThenEachGroupUnderAHeading)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            DxuiListView   * list = nullptr;



            window.OnCreate();
            window.TakeSnapshot (MakeSnapshot());

            list = window.GetBreakpointsPane().GetList();

            Assert::AreEqual ((size_t) 1, list->GetGroups().size(), L"one heading");
            Assert::AreEqual (std::wstring (L"Loop (2)"), list->GetGroups()[0].label);
            Assert::AreEqual (1, list->GetGroups()[0].firstRow, L"after the ungrouped row");
            Assert::AreEqual (2, window.GetBreakpointsPane().GetBreakpointOfRow (0)->id, L"the ungrouped breakpoint first");
        }


        //  A collapsed group is collapsed again after its count changes,
        //  which changes its heading.
        TEST_METHOD (ACollapsedGroupIsCollapsedAgainAfterItsCountChanges)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            DxuiListView   * list     = nullptr;
            auto             snapshot = MakeSnapshot();



            window.OnCreate();
            window.TakeSnapshot (snapshot);

            list = window.GetBreakpointsPane().GetList();
            list->SetGroupCollapsed (0, true);

            snapshot = MakeSnapshot();
            snapshot->breakpoints[1].info.group = "Loop";
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (std::wstring (L"Loop (3)"), list->GetGroups()[0].label);
            Assert::IsTrue   (list->IsGroupCollapsed (0), L"collapsed again");
        }


        TEST_METHOD (AddToAGroupActsOnTheSelectionAsOneStep)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            DxuiListView   * list = nullptr;
            PaneMenuItems    items;



            window.OnCreate();
            window.TakeSnapshot (MakeSnapshot());

            list = window.GetBreakpointsPane().GetList();
            list->SetSelectedRows ({ 0, 1 }, 0);

            Assert::IsTrue (window.GetBreakpointsPane().AddMenuItems (list, 0, items));

            for (const auto & [label, run] : items)
            {
                if (label == PaneMenu::MakeInSubmenu (L"Add to breakpoint group", L"Loop"))
                {
                    run();
                }
            }

            Assert::AreEqual (std::string ("\"Loop\" 2,1"), GetStepAction (host).command.text, L"every selected breakpoint");
            Assert::IsTrue   (std::ranges::any_of (items, [] (const auto & item) { return item.first == L"Remove from group"; }), L"one selected is in a group");
            Assert::IsTrue   (std::ranges::any_of (items, [] (const auto & item) { return item.first == PaneMenu::MakeInSubmenu (L"Add to breakpoint group", L"New group..."); }));
        }


        TEST_METHOD (AHeadingsMenuActsOnItsGroup)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            PaneMenuItems    items;



            window.OnCreate();
            window.TakeSnapshot (MakeSnapshot());
            window.GetBreakpointsPane().AddHeadingItems ("Loop", items);

            for (const wchar_t * label : { L"Enable all", L"Disable all", L"Rename group...", L"Ungroup", L"Delete group and its breakpoints" })
            {
                Assert::IsTrue (std::ranges::any_of (items, [label] (const auto & item) { return item.first == label; }), label);
            }

            for (const auto & [label, run] : items)
            {
                if (label == L"Disable all")
                {
                    run();
                }
            }

            Assert::AreEqual (std::string ("DISABLE \"Loop\""), GetStepAction (host).command.text);

            for (const auto & [label, run] : items)
            {
                if (label == L"Ungroup")
                {
                    run();
                }
            }

            Assert::AreEqual (std::string ("UNGROUP \"Loop\""), GetStepAction (host).command.text);
        }
    };
}