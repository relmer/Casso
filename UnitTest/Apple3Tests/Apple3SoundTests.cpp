#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Machines/Apple3/Apple3Sound.h"
#include "Machines/Apple3/Apple3System.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SoundTests
//
//  The beeper's tone and length, the DAC's samples and their timing on the
//  tick counter, and the speaker, beeper and DAC addresses on a built
//  machine.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SoundTests)
{
public:

    static constexpr double    kTicksPerSecond = 2040968.0;
    static constexpr uint32_t  kSampleRate     = 48000;


    //  How many times the samples cross zero.
    static int CountCrossings (const std::vector<float> & samples)
    {
        int  crossings = 0;



        for (size_t i = 1; i < samples.size(); i++)
        {
            crossings += ((samples[i - 1] < 0.0f) != (samples[i] < 0.0f)) ? 1 : 0;
        }

        return crossings;
    }


    //  A tenth of a second of 1 kHz is 100 periods, 200 crossings.
    TEST_METHOD (Beeper_IsATenthOfASecondAtOneKilohertz)
    {
        uint64_t             ticks = 0;
        Apple3Sound          sound;
        std::vector<float>   during (kSampleRate / 10);
        std::vector<float>   after  (kSampleRate / 10);



        sound.SetTickSource (&ticks, kTicksPerSecond);
        sound.TriggerBeep();
        Assert::IsTrue (sound.IsBeeping());

        ticks = (uint64_t) (kTicksPerSecond / 10);
        sound.GeneratePCM (during.data(), (uint32_t) during.size());
        Assert::IsFalse (sound.IsBeeping());

        ticks *= 2;
        sound.GeneratePCM (after.data(), (uint32_t) after.size());

        Assert::IsTrue (std::abs (CountCrossings (during) - 200) <= 2, std::format (L"{} crossings", CountCrossings (during)).c_str());
        Assert::IsTrue (std::all_of (after.begin(), after.end(), [] (float s) { return std::abs (s) < 1e-3f; }), L"silent after it ends");
    }


    //  The DAC alternating between its lowest and highest levels every
    //  millisecond of ticks is a 500 Hz square: 100 crossings in a tenth of
    //  a second, however the changes fall within the frame.
    TEST_METHOD (Dac_PitchFollowsTheTicks)
    {
        uint64_t             ticks    = 0;
        Apple3Sound          sound;
        std::vector<float>   samples  (kSampleRate / 10);
        uint64_t             interval = (uint64_t) (kTicksPerSecond / 1000);



        sound.SetTickSource (&ticks, kTicksPerSecond);

        for (int change = 0; change < 100; change++)
        {
            ticks += interval;
            sound.SetDacPort ((change % 2 == 0) ? 0x3F : 0x00);
        }

        sound.GeneratePCM (samples.data(), (uint32_t) samples.size());

        Assert::IsTrue (std::abs (CountCrossings (samples) - 100) <= 2, std::format (L"{} crossings", CountCrossings (samples)).c_str());
    }


    TEST_METHOD (Dac_IsTheLowSixBitsOfThePort)
    {
        uint64_t     ticks = 0;
        Apple3Sound  sound;



        sound.SetTickSource (&ticks, kTicksPerSecond);
        sound.SetDacPort (0xFF);

        Assert::AreEqual<int> (0x3F, sound.GetDacLevel());
    }


    TEST_METHOD (Machine_C030TogglesTheSpeaker_AndC040StartsTheBeeper)
    {
        TestMachine    machine ("Apple3");
        Apple3Sound  & sound   = machine.GetApple3()->GetSound();
        size_t         before  = 0;



        machine.GetRunner().RunCycles ((uint64_t) (kTicksPerSecond / 10));

        before = sound.GetSpeaker().GetToggleTimestamps().size();
        (void) machine.GetCpu()->ReadByte (0xC030);
        Assert::AreEqual (before + 1, sound.GetSpeaker().GetToggleTimestamps().size());

        (void) machine.GetCpu()->ReadByte (0xC040);
        Assert::IsTrue (sound.IsBeeping());
    }


    TEST_METHOD (Machine_EViaPortBIsTheDac)
    {
        TestMachine    machine ("Apple3");
        Apple3Sound  & sound   = machine.GetApple3()->GetSound();



        machine.GetRunner().RunCycles ((uint64_t) (kTicksPerSecond / 10));

        machine.GetApple3()->GetVias().GetE().WriteRegister (Via6522::kRegDdrb, 0x3F);
        machine.GetApple3()->GetVias().GetE().WriteRegister (Via6522::kRegOrb,  0x2A);
        machine.GetRunner().RunCycles (1);

        Assert::AreEqual<int> (0x2A, sound.GetDacLevel());
    }
};
