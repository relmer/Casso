#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  TapeAudio
//
//  A tape recording decoded to mono samples in -1..1 at its own sample rate.
//  This is what every tape file reader produces and what the signal decoder
//  and the recorder consume.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeAudio
{
    static constexpr uint32_t  kMinSampleRate = 8000;
    static constexpr uint32_t  kMaxSampleRate = 96000;
    static constexpr uint32_t  kMaxChannels   = 2;

    std::vector<float>  samples;
    uint32_t            sampleRate    = 0;
    uint16_t            bitsPerSample = 16;   // what the file had, and what a rewrite keeps: 8 or 16
};
