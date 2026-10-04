#pragma once

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignal
//
//  The level a cassette input sees, as a starting level plus the times it
//  flips. Times are fractional sample indices at the recording's own rate.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeSignal
{
    std::vector<double>  transitions;
    bool                 initialLevel  = false;
    uint64_t             lengthSamples = 0;
    uint32_t             sampleRate    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder
//
//  Turns recorded audio into level transitions the way a cassette input's
//  comparator would, but tolerant of real recordings: DC offset, uneven level
//  and hiss. It knows nothing about what the transitions encode.
//
////////////////////////////////////////////////////////////////////////////////

class TapeSignalDecoder
{
public:
    static void Decode (const TapeAudio & audio, TapeSignal & signal);

private:
    static constexpr double  kHighPassHz        = 20.0;
    static constexpr double  kLowPassHz         = 6000.0;
    static constexpr double  kMinLowPassRate    = 22000.0;   // below this the low-pass would eat the tones
    static constexpr double  kReleaseSeconds    = 0.050;
    static constexpr double  kThresholdFloor    = 0.02;      // about -34 dBFS; quieter is treated as silence

    // Where the comparator switches, as a fraction of the peak envelope.
    // A CLEAN recording switches as close to zero as the silence floor
    // allows, as the Apple's own input does. Real transfers need it: one
    // tape's half-cycles drop to a fifth of their neighbors' peak, and
    // another has a glitch inside its sync bit that merged the sync into the
    // leader at anything higher. A NOISY one is smoothed and switches
    // higher, or hiss would flip it between the tones' own crossings.
    static constexpr double  kCleanThreshold    = 0.02;
    static constexpr double  kNoisyThreshold    = 0.12;

    // How a recording is judged noisy: switching close to zero, more than
    // this share of its crossings come closer together than any Apple tone
    // allows. Archive transfers measure 0.03% and below; white noise at
    // 15 dB SNR measures 4%.
    static constexpr double  kChatterSeconds    = 100.0e-6;  // the shortest real half-cycle, the sync, is 200 us
    static constexpr double  kNoisyChatterShare = 0.005;

    static void    Filter       (const TapeAudio & audio, bool lowPass, std::vector<double> & filtered);
    static void    Compare      (const std::vector<double> & filtered, uint32_t sampleRate, double ratio, TapeSignal & signal);
    static double  ChatterShare (const TapeSignal & signal);
};
