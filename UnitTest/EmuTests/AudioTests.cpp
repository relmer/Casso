#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/AudioGenerator.h"
#include "Devices/AppleSpeaker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AudioTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AudioTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //  Silence
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Silence_NoToggles_AllZeroOutput)
    {
        std::vector<uint32_t> toggles;
        float samples[100];



        AudioGenerator::GeneratePCM (toggles, 17030, 0.0f, samples, 100);

        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.0f, samples[i]);
        }
    }

    TEST_METHOD (Silence_NoToggles_OutputsZero)
    {
        std::vector<uint32_t> toggles;
        float samples[100];



        AudioGenerator::GeneratePCM (toggles, 17030, 1.0f, samples, 100);

        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.0f, samples[i],
                L"No toggles should produce silence (0.0), not DC");
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Single toggle
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SingleToggle_MidFrame_ProducesStep)
    {
        std::vector<uint32_t> toggles = { 8525 };
        float samples[1000];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 1000);

        // Before midpoint: should be initial state (-0.3)
        Assert::AreEqual (-0.3f, samples[0]);
        Assert::AreEqual (-0.3f, samples[249]);

        // After midpoint: should be toggled state (+0.3)
        Assert::AreEqual (0.3f, samples[750]);
        Assert::AreEqual (0.3f, samples[999]);

        // Verify transition happens around sample 500
        Assert::AreEqual (-0.3f, samples[499]);
        Assert::AreEqual (0.3f, samples[500]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Square wave
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SquareWave_EvenToggles_ProducesSymmetricWave)
    {
        std::vector<uint32_t> toggles = { 0, 4262, 8525, 12787 };
        float samples[1000];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 1000);

        // 4 toggles starting from -0.3: toggle at 0 → +0.3, at 4262 → -0.3,
        // at 8525 → +0.3, at 12787 → -0.3
        // Each region ~250 samples

        // First region (after first toggle at cycle 0): +0.3
        Assert::AreEqual (0.3f, samples[0]);
        Assert::AreEqual (0.3f, samples[124]);

        // Second region (after toggle at cycle 4262): -0.3
        Assert::AreEqual (-0.3f, samples[375]);

        // Third region (after toggle at cycle 8525): +0.3
        Assert::AreEqual (0.3f, samples[625]);

        // Fourth region (after toggle at cycle 12787): -0.3
        Assert::AreEqual (-0.3f, samples[875]);
        Assert::AreEqual (-0.3f, samples[999]);
    }

    TEST_METHOD (SquareWave_KnownFrequency_CorrectPeriod)
    {
        // Toggle every 1000 cycles in a 17030-cycle frame
        std::vector<uint32_t> toggles;

        for (uint32_t c = 0; c < 17030; c += 1000)
        {
            toggles.push_back (c);
        }

        // 18 toggles (0, 1000, 2000, ... 17000)
        const uint32_t numSamples = 4410;
        std::vector<float> samples (numSamples);



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples.data (), numSamples);

        // Count sign changes (zero crossings)
        int crossings = 0;

        for (uint32_t i = 1; i < numSamples; i++)
        {
            if ((samples[i] > 0.0f) != (samples[i - 1] > 0.0f))
            {
                crossings++;
            }
        }

        // 18 toggles = 18 sign changes
        Assert::AreEqual (static_cast<int> (toggles.size ()), crossings);
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Rapid toggles
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (RapidToggles_HighFrequency_DoesNotCrash)
    {
        std::vector<uint32_t> toggles;

        for (uint32_t i = 0; i < 1000; i++)
        {
            toggles.push_back (i * 17);
        }

        float samples[735];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 735);

        // Just verify no crash and output has values
        bool hasNonZero = false;

        for (int i = 0; i < 735; i++)
        {
            if (samples[i] != 0.0f)
            {
                hasNonZero = true;
                break;
            }
        }

        Assert::IsTrue (hasNonZero);
    }

    TEST_METHOD (RapidToggles_AlternatingValues)
    {
        std::vector<uint32_t> toggles;

        for (uint32_t i = 0; i < 500; i++)
        {
            toggles.push_back (i * 34);
        }

        float samples[735];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 735);

        // All output values should be exactly +0.3 or -0.3 (no intermediate)
        for (int i = 0; i < 735; i++)
        {
            bool valid = (samples[i] == 0.3f || samples[i] == -0.3f);
            Assert::IsTrue (valid, L"Sample value must be +0.3 or -0.3");
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Edge cases
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AllTogglesAtStart_StateChangesImmediately)
    {
        // 3 toggles all at cycle 0: -0.3 → +0.3 → -0.3 → +0.3
        std::vector<uint32_t> toggles = { 0, 0, 0 };
        float samples[100];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 100);

        // Odd number of toggles from -0.3: final state is +0.3
        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.3f, samples[i]);
        }
    }

    TEST_METHOD (AllTogglesAtEnd_InitialStateFillsBuffer)
    {
        // All toggles at last cycle — initial state fills almost all
        std::vector<uint32_t> toggles = { 17049, 17049, 17049 };
        float samples[1000];



        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, samples, 1000);

        // All samples except possibly the very last should be initial state
        // since toggles happen at the last cycle position
        for (int i = 0; i < 999; i++)
        {
            Assert::AreEqual (-0.3f, samples[i]);
        }
    }

    TEST_METHOD (ZeroCyclesPerFrame_NoExplode)
    {
        std::vector<uint32_t> toggles = { 100, 200 };
        float samples[100];



        // totalCyclesThisFrame = 0 should not divide by zero
        AudioGenerator::GeneratePCM (toggles, 0, -0.3f, samples, 100);

        // With 0 totalCycles, all toggle positions map to sample 0
        // so the output is all zeros (no valid cycle-to-sample mapping)
        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.0f, samples[i]);
        }
    }

    TEST_METHOD (ZeroSamples_NoExplode)
    {
        std::vector<uint32_t> toggles = { 100 };
        float dummy = 0.0f;



        // numSamples = 0 should not crash
        AudioGenerator::GeneratePCM (toggles, 17030, -0.3f, &dummy, 0);
    }

    TEST_METHOD (EmptyTimestamps_OutputsSilence)
    {
        std::vector<uint32_t> toggles;
        float samples[100];



        AudioGenerator::GeneratePCM (toggles, 17030, 1.0f, samples, 100);

        // Silence output (0.0), not DC
        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.0f, samples[i]);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Speaker device integration
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SpeakerToggle_RecordsTimestamp)
    {
        AppleSpeaker spk;
        uint64_t cycles = 5000;

        spk.SetCycleCounter (&cycles);
        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps ().size ());
        Assert::AreEqual (uint32_t (5000), spk.GetToggleTimestamps ()[0]);
    }

    TEST_METHOD (SpeakerToAudioPipeline_EndToEnd)
    {
        AppleSpeaker spk;
        uint64_t cycles = 0;
        spk.SetCycleCounter (&cycles);



        // Toggle at known cycles
        cycles = 4000;
        spk.Read (0xC030);   // 0 → 0.3

        cycles = 8000;
        spk.Read (0xC030);   // 0.3 → -0.3

        cycles = 12000;
        spk.Read (0xC030);   // -0.3 → 0.3

        // Get timestamps and initial state
        const auto & timestamps = spk.GetToggleTimestamps ();
        // Initial state before toggles: speaker starts at 0.0,
        // after 3 toggles it's at 0.3. Initial = 0.0.
        float initialState = 0.0f;

        Assert::AreEqual (size_t (3), timestamps.size ());
        Assert::AreEqual (uint32_t (4000), timestamps[0]);
        Assert::AreEqual (uint32_t (8000), timestamps[1]);
        Assert::AreEqual (uint32_t (12000), timestamps[2]);

        // Generate PCM
        float samples[1000];

        AudioGenerator::GeneratePCM (timestamps, 16000, initialState, samples, 1000);

        // Before first toggle (cycle 0-4000): initial state 0.0
        Assert::AreEqual (0.0f, samples[0]);
        Assert::AreEqual (0.0f, samples[124]);

        // After first toggle (cycle 4000): negated → -0.0 = 0.0 (edge case with 0)
        // Use non-zero initial for meaningful test
    }

    TEST_METHOD (SpeakerToAudioPipeline_NonZeroInitial)
    {
        // Full pipeline test with non-zero amplitude
        std::vector<uint32_t> timestamps = { 4000, 8000, 12000 };
        float samples[1000];

        float initialState = -0.3f;



        AudioGenerator::GeneratePCM (timestamps, 16000, initialState, samples, 1000);

        // Region 0-4000 (samples 0-249): -0.3
        Assert::AreEqual (-0.3f, samples[0]);
        Assert::AreEqual (-0.3f, samples[124]);

        // Region 4000-8000 (samples 250-499): +0.3
        Assert::AreEqual (0.3f, samples[375]);

        // Region 8000-12000 (samples 500-749): -0.3
        Assert::AreEqual (-0.3f, samples[625]);

        // Region 12000-16000 (samples 750-999): +0.3
        Assert::AreEqual (0.3f, samples[875]);
        Assert::AreEqual (0.3f, samples[999]);
    }
};
