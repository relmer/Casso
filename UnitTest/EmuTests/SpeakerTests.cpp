#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

#include "../Casso65Emu/Devices/AppleSpeaker.h"

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

        Assert::AreEqual (0.0f, spk.GetSpeakerState ());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }

    TEST_METHOD (Reset_ClearsState)
    {
        AppleSpeaker spk;
        spk.Read (0xC030);
        spk.Reset ();

        Assert::AreEqual (0.0f, spk.GetSpeakerState ());
        Assert::AreEqual (size_t (0), spk.GetToggleTimestamps ().size ());
    }
};
