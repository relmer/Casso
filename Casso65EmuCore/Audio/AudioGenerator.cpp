#include "Pch.h"

#include "Audio/AudioGenerator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
////////////////////////////////////////////////////////////////////////////////

void AudioGenerator::GeneratePCM (
    const std::vector<uint32_t> & toggleTimestamps,
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



    if (toggleTimestamps.empty () || totalCyclesThisFrame == 0)
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
            while (toggleIdx < toggleTimestamps.size () &&
                   toggleTimestamps[toggleIdx] <= sampleCycle)
            {
                state = -state;
                toggleIdx++;
            }

            outSamples[i] = state;
        }
    }
}
