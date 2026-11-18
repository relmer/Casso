#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Mm58167.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Mm58167Tests
//
//  The clock chip on a time source the test moves: the counters from the
//  source at power on and their ripple, writes and resets, the status bit,
//  the interrupts, the year SOS stores in the latches, and saved state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Mm58167Tests)
{
public:

    //  1983-12-31 (a Saturday) at 23:59:58.765, local.
    static constexpr int64_t  kNewYearsEveMs = 441763198765;

    struct Rig
    {
        int64_t   now   = kNewYearsEveMs;
        Mm58167   clock;

        Rig()
        {
            clock.SetTimeSource ([this] () { return now; });
            clock.PowerOn();
        }
    };


    TEST_METHOD (PowerOn_CountersAreTheSourcesTime)
    {
        Rig  rig;



        Assert::AreEqual<int> (0x50, rig.clock.Read (Mm58167::kThousandths), L"5 thousandths, in bits 7-4");
        Assert::AreEqual<int> (0x76, rig.clock.Read (Mm58167::kHundredths),  L"7 tenths and 6 hundredths");
        Assert::AreEqual<int> (0x58, rig.clock.Read (Mm58167::kSeconds));
        Assert::AreEqual<int> (0x59, rig.clock.Read (Mm58167::kMinutes));
        Assert::AreEqual<int> (0x23, rig.clock.Read (Mm58167::kHours));
        Assert::AreEqual<int> (7,    rig.clock.Read (Mm58167::kWeekday),     L"Saturday");
        Assert::AreEqual<int> (0x31, rig.clock.Read (Mm58167::kDay));
        Assert::AreEqual<int> (0x12, rig.clock.Read (Mm58167::kMonth));
    }


    TEST_METHOD (Counters_RippleIntoTheNextMonthAndWeek)
    {
        Rig  rig;



        rig.now += 2000;

        Assert::AreEqual<int> (0x00, rig.clock.Read (Mm58167::kSeconds));
        Assert::AreEqual<int> (0x00, rig.clock.Read (Mm58167::kMinutes));
        Assert::AreEqual<int> (0x00, rig.clock.Read (Mm58167::kHours));
        Assert::AreEqual<int> (1,    rig.clock.Read (Mm58167::kWeekday), L"Sunday");
        Assert::AreEqual<int> (0x01, rig.clock.Read (Mm58167::kDay));
        Assert::AreEqual<int> (0x01, rig.clock.Read (Mm58167::kMonth));
    }


    TEST_METHOD (February_Has28Days)
    {
        Rig  rig;



        rig.clock.Write (Mm58167::kMonth, 0x02);
        rig.clock.Write (Mm58167::kDay,   0x28);
        rig.clock.Write (Mm58167::kHours, 0x23);
        rig.now += 3000;

        Assert::AreEqual<int> (0x03, rig.clock.Read (Mm58167::kMonth));
        Assert::AreEqual<int> (0x01, rig.clock.Read (Mm58167::kDay));
    }


    TEST_METHOD (StatusBit_IsSetWhenACounterMoved_AndClearedByTheRead)
    {
        Rig  rig;



        (void) rig.clock.Read (Mm58167::kSeconds);
        (void) rig.clock.Read (Mm58167::kStatus);
        (void) rig.clock.Read (Mm58167::kSeconds);
        Assert::AreEqual<int> (0, rig.clock.Read (Mm58167::kStatus), L"no time passed");

        rig.now += 1;
        (void) rig.clock.Read (Mm58167::kMinutes);
        Assert::AreEqual<int> (1, rig.clock.Read (Mm58167::kStatus), L"a millisecond moved the counters");
        Assert::AreEqual<int> (0, rig.clock.Read (Mm58167::kStatus), L"and the read cleared it");
    }


    TEST_METHOD (CounterReset_ClearsTheSelectedCounters)
    {
        Rig  rig;



        rig.clock.Write (Mm58167::kCounterReset, 0xFF);

        Assert::AreEqual<int> (0x00, rig.clock.Peek (Mm58167::kSeconds));
        Assert::AreEqual<int> (0x00, rig.clock.Peek (Mm58167::kHours));
        Assert::AreEqual<int> (1,    rig.clock.Peek (Mm58167::kWeekday));
        Assert::AreEqual<int> (0x01, rig.clock.Peek (Mm58167::kMonth));
    }


    //  SOS's GET.TIME: SEC, the month latch shifted left twice with ROL,
    //  then ANDed with the day latch.
    TEST_METHOD (PowerOn_TheLatchesHaveTheYearInSosForm)
    {
        Rig    rig;
        Byte   month = rig.clock.Read (Mm58167::kMonthLatch);
        Byte   day   = rig.clock.Read (Mm58167::kDayLatch);
        int    rol   = ((month << 1) | 1) & 0xFF;
        int    bit7  = month >> 7;



        rol = ((rol << 1) | bit7) & 0xFF;

        Assert::AreEqual<int> (0x83, rol & day);
    }


    TEST_METHOD (Interrupt_SecondIsAssertedWhenEnabled_AndClearedByTheStatusRead)
    {
        Rig  rig;



        rig.now += 2000;
        (void) rig.clock.Read (Mm58167::kSeconds);
        Assert::IsFalse (rig.clock.IsInterruptAsserted(), L"not enabled");

        rig.clock.Write (Mm58167::kInterruptControl, Mm58167::kIrqSecond);
        rig.now += 1000;
        (void) rig.clock.Read (Mm58167::kSeconds);
        Assert::IsTrue (rig.clock.IsInterruptAsserted());

        Assert::AreEqual<int> (Mm58167::kIrqSecond, rig.clock.Read (Mm58167::kInterruptStatus));
        Assert::IsFalse (rig.clock.IsInterruptAsserted());
    }


    TEST_METHOD (State_RoundTripsTheCountersAndLatches)
    {
        Rig          rig;
        Rig          loaded;
        StateWriter  writer;
        HRESULT      hr = S_OK;



        rig.clock.Write (Mm58167::kMinutes, 0x42);
        rig.clock.Write (Mm58167::kFirstLatch, 0x17);

        hr = rig.clock.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        StateReader  reader (writer.GetBytes());

        hr = loaded.clock.LoadState (reader);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual<int> (0x42, loaded.clock.Peek (Mm58167::kMinutes));
        Assert::AreEqual<int> (0x17, loaded.clock.Peek (Mm58167::kFirstLatch));
    }
};
