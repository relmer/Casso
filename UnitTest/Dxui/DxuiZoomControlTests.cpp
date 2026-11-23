#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControlTests
//
//  The zoom control: steps move between the standard stops, a step from
//  between two stops lands on the nearer one in its direction, the steps
//  stop at the ends, the button sits at the right end of its row, and the
//  bounds take in the panel while it is open.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiZoomControlTests)
{
public:

    TEST_METHOD (StepsMoveBetweenTheStops)
    {
        Assert::AreEqual (1.1, DxuiZoomControl::StepZoom (1.0,  1), 1e-9);
        Assert::AreEqual (0.9, DxuiZoomControl::StepZoom (1.0, -1), 1e-9);
        Assert::AreEqual (1.2, DxuiZoomControl::StepZoom (1.1, 1), 1e-9);
    }



    TEST_METHOD (AStepFromBetweenStopsLandsOnTheNextOneThatWay)
    {
        Assert::AreEqual (1.0, DxuiZoomControl::StepZoom (1.05, -1), 1e-9);
        Assert::AreEqual (1.1, DxuiZoomControl::StepZoom (1.05,  1), 1e-9);
    }



    TEST_METHOD (StepsStopAtTheEnds)
    {
        Assert::AreEqual (DxuiZoomControl::kMinZoom, DxuiZoomControl::StepZoom (DxuiZoomControl::kMinZoom, -1), 1e-9);
        Assert::AreEqual (DxuiZoomControl::kMaxZoom, DxuiZoomControl::StepZoom (DxuiZoomControl::kMaxZoom,  1), 1e-9);
    }



    TEST_METHOD (TheButtonSitsAtTheRowsRightEndCenteredOnIt)
    {
        DxuiZoomControl  control;
        DxuiDpiScaler    scaler;
        RECT             button = {};
        RECT             bounds = {};



        control.PlaceInRow ({ 0, 100, 300, 130 }, RECT {}, scaler);
        button = control.GetButtonRect();

        Assert::AreEqual (300L, button.right);
        Assert::AreEqual (115L, (button.top + button.bottom) / 2);
        bounds = control.GetBounds();
        Assert::IsTrue   (EqualRect (&button, &bounds) != FALSE, L"at rest the bounds are the button");
    }



    TEST_METHOD (TheBoundsTakeInThePanelWhileItIsOpen)
    {
        DxuiZoomControl  control;
        DxuiDpiScaler    scaler;
        RECT             button = {};
        RECT             bounds = {};



        control.PlaceInRow ({ 0, 100, 300, 130 }, RECT {}, scaler);
        button = control.GetButtonRect();

        Assert::IsTrue   (control.UpdateHover ({ button.left + 1, button.top + 1 }, 0), L"opened over the button");
        bounds = control.GetBounds();

        Assert::IsTrue   (control.IsSliderShown());
        Assert::IsTrue   (bounds.top < button.top);
        Assert::AreEqual (button.right, bounds.right);

        control.Layout   (control.GetBounds(), scaler);
        bounds = control.GetButtonRect();
        Assert::IsTrue   (EqualRect (&button, &bounds) != FALSE, L"the button doesn't move on a layout pass");
    }



    TEST_METHOD (TheSliderOverAnEarlierControlTakesThePointer)
    {
        DxuiPanel          host;
        DxuiDpiScaler      scaler;
        RECT               button  = {};
        RECT               bounds  = {};
        double             zoomed  = 0.0;
        DxuiMouseEvent     ev;
        ConsumingControl * content = host.CreateChild<ConsumingControl>();
        DxuiZoomControl  * control = host.CreateChild<DxuiZoomControl>();



        content->SetBounds  ({ 0, 0, 300, 100 });
        control->SetOnZoom  ([&zoomed] (double zoom) { zoomed = zoom; });
        control->PlaceInRow ({ 0, 100, 300, 130 }, RECT {}, scaler);
        button = control->GetButtonRect();

        Assert::IsTrue (control->UpdateHover ({ button.left + 1, button.top + 1 }, 0));
        bounds = control->GetBounds();

        //  The slider's row, over the content control.
        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = { (bounds.left + bounds.right) / 2, bounds.top + (DxuiZoomPopup::kPadDip + DxuiZoomPopup::kRowDip) / 2 };
        Assert::IsTrue  (ev.positionDip.y < 100, L"the slider is over the content");
        Assert::IsTrue  (host.OnMouse (ev), L"a move over the slider is taken");
        Assert::IsTrue  (control->IsSliderShown(), L"the slider stays open over it");

        ev.kind   = DxuiMouseEventKind::Down;
        ev.button = DxuiMouseButton::Left;
        Assert::IsTrue  (host.OnMouse (ev));
        Assert::IsTrue  (zoomed > 0.0, L"a press on the track zooms");

        ev.kind = DxuiMouseEventKind::Up;
        Assert::IsTrue  (host.OnMouse (ev));

        ev.kind        = DxuiMouseEventKind::Move;
        ev.button      = DxuiMouseButton::None;
        ev.positionDip = { 10, 10 };
        Assert::IsTrue  (host.OnMouse (ev), L"the content takes a move away from the slider");
        Assert::IsFalse (control->IsSliderShown(), L"the slider closes far from it");
    }



private:

    //  A control under the slider that takes every mouse event.
    class ConsumingControl : public DxuiPanel
    {
    public:
        bool  OnMouse (const DxuiMouseEvent & ev) override { (void) ev; return true; }
    };
};
