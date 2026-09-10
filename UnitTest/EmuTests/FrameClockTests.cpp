#include "Pch.h"

#include "Shell/AudioSampleBudget.h"
#include "Shell/FrameClock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FrameClockTests
//
//  The CPU thread's timing, with the clock in the test's hand.
//
//  These are the rules a held key and a Maximum-speed run live by, asserted
//  without a single real millisecond passing. The clock is advanced by
//  assignment; a stall is a big number, not a Sleep.
//
//  The sample budget rides along because it is the other half of what keeps
//  audio in step with the picture, and its whole point is what happens over
//  a long run -- which is exactly the test nobody writes when the run has to
//  be real.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FrameClockTests)
{
public:

    TEST_METHOD (TheFirstReadingAnchorsAndReportsNothing)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        ticker.us = 5'000'000;

        Assert::AreEqual (0u, clock.TakeKeyRepeatElapsedUs (kCapUs),
                          L"a machine that has just come up has not been waiting");
    }


    TEST_METHOD (TheElapsedTimeIsWhatTheClockSays)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        clock.TakeKeyRepeatElapsedUs (kCapUs);

        ticker.us += 16'667;
        Assert::AreEqual (16'667u, clock.TakeKeyRepeatElapsedUs (kCapUs), L"one frame later");

        ticker.us += 3;
        Assert::AreEqual (3u, clock.TakeKeyRepeatElapsedUs (kCapUs), L"and the anchor moved with the reading");
    }


    TEST_METHOD (AStallIsChargedOnceAtTheCap)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        clock.TakeKeyRepeatElapsedUs (kCapUs);

        //  An hour of stall does not fit the interval the keyboard takes,
        //  and the device caps anything past the initial delay at one repeat
        //  anyway, so nothing is lost by reporting the cap.
        ticker.us += 3'600'000'000LL;
        Assert::AreEqual (kCapUs, clock.TakeKeyRepeatElapsedUs (kCapUs), L"capped");

        ticker.us += 1'000;
        Assert::AreEqual (1'000u, clock.TakeKeyRepeatElapsedUs (kCapUs),
                          L"and the interval starts fresh from the reading, not from the stall");
    }


    TEST_METHOD (AClockThatDidNotMoveReportsNothingAndKeepsItsAnchor)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        clock.TakeKeyRepeatElapsedUs (kCapUs);
        ticker.us += 500;
        clock.TakeKeyRepeatElapsedUs (kCapUs);

        Assert::AreEqual (0u, clock.TakeKeyRepeatElapsedUs (kCapUs), L"nothing passed");

        ticker.us += 250;
        Assert::AreEqual (250u, clock.TakeKeyRepeatElapsedUs (kCapUs), L"measured from the last real reading");
    }


    TEST_METHOD (BelowMaximumSpeedEveryFrameIsPublished)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        //  The CPU thread is already paced to one frame per frame there;
        //  a second throttle would only drop frames.
        Assert::IsTrue (clock.ShouldPublish (false, kPublishIntervalUs));
        Assert::IsTrue (clock.ShouldPublish (false, kPublishIntervalUs), L"with no time passing at all");
    }


    TEST_METHOD (AtMaximumSpeedAFrameIsPublishedOncePerInterval)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        ticker.us = 1'000'000;

        Assert::IsTrue  (clock.ShouldPublish (true, kPublishIntervalUs), L"the first one goes out");
        Assert::IsFalse (clock.ShouldPublish (true, kPublishIntervalUs), L"not again in the same instant");

        ticker.us += kPublishIntervalUs - 1;
        Assert::IsFalse (clock.ShouldPublish (true, kPublishIntervalUs), L"one microsecond short");

        ticker.us += 1;
        Assert::IsTrue  (clock.ShouldPublish (true, kPublishIntervalUs), L"at the interval");

        ticker.us += 5 * kPublishIntervalUs;
        Assert::IsTrue  (clock.ShouldPublish (true, kPublishIntervalUs), L"a long gap earns one, not five");
        Assert::IsFalse (clock.ShouldPublish (true, kPublishIntervalUs));
    }


    TEST_METHOD (ResetForgetsBothAnchors)
    {
        Ticker      ticker;
        FrameClock  clock (ticker.Now());

        ticker.us = 1'000'000;
        clock.TakeKeyRepeatElapsedUs (kCapUs);
        clock.ShouldPublish (true, kPublishIntervalUs);

        clock.Reset();

        ticker.us += 100;
        Assert::AreEqual (0u, clock.TakeKeyRepeatElapsedUs (kCapUs), L"anchoring again");
        Assert::IsTrue (clock.ShouldPublish (true, kPublishIntervalUs), L"and the publish gate is open");
    }


    TEST_METHOD (TheSampleBudgetDoesNotDriftOverALongRun)
    {
        AudioSampleBudget  budget;
        uint64_t           total = 0;

        //  1,023,000 Hz into 48,000 Hz is 21.3125 cycles per sample, so a
        //  1,023-cycle slice is worth 48.0 samples exactly once every 16
        //  slices and a fraction under the rest of the time. Sixty seconds
        //  of slices must come to sixty seconds of samples.
        for (int i = 0; i < 60'000; i++)
        {
            total += budget.SamplesFor (1'023, kCyclesPerSample);
        }

        Assert::AreEqual (uint64_t (60'000) * 1'023 * 48'000 / 1'023'000, total,
                          L"sixty seconds of cycles is sixty seconds of samples, to the sample");
    }


    TEST_METHOD (TheSampleBudgetCarriesTheFractionForward)
    {
        AudioSampleBudget  budget;

        //  Ten cycles at four per sample is two and a half: two now, and the
        //  half is owed. Ten more is another two and a half: the half owed
        //  makes three.
        Assert::AreEqual (2u, budget.SamplesFor (10, 4.0));
        Assert::AreEqual (3u, budget.SamplesFor (10, 4.0));
        Assert::AreEqual (2u, budget.SamplesFor (10, 4.0));

        budget.Reset();
        Assert::AreEqual (2u, budget.SamplesFor (10, 4.0), L"after a reset nothing is owed");
    }


private:

    static constexpr uint32_t  kCapUs             = 500'000;     // the keyboard's initial delay
    static constexpr int64_t   kPublishIntervalUs = 16'667;     // ~60 Hz
    static constexpr double    kCyclesPerSample   = 1'023'000.0 / 48'000.0;


    //  A clock the test moves by writing to it.
    struct Ticker
    {
        int64_t  us = 0;

        FrameClock::Now  Now()
        {
            return [this] { return FrameClock::TimePoint (std::chrono::microseconds (us)); };
        }
    };
};
