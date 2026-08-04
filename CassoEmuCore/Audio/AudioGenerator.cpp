#include "Pch.h"

#include "Audio/AudioGenerator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Turns Apple II speaker toggles into PCM samples.
//
//  The Apple II speaker has exactly one control: writing $C030 FLIPS the cone.
//  All its music is square waves made by toggling at the right rate, so
//  synthesis here is just holding a state and inverting it at each recorded
//  timestamp.
//
//  A frame with NO toggles emits silence rather than the held DC level, which
//  is the important case. A speaker parked at a non-zero state would otherwise
//  push a constant offset into the mix and buzz continuously between sounds.
//
//  Toggle timestamps are in CPU cycles and are mapped to sample positions by
//  scaling, so the same code works at any host sample rate and any emulated
//  speed without a resampler.
//
//  The scaling multiplies before dividing, in 64-bit, so a long frame at a
//  high sample rate cannot overflow or lose resolution to an early truncation.
//
//  The toggle index advances monotonically across the whole buffer rather than
//  being searched per sample, making this one linear pass over both arrays.
//
////////////////////////////////////////////////////////////////////////////////

void AudioGenerator::GeneratePCM (
    const vector<uint32_t> & toggleTimestamps,
    uint32_t totalCyclesThisFrame,
    float initialState,
    float * outSamples,
    uint32_t numSamples)
{
    if (numSamples == 0 || outSamples == nullptr)
    {
        return;
    }

    float state = initialState;



    if (toggleTimestamps.empty() || totalCyclesThisFrame == 0)
    {
        // No toggles this frame — output silence (not DC) to avoid
        // constant buzzing from a non-zero speaker state
        for (uint32_t i = 0; i < numSamples; i++)
        {
            outSamples[i] = 0.0f;
        }
    }
    else
    {
        // Convert toggle timestamps to sample positions
        size_t toggleIdx = 0;

        for (uint32_t i = 0; i < numSamples; i++)
        {
            // Map sample position to cycle count
            uint32_t sampleCycle = static_cast<uint32_t> (
                static_cast<uint64_t> (i) * totalCyclesThisFrame / numSamples);

            // Process any toggles up to this sample
            while (toggleIdx < toggleTimestamps.size() &&
                   toggleTimestamps[toggleIdx] <= sampleCycle)
            {
                state = -state;
                toggleIdx++;
            }

            outSamples[i] = state;
        }
    }
}
