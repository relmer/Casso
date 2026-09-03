#include "Pch.h"
#include "Audio/Disk2AudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AudioSourceStateTests
//
//  Verifies that the IDriveAudioSink event hooks mutate internal
//  state in the documented way without touching the host filesystem
//  or producing audio (spec FR-001..FR-004, FR-012..FR-014).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2AudioSourceStateTests)
{
public:

    TEST_METHOD (OnMotorEngaged_setsRunningTrue)
    {
        Disk2AudioSource  src;

        Assert::IsFalse (src.IsMotorRunning());
        src.OnMotorEngaged();
        Assert::IsTrue  (src.IsMotorRunning());
    }

    TEST_METHOD (OnMotorDisengaged_setsRunningFalse)
    {
        Disk2AudioSource  src;

        src.OnMotorEngaged();
        src.OnMotorDisengaged();
        Assert::IsFalse  (src.IsMotorRunning());
    }

    TEST_METHOD (OnHeadStep_resetsHeadPos_andSelectsStepBuffer)
    {
        Disk2AudioSource   src;
        float              out[16] = {};

        // 140 samples is one per quarter-track of a full stroke, so a step of
        // N quarter-tracks buys exactly N samples and the arithmetic below
        // needs no rounding argument.
        src.SetSampleBufferForTest (L"HeadStep", vector<float> (140, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (140, 0.9f));

        // First step starts a fresh shot from the step buffer. It plays a
        // SLICE, not the whole clip: the recording is a full-stroke seek, and
        // one step did not travel a full stroke. The first step after a reset
        // is charged one half-track, which is two quarter-tracks.
        src.Tick      (100000);
        src.OnHeadStep (1);
        src.GeneratePCM (out, 16);

        for (int i = 0; i < 2; i++)
        {
            Assert::AreEqual (0.5f * Disk2AudioSource::kHeadVolume, out[i], 1e-5f,
                              L"the step's own slice plays from the step buffer");
        }

        for (int i = 2; i < 16; i++)
        {
            Assert::AreEqual (0.0f, out[i], 1e-5f,
                              L"and stops there rather than running the whole recording");
        }
    }

    TEST_METHOD (OnHeadBump_selectsStopBufferDistinctFromStep)
    {
        Disk2AudioSource   src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (32, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (32, 0.9f));

        src.OnHeadBump();
        src.GeneratePCM (out, 8);

        // Bump should hit the stop buffer (0.9), not the step buffer
        // (0.5). Volume is the same kHeadVolume for both.
        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.9f * Disk2AudioSource::kHeadVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnDiskInserted_selectsCloseBuffer_resetsDoorPos)
    {
        Disk2AudioSource   src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.6f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.3f));

        src.OnDiskInserted();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.6f * Disk2AudioSource::kDoorVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnDiskEjected_selectsOpenBuffer_resetsDoorPos)
    {
        Disk2AudioSource   src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.6f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.3f));

        src.OnDiskEjected();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.3f * Disk2AudioSource::kDoorVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (SetPan_storesValuesAndReturnsThem)
    {
        Disk2AudioSource  src;

        src.SetPan (0.25f, 0.75f);
        Assert::AreEqual (0.25f, src.GetPanLeft(),  1e-6f);
        Assert::AreEqual (0.75f, src.GetPanRight(), 1e-6f);
    }
};
