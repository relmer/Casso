#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AudioGenerator
//
//  Converts speaker toggle timestamps into PCM float32 mono samples.
//  Pure computation — no audio device dependency.
//
////////////////////////////////////////////////////////////////////////////////

class AudioGenerator
{
public:
    // Convert speaker toggle timestamps to PCM float32 mono samples.
    // toggleTimestamps:     cycle counts when $C030 was read (each toggles state)
    // totalCyclesThisFrame: total CPU cycles executed this frame
    // initialState:         speaker state at start of frame (e.g. +0.3 or -0.3)
    // outSamples:           output buffer (pre-allocated by caller)
    // numSamples:           number of samples to generate
    static void GeneratePCM (
        const vector<uint32_t> & toggleTimestamps,
        uint32_t totalCyclesThisFrame,
        float initialState,
        float * outSamples,
        uint32_t numSamples);
};
