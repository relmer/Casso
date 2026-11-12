#include "Pch.h"
#include "Audio/SpeakerFilter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SpeakerFilterTests
//
//  The speaker's frequency response: a parked level decays to silence without
//  a thump, tones in the audible band pass, a PWM carrier is attenuated, and
//  the state continues across calls the way consecutive slices arrive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SpeakerFilterTests)
{
public:

    static constexpr uint32_t  kSampleRate = 48'000;
    static constexpr float     kLevel      = 0.25f;


    TEST_METHOD (ParkedLevel_DecaysToSilenceWithoutAThump)
    {
        constexpr float  kMaxFirst  = 1e-6f;
        constexpr float  kMaxParked = 1e-3f;

        SpeakerFilter       filter;
        std::vector<float>  samples (kSampleRate / 2, kLevel);



        filter.SetSampleRate (kSampleRate);
        filter.Process (samples.data(), static_cast<uint32_t> (samples.size()));

        Assert::IsTrue (std::fabs (samples.front()) < kMaxFirst, L"the first sample primes the DC blocker");
        Assert::IsTrue (std::fabs (samples.back())  < kMaxParked, L"half a second later the parked level is silent");
    }


    TEST_METHOD (OneKilohertzTone_Passes)
    {
        constexpr double  kFrequency    = 1000.0;
        constexpr double  kMinGain      = 0.95;
        constexpr double  kMaxGain      = 1.05;

        double  gain = MeasureSineGain (kFrequency);



        Assert::IsTrue (gain > kMinGain && gain < kMaxGain,
                        std::format (L"a 1 kHz tone passes at {:.3f}", gain).c_str());
    }


    TEST_METHOD (PwmCarrier_IsAttenuated)
    {
        constexpr double  kCarrier = 14'204.0;
        constexpr double  kMaxGain = 0.5;

        double  gain = MeasureSineGain (kCarrier);



        Assert::IsTrue (gain < kMaxGain,
                        std::format (L"a 14.2 kHz carrier is cut to {:.3f}", gain).c_str());
    }


    TEST_METHOD (SplitCalls_MatchOneCall)
    {
        constexpr uint32_t  kCount = 480;
        constexpr uint32_t  kSplit = 47;

        SpeakerFilter       whole;
        SpeakerFilter       split;
        std::vector<float>  a (kCount);
        std::vector<float>  b (kCount);



        for (uint32_t i = 0; i < kCount; i++)
        {
            a[i] = ((i / kSplit) % 2 == 0) ? kLevel : -kLevel;
            b[i] = a[i];
        }

        whole.SetSampleRate (kSampleRate);
        split.SetSampleRate (kSampleRate);

        whole.Process (a.data(), kCount);
        split.Process (b.data(), kSplit);
        split.Process (b.data() + kSplit, kCount - kSplit);

        for (uint32_t i = 0; i < kCount; i++)
        {
            Assert::AreEqual (a[i], b[i]);
        }
    }


private:

    //  The steady-state output amplitude of a sine through the filter, as a
    //  fraction of the input amplitude: the peak over the second half of a
    //  tenth of a second, after the filter has settled.
    static double MeasureSineGain (double frequency)
    {
        constexpr uint32_t  kCount = kSampleRate / 10;

        SpeakerFilter       filter;
        std::vector<float>  samples (kCount);
        double              peak    = 0.0;



        for (uint32_t i = 0; i < kCount; i++)
        {
            samples[i] = kLevel * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / kSampleRate));
        }

        filter.SetSampleRate (kSampleRate);
        filter.Process (samples.data(), kCount);

        for (uint32_t i = kCount / 2; i < kCount; i++)
        {
            peak = std::max (peak, static_cast<double> (std::fabs (samples[i])));
        }

        return peak / kLevel;
    }
};
