#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/AppleSpeaker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SpeakerTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SpeakerTests)
{
public:

    TEST_METHOD (Read_TogglesSpeakerState)
    {
        AppleSpeaker spk;

        float before = spk.GetSpeakerState ();
        spk.Read (0xC030);
        float after = spk.GetSpeakerState ();

        Assert::AreNotEqual (before, after);
    }

    TEST_METHOD (Read_AccumulatesTimestamps)
    {
        AppleSpeaker spk;
        uint64_t cycles = 100;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps ().size ());
    }

    TEST_METHOD (ClearTimestamps_EmptiesVector)
    {
        AppleSpeaker spk;
        uint64_t cycles = 50;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);
        spk.Read (0xC030);
        spk.ClearTimestamps ();

        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }

    TEST_METHOD (NoToggles_SilentState)
    {
        AppleSpeaker spk;

        Assert::AreEqual (-0.25f, spk.GetSpeakerState ());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }

    TEST_METHOD (Reset_ClearsState)
    {
        AppleSpeaker spk;
        spk.Read (0xC030);
        spk.Reset ();

        Assert::AreEqual (-0.25f, spk.GetSpeakerState ());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }

    TEST_METHOD (CycleCounter_RecordsCorrectTimestamp)
    {
        AppleSpeaker spk;
        uint64_t cycles = 500;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps ().size ());
        Assert::AreEqual (static_cast<uint32_t> (500), spk.GetToggleTimestamps ()[0]);
    }

    TEST_METHOD (CycleCounter_AdvancingCyclesRecordsDifferentTimestamps)
    {
        AppleSpeaker spk;
        uint64_t cycles = 100;
        spk.SetCycleCounter (&cycles);

        spk.Read (0xC030);
        cycles = 200;
        spk.Read (0xC030);

        Assert::AreEqual (size_t (2), spk.GetToggleTimestamps ().size ());
        Assert::AreEqual (static_cast<uint32_t> (100), spk.GetToggleTimestamps ()[0]);
        Assert::AreEqual (static_cast<uint32_t> (200), spk.GetToggleTimestamps ()[1]);
    }

    TEST_METHOD (NoCycleCounter_NoTimestampsRecorded)
    {
        AppleSpeaker spk;

        spk.Read (0xC030);

        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }
};
