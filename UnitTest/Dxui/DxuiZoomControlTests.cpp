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

    TEST_METHOD (StepsMoveBetweenTheStops)
    {
        Assert::AreEqual (1.1, DxuiZoomControl::StepZoom (1.0,  1), 1e-9);
        Assert::AreEqual (0.9, DxuiZoomControl::StepZoom (1.0, -1), 1e-9);
        Assert::AreEqual (1.5, DxuiZoomControl::StepZoom (1.25, 1), 1e-9);
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
};
