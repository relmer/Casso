#pragma once

#include "Pch.h"

#include "CassoLink/Audio/CassetteCodec.h"
#include "CassoLink/Audio/ICassetteAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScriptedCassetteAudio
//
//  The PC's cassette audio for unit tests: Capture returns the queued input
//  samples, then silence, at most waitMs of them a call. Samples passed to
//  Play are appended to `played`.
//
////////////////////////////////////////////////////////////////////////////////

class ScriptedCassetteAudio : public ICassetteAudio
{
public:
    static constexpr uint32_t  kSampleRate = 48000;

    std::deque<float>   input;
    std::vector<float>  played;

    uint32_t  GetSampleRate () const override { return kSampleRate; }
    double    GetPlayingMs  () const override { return 0.0; }

    HRESULT  Play (std::span<const float> samples) override
    {
        played.insert (played.end(), samples.begin(), samples.end());
        return S_OK;
    }

    HRESULT  Capture (DWORD waitMs, std::vector<float> & outSamples) override
    {
        constexpr DWORD  kMsPerSecond = 1000;
        size_t           count        = std::max<size_t> (1, (size_t) waitMs * kSampleRate / kMsPerSecond);

        outSamples.clear();

        while (outSamples.size() < count)
        {
            outSamples.push_back (input.empty() ? 0.0f : input.front());

            if (!input.empty())
            {
                input.pop_front();
            }
        }

        return S_OK;
    }

    //  Silence, then the halves as a square wave, then silence.
    void  Queue (const std::vector<double> & halvesUs)
    {
        std::vector<float>  samples;

        input.insert (input.end(), kSampleRate / 20, 0.0f);
        CassetteCodec::Render (halvesUs, kSampleRate, 0.7f, samples);
        input.insert (input.end(), samples.begin(), samples.end());
        input.insert (input.end(), kSampleRate / 20, 0.0f);
    }
};
