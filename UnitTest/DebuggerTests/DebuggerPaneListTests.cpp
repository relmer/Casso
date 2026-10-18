#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerPaneListTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TestPane
    //
    //  A pane of one list in a frame, which counts what the window calls.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TestPane : public IDebuggerPane
    {
    public:
        explicit TestPane (IDebuggerPaneHost & host) : IDebuggerPane (host) {}

        std::wstring  GetId() const override { return L"testpane"; }
        std::wstring  GetTitle() const override { return L"Test pane"; }

        void  CreateControls() override
        {
            list = m_host.GetControlParent().CreateChild<DxuiListView>();
        }

        void  Configure() override
        {
            configured++;
        }

        void  AddToDock (DxuiDockSite & dock) override
        {
            frame = std::make_unique<DebuggerPaneFrame> (GetTitle());
            frame->AddPart (list);
            dock.AddPane (GetId(), GetTitle(), frame.get());
            docked++;
        }

        IDxuiControl               * GetContent() const override { return frame.get(); }
        std::vector<IDxuiControl *>  GetControls() const override { return { list }; }
        std::vector<DxuiListView *>  GetLists() const override { return { list }; }

        void  Apply (const DebuggerViewSnapshot & snapshot) override
        {
            lastPc = snapshot.pc;
            applied++;
        }

        DxuiListView                       * list       = nullptr;
        std::unique_ptr<DebuggerPaneFrame>   frame;
        int                                  configured = 0;
        int                                  docked     = 0;
        int                                  applied    = 0;
        Word                                 lastPc     = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneListWindow
    //
    //  The debugger window with its controls built and no HWND, and the test
    //  pane added to its pane list.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneListWindow : public DebuggerWindow
    {
    public:
        PaneListWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            std::unique_ptr<TestPane>  each = std::make_unique<TestPane> (GetPaneHostForTest());



            m_theme = &theme;
            m_host  = &host;
            pane    = each.get();

            AddPane (std::move (each));
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetPaneContentForTest;
        using DebuggerWindow::GetPaneOfControl;

        TestPane  * pane = nullptr;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneListTests
    //
    //  A pane added to the pane list is created, configured, docked and given
    //  each snapshot, with no other change to the window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PaneListTests)
    {
    public:

        TEST_METHOD (AnAddedPaneIsBuiltDockedAndGivenEachSnapshot)
        {
            CassoTheme              theme    = CassoTheme::MakeSkeuomorphic();
            NullDebuggerWindowHost  host;
            PaneListWindow          window   (theme, host);
            DxuiDpiScaler           scaler;
            auto                    snapshot = std::make_shared<DebuggerViewSnapshot>();



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsNotNull (window.pane->list);
            Assert::AreEqual  (1, window.pane->configured);
            Assert::AreEqual  (1, window.pane->docked);
            Assert::IsTrue    (window.GetPaneContentForTest (L"testpane") == window.pane->frame.get(), L"the dock places the pane's frame");
            Assert::AreEqual  (std::wstring (L"testpane"), window.GetPaneOfControl (window.pane->list), L"its list is in the pane");

            snapshot->pc = 0x0300;
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (1, window.pane->applied);
            Assert::AreEqual ((Word) 0x0300, window.pane->lastPc);
        }
    };
}
