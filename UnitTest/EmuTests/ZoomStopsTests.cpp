#include "Pch.h"

#include "Ui/DiskInspector/ZoomStops.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ZoomStopsTests
//
//  The zooms a view steps through: even steps up to the first detail zoom,
//  then each detail zoom, and a step from a zoom between two stops.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ZoomStopsTests)
{
public:

    TEST_METHOD (EvenStepsStopShortOfTheFirstDetail)
    {
        vector<double>  stops = ZoomStops::Build (1.0, 4.0, { 70.0, 300.0, 375.0 }, 1.2);



        Assert::AreEqual (static_cast<size_t> (6), stops.size());
        Assert::AreEqual (1.0,   stops[0]);
        Assert::AreEqual (4.0,   stops[1]);
        Assert::AreEqual (16.0,  stops[2]);
        Assert::AreEqual (70.0,  stops[3], L"64 is within 1.2 of 70, so it is left out");
        Assert::AreEqual (300.0, stops[4]);
        Assert::AreEqual (375.0, stops[5]);
    }



    TEST_METHOD (ADetailTooCloseToTheOneBeforeIsLeftOut)
    {
        vector<double>  stops = ZoomStops::Build (1.0, 2.0, { 24.0, 22.0, 80.0 }, 1.2);



        Assert::AreEqual (22.0, stops[stops.size() - 2], L"the details are sorted, and 24 is within 1.2 of 22");
        Assert::AreEqual (80.0, stops.back());
    }



    TEST_METHOD (AStepFromBetweenTwoStopsGoesToTheNearerOneInItsDirection)
    {
        vector<double>  stops = { 1.0, 4.0, 16.0, 90.0 };



        Assert::AreEqual (4.0,  ZoomStops::Step (stops, 1.0,  1));
        Assert::AreEqual (16.0, ZoomStops::Step (stops, 5.0,  1));
        Assert::AreEqual (4.0,  ZoomStops::Step (stops, 5.0, -1));
        Assert::AreEqual (90.0, ZoomStops::Step (stops, 16.0, 5),  L"clamped to the last stop");
        Assert::AreEqual (1.0,  ZoomStops::Step (stops, 4.0, -5),  L"clamped to the first stop");
        Assert::AreEqual (16.0, ZoomStops::Step (stops, 4.0004, 1), L"a hair off a stop steps from it");
    }



    TEST_METHOD (EachWheelTurnStepsAtLeastOnce)
    {
        Assert::AreEqual (1,  ZoomStops::GetWheelSteps (0.25f));
        Assert::AreEqual (-1, ZoomStops::GetWheelSteps (-0.25f));
        Assert::AreEqual (3,  ZoomStops::GetWheelSteps (3.0f));
        Assert::AreEqual (0,  ZoomStops::GetWheelSteps (0.0f));
    }
};
