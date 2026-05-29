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
        Disk2AudioSource  src;
        float              out[16] = {};

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (32, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (32, 0.9f));

        // First step starts a fresh shot from the step buffer.
        src.Tick      (100000);
        src.OnHeadStep (1);
        src.GeneratePCM (out, 16);

        // Step volume is 0.30; sample is 0.5 -> 0.15 per output sample.
        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual (0.5f * Disk2AudioSource::kHeadVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnHeadBump_selectsStopBufferDistinctFromStep)
    {
        Disk2AudioSource  src;
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
        Disk2AudioSource  src;
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
        Disk2AudioSource  src;
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
        Assert::AreEqual (0.25f, src.PanLeft(),  1e-6f);
        Assert::AreEqual (0.75f, src.PanRight(), 1e-6f);
    }
};
