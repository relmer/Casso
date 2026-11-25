#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerPathTests
//
//  A control the pointer moves off is cleared by the pointer path, whatever
//  the window's routing does with the move. Each test routes a move only to
//  the control under the pointer, as a host that routes selectively does,
//  so nothing but the pointer path can clear the control left.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPointerPathTestTypes
{
    //  A control that counts the events the pointer path sends it.
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


    //  A root panel and the pointer path over it.
    class PointerFixture
    {
    public:
        //  The path updated, then the move routed to `to` alone. Returns
        //  whether any control was sent away.
        bool  Move (POINT at, IDxuiControl * to)
        {
            DxuiMouseEvent  ev;
            bool            isSent = path.Update (root, at, false);

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = at;

            if (to != nullptr)
            {
                (void) to->OnMouse (ev);
            }

            return isSent;
        }

        bool  LeaveWindow() { return path.Update (root, POINT { -1, -1 }, true); }

        DxuiPanel        root;
        DxuiPointerPath  path;
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
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        DxuiCheckbox   * box    = fixture.root.CreateChild<DxuiCheckbox> (L"Box");
        DxuiToggle     * toggle = fixture.root.CreateChild<DxuiToggle>();

        box->Layout    (kLeft,  scaler);
        toggle->Layout (kRight, scaler);

        fixture.Move (POINT { 10, 10 }, box);
        Assert::IsTrue (box->IsHovered(), L"the box is hovered under the pointer");

        fixture.Move (POINT { 210, 10 }, toggle);

        Assert::IsFalse (box->IsHovered(), L"the box the pointer left isn't, though it saw no move of its own");
        Assert::IsTrue  (toggle->IsHovered());
    }


    TEST_METHOD (LeavingTheWindow_ClearsEveryControlUnderThePointer)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        DxuiSlider     * slider = fixture.root.CreateChild<DxuiSlider>();

        slider->Layout (kLeft, scaler);

        fixture.Move (POINT { 10, 10 }, slider);
        Assert::IsTrue (slider->IsHovered());

        fixture.LeaveWindow();

        Assert::IsFalse (slider->IsHovered());
    }


    TEST_METHOD (TheControlLeft_GetsAMoveAwayAndALeave_InsideAPanelToo)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        DxuiPanel      * panel = fixture.root.CreateChild<DxuiPanel>();
        PointerProbe   * probe = panel->CreateChild<PointerProbe>();
        PointerProbe   * other = fixture.root.CreateChild<PointerProbe>();

        panel->Layout (kLeft,  scaler);
        probe->Layout (kLeft,  scaler);
        other->Layout (kRight, scaler);

        fixture.Move (POINT { 10, 10 }, nullptr);
        fixture.Move (POINT { 210, 10 }, nullptr);

        Assert::IsTrue   (probe->leaves >= 1, L"at least one Leave: its panel may pass the panel's own on as well");
        Assert::IsTrue   (probe->movedAway, L"after a move to a point outside every control");
        Assert::AreEqual (0, other->leaves, L"and the control now under the pointer has none");
    }


    TEST_METHOD (MovingWithinAControl_SendsItNothing)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        PointerProbe   * probe = fixture.root.CreateChild<PointerProbe>();

        probe->Layout (kLeft, scaler);

        fixture.Move (POINT { 10, 10 }, nullptr);
        fixture.Move (POINT { 50, 20 }, nullptr);

        Assert::AreEqual (0, probe->leaves);
    }


    TEST_METHOD (AHiddenControl_IsNotUnderThePointer)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        PointerProbe   * shown  = fixture.root.CreateChild<PointerProbe>();
        PointerProbe   * hidden = fixture.root.CreateChild<PointerProbe>();   // added last, so on top

        shown->Layout  (kLeft, scaler);
        hidden->Layout (kLeft, scaler);
        hidden->SetVisible (false);

        fixture.Move (POINT { 10, 10 }, nullptr);
        fixture.Move (POINT { 210, 10 }, nullptr);

        Assert::AreEqual (1, shown->leaves,  L"the shown control under the hidden one is left");
        Assert::AreEqual (0, hidden->leaves, L"and the hidden one was never under the pointer");
    }


    TEST_METHOD (Update_ReturnsTrueOnlyWhenAControlIsSentAway)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        PointerProbe   * probe = fixture.root.CreateChild<PointerProbe>();

        probe->Layout (kLeft, scaler);

        Assert::IsFalse (fixture.Move (POINT { 10, 10 }, nullptr),  L"nothing was under the pointer before");
        Assert::IsFalse (fixture.Move (POINT { 50, 20 }, nullptr),  L"the pointer moved within the control");
        Assert::IsTrue  (fixture.Move (POINT { 210, 10 }, nullptr), L"the pointer moved off the control");
        Assert::IsFalse (fixture.Move (POINT { 220, 10 }, nullptr), L"nothing was under the pointer to leave");
        Assert::IsFalse (fixture.LeaveWindow(),                     L"the path is already empty");

        fixture.Move (POINT { 10, 10 }, nullptr);
        Assert::IsTrue  (fixture.LeaveWindow(), L"the pointer left the window from over the control");
    }


    TEST_METHOD (AControlRemovedFromTheTree_IsSentNothing)
    {
        PointerFixture   fixture;
        DxuiDpiScaler    scaler;
        PointerProbe     probe;

        probe.Layout (kLeft, scaler);
        fixture.root.Adopt (probe);

        fixture.Move (POINT { 10, 10 }, nullptr);
        AssertSucceeded (fixture.root.RemoveAdopted (probe));

        Assert::IsFalse  (fixture.Move (POINT { 210, 10 }, nullptr), L"a control no longer in the tree is not sent away");
        Assert::AreEqual (0, probe.leaves);
        Assert::IsFalse  (probe.movedAway);
    }
};