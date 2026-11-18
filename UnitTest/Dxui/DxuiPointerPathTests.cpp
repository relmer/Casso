#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerPathTests
//
//  A control the pointer moves off is cleared by the window, whatever the
//  window's routing does with the move. Each test routes a move only to
//  the control under the pointer, as a host that routes selectively does,
//  so nothing but the window's pointer path can clear the control left.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPointerPathTestTypes
{
    //  A control that counts the events the window sends it.
    class PointerProbe : public IDxuiControl
    {
    public:
        void  Layout (const RECT & boundsDip, const DxuiDpiScaler &) override { SetBounds (boundsDip); }
        void  Paint  (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &) override {}

        bool  OnMouse (const DxuiMouseEvent & ev) override
        {
            leaves     += (ev.kind == DxuiMouseEventKind::Leave) ? 1 : 0;
            movedAway   = movedAway || (ev.kind == DxuiMouseEventKind::Move && ev.positionDip.x < 0);
            return false;
        }

        int   leaves    = 0;
        bool  movedAway = false;
    };


    class PointerWindow : public DxuiWindow
    {
    public:
        //  The window's path updated, then the move routed to `to` alone.
        void  Move (POINT at, IDxuiControl * to)
        {
            DxuiMouseEvent  ev;

            UpdatePointerPath (at, false);

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = at;

            if (to != nullptr)
            {
                (void) to->OnMouse (ev);
            }
        }

        void  LeaveWindow() { UpdatePointerPath (POINT { -1, -1 }, true); }
    };
}

using namespace DxuiPointerPathTestTypes;





TEST_CLASS (DxuiPointerPathTests)
{
public:

    static constexpr RECT  kLeft  = { 0,   0, 100, 30 };
    static constexpr RECT  kRight = { 200, 0, 300, 30 };

    TEST_METHOD (MovingToAnotherControl_ClearsTheHoverOfTheOneLeft)
    {
        PointerWindow    window;
        DxuiDpiScaler    scaler;
        DxuiCheckbox   * box    = window.CreateChild<DxuiCheckbox> (L"Box");
        DxuiToggle     * toggle = window.CreateChild<DxuiToggle>();

        box->Layout    (kLeft,  scaler);
        toggle->Layout (kRight, scaler);

        window.Move (POINT { 10, 10 }, box);
        Assert::IsTrue (box->IsHovered(), L"the box is hovered under the pointer");

        window.Move (POINT { 210, 10 }, toggle);

        Assert::IsFalse (box->IsHovered(), L"the box the pointer left isn't, though it saw no move of its own");
        Assert::IsTrue  (toggle->IsHovered());
    }


    TEST_METHOD (LeavingTheWindow_ClearsEveryControlUnderThePointer)
    {
        PointerWindow    window;
        DxuiDpiScaler    scaler;
        DxuiSlider     * slider = window.CreateChild<DxuiSlider>();

        slider->Layout (kLeft, scaler);

        window.Move (POINT { 10, 10 }, slider);
        Assert::IsTrue (slider->IsHovered());

        window.LeaveWindow();

        Assert::IsFalse (slider->IsHovered());
    }


    TEST_METHOD (TheControlLeft_GetsAMoveAwayAndALeave_InsideAPanelToo)
    {
        PointerWindow    window;
        DxuiDpiScaler    scaler;
        DxuiPanel      * panel = window.CreateChild<DxuiPanel>();
        PointerProbe   * probe = panel->CreateChild<PointerProbe>();
        PointerProbe   * other = window.CreateChild<PointerProbe>();

        panel->Layout (kLeft,  scaler);
        probe->Layout (kLeft,  scaler);
        other->Layout (kRight, scaler);

        window.Move (POINT { 10, 10 }, nullptr);
        window.Move (POINT { 210, 10 }, nullptr);

        Assert::IsTrue   (probe->leaves >= 1, L"at least one Leave: its panel may pass the panel's own on as well");
        Assert::IsTrue   (probe->movedAway, L"after a move to a point outside every control");
        Assert::AreEqual (0, other->leaves, L"and the control now under the pointer has none");
    }


    TEST_METHOD (MovingWithinAControl_SendsItNothing)
    {
        PointerWindow    window;
        DxuiDpiScaler    scaler;
        PointerProbe   * probe = window.CreateChild<PointerProbe>();

        probe->Layout (kLeft, scaler);

        window.Move (POINT { 10, 10 }, nullptr);
        window.Move (POINT { 50, 20 }, nullptr);

        Assert::AreEqual (0, probe->leaves);
    }


    TEST_METHOD (AHiddenControl_IsNotUnderThePointer)
    {
        PointerWindow    window;
        DxuiDpiScaler    scaler;
        PointerProbe   * shown  = window.CreateChild<PointerProbe>();
        PointerProbe   * hidden = window.CreateChild<PointerProbe>();   // added last, so on top

        shown->Layout  (kLeft, scaler);
        hidden->Layout (kLeft, scaler);
        hidden->SetVisible (false);

        window.Move (POINT { 10, 10 }, nullptr);
        window.Move (POINT { 210, 10 }, nullptr);

        Assert::AreEqual (1, shown->leaves,  L"the shown control under the hidden one is left");
        Assert::AreEqual (0, hidden->leaves, L"and the hidden one was never under the pointer");
    }};
