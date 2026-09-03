#include "Pch.h"
#include "Audio/Disk2AudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AudioSourceSeekTests
//
//  Step-vs-seek discrimination (spec FR-005). The source enters
//  "seek mode" when steps arrive within kSeekThresholdCycles of each
//  other, and auto-clears the mode after kHeadIdleCycles of silence.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2AudioSourceSeekTests)
{
public:

    TEST_METHOD (FourStepsWithin16ms_doNotResetHeadPosFourTimes)
    {
        Disk2AudioSource  src;
        uint64_t          cycle  = 0;
        float             out[4] = {};
        vector<float>      step (8);
        cycle = 100000;

        // Ramp so we can identify which sample position we're at.
        for (int i = 0; i < 8; i++)
        {
            step[i] = static_cast<float> (i + 1) * 0.1f;
        }

        src.SetSampleBufferForTest (L"HeadStep", std::move (step));

        // First step. Tick precedes the event so the source can stamp
        // m_currentCycle.
        src.Tick      (cycle);
        src.OnHeadStep (1);
        src.GeneratePCM (out, 4);
        Assert::IsFalse (src.IsSeekMode());

        // Three more steps, each 2,000 cycles after the previous --
        // well inside kSeekThresholdCycles (16,368). The source should
        // enter seek mode and stop restarting the step buffer.
        for (int n = 0; n < 3; n++)
        {
            cycle += 2000;
            src.Tick      (cycle);
            src.OnHeadStep (1 + n);
        }

        Assert::IsTrue (src.IsSeekMode());
        Assert::AreEqual (cycle, src.GetLastStepCycle());
    }

    TEST_METHOD (StepsApartByMoreThan16ms_treatBothAsSingleClicks)
    {
        Disk2AudioSource  src;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));

        src.Tick      (100000);
        src.OnHeadStep (1);
        Assert::IsFalse (src.IsSeekMode());

        // 30 ms gap ~= 30,690 cycles -- comfortably above the
        // 16,368-cycle threshold.
        src.Tick      (100000 + 30690);
        src.OnHeadStep (2);
        Assert::IsFalse (src.IsSeekMode());
    }

    TEST_METHOD (SeekModeIdleTimeout_after50ms_clearsSeekMode)
    {
        Disk2AudioSource  src;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));

        // Enter seek mode with two close steps.
        src.Tick      (100000);
        src.OnHeadStep (1);
        src.Tick      (100000 + 2000);
        src.OnHeadStep (2);
        Assert::IsTrue (src.IsSeekMode());

        // Tick well past kHeadIdleCycles (51,150).
        src.Tick (100000 + 2000 + 60000);
        Assert::IsFalse (src.IsSeekMode());
    }

    TEST_METHOD (OnHeadBump_alwaysRestartsAndClearsSeekMode)
    {
        Disk2AudioSource  src;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (16, 0.9f));

        src.Tick      (100000);
        src.OnHeadStep (1);
        src.Tick      (101000);
        src.OnHeadStep (2);
        Assert::IsTrue (src.IsSeekMode());

        src.OnHeadBump();
        Assert::IsFalse (src.IsSeekMode());
    }


    //
    //  How long a seek SOUNDS is how far the head went.
    //
    //  HeadStep.wav is a recording of a seek, not of a step: both shipped
    //  mechanisms are a sustained rattle with no discrete impulses. So a step
    //  buys a slice of it sized by the distance travelled, and a longer seek
    //  buys proportionally more.
    //
    //  Measured by counting rendered samples, which needs no tolerance: the
    //  buffer is all ones, MixHead adds one buffer sample per output sample,
    //  and the head is the only channel running. GeneratePCM has to be
    //  interleaved with the steps because m_headPos only advances inside it.
    //
    static int RenderSeek (int quarterTracksPerStep, int steps)
    {
        Disk2AudioSource  src;
        float             out[64] = {};
        int               nonZero = 0;
        int               qt      = 0;
        uint64_t          cycle   = 100000;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (14000, 1.0f));

        for (int n = 0; n < steps; n++)
        {
            qt    += quarterTracksPerStep;
            cycle += 2000;                   // inside the seek window

            src.Tick       (cycle);
            src.OnHeadStep (qt);
        }

        // Drain well past anything the seek could have bought.
        for (int frame = 0; frame < 64; frame++)
        {
            for (int i = 0; i < 64; i++)
            {
                out[i] = 0.0f;
            }

            src.GeneratePCM (out, 64);

            for (int i = 0; i < 64; i++)
            {
                if (out[i] != 0.0f)
                {
                    nonZero++;
                }
            }
        }

        return nonZero;
    }


    TEST_METHOD (SeekDuration_growsWithDistanceTravelled)
    {
        int  shortSeek = RenderSeek (2, 2);    //  4 quarter-tracks, one track
        int  longSeek  = RenderSeek (2, 20);   // 40 quarter-tracks, ten tracks

        Assert::IsTrue (shortSeek > 0,
                        L"a one-track move still makes a sound");
        Assert::IsTrue (longSeek > shortSeek * 5,
                        L"ten times the distance lasts far longer, rather than "
                        L"both playing the same fixed clip");
    }


    TEST_METHOD (ZeroDistanceRepeat_buysNoMoreAudio)
    {
        // The same phase written twice moves nothing. Before distance was
        // honored every one of these restarted the whole clip.
        Disk2AudioSource  src;
        float             out[64] = {};
        int               nonZero = 0;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (14000, 1.0f));

        src.Tick       (100000);
        src.OnHeadStep (40);

        for (int n = 0; n < 8; n++)
        {
            src.Tick       (100000 + (uint64_t) (n + 1) * 2000);
            src.OnHeadStep (40);               // same position every time
        }

        for (int frame = 0; frame < 64; frame++)
        {
            for (int i = 0; i < 64; i++)
            {
                out[i] = 0.0f;
            }

            src.GeneratePCM (out, 64);

            for (int i = 0; i < 64; i++)
            {
                if (out[i] != 0.0f)
                {
                    nonZero++;
                }
            }
        }

        Assert::IsTrue (nonZero <= RenderSeek (2, 1),
                        L"standing still buys no more sound than a single step");
    }
};
