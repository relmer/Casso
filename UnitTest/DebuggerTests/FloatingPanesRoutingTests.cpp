#include "Pch.h"

#include "Ui/Debugger/FloatingPanes.h"

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
    };
}
