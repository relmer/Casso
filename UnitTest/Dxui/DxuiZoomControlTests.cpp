#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControlTests
//
//  The zoom control: steps move between the standard stops, a step
//  from between two stops lands on the nearer one in its direction, the
//  steps stop at the ends, and the percentage is a whole number.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiZoomControlTests)
{
public:

    TEST_METHOD (TheSliderShowsOverThePercentageAndStaysNearTheControl)
    {
        DxuiZoomControl  control;
        DxuiDpiScaler    scaler;



        control.Layout ({ 0, 0, 270, 30 }, scaler);
        Assert::IsFalse (control.IsSliderShown());

        control.UpdateHover ({ 100, 15 });
        Assert::IsFalse (control.IsSliderShown(), L"not yet, off the percentage");

        control.UpdateHover ({ 250, 15 });
        Assert::IsTrue (control.IsSliderShown(), L"over the percentage");

        control.UpdateHover ({ 100, 15 });
        Assert::IsTrue (control.IsSliderShown(), L"over the slider");

        control.UpdateHover ({ 100, 40 });
        Assert::IsTrue (control.IsSliderShown(), L"just below it");

        control.UpdateHover ({ 100, 200 });
        Assert::IsFalse (control.IsSliderShown(), L"away from it");
    }



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



    TEST_METHOD (TheZoomReadsAsAWholePercentage)
    {
        Assert::AreEqual (std::wstring (L"100%"), DxuiZoomControl::FormatZoom (1.0));
        Assert::AreEqual (std::wstring (L"67%"),  DxuiZoomControl::FormatZoom (2.0 / 3.0));
    }


    TEST_METHOD (DoubleClickingThePercentageReturnsTo100AndLeavesTheSlider)
    {
        DxuiZoomControl  control;



        control.SetZoom        (1.25);
        control.OnPercentClick (1000);
        Assert::AreEqual       (1.25, control.GetZoom(), 1e-9);
        Assert::IsTrue         (control.IsSliderShown());

        control.OnPercentClick (1001);
        Assert::AreEqual       (1.0, control.GetZoom(), 1e-9);
        Assert::IsFalse        (control.IsSliderShown());
    }


    TEST_METHOD (AHostRangeAndStepReplaceTheStandardOnes)
    {
        DxuiZoomControl  control;
        DxuiDpiScaler    scaler;
        DxuiMouseEvent   press   = {};
        DxuiMouseEvent   release = {};
        double           zoomed  = 0;
        int              closed  = 0;



        control.SetRange  (1.0, 64.0, true);
        control.SetOnStep ([] (double zoom, int steps) { return zoom * (steps > 0 ? 4.0 : 0.25); });
        control.SetOnZoom ([&zoomed] (double zoom) { zoomed = zoom; });
        control.SetZoom   (0.5);
        Assert::AreEqual  (1.0, control.GetZoom(), 1e-9, L"clamped to the host's least zoom");

        control.SetZoom   (500.0);
        Assert::AreEqual  (64.0, control.GetZoom(), 1e-9, L"and to its most");

        control.SetZoom   (2.0);
        control.Layout    ({ 0, 0, 270, 30 }, scaler);
        closed = control.GetShownWidthPx (scaler);

        Assert::IsTrue    (control.UpdateHover ({ 250, 15 }), L"the slider appears over the percentage");
        Assert::IsFalse   (control.UpdateHover ({ 250, 15 }), L"and isn't reported again");
        Assert::IsTrue    (control.GetShownWidthPx (scaler) > closed);

        press.kind          = DxuiMouseEventKind::Down;
        press.button        = DxuiMouseButton::Left;
        press.positionDip   = { 200, 15 };
        release             = press;
        release.kind        = DxuiMouseEventKind::Up;

        control.OnMouse  (press);
        control.OnMouse  (release);
        Assert::AreEqual (8.0, zoomed, 1e-9, L"the + button steps by the host's step");
    }
};
