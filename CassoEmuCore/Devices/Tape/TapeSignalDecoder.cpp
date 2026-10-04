#include "Pch.h"

#include "Devices/Tape/TapeSignalDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Decode
//
//  FILTERING ONLY WHAT NEEDS IT. A clean recording is decoded as the Apple's
//  own input would: DC removed, then a comparator switching close to zero.
//  That is the reading that survives the uneven half-cycles real transfers
//  have. But it is also the reading hiss flips at random, so the result is
//  checked first: when too many of its crossings come closer together than
//  any Apple tone can, the recording is noisy, and it is decoded again
//  smoothed, with the comparator switching further from zero.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Decode (const TapeAudio & audio, TapeSignal & signal)
{
    std::vector<double>  filtered;



    signal               = TapeSignal();
    signal.lengthSamples = audio.samples.size();
    signal.sampleRate    = audio.sampleRate;

    if (audio.samples.empty() || audio.sampleRate == 0)
    {
        return;
    }

    Filter  (audio, false, filtered);
    Compare (filtered, audio.sampleRate, kCleanThreshold, signal);

    if (ChatterShare (signal) > kNoisyChatterShare)
    {
        Filter  (audio, true, filtered);
        Compare (filtered, audio.sampleRate, kNoisyThreshold, signal);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Compare
//
//  A comparator with hysteresis that follows the signal's own level: the level
//  goes high when the filtered signal rises past +h and low when it falls past
//  -h, where h is `ratio` of a running peak envelope, never below a fixed
//  floor. Each crossing is placed between its two samples by linear
//  interpolation. The first crossing fixes the starting level as its
//  opposite, so the first event is always a transition.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Compare (const std::vector<double> & filtered, uint32_t sampleRate, double ratio, TapeSignal & signal)
{
    double  releaseCoefficient = exp (-1.0 / (kReleaseSeconds * sampleRate));
    double  envelope           = 0.0;
    double  previous           = 0.0;
    bool    hasLevel           = false;
    bool    level              = false;



    signal.transitions.clear();
    signal.initialLevel = false;

    for (size_t i = 0; i < filtered.size(); i++)
    {
        double  sample    = filtered[i];
        double  threshold = 0.0;
        double  target    = 0.0;
        double  fraction  = 0.0;
        bool    rises     = false;
        bool    falls     = false;



        envelope  = max (fabs (sample), envelope * releaseCoefficient);
        threshold = max (envelope * ratio, kThresholdFloor);
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
//  TapeSignalDecoder::ChatterShare
//
//  The share of crossings that follow the one before sooner than any Apple
//  tone can: noise, not signal.
//
////////////////////////////////////////////////////////////////////////////////

double TapeSignalDecoder::ChatterShare (const TapeSignal & signal)
{
    const std::vector<double>  & transitions = signal.transitions;
    double                       limit       = kChatterSeconds * signal.sampleRate;
    size_t                       chatter     = 0;



    if (transitions.size() < 2)
    {
        return 0.0;
    }

    for (size_t i = 1; i < transitions.size(); i++)
    {
        if (transitions[i] - transitions[i - 1] < limit)
        {
            chatter++;
        }
    }

    return (double) chatter / (double) (transitions.size() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Filter
//
//  A one-pole high-pass removes DC offset and rumble far below the tones. A
//  one-pole low-pass, when asked for, trims hiss if the sample rate leaves
//  room for it; it delays every edge equally, so the spacing the guest
//  measures is unchanged. The high-pass starts settled on the first sample,
//  so a constant offset produces no opening step.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Filter (const TapeAudio & audio, bool lowPass, std::vector<double> & filtered)
{
    double  dt          = 1.0 / audio.sampleRate;
    double  highPassRc  = 1.0 / (2.0 * std::numbers::pi * kHighPassHz);
    double  lowPassRc   = 1.0 / (2.0 * std::numbers::pi * kLowPassHz);
    double  highAlpha   = highPassRc / (highPassRc + dt);
    double  lowAlpha    = dt / (lowPassRc + dt);
    bool    useLowPass  = lowPass && audio.sampleRate >= kMinLowPassRate;
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
