#include "Pch.h"

#include "Config/WindowPlacementProfile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementKeyTests
//
//  The rules that decide WHERE a window comes back, each one written after it
//  had already shipped wrong:
//
//    the key names an arrangement of screens, and only that -- not which
//    monitor is active, not the work area a taskbar moves on its own
//
//    a minimized window's parking spot is not a placement
//
//  Both used to live inside functions that walk the OS for their inputs, so
//  nothing could reach them except by hand, on a machine, with two screens.
//  They take their inputs as data now.
//
////////////////////////////////////////////////////////////////////////////////

namespace WindowPlacementKeyTests
{
    using Snapshot = WindowPlacementProfile::MonitorSnapshot;




    static Snapshot Monitor (const wchar_t * device, LONG left, LONG top, LONG right, LONG bottom)
    {
        Snapshot  monitor;



        monitor.device    = device;
        monitor.rcMonitor = { left, top, right, bottom };
        monitor.rcWork    = { left, top, right, bottom };
        monitor.flags     = 0;

        return monitor;
    }




    TEST_CLASS (WindowPlacementKeyTests)
    {
    public:

        TEST_METHOD (OneArrangementHasOneKeyWhateverTheTaskbarIsDoing)
        {
            std::vector<Snapshot>  shown  = { Monitor (L"\\\\.\\DISPLAY1", -2160, -827, 0, 3013) };
            std::vector<Snapshot>  hidden = shown;



            //  The taskbar slides away: the work area grows, the screens do not
            //  move. Three keys inside ten seconds came of counting this.
            shown[0].rcWork.bottom  = 2941;
            hidden[0].rcWork.bottom = 3013;

            Assert::AreEqual (WindowPlacementProfile::BuildTopologyKeyFrom (shown),
                              WindowPlacementProfile::BuildTopologyKeyFrom (hidden),
                              L"a taskbar is not a different set of screens");
        }


        TEST_METHOD (TheKeyDoesNotDependOnWhichMonitorIsPrimary)
        {
            std::vector<Snapshot>  first  = { Monitor (L"\\\\.\\DISPLAY1", 0, 0, 1920, 1080) };
            std::vector<Snapshot>  second = first;



            first[0].flags  = MONITORINFOF_PRIMARY;
            second[0].flags = 0;

            Assert::AreEqual (WindowPlacementProfile::BuildTopologyKeyFrom (first),
                              WindowPlacementProfile::BuildTopologyKeyFrom (second));
        }


        TEST_METHOD (TheKeyDoesNotDependOnEnumerationOrder)
        {
            std::vector<Snapshot>  oneWay = { Monitor (L"\\\\.\\DISPLAY1",     0,    0, 3840, 2160),
                                              Monitor (L"\\\\.\\DISPLAY2", -2160, -827,    0, 3013) };
            std::vector<Snapshot>  other  = { oneWay[1], oneWay[0] };



            Assert::AreEqual (WindowPlacementProfile::BuildTopologyKeyFrom (oneWay),
                              WindowPlacementProfile::BuildTopologyKeyFrom (other));
        }


        TEST_METHOD (MovingAScreenIsADifferentArrangement)
        {
            std::vector<Snapshot>  before = { Monitor (L"\\\\.\\DISPLAY1",     0, 0, 3840, 2160),
                                              Monitor (L"\\\\.\\DISPLAY2", -2160, 0,    0, 1200) };
            std::vector<Snapshot>  after  = before;



            //  The second screen moves above the first rather than beside it.
            after[1].rcMonitor = { 0, -1200, 2160, 0 };

            Assert::AreNotEqual (WindowPlacementProfile::BuildTopologyKeyFrom (before),
                                 WindowPlacementProfile::BuildTopologyKeyFrom (after));
        }


        TEST_METHOD (UnpluggingAScreenIsADifferentArrangement)
        {
            std::vector<Snapshot>  both = { Monitor (L"\\\\.\\DISPLAY1",     0, 0, 3840, 2160),
                                            Monitor (L"\\\\.\\DISPLAY2", -2160, 0,    0, 1200) };
            std::vector<Snapshot>  one  = { both[0] };



            Assert::AreNotEqual (WindowPlacementProfile::BuildTopologyKeyFrom (both),
                                 WindowPlacementProfile::BuildTopologyKeyFrom (one));
        }


        TEST_METHOD (AMinimizedWindowsParkingSpotIsNotAPlacement)
        {
            //  Where Windows puts a minimized window, and what reached the
            //  preferences file as the arrangement for a whole monitor set.
            RECT  parked = { -31991, -32000, -31810, -31966 };



            Assert::IsFalse (WindowPlacementProfile::IsPlaceableRect (parked));
        }


        TEST_METHOD (AnEmptyRectIsNotAPlacement)
        {
            Assert::IsFalse (WindowPlacementProfile::IsPlaceableRect (RECT { 100, 100, 100, 100 }));
            Assert::IsFalse (WindowPlacementProfile::IsPlaceableRect (RECT { 100, 100,  40, 300 }));
            Assert::IsFalse (WindowPlacementProfile::IsPlaceableRect (RECT {}));
        }


        TEST_METHOD (AWindowOnAScreenLeftOfTheOriginIsAPlacement)
        {
            //  Negative coordinates are ordinary: a second monitor to the left
            //  of the primary has them, and this branch must not reject it.
            Assert::IsTrue (WindowPlacementProfile::IsPlaceableRect (RECT { -2160, -827, 0, 1057 }));
        }
    };
}
