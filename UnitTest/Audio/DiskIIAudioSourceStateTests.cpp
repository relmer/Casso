#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DiskIIAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIAudioSourceStateTests
//
//  Verifies that the IDriveAudioSink event hooks mutate internal
//  state in the documented way without touching the host filesystem
//  or producing audio (spec FR-001..FR-004, FR-012..FR-014).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIIAudioSourceStateTests)
{
public:

    TEST_METHOD (OnMotorStart_setsRunningTrue)
    {
        DiskIIAudioSource  src;

        Assert::IsFalse (src.IsMotorRunning());
        src.OnMotorStart();
        Assert::IsTrue  (src.IsMotorRunning());
    }

    TEST_METHOD (OnMotorStop_setsRunningFalse)
    {
        DiskIIAudioSource  src;

        src.OnMotorStart();
        src.OnMotorStop  ();
        Assert::IsFalse  (src.IsMotorRunning());
    }

    TEST_METHOD (OnHeadStep_resetsHeadPos_andSelectsStepBuffer)
    {
        DiskIIAudioSource  src;
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
            Assert::AreEqual (0.5f * DiskIIAudioSource::kHeadVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnHeadBump_selectsStopBufferDistinctFromStep)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (32, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (32, 0.9f));

        src.OnHeadBump();
        src.GeneratePCM (out, 8);

        // Bump should hit the stop buffer (0.9), not the step buffer
        // (0.5). Volume is the same kHeadVolume for both.
        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.9f * DiskIIAudioSource::kHeadVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnDiskInserted_selectsCloseBuffer_resetsDoorPos)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.6f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.3f));

        src.OnDiskInserted();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.6f * DiskIIAudioSource::kDoorVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (OnDiskEjected_selectsOpenBuffer_resetsDoorPos)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.6f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.3f));

        src.OnDiskEjected();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.3f * DiskIIAudioSource::kDoorVolume, out[i], 1e-5f);
        }
    }

    TEST_METHOD (SetPan_storesValuesAndReturnsThem)
    {
        DiskIIAudioSource  src;

        src.SetPan (0.25f, 0.75f);
        Assert::AreEqual (0.25f, src.PanLeft  (), 1e-6f);
        Assert::AreEqual (0.75f, src.PanRight(), 1e-6f);
    }
};
