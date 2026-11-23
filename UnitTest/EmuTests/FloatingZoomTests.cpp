#include "Pch.h"

#include "Ui/DiskInspector/FloatingZoom.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoomTests
//
//  A map's floating zoom: at rest only the percentage, with the group's
//  right edge at the corner it is placed at.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FloatingZoomTests)
{
public:

    TEST_METHOD (AtRestTheGroupIsThePercentageInTheCorner)
    {
        FloatingZoom   zoom;
        DxuiDpiScaler  scaler;
        RECT           bounds = {};



        zoom.Place (POINT { 500, 300 }, 36, 4, scaler);
        bounds = zoom.GetBounds();

        Assert::AreEqual (500L, bounds.right, L"the group's right edge is at the corner");
        Assert::AreEqual (300L, bounds.bottom);
        Assert::AreEqual (264L, bounds.top);
        Assert::AreEqual ((LONG) FloatingZoom::GetClosedWidthPx (4, scaler), bounds.right - bounds.left, L"only the percentage, with its padding");
        Assert::IsFalse  (zoom.GetControl().IsSliderShown());
    }
};
