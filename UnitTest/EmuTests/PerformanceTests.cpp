#include "Pch.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PerformanceTests
//
//  Phase 15 (User Story 6, FR-042 / SC-007). These tests are meaningful
//  only against the optimized build: in Debug builds the EmuCpu / bus /
//  device dispatch is unoptimized and the measurements have no relation
//  to shipping performance. The whole class is conditionally compiled
//  on NDEBUG (set by Release configurations of UnitTest.vcxproj). In
//  Debug builds the class still exists with a single sentinel method so
//  CppUnitTestFramework discovery doesn't complain — it reports
//  Inconclusive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PerformanceTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Performance budget constants
    //
    //  Real //e runs at 1.023 MHz, so 1,000,000 emulated cycles cost
    //  977.5 ms of wall-clock on real hardware. The spec target (FR-042,
    //  SC-007) is "≤ ~1% of one host core when throttled" — i.e. roughly
    //  9.775 ms of host time per 1,000,000 emulated cycles. The
    //  unthrottled measurement here just needs to demonstrate ≥ 10×
    //  headroom over real //e speed, so the ceiling is 10× the 1% target
    //  ≈ 97.75 ms. See plan.md §"Performance measurement strategy".
    //
    //  The stability gate (T125) requires that the worst of 5 runs lie
    //  within 30% of the median — protects against jitter from background
    //  host activity without masking a real perf regression.
    //
    ////////////////////////////////////////////////////////////////////////////

    static constexpr uint64_t   kPerfMeasureCycles          = 1'000'000ULL;
    static constexpr uint64_t   kPerfWarmupCycles           =   100'000ULL;
    static constexpr uint64_t   kColdBootCycles             = 5'000'000ULL;
    static constexpr double     kPerformanceCeilingMs       = 97.75;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  QpcFrequencyHz — cached QueryPerformanceFrequency value. Stable
    //  for the process lifetime per Win32 contract.
    //
    ////////////////////////////////////////////////////////////////////////////

    int64_t QpcFrequencyHz()
    {
        LARGE_INTEGER   freq = {};

        QueryPerformanceFrequency (&freq);
        return freq.QuadPart;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ElapsedMs — wall-clock delta in milliseconds between two
    //  QueryPerformanceCounter readings.
    //
    ////////////////////////////////////////////////////////////////////////////

    double ElapsedMs (int64_t startTicks, int64_t endTicks, int64_t freqHz)
    {
        double   elapsedSec;

        elapsedSec = static_cast<double> (endTicks - startTicks)
                   / static_cast<double> (freqHz);
        return elapsedSec * 1000.0;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MeasureMillionCycles — build a //e via HeadlessHost, cold-boot it
    //  to the Applesoft idle prompt, run a small warmup so caches and
    //  branch predictors stabilize, then time exactly
    //  kPerfMeasureCycles emulated cycles and return the wall-clock cost.
    //
    //  Pinned PRNG seed (HeadlessHost::kPinnedSeed = 0xCA550001) ensures
    //  RAM init is deterministic, so two runs measure the same workload.
    //
    ////////////////////////////////////////////////////////////////////////////

    HRESULT MeasureMillionCycles (double & outElapsedMs)
    {
        HRESULT          hr        = S_OK;
        HeadlessHost     host;
        EmulatorCore     core;
        LARGE_INTEGER    startQpc  = {};
        LARGE_INTEGER    endQpc    = {};
        int64_t          freqHz    = 0;

        outElapsedMs = 0.0;

        hr = host.BuildApple2e (core);
        CHRA (hr);

        if (!core.HasApple2e())
        {
            hr = E_UNEXPECTED;
            CHRA (hr);
        }

        core.PowerCycle();
        core.RunCycles  (kColdBootCycles);
        core.RunCycles  (kPerfWarmupCycles);

        freqHz = QpcFrequencyHz();
        if (freqHz <= 0)
        {
            hr = E_UNEXPECTED;
            CHRA (hr);
        }

        QueryPerformanceCounter (&startQpc);
        core.RunCycles (kPerfMeasureCycles);
        QueryPerformanceCounter (&endQpc);

        outElapsedMs = ElapsedMs (startQpc.QuadPart, endQpc.QuadPart, freqHz);

    Error:
        return hr;
    }


#ifdef NDEBUG

    ////////////////////////////////////////////////////////////////////////
    //
    //  CycleEmulation_MeetsBudget — T123. Single run; must complete
    //  1,000,000 emulated //e cycles within kPerformanceCeilingMs (≈
    //  10× headroom over real //e speed of 977.5 ms / Mcycle).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (CycleEmulation_MeetsBudget)
    {
        HRESULT     hr        = S_OK;
        double      elapsedMs = 0.0;
        wchar_t     msg[256]  = {};

        hr = MeasureMillionCycles (elapsedMs);
        AssertSucceeded (hr,
            L"MeasureMillionCycles must succeed");

        swprintf_s (msg, L"1M cycles took %.2f ms (ceiling %.2f ms)",
            elapsedMs, kPerformanceCeilingMs);
        Logger::WriteMessage (msg);

        Assert::IsTrue (elapsedMs <= kPerformanceCeilingMs, msg);
    }


    //
    //  T125's CycleEmulation_StableRunToRun used to sit here. It took five
    //  samples and asserted worst <= median * 1.6 -- a pure VARIANCE gate on
    //  wall-clock time, which measures the host's scheduler rather than this
    //  code. It carried no signal CycleEmulation_MeetsBudget lacks (a genuine
    //  slowdown moves the median, which the budget catches) and failed about
    //  one full-suite run in three on a developer machine doing anything else.
    //
    //  Its tolerance had already been widened 30% -> 60% chasing a hosted-CI
    //  outlier, which is the tell: the number was tracking the noise floor of
    //  whatever machine ran it, not a property of the emulator.
    //
    //  Throughput still has a gate. Stability of an elapsed-time measurement on
    //  a shared machine is not something a unit test can assert.
    //


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Shared-image detection cost (spec 028, FR-031 / SC-006)
    //
    //  WHAT THIS FEATURE ADDED THAT RUNS ALL THE TIME is one thing: a callback
    //  the disk controller fires once per emulated frame, which walks the
    //  sixteen bays looking for a change that has settled. The watcher threads
    //  are blocked in ReadDirectoryChangesW and consume nothing until the
    //  kernel wakes them, so there is no steady-state figure to take from them.
    //
    //  MEASURED DIRECTLY RATHER THAN THROUGH FRAME TIME, and that is the whole
    //  point of doing it this way. The emulator is SPEED-CAPPED: it targets
    //  1.02 MHz and sleeps the rest of each frame, so work added to the CPU
    //  thread eats headroom instead of moving frame time. A frame-time A/B is
    //  near-blind to this by construction until the cost is enormous -- and it
    //  would mostly be measuring the host's scheduler, which is exactly why the
    //  run-to-run variance gate above this was deleted.
    //
    //  The spec originally asked for three five-minute Release runs per arm
    //  comparing p99 frame time. That buys statistical comfort on a metric that
    //  cannot see the thing; this answers the question in milliseconds.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  Every bay full, which is the worst case for a walk over the bays.
    static constexpr int       kIdleProbeCalls   = 1'000'000;

    //  One emulated frame at 1.0205 MHz, matching Disk2Controller's own gate.
    static constexpr double    kFrameCycles      = 17030.0;
    static constexpr double    kApple2ClockHz    = 1'020'484.0;

    //  The budget this tree already holds itself to: ~1% of one host core while
    //  throttled, which is 9.775 ms of host time per second of emulated time.
    static constexpr double    kOnePercentCoreMsPerEmulatedSecond = 9.775;


    //  A store with every bay mounted, so the per-frame walk has the most work
    //  it can ever have.
    static void  FillEveryBay (DiskImageStore & store)
    {
        std::vector<Byte>  image (NibblizationLayer::kImageByteSize, 0x00);
        int                slot  = 0;
        int                drive = 0;

        for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
        {
            for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
            {
                HRESULT  hr = store.MountFromBytes (slot, drive, "perf.dsk",
                                                    DiskFormat::Dsk, image);

                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }


#ifdef NDEBUG

    ////////////////////////////////////////////////////////////////////////
    //
    //  SharedImageIdleProbe_CostsNothingTheUserCanFeel
    //
    //  FR-031 / SC-006, measured rather than asserted.
    //
    //  THE COMPARISON IS AGAINST THE TREE'S OWN THROTTLED BUDGET -- 1% of a
    //  core, 9.775 ms of host time per emulated second -- rather than against
    //  an absolute nanosecond figure, which would mean nothing on a different
    //  machine. Sixty of these run per emulated second, and the whole feature
    //  is allowed a thousandth of that budget.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SharedImageIdleProbe_CostsNothingTheUserCanFeel)
    {
        DiskImageStore   store;
        LARGE_INTEGER    startQpc      = {};
        LARGE_INTEGER    endQpc        = {};
        int64_t          freqHz        = QpcFrequencyHz();
        double           elapsedMs     = 0.0;
        double           nsPerCall     = 0.0;
        double           msPerSecond   = 0.0;
        double           shareOfBudget = 0.0;
        int              i             = 0;
        wchar_t          msg[512]      = {};



        Assert::IsTrue (freqHz > 0);

        FillEveryBay (store);

        //  Warm the caches, so the figure is the steady-state cost rather than
        //  the first walk's page faults.
        for (i = 0; i < 1000; i++)
        {
            store.ApplyPendingReload();
        }

        QueryPerformanceCounter (&startQpc);

        for (i = 0; i < kIdleProbeCalls; i++)
        {
            store.ApplyPendingReload();
        }

        QueryPerformanceCounter (&endQpc);

        elapsedMs = ElapsedMs (startQpc.QuadPart, endQpc.QuadPart, freqHz);
        nsPerCall = (elapsedMs * 1'000'000.0) / (double) kIdleProbeCalls;

        //  Sixty walks per emulated second, which is what the controller's own
        //  rate limit produces.
        msPerSecond   = (nsPerCall * (kApple2ClockHz / kFrameCycles)) / 1'000'000.0;
        shareOfBudget = msPerSecond / kOnePercentCoreMsPerEmulatedSecond;

        swprintf_s (msg,
            L"idle probe: %.1f ns/call, %.5f ms per emulated second, "
            L"%.4f%% of the 1%%-of-a-core budget",
            nsPerCall, msPerSecond, shareOfBudget * 100.0);
        Logger::WriteMessage (msg);

        //  A thousandth of the throttled budget. Generous by design: the point
        //  is to catch this turning into something that walks disks or takes a
        //  contended lock, not to police a handful of nanoseconds.
        Assert::IsTrue (shareOfBudget <= 0.001, msg);
    }

#endif // NDEBUG


#else // !NDEBUG

    ////////////////////////////////////////////////////////////////////////
    //
    //  Debug build sentinel — perf measurements are meaningless against
    //  the unoptimized build. Skip cleanly and document why.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (CycleEmulation_SkippedInDebug)
    {
        Logger::WriteMessage (
            L"PerformanceTests are Release-only (NDEBUG). Skipped in Debug.");
    }

#endif // NDEBUG
};

