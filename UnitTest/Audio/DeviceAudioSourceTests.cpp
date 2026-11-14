#include "Pch.h"

#include "Audio/Floppy35AudioSource.h"
#include "Audio/HardDiskAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceAudioSourceTests
//
//  The synthesized 3.5-inch drive and hard disk sounds: each event makes
//  sound, silence otherwise, and the volumes scale them.
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


    TEST_METHOD (Floppy35_SilentUntilAnEvent)
    {
        Floppy35AudioSource  source;



        source.SetSampleRate (kRate);
        Assert::AreEqual (0.0f, Peak (source));
        Assert::IsTrue (source.IsSilent());
    }


    TEST_METHOD (Floppy35_EachEventMakesSound)
    {
        for (Floppy35Op op : { Floppy35Op::MotorOn, Floppy35Op::Step, Floppy35Op::Eject, Floppy35Op::Insert })
        {
            Floppy35AudioSource  source;



            source.SetSampleRate (kRate);
            source.OnFloppy35Event (Event (op));

            Assert::IsTrue (Peak (source) > 0.001f, std::format (L"op {}", (int) op).c_str());
        }
    }


    //  The motor's hum stops when the motor does; a step's click is over in
    //  10 ms.
    TEST_METHOD (Floppy35_SoundsEnd)
    {
        Floppy35AudioSource  source;



        source.SetSampleRate (kRate);
        source.OnFloppy35Event (Event (Floppy35Op::MotorOn));
        Peak (source, kRate);
        source.OnFloppy35Event (Event (Floppy35Op::MotorOff));
        Peak (source, kRate);
        Assert::AreEqual (0.0f, Peak (source));

        source.OnFloppy35Event (Event (Floppy35Op::Step));
        Peak (source, kRate / 100);
        Assert::AreEqual (0.0f, Peak (source));
    }


    TEST_METHOD (Floppy35_FiltersOneDrive)
    {
        Floppy35AudioSource  driveOne (0);



        driveOne.SetSampleRate (kRate);
        driveOne.OnFloppy35Event (Event (Floppy35Op::Step, 1));
        Assert::AreEqual (0.0f, Peak (driveOne), L"drive 2's step isn't drive 1's");
    }


    TEST_METHOD (Floppy35_VolumesScale)
    {
        Floppy35AudioSource  loud;
        Floppy35AudioSource  muted;



        loud.SetSampleRate (kRate);
        muted.SetSampleRate (kRate);
        muted.SetVolumes (0.0f, 0.0f, 0.0f);

        loud.OnFloppy35Event (Event (Floppy35Op::Step));
        muted.OnFloppy35Event (Event (Floppy35Op::Step));

        Assert::IsTrue (Peak (loud) > 0.001f);
        Assert::AreEqual (0.0f, Peak (muted));
    }


    //  The spindle comes up to full speed over its spin-up time, and reads and
    //  writes tick.
    TEST_METHOD (HardDisk_SpinsUpAndTicks)
    {
        HardDiskAudioSource  source (500, 0.2f);
        BlockDeviceEvent     read;



        source.SetSampleRate (kRate);
        Assert::AreEqual (0.0f, Peak (source), L"at rest");

        source.PowerOn();
        Peak (source, kRate / 4);
        Assert::IsTrue (source.GetSpeed() > 0.4f && source.GetSpeed() < 0.6f, L"halfway after half the spin-up");

        Peak (source, kRate);
        Assert::AreEqual (1.0f, source.GetSpeed());

        read.op = BlockDeviceOp::Read;
        source.OnBlockDeviceEvent (read);
        Assert::IsTrue (source.IsTickActive());
        Peak (source, kRate / 100);
        Assert::IsFalse (source.IsTickActive());
    }
};
