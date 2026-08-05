#include "Pch.h"
#include "Devices/AppleSpeaker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SpeakerTests
//
//  The speaker device: $C030 toggling the cone, and the timestamps that become
//  audio.
//
//  The speaker has exactly ONE control -- any access to $C030 flips it -- so
//  the tests assert that reads toggle as well as writes, which is what period
//  software relies on and what an implementation modelling it as a data
//  register gets wrong.
//
//  Timestamps are recorded in CPU CYCLES rather than samples, since the audio
//  layer resamples them later; that separation is what lets the same capture
//  serve any host sample rate.
//
//  Frame boundaries are covered because the buffer is drained per slice: the
//  initial state must carry across so a tone spanning two slices does not
//  develop a discontinuity at the seam.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SpeakerTests)
{
public:

    TEST_METHOD (Read_TogglesSpeakerState)
    {
        AppleSpeaker  spk;
        float         after = 0.0f;

        float before = spk.GetSpeakerState();
        spk.Read (0xC030);
        after = spk.GetSpeakerState();

        Assert::AreNotEqual (before, after);
    }

    TEST_METHOD (Read_AccumulatesTimestamps)
    {
        AppleSpeaker  spk;
        uint64_t      cycles = 100;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps().size());
    }

    TEST_METHOD (ClearTimestamps_EmptiesVector)
    {
        AppleSpeaker  spk;
        uint64_t      cycles = 50;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);
        spk.Read (0xC030);
        spk.ClearTimestamps();

        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps().size());
    }

    TEST_METHOD (NoToggles_SilentState)
    {
        AppleSpeaker spk;

        Assert::AreEqual (-0.25f, spk.GetSpeakerState());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps().size());
    }

    TEST_METHOD (Reset_ClearsState)
    {
        AppleSpeaker spk;
        spk.Read (0xC030);
        spk.Reset();

        Assert::AreEqual (-0.25f, spk.GetSpeakerState());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps().size());
    }

    TEST_METHOD (CycleCounter_RecordsCorrectTimestamp)
    {
        AppleSpeaker  spk;
        uint64_t      cycles = 500;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps().size());
        Assert::AreEqual (static_cast<uint32_t> (500), spk.GetToggleTimestamps()[0]);
    }

    TEST_METHOD (CycleCounter_AdvancingCyclesRecordsDifferentTimestamps)
    {
        AppleSpeaker  spk;
        uint64_t      cycles = 100;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);
        cycles = 200;
        spk.Read (0xC030);

        Assert::AreEqual (size_t (2), spk.GetToggleTimestamps().size());
        Assert::AreEqual (static_cast<uint32_t> (100), spk.GetToggleTimestamps()[0]);
        Assert::AreEqual (static_cast<uint32_t> (200), spk.GetToggleTimestamps()[1]);
    }

    TEST_METHOD (NoCycleCounter_NoTimestampsRecorded)
    {
        AppleSpeaker spk;

        spk.Read (0xC030);

        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps().size());
    }

    TEST_METHOD (Write_TogglesSpeakerState)
    {
        AppleSpeaker  spk;
        float         after = 0.0f;

        float before = spk.GetSpeakerState();
        spk.Write (0xC030, 0x42);
        after = spk.GetSpeakerState();

        Assert::AreNotEqual (before, after,
            L"STA $C030 must toggle the speaker the same as LDA $C030");
    }

    TEST_METHOD (Mirror_C03X_AllAddressesToggle)
    {
        AppleSpeaker    spk;
        Word            addr;
        float           before;
        float           after;

        for (addr = 0xC030; addr <= 0xC03F; addr++)
        {
            before = spk.GetSpeakerState();
            spk.Read (addr);
            after  = spk.GetSpeakerState();

            Assert::AreNotEqual (before, after,
                L"Every address in $C030..$C03F must toggle on read (16-byte mirror)");
        }
    }

    TEST_METHOD (Mirror_C03X_WriteAlsoToggles)
    {
        AppleSpeaker    spk;
        Word            addr;
        float           before;
        float           after;

        for (addr = 0xC030; addr <= 0xC03F; addr++)
        {
            before = spk.GetSpeakerState();
            spk.Write (addr, 0x00);
            after  = spk.GetSpeakerState();

            Assert::AreNotEqual (before, after,
                L"Every address in $C030..$C03F must toggle on write (16-byte mirror)");
        }
    }
};
