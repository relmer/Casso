#include "Pch.h"

#include "Debugger/CycleStopwatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatchTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CycleStopwatchTests)
    {
    public:

        TEST_METHOD (NotArmed_RecordsNothing)
        {
            CycleStopwatch  watch;



            watch.OnInstruction (0x0300, 10);
            watch.OnInstruction (0x0310, 20);

            Assert::IsFalse  (watch.IsArmed());
            Assert::IsFalse  (watch.IsTiming());
            Assert::AreEqual ((uint64_t) 0, watch.GetLapCount());
        }



        TEST_METHOD (Armed_TimesFromStartToStop_KeepsShortestLongestAndTotal)
        {
            CycleStopwatch  watch;



            watch.Arm (0x0310, 0x0320);
            watch.OnInstruction (0x0320, 5);
            Assert::AreEqual ((uint64_t) 0, watch.GetLapCount(), L"a stop before any start is no lap");

            watch.OnInstruction (0x0310, 100);
            Assert::IsTrue   (watch.IsTiming());
            watch.OnInstruction (0x0315, 104);
            watch.OnInstruction (0x0320, 130);
            watch.OnInstruction (0x0310, 200);
            watch.OnInstruction (0x0320, 210);

            Assert::IsFalse  (watch.IsTiming());
            Assert::AreEqual ((uint64_t) 2,  watch.GetLapCount());
            Assert::AreEqual ((uint64_t) 10, watch.GetLastLap());
            Assert::AreEqual ((uint64_t) 10, watch.GetShortest());
            Assert::AreEqual ((uint64_t) 30, watch.GetLongest());
            Assert::AreEqual ((uint64_t) 40, watch.GetTotal());
        }



        TEST_METHOD (StartReachedWhileTiming_KeepsTheOutermostEntry)
        {
            CycleStopwatch  watch;



            watch.Arm (0x0310, 0x0320);
            watch.OnInstruction (0x0310, 100);
            watch.OnInstruction (0x0310, 150);
            watch.OnInstruction (0x0320, 175);

            Assert::AreEqual ((uint64_t) 75, watch.GetLastLap());
        }



        TEST_METHOD (SameStartAndStop_TimesEachArrivalToTheNext)
        {
            CycleStopwatch  watch;



            watch.Arm (0x0300, 0x0300);
            watch.OnInstruction (0x0300, 0);
            watch.OnInstruction (0x0303, 2);
            watch.OnInstruction (0x0300, 5);
            watch.OnInstruction (0x0300, 12);

            Assert::AreEqual ((uint64_t) 2, watch.GetLapCount());
            Assert::AreEqual ((uint64_t) 5, watch.GetShortest());
            Assert::AreEqual ((uint64_t) 7, watch.GetLongest());
            Assert::IsTrue   (watch.IsTiming(), L"the last arrival starts the next lap");
        }



        TEST_METHOD (CountGoingBackward_DropsTheLap)
        {
            CycleStopwatch  watch;



            watch.Arm (0x0310, 0x0320);
            watch.OnInstruction (0x0310, 1000);
            watch.OnInstruction (0x0320, 4);

            Assert::AreEqual ((uint64_t) 0, watch.GetLapCount());
            Assert::IsFalse  (watch.IsTiming());
        }



        TEST_METHOD (ResetAndDisarm_ClearLapsOrStopTiming)
        {
            CycleStopwatch  watch;



            watch.Arm (0x0310, 0x0320);
            watch.OnInstruction (0x0310, 0);
            watch.OnInstruction (0x0320, 9);
            watch.OnInstruction (0x0310, 20);

            watch.Disarm();
            Assert::IsFalse  (watch.IsTiming());
            Assert::AreEqual ((uint64_t) 1, watch.GetLapCount(), L"disarming keeps the laps");

            watch.OnInstruction (0x0310, 30);
            watch.OnInstruction (0x0320, 40);
            Assert::AreEqual ((uint64_t) 1, watch.GetLapCount(), L"a disarmed stopwatch times nothing");

            watch.Reset();
            Assert::AreEqual ((uint64_t) 0, watch.GetLapCount());
            Assert::AreEqual ((uint64_t) 0, watch.GetLongest());
        }
    };
}
