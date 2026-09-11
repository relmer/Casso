#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AudioSampleBudget
//
//  How many audio samples a slice of emulated cycles is worth, without drift.
//
//  Cycles per sample is rarely integral -- 1,023,000 Hz into 48,000 Hz is
//  21.3125 -- so a slice of 1,023 cycles is worth 48.0 samples one time and
//  47.99 the next. Truncating each slice on its own throws the fraction away
//  and loses a sample every few frames, which is enough to walk the audio
//  out of step with the picture over a minute. The fraction is carried here
//  instead, so over any run the samples generated equal the cycles run
//  divided by cycles per sample, to within one.
//
////////////////////////////////////////////////////////////////////////////////

class AudioSampleBudget
{
public:

    uint32_t  SamplesFor (uint32_t cycles, double cyclesPerSample)
    {
        double    exact   = static_cast<double> (cycles) / cyclesPerSample + m_remainder;
        uint32_t  samples = static_cast<uint32_t> (exact);

        m_remainder = exact - static_cast<double> (samples);

        return samples;
    }


    void  Reset ()
    {
        m_remainder = 0.0;
    }

private:

    double  m_remainder = 0.0;
};
