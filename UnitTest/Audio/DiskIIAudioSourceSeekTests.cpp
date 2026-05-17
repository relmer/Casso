#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DiskIIAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIAudioSourceSeekTests
//
//  Step-vs-seek discrimination (spec FR-005). The source enters
//  "seek mode" when steps arrive within kSeekThresholdCycles of each
//  other, and auto-clears the mode after kHeadIdleCycles of silence.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIIAudioSourceSeekTests)
{
public:

    TEST_METHOD (FourStepsWithin16ms_doNotResetHeadPosFourTimes)
    {
        DiskIIAudioSource  src;
        vector<float>      step (8);
        float              out[4] = {};
        uint64_t           cycle  = 100000;

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
        DiskIIAudioSource  src;

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
        DiskIIAudioSource  src;

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
        DiskIIAudioSource  src;

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
};
