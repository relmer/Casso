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
    // Convert one slice of speaker toggles to PCM float32 mono samples.
    // toggleTimestamps: cycle counts within the slice when $C030 was accessed
    // sliceCycles:      CPU cycles executed in the slice
    // initialState:     speaker level at the start of the slice
    // cyclesPerSample:  CPU cycles per output sample
    // outSamples:       output buffer (pre-allocated by caller)
    // numSamples:       samples that end within the slice
    void  GeneratePCM (const vector<uint32_t> & toggleTimestamps,
                       uint32_t                 sliceCycles,
                       float                    initialState,
                       double                   cyclesPerSample,
                       float                  * outSamples,
                       uint32_t                 numSamples);
    void  Reset       ();

private:
    static double  Integrate (const vector<uint32_t> & toggleTimestamps,
                              size_t                 & toggleIdx,
                              float                  & state,
                              double                   from,
                              double                   to,
                              bool                   & hasEdge);

    // The start of the next sample, from a previous slice: its length in
    // cycles and the speaker level integrated over them.
    double  m_carriedCycles = 0.0;
    double  m_carriedArea   = 0.0;
};
