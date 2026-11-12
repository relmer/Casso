#include "Pch.h"
#include "Audio/AudioGenerator.h"
#include "Audio/SpeakerFilter.h"
#include "Shell/AudioSampleBudget.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AudioTests
//
//  PCM generation from speaker toggles: the waveform, the cycle-to-sample
//  mapping, and the level between toggles.
//
//  Samples in a slice with no toggles are set to its starting level, because
//  a slice is shorter than the half period of a low tone. SpeakerFilterTests covers the decay of a
//  parked level to silence.
//
//  Each sample is the average level over its cycles, so a pulse-width
//  modulated stream produces its duty cycle rather than aliased edges.
//
//  The cycle-to-sample mapping is asserted at more than one rate, since the
//  same toggle stream must produce the same tone at any host sample rate --
//  that scaling is what replaces a resampler.
//
//  Toggles landing on a sample boundary are covered, since that is where an
//  off-by-one puts an edge in the wrong sample and shifts the phase.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AudioTests)
{
public:

    static constexpr float  kTolerance = 1e-5f;

    ////////////////////////////////////////////////////////////////////////////
    //  Silence
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Silence_NoToggles_AllZeroOutput)
    {
        std::vector<uint32_t>  toggles;
        float                  samples[100];



        Render (toggles, 17030, 0.0f, samples, 100);

        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.0f, samples[i]);
        }
    }

    TEST_METHOD (NoToggles_KeepsTheLevel)
    {
        std::vector<uint32_t>  toggles;
        float                  samples[100];



        Render (toggles, 17030, 1.0f, samples, 100);

        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (1.0f, samples[i],
                L"samples in a slice with no toggles are set to its starting level");
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Single toggle
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SingleToggle_MidFrame_ProducesStep)
    {
        std::vector<uint32_t>  toggles       = { 8515 };
        float                  samples[1000];



        Render (toggles, 17030, -0.3f, samples, 1000);

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
        std::vector<uint32_t>  toggles       = { 0, 4258, 8515, 12773 };
        float                  samples[1000];



        Render (toggles, 17030, -0.3f, samples, 1000);

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
        const uint32_t  numSamples = 4410;
        int             crossings  = 0;



        // Toggle every 1000 cycles in a 17030-cycle frame
        std::vector<uint32_t> toggles;

        for (uint32_t c = 0; c < 17030; c += 1000)
        {
            toggles.push_back (c);
        }

        // 18 toggles (0, 1000, 2000, ... 17000)
        std::vector<float> samples (numSamples);



        Render (toggles, 17030, -0.3f, samples.data(), numSamples);

        // Count sign changes (zero crossings)

        for (uint32_t i = 1; i < numSamples; i++)
        {
            if ((samples[i] > 0.0f) != (samples[i - 1] > 0.0f))
            {
                crossings++;
            }
        }

        // 18 toggles, but the first fires at sample 0 (no prior sample
        // to compare), so we observe 17 sign changes between adjacent samples.
        Assert::AreEqual (static_cast<int> (toggles.size()) - 1, crossings);
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Rapid toggles
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (RapidToggles_HighFrequency_DoesNotCrash)
    {
        std::vector<uint32_t>  toggles;
        float                  samples[735];
        bool                   hasNonZero   = false;

        for (uint32_t i = 0; i < 1000; i++)
        {
            toggles.push_back (i * 17);
        }




        Render (toggles, 17030, -0.3f, samples, 735);

        // Just verify no crash and output has values

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

    TEST_METHOD (RapidToggles_StayWithinTheLevels)
    {
        std::vector<uint32_t>  toggles;
        float                  samples[735];

        for (uint32_t i = 0; i < 500; i++)
        {
            toggles.push_back (i * 34);
        }




        Render (toggles, 17030, -0.3f, samples, 735);

        for (int i = 0; i < 735; i++)
        {
            bool valid = (samples[i] <= 0.3f + kTolerance && samples[i] >= -0.3f - kTolerance);
            Assert::IsTrue (valid, L"an average of +0.3 and -0.3 lies between them");
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Averaging within a sample
    ////////////////////////////////////////////////////////////////////////////

    //  A toggle a quarter of the way into a sample: the sample is the
    //  time-weighted average of the two levels, -0.3 for a quarter and +0.3
    //  for the rest.
    TEST_METHOD (ToggleInsideASample_AveragesTheLevels)
    {
        std::vector<uint32_t>  toggles     = { 425 };
        float                  samples[10];



        Render (toggles, 1000, -0.3f, samples, 10);

        Assert::AreEqual (-0.3f, samples[3]);
        Assert::AreEqual (0.15f, samples[4], kTolerance);
        Assert::AreEqual (0.3f,  samples[5]);
    }

    //  Two toggles inside one sample, a pulse narrower than the sample: the
    //  point-sampled level would be -0.3 and the pulse would vanish.
    TEST_METHOD (PulseNarrowerThanASample_ContributesItsArea)
    {
        std::vector<uint32_t>  toggles     = { 420, 440 };
        float                  samples[10];



        Render (toggles, 1000, -0.3f, samples, 10);

        // 20 of 100 cycles at +0.3, 80 at -0.3.
        Assert::AreEqual (-0.18f, samples[4], kTolerance);
        Assert::AreEqual (-0.3f,  samples[5]);
    }

    //  Pulse-width modulation at a constant duty cycle, Archon's 8-and-64-cycle
    //  pairs, rendered the way the CPU thread renders it: 1023-cycle slices at
    //  48 kHz, then the speaker filter. The audible content of a constant duty
    //  cycle is a constant level, which the filter removes, so all that may be
    //  left is the attenuated carrier.
    //
    //  Point sampling aliases the carrier's harmonics into the audible band.
    //  That measures 0.093 RMS here, against 0.023 with averaging.
    TEST_METHOD (ConstantDutyPwm_LeavesNoAudibleAlias)
    {
        constexpr size_t  kSettleSamples = kSampleRate / 20;
        constexpr double  kMaxRms        = 0.04;

        SpeakerFilter       filter;
        std::vector<float>  samples = RenderPwm (kSlice, kSlices);
        double              sumSq   = 0.0;
        size_t              count   = samples.size();
        double              rms     = 0.0;



        Assert::IsTrue (count > kSettleSamples, L"the measurement covers samples");

        filter.SetSampleRate (kSampleRate);
        filter.Process (samples.data(), static_cast<uint32_t> (count));

        for (size_t i = kSettleSamples; i < count; i++)
        {
            sumSq += static_cast<double> (samples[i]) * samples[i];
        }

        rms = std::sqrt (sumSq / static_cast<double> (count - kSettleSamples));

        Assert::IsTrue (rms < kMaxRms,
                        std::format (L"only the carrier remains of constant-duty PWM: {:.4f} RMS", rms).c_str());
    }

    //  The samples sit on one grid across slices, so cutting the same stream
    //  into 1023-cycle slices produces what one long slice does. Stretching
    //  each slice's samples to fill it moves the grid by up to half a sample
    //  every slice, which spreads a PWM carrier across the audible band.
    TEST_METHOD (SlicedStream_MatchesOneSlice)
    {
        constexpr float  kMaxDifference = 1e-4f;

        std::vector<float>  sliced = RenderPwm (kSlice, kSlices);
        std::vector<float>  whole  = RenderPwm (kSlice * kSlices, 1);
        size_t              count  = std::min (sliced.size(), whole.size());
        float               worst  = 0.0f;



        Assert::IsTrue (count > 0, L"the comparison covers samples");

        for (size_t i = 0; i < count; i++)
        {
            worst = std::max (worst, std::fabs (sliced[i] - whole[i]));
        }

        Assert::IsTrue (worst < kMaxDifference,
                        std::format (L"sliced and whole renders differ by {:.4f}", worst).c_str());
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Edge cases
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AllTogglesAtStart_StateChangesImmediately)
    {
        // 3 toggles all at cycle 0: -0.3 → +0.3 → -0.3 → +0.3
        std::vector<uint32_t>  toggles      = { 0, 0, 0 };
        float                  samples[100];



        Render (toggles, 17030, -0.3f, samples, 100);

        // Odd number of toggles from -0.3: final state is +0.3
        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (0.3f, samples[i]);
        }
    }

    TEST_METHOD (AllTogglesAtEnd_InitialStateFillsBuffer)
    {
        // All toggles at last cycle — initial state fills almost all
        std::vector<uint32_t>  toggles       = { 17049, 17049, 17049 };
        float                  samples[1000];



        Render (toggles, 17030, -0.3f, samples, 1000);

        // All samples except possibly the very last should be initial state
        // since toggles happen at the last cycle position
        for (int i = 0; i < 999; i++)
        {
            Assert::AreEqual (-0.3f, samples[i]);
        }
    }

    TEST_METHOD (ZeroCyclesPerFrame_NoExplode)
    {
        std::vector<uint32_t>  toggles      = { 100, 200 };
        float                  samples[100];



        // totalCyclesThisFrame = 0 should not divide by zero
        Render (toggles, 0, -0.3f, samples, 100);

        // In a slice of 0 cycles, no time passes, so every toggle falls outside
        // the slice and the starting level fills it
        for (int i = 0; i < 100; i++)
        {
            Assert::AreEqual (-0.3f, samples[i]);
        }
    }

    TEST_METHOD (ZeroSamples_NoExplode)
    {
        std::vector<uint32_t>  toggles = { 100 };
        float                  dummy   = 0.0f;



        // numSamples = 0 should not crash
        Render (toggles, 17030, -0.3f, &dummy, 0);
    }

    ////////////////////////////////////////////////////////////////////////////
    //  Speaker device integration
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SpeakerToggle_RecordsTimestamp)
    {
        AppleSpeaker  spk;
        uint64_t      cycles = 5000;

        spk.SetCycleCounter (&cycles);
        spk.Read (0xC030);

        Assert::AreEqual (size_t (1), spk.GetToggleTimestamps().size());
        Assert::AreEqual (uint32_t (5000), spk.GetToggleTimestamps()[0]);
    }

    TEST_METHOD (SpeakerToAudioPipeline_EndToEnd)
    {
        AppleSpeaker  spk;
        uint64_t      cycles        = 0;
        float         initialState  = 0.0f;
        float         samples[1000];
        spk.SetCycleCounter (&cycles);



        // Toggle at known cycles
        cycles = 4000;
        spk.Read (0xC030);   // 0 → 0.3

        cycles = 8000;
        spk.Read (0xC030);   // 0.3 → -0.3

        cycles = 12000;
        spk.Read (0xC030);   // -0.3 → 0.3

        // Get timestamps and initial state
        const auto & timestamps = spk.GetToggleTimestamps();
        // Initial state before toggles: speaker starts at 0.0,
        // after 3 toggles it's at 0.3. Initial = 0.0.

        Assert::AreEqual (size_t (3), timestamps.size());
        Assert::AreEqual (uint32_t (4000), timestamps[0]);
        Assert::AreEqual (uint32_t (8000), timestamps[1]);
        Assert::AreEqual (uint32_t (12000), timestamps[2]);

        // Generate PCM

        Render (timestamps, 16000, initialState, samples, 1000);

        // Before first toggle (cycle 0-4000): initial state 0.0
        Assert::AreEqual (0.0f, samples[0]);
        Assert::AreEqual (0.0f, samples[124]);

        // After first toggle (cycle 4000): negated → -0.0 = 0.0 (edge case with 0)
        // Use non-zero initial for meaningful test
    }

    TEST_METHOD (SpeakerToAudioPipeline_NonZeroInitial)
    {
        // Full pipeline test with non-zero amplitude
        std::vector<uint32_t>  timestamps    = { 4000, 8000, 12000 };
        float                  samples[1000];

        float initialState = -0.3f;



        Render (timestamps, 16000, initialState, samples, 1000);

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


private:

    static constexpr uint32_t  kClock      = 1'020'484;
    static constexpr uint32_t  kSampleRate = 48'000;
    static constexpr uint32_t  kSlice      = 1023;
    static constexpr uint32_t  kSlices     = 200;


    //  One slice from a fresh generator, its samples spread evenly over it.
    static void Render (const std::vector<uint32_t> & toggles,
                        uint32_t                      sliceCycles,
                        float                         initialState,
                        float                       * outSamples,
                        uint32_t                      numSamples)
    {
        AudioGenerator  generator;
        double          cyclesPerSample = (numSamples > 0) ? static_cast<double> (sliceCycles) / numSamples : 0.0;



        generator.GeneratePCM (toggles, sliceCycles, initialState, cyclesPerSample, outSamples, numSamples);
    }


    //  Archon's PWM, 8 cycles high and 64 low, rendered at 48 kHz the way the
    //  CPU thread renders it: in slices of sliceCycles, each worth the samples
    //  AudioSampleBudget::SamplesFor returns.
    static std::vector<float> RenderPwm (uint32_t sliceCycles, uint32_t numSlices)
    {
        constexpr uint32_t  kHighCycles = 8;
        constexpr uint32_t  kLowCycles  = 64;

        AudioGenerator         generator;
        AudioSampleBudget      budget;
        std::vector<uint32_t>  toggles;
        std::vector<float>     samples;
        float                  level           = -0.25f;
        float                  initial         = 0.0f;
        uint64_t               nextEdge        = 0;
        uint64_t               sliceStart      = 0;
        bool                   isHigh          = false;
        double                 cyclesPerSample = static_cast<double> (kClock) / kSampleRate;
        uint32_t               count           = 0;
        size_t                 written         = 0;



        for (uint32_t s = 0; s < numSlices; s++)
        {
            sliceStart = static_cast<uint64_t> (s) * sliceCycles;
            initial    = level;

            toggles.clear();

            while (nextEdge < sliceStart + sliceCycles)
            {
                toggles.push_back (static_cast<uint32_t> (nextEdge - sliceStart));
                level     = -level;
                isHigh    = !isHigh;
                nextEdge += isHigh ? kHighCycles : kLowCycles;
            }

            count   = budget.SamplesFor (sliceCycles, cyclesPerSample);
            written = samples.size();

            samples.resize (written + count);
            generator.GeneratePCM (toggles, sliceCycles, initial, cyclesPerSample, samples.data() + written, count);
        }

        return samples;
    }
};
