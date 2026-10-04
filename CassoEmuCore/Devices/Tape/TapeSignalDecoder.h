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

    // One way of decoding: smoothed or not, and where the comparator
    // switches, as a fraction of the envelope and never below a floor.
    struct Settings
    {
        bool    lowPass;
        double  ratio;
        double  floor;
    };

    // What a record that fails its checksum is decoded again with, in
    // order, until one gives it a good checksum. Few on purpose: an 8-bit
    // checksum passes a damaged record about one time in 256, so every
    // extra try is an extra chance of accepting one.
    static constexpr Settings  kRescueSettings[] =
    {
        { false, 0.02, 0.01  },     // a lower floor: tiny swings near zero count
        { false, 0.0,  0.005 },     // lower still: almost a bare zero crossing
        { true,  0.12, 0.02  },     // smoothed, for hiss
        { true,  0.25, 0.02  },     // smoothed and switching high, for heavy hiss
    };

    static constexpr double  kRescueLeadSeconds = 0.5;   // decoded ahead of the record, for the filters to settle

    static void    Filter        (const TapeAudio & audio, bool lowPass, std::vector<double> & filtered);
    static void    Compare       (const std::vector<double> & filtered, uint32_t sampleRate, double ratio,
                                  double floor, TapeSignal & signal);
    static double  ChatterShare  (const TapeSignal & signal);
    static void    RescueRecords (const TapeAudio & audio, TapeSignal & signal);
    static bool    TryRescue     (const TapeAudio & audio, double from, double to, double dataStart,
                                  const Settings & settings, std::vector<double> & transitions);
};
