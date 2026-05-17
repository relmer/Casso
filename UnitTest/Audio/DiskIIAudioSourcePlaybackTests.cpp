#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DiskIIAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIAudioSourcePlaybackTests
//
//  Exercises GeneratePCM with synthetic in-memory sample buffers
//  (spec FR-001, FR-003, FR-004, FR-009, FR-013, FR-014).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIIAudioSourcePlaybackTests)
{
public:

    TEST_METHOD (MotorRunning_outputContainsScaledMotorSamples)
    {
        DiskIIAudioSource  src;
        float              out[16] = {};

        src.SetSampleBufferForTest (L"MotorLoop", vector<float> (32, 1.0f));
        src.OnMotorStart();
        src.GeneratePCM (out, 16);

        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual (DiskIIAudioSource::kMotorVolume, out[i], 1e-6f);
        }
    }

    TEST_METHOD (MotorRunning_wrapsAtBufferEnd)
    {
        DiskIIAudioSource  src;
        vector<float>      motor (4);
        float              out[16] = {};

        motor[0] = 1.0f;
        motor[1] = 2.0f;
        motor[2] = 3.0f;
        motor[3] = 4.0f;

        src.SetSampleBufferForTest (L"MotorLoop", std::move (motor));
        src.OnMotorStart();
        src.GeneratePCM (out, 16);

        // Pattern wraps every 4 samples.
        for (int i = 0; i < 16; i++)
        {
            float  expected = static_cast<float> ((i % 4) + 1) * DiskIIAudioSource::kMotorVolume;

            Assert::AreEqual (expected, out[i], 1e-6f);
        }
    }

    TEST_METHOD (MotorNotRunning_motorContributionIsZero)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"MotorLoop", vector<float> (16, 1.0f));
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }

    TEST_METHOD (OnHeadStep_then_GeneratePCM_outputsScaledStepSampleOnce_thenZero)
    {
        DiskIIAudioSource  src;
        vector<float>      step (4, 0.5f);
        float              out[8] = {};

        src.SetSampleBufferForTest (L"HeadStep", std::move (step));
        src.OnHeadStep (1);
        src.GeneratePCM (out, 8);

        // First 4 samples carry the step sound; remainder are silent.
        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual (0.5f * DiskIIAudioSource::kHeadVolume, out[i], 1e-6f);
        }
        for (int i = 4; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i], 1e-6f);
        }
    }

    TEST_METHOD (MotorPlusHeadPlusDoor_simultaneouslyMixedAdditively)
    {
        DiskIIAudioSource  src;
        float              out[4] = {};

        src.SetSampleBufferForTest (L"MotorLoop", vector<float> (16, 1.0f));
        src.SetSampleBufferForTest (L"HeadStep",  vector<float> (16, 1.0f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 1.0f));

        src.OnMotorStart   ();
        src.OnHeadStep     (1);
        src.OnDiskInserted();

        src.GeneratePCM (out, 4);

        float  expected = DiskIIAudioSource::kMotorVolume +
                          DiskIIAudioSource::kHeadVolume  +
                          DiskIIAudioSource::kDoorVolume;

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual (expected, out[i], 1e-6f);
        }
    }

    TEST_METHOD (MissingMotorBuffer_emptyBuffer_outputsSilenceForMotor)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        // No MotorLoop set -> empty buffer; FR-009.
        src.OnMotorStart();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }

    TEST_METHOD (MissingHeadBuffer_OnHeadStep_outputsSilence)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.OnHeadStep (1);
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }

    TEST_METHOD (MissingDoorBuffer_OnDiskInserted_outputsSilence)
    {
        DiskIIAudioSource  src;
        float              out[8] = {};

        src.OnDiskInserted();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }
};
