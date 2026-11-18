#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AudioFileDecoder
//
//  Decodes a sound file through Media Foundation to mono float samples at a
//  given rate. The caller starts Media Foundation first.
//
////////////////////////////////////////////////////////////////////////////////

class AudioFileDecoder
{
public:
    static HRESULT  DecodeToMono (const wchar_t * path, uint32_t targetSampleRate, vector<float> & outSamples);
};
