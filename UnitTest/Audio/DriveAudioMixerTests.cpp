#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DriveAudioMixer.h"
#include "Audio/IDriveAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockDriveAudioSource
//
//  Emits a known constant PCM pattern for mixing verification.
//  Holds its pan coefficients caller-side; sink methods are no-op.
//
////////////////////////////////////////////////////////////////////////////////

class MockDriveAudioSource : public IDriveAudioSource
{
public:
    float    m_value = 1.0f;
    float    m_panL  = IDriveAudioSource::kCenterPan;
    float    m_panR  = IDriveAudioSource::kCenterPan;

    void  GeneratePCM   (float * outMono, uint32_t n) override
    {
        for (uint32_t i = 0; i < n; i++)
        {
            outMono[i] = m_value;
        }
    }

    float PanLeft   () const override { return m_panL; }
    float PanRight  () const override { return m_panR; }
    void  SetPan    (float l, float r) override { m_panL = l; m_panR = r; }

    void  OnMotorStart   () override {}
    void  OnMotorStop    () override {}
    void  OnHeadStep     (int)       override {}
    void  OnHeadBump     ()          override {}
    void  OnDiskInserted()          override {}
    void  OnDiskEjected  ()          override {}
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixerTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveAudioMixerTests)
{
public:

    TEST_METHOD (NoSources_outputsSilence)
    {
        DriveAudioMixer  mixer;
        float            out[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

        mixer.GeneratePCM (out, 4);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }

    TEST_METHOD (OneSource_centerPan_appearsEqualOnBothChannels)
    {
        DriveAudioMixer       mixer;
        MockDriveAudioSource  src;
        float                 out[8] = {};

        src.m_value = 0.5f;
        src.SetPan (DriveAudioMixer::kSpeakerCenter, DriveAudioMixer::kSpeakerCenter);
        mixer.RegisterSource (&src);

        mixer.GeneratePCM (out, 4);

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual (out[2 * i], out[2 * i + 1], 1e-6f);
            Assert::AreEqual (0.5f * DriveAudioMixer::kSpeakerCenter, out[2 * i], 1e-6f);
        }
    }

    TEST_METHOD (OneSource_leftPan_appearsMostlyOnLeft)
    {
        DriveAudioMixer       mixer;
        MockDriveAudioSource  src;
        float                 out[8] = {};

        src.m_value = 1.0f;
        src.SetPan (0.9f, 0.1f);
        mixer.RegisterSource (&src);

        mixer.GeneratePCM (out, 4);

        for (int i = 0; i < 4; i++)
        {
            Assert::IsTrue (out[2 * i] > out[2 * i + 1]);
            Assert::AreEqual (0.9f, out[2 * i],     1e-6f);
            Assert::AreEqual (0.1f, out[2 * i + 1], 1e-6f);
        }
    }

    TEST_METHOD (TwoSources_oppositePans_appearOnOppositeChannels)
    {
        DriveAudioMixer       mixer;
        MockDriveAudioSource  left;
        MockDriveAudioSource  right;
        float                 out[8] = {};

        left.m_value  = 1.0f;
        right.m_value = 1.0f;
        left.SetPan  (0.9f, 0.1f);
        right.SetPan (0.1f, 0.9f);

        mixer.RegisterSource (&left);
        mixer.RegisterSource (&right);

        mixer.GeneratePCM (out, 4);

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual (1.0f, out[2 * i],     1e-6f);
            Assert::AreEqual (1.0f, out[2 * i + 1], 1e-6f);
        }
    }

    TEST_METHOD (SetEnabledFalse_outputsSilenceRegardlessOfSources)
    {
        DriveAudioMixer       mixer;
        MockDriveAudioSource  src;
        float                 out[8] = {};

        src.m_value = 1.0f;
        mixer.RegisterSource (&src);
        mixer.SetEnabled (false);

        mixer.GeneratePCM (out, 4);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i]);
        }
    }

    TEST_METHOD (TwoSourcesBothMotorRunning_eachContributesIndependentlyWithItsPan)
    {
        DriveAudioMixer       mixer;
        MockDriveAudioSource  a;
        MockDriveAudioSource  b;
        float                 out[4] = {};

        a.m_value = 0.5f;
        b.m_value = 0.5f;
        a.SetPan (0.8f, 0.2f);
        b.SetPan (0.2f, 0.8f);

        mixer.RegisterSource (&a);
        mixer.RegisterSource (&b);

        mixer.GeneratePCM (out, 2);

        // FR-008: independent sum, no dedup. Each frame =
        //   L = 0.5 * 0.8 + 0.5 * 0.2 = 0.5
        //   R = 0.5 * 0.2 + 0.5 * 0.8 = 0.5
        Assert::AreEqual (0.5f, out[0], 1e-6f);
        Assert::AreEqual (0.5f, out[1], 1e-6f);
        Assert::AreEqual (0.5f, out[2], 1e-6f);
        Assert::AreEqual (0.5f, out[3], 1e-6f);
    }

    TEST_METHOD (MixDriveIntoSpeakerStereo_speakerOnly_driveNull_unchanged)
    {
        float  speaker[4] = { 0.1f, 0.2f, 0.3f, 0.4f };

        DriveAudioMixer::MixDriveIntoSpeakerStereo (speaker, nullptr, 2);

        Assert::AreEqual (0.1f, speaker[0]);
        Assert::AreEqual (0.2f, speaker[1]);
        Assert::AreEqual (0.3f, speaker[2]);
        Assert::AreEqual (0.4f, speaker[3]);
    }

    TEST_METHOD (MixDriveIntoSpeakerStereo_sumsThenClampsPerChannel)
    {
        float  speaker[4] = { 0.3f, 0.3f, 0.3f, 0.3f };
        float  drive[4]   = { 0.2f, 0.2f, 0.2f, 0.2f };

        DriveAudioMixer::MixDriveIntoSpeakerStereo (speaker, drive, 2);

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual (0.5f, speaker[i], 1e-6f);
        }
    }

    TEST_METHOD (MixDriveIntoSpeakerStereo_clampsAboveOne)
    {
        float  speaker[2] = { 0.8f, -0.8f };
        float  drive[2]   = { 0.5f, -0.5f };

        DriveAudioMixer::MixDriveIntoSpeakerStereo (speaker, drive, 1);

        Assert::AreEqual ( 1.0f, speaker[0], 1e-6f);
        Assert::AreEqual (-1.0f, speaker[1], 1e-6f);
    }
};
