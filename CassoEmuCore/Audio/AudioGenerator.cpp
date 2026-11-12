#include "Pch.h"

#include "Audio/AudioGenerator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Turns Apple II speaker toggles into PCM samples.
//
//  The Apple II speaker has exactly one control: an access to $C030 flips the
//  cone. Simple tones are square waves made by toggling at the right rate, but
//  much music is pulse-width modulation. Archon toggles in pairs 72 cycles
//  apart, a 14.2 kHz carrier whose duty cycle is the waveform. The cone
//  responds to the average level, not to each edge.
//
//  Each sample is therefore the average level over the span of cycles it
//  covers, integrated exactly between toggles. Reading the level at one cycle
//  per sample instead aliases the carrier and its harmonics down into the
//  audible band, which turns PWM music into something close to white noise.
//
//  Samples sit on one grid of cyclesPerSample that runs across slices. A
//  slice rarely contains a whole number of samples, so the cycles after its
//  last sample are integrated here and finish the first sample of the next.
//  Stretching each slice's samples to fill it instead moves the grid by up to
//  half a sample every slice, and that jitter on a 14.2 kHz carrier spreads it
//  into sidebands across the audible band.
//
//  Samples in a slice with no toggles are set to its starting level. A slice is
//  about a millisecond, shorter than the half period of any tone below 500 Hz,
//  so emitting zero there chopped low notes and the pauses between PWM pulses.
//  SpeakerFilter removes the resting DC level downstream.
//
//  A toggle at or after the end of the slice falls outside it and is ignored;
//  the next slice starts from the level after it.
//
////////////////////////////////////////////////////////////////////////////////

void AudioGenerator::GeneratePCM (
    const vector<uint32_t> & toggleTimestamps,
    uint32_t                 sliceCycles,
    float                    initialState,
    double                   cyclesPerSample,
    float                  * outSamples,
    uint32_t                 numSamples)
{
    float   state     = initialState;
    size_t  toggleIdx = 0;
    double  sliceEnd  = static_cast<double> (sliceCycles);
    double  start     = 0.0;
    double  end       = 0.0;
    double  area      = 0.0;
    double  width     = 0.0;
    double  tail      = 0.0;
    bool    hasEdge   = false;



    if (cyclesPerSample <= 0.0 || (numSamples > 0 && outSamples == nullptr))
    {
        for (uint32_t i = 0; outSamples != nullptr && i < numSamples; i++)
        {
            outSamples[i] = state;
        }

        Reset();
        return;
    }

    for (uint32_t i = 0; i < numSamples; i++)
    {
        end     = static_cast<double> (i + 1) * cyclesPerSample - m_carriedCycles;
        end     = std::clamp (end, start, sliceEnd);
        hasEdge = false;
        area    = Integrate (toggleTimestamps, toggleIdx, state, start, end, hasEdge);
        width   = end - start;

        if (i == 0 && m_carriedCycles > 0.0)
        {
            area    += m_carriedArea;
            width   += m_carriedCycles;
            hasEdge  = true;
        }

        outSamples[i] = (hasEdge && width > 0.0) ? static_cast<float> (area / width) : state;
        start         = end;
    }

    tail = Integrate (toggleTimestamps, toggleIdx, state, start, sliceEnd, hasEdge);

    if (numSamples == 0)
    {
        m_carriedArea   += tail;
        m_carriedCycles += sliceEnd;
    }
    else
    {
        m_carriedArea   = tail;
        m_carriedCycles = sliceEnd - start;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Drops the partial sample at the end of the previous slice, for a stream
//  that is not continuous with it.
//
////////////////////////////////////////////////////////////////////////////////

void AudioGenerator::Reset()
{
    m_carriedCycles = 0.0;
    m_carriedArea   = 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Integrate
//
//  The speaker level integrated over the cycles from `from` to `to`, applying
//  every toggle before `to`. The toggle index and the level advance past them,
//  and hasEdge is set when there is one.
//
////////////////////////////////////////////////////////////////////////////////

double AudioGenerator::Integrate (
    const vector<uint32_t> & toggleTimestamps,
    size_t                 & toggleIdx,
    float                  & state,
    double                   from,
    double                   to,
    bool                   & hasEdge)
{
    size_t  toggleCount = toggleTimestamps.size();
    double  position    = from;
    double  area        = 0.0;
    double  edge        = 0.0;



    while (toggleIdx < toggleCount && toggleTimestamps[toggleIdx] < to)
    {
        edge      = std::max (static_cast<double> (toggleTimestamps[toggleIdx]), position);
        area     += state * (edge - position);
        position  = edge;
        state     = -state;
        hasEdge   = true;
        toggleIdx++;
    }

    area += state * (to - position);

    return area;
}





