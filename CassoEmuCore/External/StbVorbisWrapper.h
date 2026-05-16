#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StbVorbisWrapper
//
//  Thin C++ wrapper around Sean Barrett's `stb_vorbis.c` (vendored at
//  `CassoEmuCore/External/stb_vorbis.c`). The wrapper exists so the
//  rest of the codebase never has to see the C-style stb header
//  surface and so the heap of C-style warnings + analyzer noise the
//  upstream file ships with can be quarantined to a single
//  translation unit (`StbVorbisWrapper.cpp` is built with
//  `EnableCodeAnalysis=false` and includes the upstream file inside a
//  push-0 / specific-disable block; see Q5 in the spec answers).
//
//  The wrapper exposes exactly the surface Casso needs for spec
//  005-disk-ii-audio Phase 13: in-memory OGG Vorbis → interleaved
//  int16 PCM, plus the source-rate / channel-count metadata the
//  resampler needs. Anything fancier (push-data API, file-IO API,
//  loopstart tags, ...) stays compiled out via the
//  STB_VORBIS_NO_PUSHDATA_API / STB_VORBIS_NO_STDIO defines.
//
////////////////////////////////////////////////////////////////////////////////

class StbVorbisWrapper
{
public:

    // Decode the supplied OGG Vorbis bytes into an interleaved int16
    // PCM buffer. Returns S_OK and fills `outPcm` /
    // `outSampleRate` / `outChannels` on success. Returns E_FAIL with
    // `outError` populated on any decoder error, including malformed
    // input. The caller owns the resulting buffer (it lives only as
    // long as `outPcm` does); the original `bytes` span may be
    // discarded immediately after the call returns.
    static HRESULT DecodeOggToInterleavedShort (
        const uint8_t *      bytes,
        size_t               byteCount,
        vector<int16_t>    & outPcm,
        uint32_t           & outSampleRate,
        uint32_t           & outChannels,
        string             & outError);
};
