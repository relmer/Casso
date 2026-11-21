#include "Pch.h"

#include "Audio/Floppy35AudioSource.h"
#include "Audio/HardDiskAudioSource.h"
#include "../EmuTests/FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceAudioSourceTests
//
//  Each 3.5-inch mechanism event starts its recording, and each hard disk
//  read starts a synthesized tick. The volumes scale both.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeviceAudioSourceTests)
{
public:

    static constexpr uint32_t  kRate  = 48'000;
    static constexpr uint32_t  kBlock = 4'800;


    static float Peak (IDriveAudioSource & source, uint32_t samples = kBlock)
    {
        std::vector<float>  pcm (samples, 0.0f);
        float               peak = 0.0f;



        source.GeneratePCM (pcm.data(), samples);

        for (float s : pcm)
        {
            peak = std::max (peak, std::abs (s));
        }

        return peak;
    }


    static Floppy35Event Event (Floppy35Op op, Byte drive = 0)
    {
        Floppy35Event  event;



        event.op    = op;
        event.drive = drive;

        return event;
    }


    //  Each recording is a constant level, so the peak of a block identifies
    //  the recording playing in it.
    static constexpr float  kSpinStartLevel = 0.1f;
    static constexpr float  kSpinLoopLevel  = 0.2f;
    static constexpr float  kSpinEndLevel   = 0.3f;
    static constexpr float  kStepLevel      = 0.4f;
    static constexpr float  kSeekLevel      = 0.5f;
    static constexpr float  kTolerance      = 0.0001f;


    static void LoadTestSamples (Floppy35AudioSource & source)
    {
        using Sound = Floppy35AudioSource::Sound;

        source.SetSampleRate (kRate);
        source.SetSampleForTest (Sound::SpinStart, std::vector<float> (kBlock, kSpinStartLevel));
        source.SetSampleForTest (Sound::SpinLoop,  std::vector<float> (kBlock, kSpinLoopLevel));
        source.SetSampleForTest (Sound::SpinEnd,   std::vector<float> (kBlock, kSpinEndLevel));
        source.SetSampleForTest (Sound::Step,      std::vector<float> (kBlock, kStepLevel));
        source.SetSampleForTest (Sound::Seek,      std::vector<float> (kBlock, kSeekLevel));
    }


    TEST_METHOD (Floppy35_WithoutRecordings_IsSilent)
    {
        Floppy35AudioSource  source;



        source.SetSampleRate (kRate);
        source.OnFloppy35Event (Event (Floppy35Op::MotorOn));
        source.OnFloppy35Event (Event (Floppy35Op::Step));

        Assert::AreEqual (0.0f, Peak (source));
        Assert::IsTrue (source.IsSilent());
    }


    TEST_METHOD (Floppy35_MotorSpinsUp_Runs_AndSpinsDown)
    {
        Floppy35AudioSource  source;



        LoadTestSamples (source);
        source.OnFloppy35Event (Event (Floppy35Op::MotorOn));

        Assert::AreEqual (kSpinStartLevel, Peak (source), kTolerance, L"spin-up");
        Assert::AreEqual (kSpinLoopLevel,  Peak (source), kTolerance, L"running");
        Assert::AreEqual (kSpinLoopLevel,  Peak (source), kTolerance, L"still running");

        source.OnFloppy35Event (Event (Floppy35Op::MotorOff));

        Assert::AreEqual (kSpinEndLevel, Peak (source), kTolerance, L"spin-down");
        Assert::AreEqual (0.0f, Peak (source), L"stopped");
        Assert::IsTrue (source.IsSilent());
    }


    TEST_METHOD (Floppy35_OneStep_PlaysTheStepOnce)
    {
        Floppy35AudioSource  source;



        LoadTestSamples (source);
        source.OnFloppy35Event (Event (Floppy35Op::Step));

        Assert::IsFalse (source.IsSeeking());
        Assert::AreEqual (kStepLevel, Peak (source), kTolerance);
        Assert::AreEqual (0.0f, Peak (source));
    }


    TEST_METHOD (Floppy35_StepsInQuickSuccession_PlayTheSeek_UntilTheyStop)
    {
        Floppy35AudioSource  source;
        uint32_t             stepGap = kRate * 12 / 1000;



        LoadTestSamples (source);
        source.OnFloppy35Event (Event (Floppy35Op::Step));
        Peak (source, stepGap);

        for (int i = 0; i < 20; i++)
        {
            source.OnFloppy35Event (Event (Floppy35Op::Step));
            Assert::AreEqual (kSeekLevel, Peak (source, stepGap), kTolerance, std::format (L"step {}", i).c_str());
            Assert::IsTrue (source.IsSeeking());
        }

        Peak (source, kRate / 10);
        Assert::IsFalse (source.IsSeeking());
        Assert::AreEqual (0.0f, Peak (source), L"the seek ends with the steps");
    }


    TEST_METHOD (Floppy35_FiltersOneDrive)
    {
        Floppy35AudioSource  driveOne (0);



        LoadTestSamples (driveOne);
        driveOne.OnFloppy35Event (Event (Floppy35Op::Step, 1));
        Assert::AreEqual (0.0f, Peak (driveOne), L"drive 2's step isn't drive 1's");
    }


    TEST_METHOD (Floppy35_VolumesScale)
    {
        Floppy35AudioSource  source;



        LoadTestSamples (source);
        source.SetVolumes (0.5f, 0.0f, 1.0f);
        source.OnFloppy35Event (Event (Floppy35Op::MotorOn));
        source.OnFloppy35Event (Event (Floppy35Op::Step));

        Assert::AreEqual (kSpinStartLevel * 0.5f, Peak (source), kTolerance, L"head muted, motor at half");
    }


    TEST_METHOD (Floppy35_LoadsTheRecordingsFromAFolder)
    {
        Floppy35AudioSource  source;
        fs::path             repo   = fs::path (FixtureProvider().GetRoot()).parent_path().parent_path();
        fs::path             folder = repo / L"Assets" / L"Drive Sounds" / Floppy35AudioSource::kSampleFolder;
        HRESULT              hr     = S_OK;



        hr = source.LoadSamples (folder, kRate);
        Assert::IsTrue (SUCCEEDED (hr));

        source.OnFloppy35Event (Event (Floppy35Op::Step));
        Assert::IsTrue (Peak (source) > 0.001f, folder.c_str());
    }


    //  The spindle is silent, even after a spin-up, and reads tick.
    TEST_METHOD (HardDisk_SilentSpindleAndTicks)
    {
        HardDiskAudioSource  source (0.2f);
        BlockDeviceEvent     event;



        source.SetSampleRate (kRate);
        source.OnMotorEngaged();
        event.op = BlockDeviceOp::SpinUp;
        source.OnBlockDeviceEvent (event);
        Assert::AreEqual (0.0f, Peak (source, kRate), L"spindle silent");

        event.op = BlockDeviceOp::Read;
        source.OnBlockDeviceEvent (event);
        Assert::IsTrue (source.IsTickActive());
        Assert::IsTrue (Peak (source, kRate / 100) > 0.001f, L"tick audible");
        Assert::IsFalse (source.IsTickActive());
    }
};
