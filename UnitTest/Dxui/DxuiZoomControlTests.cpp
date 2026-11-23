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
};
