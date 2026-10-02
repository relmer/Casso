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
    static constexpr double  kHighPassHz       = 20.0;
    static constexpr double  kLowPassHz        = 6000.0;
    static constexpr double  kMinLowPassRate   = 22000.0;   // below this the low-pass would eat the tones
    static constexpr double  kReleaseSeconds   = 0.050;
    static constexpr double  kThresholdRatio   = 0.25;      // of the peak envelope
    static constexpr double  kThresholdFloor   = 0.02;      // about -34 dBFS; quieter is treated as silence

    static void  Filter (const TapeAudio & audio, std::vector<double> & filtered);
};
