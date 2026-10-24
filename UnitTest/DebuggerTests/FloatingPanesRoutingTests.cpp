#include "Pch.h"

#include "Ui/Debugger/FloatingPanes.h"
#include "Dxui/MockDxuiControl.h"
#include "Ui/Chrome/CassoTheme.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace FloatingPanesRoutingTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RoutingHost
    //
    //  A host with no windows that keeps the routing pane each routed event
    //  and command ran under.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RoutingHost : public IFloatingPanesHost
    {
    public:
        HWND                           GetFloatOwner     () const override { return nullptr; }
        HINSTANCE                      GetFloatInstance  () const override { return nullptr; }
        DxuiDockSite *                 GetFloatDockSite  () const override { return nullptr; }
        const DxuiTheme *              GetTheme          () const override { return nullptr; }
        const DxuiKeyMap *             GetFloatKeyMap    () const override { return nullptr; }
        const DxuiDpiScaler &          GetFloatScaler    () const override { return m_scaler; }

        std::vector<IDxuiControl *>    GetPaneControls   (const std::wstring &) const override { return {}; }
        IDxuiControl *                 GetPaneContent    (const std::wstring &) const override { return nullptr; }
        std::wstring                   GetPaneTitle      (const std::wstring &) const override { return {}; }
        bool                           IsPaneShown       (const std::wstring &) const override { return false; }
        bool                           CanClosePane      (const std::wstring &) const override { return false; }
        void                           ClosePane         (const std::wstring &) override {}
        void                           ShowDockToMenu    (const std::wstring &, POINT) override {}
        void                           SaveLayout        () override {}
        void                           Repaint           () override {}

        std::unique_ptr<IDxuiControl>  DetachFloatControl (IDxuiControl *) override { return nullptr; }
        void                           AttachFloatControl (std::unique_ptr<IDxuiControl>) override {}

        bool  RouteFloatedMouse   (const DxuiMouseEvent &) override               { routed.push_back (floats->GetRoutingPane()); return true; }
        bool  RouteFloatedKey     (const DxuiKeyEvent &) override                 { routed.push_back (floats->GetRoutingPane()); return true; }
        bool  RunFloatedCommand   (int) override                                  { routed.push_back (floats->GetRoutingPane()); return true; }
        bool  DropFloatedFiles    (const std::vector<std::wstring> &) override    { return false; }

        void  OnFloatFocusChanged (const std::wstring &, bool) override {}
        void  OnFloatDocked       (const std::wstring &, DxuiDockedWindow *) override {}
        void  OnFloatsSynced      () override {}
        void  OnFloatTipErased    (const DxuiTooltip *) override {}

        FloatingPanes              * floats = nullptr;
        std::vector<std::wstring>    routed;

    private:
        DxuiDpiScaler  m_scaler;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DockingHost
    //
    //  A host whose dock site has "code" docked and "regs" floating alone,
    //  regs's only control a mock that tracks its focus. The debugger window
    //  is none, so the floating window is created and stays hidden.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DockingHost : public RoutingHost
    {
    public:
        DockingHost()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");



            layout.Add (L"regs", L"");
            (void) layout.Float (L"regs", L"", RECT { 0, 0, 300, 200 });

            site.AddPane       (L"code", L"Disassembly", &code);
            site.AddPane       (L"regs", L"Registers",   regs);
            site.SetPaneLayout (layout);
        }

        HINSTANCE                    GetFloatInstance() const override { return GetModuleHandleW (nullptr); }
        DxuiDockSite *               GetFloatDockSite() const override { return &site; }
        const DxuiTheme *            GetTheme() const override         { return &theme; }
        void                         SaveLayout() override             { saves++; }

        std::vector<IDxuiControl *>  GetPaneControls (const std::wstring & pane) const override { return (pane == L"regs") ? std::vector<IDxuiControl *> { regs } : std::vector<IDxuiControl *> {}; }
        IDxuiControl *               GetPaneContent  (const std::wstring & pane) const override { return (pane == L"regs") ? regs : nullptr; }

        std::unique_ptr<IDxuiControl>  DetachFloatControl (IDxuiControl * control) override { return (control == regs) ? std::move (owned) : nullptr; }
        void                           AttachFloatControl (std::unique_ptr<IDxuiControl> control) override { owned = std::move (control); }

        mutable DxuiDockSite            site;
        CassoTheme                      theme = CassoTheme::MakeSkeuomorphic();
        MockDxuiControl                 code;
        std::unique_ptr<IDxuiControl>   owned = std::make_unique<MockDxuiControl>();
        MockDxuiControl               * regs  = static_cast<MockDxuiControl *> (owned.get());
        int                             saves = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FloatingPanesRoutingTests
    //
    //  A command from a floating window's key map runs under that window's
    //  pane, as its mouse and keys do, so the focus and the find target
    //  inside it are the floating pane's.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (FloatingPanesRoutingTests)
    {
    public:

        TEST_METHOD (ACommandRunsUnderThePaneItCameFrom)
        {
            RoutingHost    host;
            FloatingPanes  floats (host);



            host.floats = &floats;

            Assert::IsTrue (floats.RouteCommand (L"basicprogram", 1));
            Assert::IsTrue (floats.RouteKey (L"source1", DxuiKeyEvent()));

            Assert::IsTrue   (host.routed == std::vector<std::wstring> { L"basicprogram", L"source1" });
            Assert::IsTrue   (floats.GetRoutingPane().empty(), L"no pane once the command has run");
        }


        //  Docking a floating window calls OnFocusChanged (false) on its
        //  pane's focused control, so the control does not draw as focused
        //  in the debugger window.
        TEST_METHOD (DockingAWindowTakesTheFocusFromItsControl)
        {
            DockingHost    host;
            FloatingPanes  floats (host);



            host.floats = &floats;

            floats.MarkSyncNeeded();
            floats.Sync();
            Assert::IsNotNull (floats.Find (L"regs"), L"regs floats in a window");

            {
                FloatingPanes::RouteScope  scope (floats, L"regs");



                floats.SetRoutedFocus (host.regs);
            }

            Assert::IsTrue (host.regs->lastFocused, L"the floating window's control has the focus");

            floats.DockBack (L"regs");
            floats.Sync();

            Assert::IsNull  (floats.Find (L"regs"), L"the window is gone");
            Assert::IsFalse (host.regs->lastFocused, L"its control lost the focus");
            Assert::IsTrue  (host.owned != nullptr, L"and is back with the host");
        }
    };
}
