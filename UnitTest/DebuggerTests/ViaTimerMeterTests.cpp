#include "Pch.h"

#include "Devices/Via6522.h"
#include "Debugger/DiagnosticsSnapshot.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ViaTimerMeterTests
//
//  The Mockingboard panel's 6522 timer meters. A meter shows a bar only while
//  its timer is armed against a latch slow enough for a once-a-frame sample to
//  follow; a timer counting with no latch, or faster than the frame rate,
//  shows a steady "running" state instead of a bar that jumps at random.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ViaTimerMeterTests)
    {
    public:

        static DiagnosticsMeters GetMeters (const Via6522 & via)
        {
            DiagnosticsMeters  meters;



            via.AppendTimerLevels ("6522", meters);
            return meters;
        }


        TEST_METHOD (AnUnarmedTimer1WithNoLatchShowsRunning)
        {
            Via6522            via;
            DiagnosticsMeters  meters;



            via.Tick (0x1234);
            meters = GetMeters (via);

            Assert::AreEqual (std::string ("running"), meters.levels[0].status, L"T1 counts with a latch of $0000, so it has nothing to measure against");
        }


        TEST_METHOD (AFreeRunningTimer2ShowsRunning)
        {
            Via6522            via;
            DiagnosticsMeters  meters;



            via.WriteRegister (Via6522::kRegT2CL, 0x10);
            via.WriteRegister (Via6522::kRegT2CH, 0x00);     // T2 = $0010, and started
            via.Tick (0x9000);                                // fires, then wraps through $FFFF
            meters = GetMeters (via);

            Assert::AreEqual (std::string ("running"), meters.levels[1].status, L"a T2 that has fired free-runs, too fast for a frame to follow");
        }


        TEST_METHOD (AnArmedTimer2ShowsWhatIsLeftOfItsStart)
        {
            Via6522            via;
            DiagnosticsMeters  meters;



            via.WriteRegister (Via6522::kRegT2CL, 0x00);
            via.WriteRegister (Via6522::kRegT2CH, 0x80);     // T2 = $8000, and started
            via.Tick (0x2000);
            meters = GetMeters (via);

            Assert::AreEqual (std::string(), meters.levels[1].status, L"an armed T2 with a long count has a bar");
            Assert::AreEqual (0.75f, meters.levels[1].level, 0.01f, L"a quarter of the count is gone");
        }


        TEST_METHOD (ATimer1FasterThanTheFrameRateShowsRunning)
        {
            Via6522            via;
            DiagnosticsMeters  meters;



            via.WriteRegister (Via6522::kRegAcr,  Via6522::kAcrT1Continuous);
            via.WriteRegister (Via6522::kRegT1CL, 0x00);
            via.WriteRegister (Via6522::kRegT1CH, 0x01);     // T1 = $0100, continuous
            meters = GetMeters (via);

            Assert::AreEqual (std::string ("running"), meters.levels[0].status, L"a period of 256 cycles wraps many times a frame");
        }
    };
}
