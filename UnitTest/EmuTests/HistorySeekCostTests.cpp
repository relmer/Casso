#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int       s_kSeekKeyframes = 40;
static constexpr int       s_kSeekRepeats   = 20;
static constexpr uint64_t  s_kSeekPixel     = 2000;
static constexpr int       s_kSeekEighths   = 8;
static constexpr uint64_t  s_kSeekShort     = 64;     // short of the next keyframe, which keyframes land a cycle or two either side of





////////////////////////////////////////////////////////////////////////////////
//
//  HistorySeekCostTests
//
//  Not a pass or fail: what a seek to a cycle costs, written to the test log,
//  for the history timeline's pixel by pixel clicks and drags. A seek restores
//  the keyframe at or before the cycle and replays forward to it, so its cost
//  runs from one restore, landing just past a keyframe, to a restore and a
//  whole keyframe interval replayed, landing just before the next. A seek
//  forward within the stretch the machine is already in replays only the
//  distance moved, as a drag to the right does. The machine is the reverse
//  rig's //e, with a disk in the drive, at the default keyframe interval.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistorySeekCostTests)
{
public:

    TEST_METHOD (EverySeekByCycleIsMeasured)
    {
        TestMachine          machine    ("Apple2e");
        ReverseController    controller (machine);
        ReverseResult        result;
        uint64_t             interval   = 0;
        uint64_t             early      = 0;
        uint64_t             late       = 0;
        uint64_t             forward    = 0;
        uint64_t             offset     = 0;
        int                  i          = 0;
        HRESULT              hr         = S_OK;
        std::string          log;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        AssertSucceeded (hr, L"Start");

        interval = controller.GetKeyframes().GetSettings().intervalCycles;

        for (i = 0; i < s_kSeekKeyframes; i++)
        {
            machine.RunCycles (interval);
        }

        Assert::IsTrue (controller.GetKeyframes().GetCount() >= (size_t) s_kSeekKeyframes / 2, L"history was recorded");

        //  Two stretches far apart, so every seek between them restores.
        early = controller.GetKeyframes().GetInfo (2).cycle;
        late  = controller.GetKeyframes().GetInfo (controller.GetKeyframes().GetCount() - 3).cycle;

        log += std::format ("history seek by cycle, {} build, keyframe every {} cycles\n", IsDebugBuild() ? "Debug" : "Release", interval);

        //  Restoring, then replaying from 0 to 8 eighths of an interval.
        for (i = 0; i <= s_kSeekEighths; i++)
        {
            offset = (uint64_t) i * interval / s_kSeekEighths;
            offset = std::clamp (offset, (uint64_t) 1, interval - s_kSeekShort);

            log += std::format ("  restore, replay {}/{} of an interval               {:8.3f} ms\n", i, s_kSeekEighths,
                                TimeAlternating (controller, early + offset, late + offset));
        }

        hr = controller.SeekToCycle (early + 1, result);
        AssertSucceeded (hr, L"SeekToCycle");

        forward = early + 1;

        log += std::format ("  forward {} cycles in the same stretch          {:8.3f} ms\n", s_kSeekPixel,
                            TimeMs ([&]
                            {
                                forward += s_kSeekPixel;
                                hr = controller.SeekToCycle (forward, result);
                            }));
        AssertSucceeded (hr, L"SeekToCycle forward");

        Logger::WriteMessage (log.c_str());
    }

private:

    static double TimeAlternating (ReverseController & controller, uint64_t first, uint64_t second)
    {
        constexpr uint64_t  kLandSlack = 8;    // the longest 6502 instruction is 7 cycles

        ReverseResult  result;
        HRESULT        hr     = S_OK;
        bool           isLate = false;
        double         ms     = 0.0;



        ms = TimeMs ([&]
        {
            isLate = !isLate;
            hr     = controller.SeekToCycle (isLate ? second : first, result);
        });

        AssertSucceeded (hr, L"SeekToCycle");
        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"each seek lands inside history");
        Assert::IsTrue (result.cycle >= (isLate ? second : first) && result.cycle < (isLate ? second : first) + kLandSlack, L"on the first instruction at or after the cycle");

        return ms;
    }


    template <typename Fn>
    static double TimeMs (Fn fn)
    {
        LARGE_INTEGER  start = {};
        LARGE_INTEGER  end   = {};
        LARGE_INTEGER  freq  = {};
        int            i     = 0;



        fn();

        QueryPerformanceFrequency (&freq);
        QueryPerformanceCounter   (&start);

        for (i = 0; i < s_kSeekRepeats; i++)
        {
            fn();
        }

        QueryPerformanceCounter (&end);

        return (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart / s_kSeekRepeats;
    }


    static bool IsDebugBuild()
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
    }
};
