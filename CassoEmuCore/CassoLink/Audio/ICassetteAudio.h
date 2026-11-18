#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICassetteAudio
//
//  The PC's end of the cassette cables: an output to the Apple's cassette
//  input and an input from its cassette output, both mono at one sample
//  rate. WASAPI implements it for a real Apple, and the scenario tests over
//  an emulated machine's cassette port.
//
////////////////////////////////////////////////////////////////////////////////

class ICassetteAudio
{
public:
    virtual ~ICassetteAudio() = default;

    virtual uint32_t  GetSampleRate () const = 0;

    //  Samples queued for the output, after those queued before them.
    virtual HRESULT   Play          (std::span<const float> samples) = 0;

    //  The input's samples since the last call, waiting up to waitMs for
    //  them.
    virtual HRESULT   Capture       (DWORD waitMs, std::vector<float> & outSamples) = 0;

    //  The milliseconds of output that are queued and not yet sent.
    virtual double    GetPlayingMs  () const = 0;
};
