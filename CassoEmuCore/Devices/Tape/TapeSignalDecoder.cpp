#include "Pch.h"

#include "Devices/Tape/TapeSignalDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Decode
//
//  A comparator with hysteresis that follows the signal's own level: the level
//  goes high when the filtered signal rises past +h and low when it falls past
//  -h, where h is a fraction of a running peak envelope, never below a fixed
//  floor. Each crossing is placed between its two samples by linear
//  interpolation. The first crossing fixes the starting level as its
//  opposite, so the first event is always a transition.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Decode (const TapeAudio & audio, TapeSignal & signal)
{
    std::vector<double>  filtered;
    double               releaseCoefficient = 0.0;
    double               envelope           = 0.0;
    double               previous           = 0.0;
    bool                 hasLevel           = false;
    bool                 level              = false;



    signal               = TapeSignal();
    signal.lengthSamples = audio.samples.size();
    signal.sampleRate    = audio.sampleRate;

    if (audio.samples.empty() || audio.sampleRate == 0)
    {
        return;
    }

    Filter (audio, filtered);
    releaseCoefficient = exp (-1.0 / (kReleaseSeconds * audio.sampleRate));

    for (size_t i = 0; i < filtered.size(); i++)
    {
        double  sample    = filtered[i];
        double  threshold = 0.0;
        double  target    = 0.0;
        double  fraction  = 0.0;
        bool    rises     = false;
        bool    falls     = false;



        envelope  = max (fabs (sample), envelope * releaseCoefficient);
        threshold = max (envelope * kThresholdRatio, kThresholdFloor);
        rises     = sample >  threshold && (!hasLevel || !level);
        falls     = sample < -threshold && (!hasLevel ||  level);

        if (rises || falls)
        {
            target = rises ? threshold : -threshold;

            if (!hasLevel)
            {
                signal.initialLevel = falls;
                hasLevel            = true;
            }

            fraction = i == 0 ? 0.0 : clamp ((target - previous) / (sample - previous), 0.0, 1.0);
            level    = rises;
            signal.transitions.push_back (i == 0 ? 0.0 : (double) (i - 1) + fraction);
        }

        previous = sample;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Filter
//
//  A one-pole high-pass removes DC offset and rumble far below the tones. A
//  one-pole low-pass trims hiss when the sample rate leaves room for it; it
//  delays every edge equally, so the spacing the guest measures is unchanged.
//  The high-pass starts settled on the first sample, so a constant offset
//  produces no opening step.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Filter (const TapeAudio & audio, std::vector<double> & filtered)
{
    double  dt          = 1.0 / audio.sampleRate;
    double  highPassRc  = 1.0 / (2.0 * std::numbers::pi * kHighPassHz);
    double  lowPassRc   = 1.0 / (2.0 * std::numbers::pi * kLowPassHz);
    double  highAlpha   = highPassRc / (highPassRc + dt);
    double  lowAlpha    = dt / (lowPassRc + dt);
    bool    useLowPass  = audio.sampleRate >= kMinLowPassRate;
    double  priorInput  = audio.samples.front();
    double  highOut     = 0.0;
    double  lowOut      = 0.0;



    filtered.resize (audio.samples.size());

    for (size_t i = 0; i < audio.samples.size(); i++)
    {
        double  input = audio.samples[i];



        highOut     = highAlpha * (highOut + input - priorInput);
        priorInput  = input;
        lowOut      = useLowPass ? lowOut + lowAlpha * (highOut - lowOut) : highOut;
        filtered[i] = lowOut;
    }
}
