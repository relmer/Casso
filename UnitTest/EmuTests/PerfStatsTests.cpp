#include "Pch.h"

#include "Core/PerfStats.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PerfStatsTests
//
//  The rolling-average timer registry, which moved out of the executable with
//  no tests at all behind it.
//
//  PerfStats::Instance() is process-wide and shared by every test in the run,
//  so each test here uses a label of its own. A label reused across tests
//  would make results depend on execution order, which Test Independence
//  forbids and which would show up as an intermittent failure rather than an
//  honest one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PerfStatsTests)
{
public:

    TEST_METHOD (UnknownLabel_ReadsBackZeroed)
    {
        PerfStats::Stat  stat = PerfStats::Instance().Get ("perfstats.never.recorded");

        Assert::AreEqual (0.0, stat.avgMs);
        Assert::AreEqual (0.0, stat.lastMs);
        Assert::AreEqual (0.0, stat.maxMs);
    }


    TEST_METHOD (FirstSample_SeedsTheAverageExactly)
    {
        PerfStats::Stat  stat;

        //  The average is seeded with the first sample rather than smoothed
        //  from zero. Without that, a first sample of 8 ms would report 0.8
        //  and the overlay would understate every timer for its first
        //  several frames.
        PerfStats::Instance().Record ("perfstats.first.sample", 8.0);
        stat = PerfStats::Instance().Get ("perfstats.first.sample");

        Assert::AreEqual (8.0, stat.avgMs,  0.0001);
        Assert::AreEqual (8.0, stat.lastMs, 0.0001);
        Assert::AreEqual (8.0, stat.maxMs,  0.0001);
    }


    TEST_METHOD (LaterSamples_FollowTheExponentialAverage)
    {
        PerfStats::Stat  stat;

        //  alpha = 0.1: 10 seeds, then 20 gives 10*0.9 + 20*0.1 = 11.
        PerfStats::Instance().Record ("perfstats.ema", 10.0);
        PerfStats::Instance().Record ("perfstats.ema", 20.0);
        stat = PerfStats::Instance().Get ("perfstats.ema");

        Assert::AreEqual (11.0, stat.avgMs,  0.0001);
        Assert::AreEqual (20.0, stat.lastMs, 0.0001);
    }


    TEST_METHOD (Maximum_IsARunningPeakAndNeverFallsBack)
    {
        PerfStats::Stat  stat;

        PerfStats::Instance().Record ("perfstats.peak", 5.0);
        PerfStats::Instance().Record ("perfstats.peak", 40.0);
        PerfStats::Instance().Record ("perfstats.peak", 1.0);
        stat = PerfStats::Instance().Get ("perfstats.peak");

        Assert::AreEqual (40.0, stat.maxMs,  0.0001, L"a spike must survive later quiet frames");
        Assert::AreEqual (1.0,  stat.lastMs, 0.0001);
    }


    TEST_METHOD (NullLabel_IsIgnoredRatherThanRecorded)
    {
        //  The hot path passes a literal, but a null would be a crash in a
        //  per-frame call. Recording nothing is the documented behavior.
        PerfStats::Instance().Record (nullptr, 3.0);
    }


    TEST_METHOD (LabelsAreIndependentOfOneAnother)
    {
        PerfStats::Stat  first;
        PerfStats::Stat  second;

        PerfStats::Instance().Record ("perfstats.independent.a", 4.0);
        PerfStats::Instance().Record ("perfstats.independent.b", 400.0);

        first  = PerfStats::Instance().Get ("perfstats.independent.a");
        second = PerfStats::Instance().Get ("perfstats.independent.b");

        Assert::AreEqual (4.0,   first.avgMs,  0.0001);
        Assert::AreEqual (400.0, second.avgMs, 0.0001);
    }


    TEST_METHOD (GetAll_ReportsARecordedLabel)
    {
        std::unordered_map<std::string, PerfStats::Stat>  all;

        PerfStats::Instance().Record ("perfstats.getall", 2.0);
        all = PerfStats::Instance().GetAll();

        Assert::IsTrue (all.find ("perfstats.getall") != all.end(),
                        L"a recorded label must appear in the snapshot");
        Assert::AreEqual (2.0, all["perfstats.getall"].lastMs, 0.0001);
    }


    TEST_METHOD (AZeroMillisecondSample_ReSeedsTheAverageInsteadOfSmoothingIt)
    {
        PerfStats::Stat  stat;

        //
        //  DOCUMENTS EXISTING BEHAVIOR, and it is arguably wrong.
        //
        //  The seed test is `avgMs == 0.0`, not "have we seen a sample", so a
        //  genuine 0.0 ms measurement leaves the average looking unseeded. The
        //  next sample then replaces the average outright instead of being
        //  smoothed into it: 0 followed by 5 reports 5.0, where the smoothing
        //  this class exists to do would report 0.5.
        //
        //  An operation fast enough to measure as zero is exactly the one a
        //  rolling average is supposed to keep steady, so the effect is a
        //  timer that jumps on its cheapest frames. Pinned here so that
        //  fixing it is a deliberate change with a test that moves, rather
        //  than a silent one.
        //
        PerfStats::Instance().Record ("perfstats.zero.reseed", 0.0);
        PerfStats::Instance().Record ("perfstats.zero.reseed", 5.0);
        stat = PerfStats::Instance().Get ("perfstats.zero.reseed");

        Assert::AreEqual (5.0, stat.avgMs, 0.0001,
                          L"today the average re-seeds; smoothing would give 0.5");
    }
};
