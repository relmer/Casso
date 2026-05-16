// Wrapper translation unit for the vendored stb_vorbis.c. This file is
// the *only* place in the codebase that includes stb_vorbis.c, and it
// is built with PCH disabled and EnableCodeAnalysis=false so the
// upstream library's well-known false positives (documented and
// rejected upstream for ~15 years) stay quarantined here. See
// spec/005-disk-ii-audio Q5 for the rationale.

// Bring in the Casso PCH-style surface (windows.h, vector, string,
// ...) up front so the upstream stb header -- which itself pulls in a
// handful of C runtime headers -- does not perturb the include order
// of any standard or platform header pulled in via our wrapper API.
#include "Pch.h"

#include "StbVorbisWrapper.h"


// Disable the chunk of Vorbis we never use so the wrapper stays small
// (Casso decodes a handful of short mechanical drive samples from
// in-memory buffers, nothing more).
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO


// Quarantine the upstream warnings. stb_vorbis.c is a single-header
// public-domain C library used in production by Unity / Unreal /
// FAudio / FNA for over a decade; the warnings below are documented
// false positives that the author has explicitly declined to fix.
#pragma warning (push, 0)
#pragma warning (disable: 4244)    // conversion from int to short
#pragma warning (disable: 4456)    // declaration hides previous local
#pragma warning (disable: 4457)    // declaration hides function parameter
#pragma warning (disable: 4701)    // potentially uninitialized local
#pragma warning (disable: 4702)    // unreachable code
#pragma warning (disable: 4706)    // assignment within conditional
// Static analyzer false positives that ship with the library.
#pragma warning (disable: 6001)    // using uninitialized memory
#pragma warning (disable: 6011)    // dereference of NULL pointer
#pragma warning (disable: 6255)    // _alloca may raise stack overflow
#pragma warning (disable: 6262)    // function uses excessive stack
#pragma warning (disable: 6385)    // reading invalid data
#pragma warning (disable: 6386)    // buffer overrun
#include "stb_vorbis.c"
#pragma warning (pop)





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeOggToInterleavedShort
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StbVorbisWrapper::DecodeOggToInterleavedShort (
    const uint8_t *      bytes,
    size_t               byteCount,
    std::vector<int16_t> & outPcm,
    uint32_t           & outSampleRate,
    uint32_t           & outChannels,
    std::string        & outError)
{
    HRESULT          hr           = S_OK;
    stb_vorbis     * vorbis       = nullptr;
    int              openErr      = 0;
    stb_vorbis_info  info         = {};
    int              totalSamples = 0;

    outPcm.clear ();
    outSampleRate = 0;
    outChannels   = 0;

    if (bytes == nullptr || byteCount == 0)
    {
        outError = "DecodeOggToInterleavedShort: empty input buffer";
        hr = E_INVALIDARG;
        goto Error;
    }

    if (byteCount > static_cast<size_t> (INT_MAX))
    {
        outError = "DecodeOggToInterleavedShort: input buffer larger than INT_MAX";
        hr = E_INVALIDARG;
        goto Error;
    }

    vorbis = stb_vorbis_open_memory (bytes,
                                     static_cast<int> (byteCount),
                                     &openErr,
                                     nullptr);

    if (vorbis == nullptr)
    {
        outError = std::format (
            "stb_vorbis_open_memory failed: error code {}", openErr);
        hr = E_FAIL;
        goto Error;
    }

    info          = stb_vorbis_get_info (vorbis);
    outSampleRate = static_cast<uint32_t> (info.sample_rate);
    outChannels   = static_cast<uint32_t> (info.channels);

    if (outChannels == 0 || outSampleRate == 0)
    {
        outError = "stb_vorbis_get_info returned zero channels or rate";
        hr = E_FAIL;
        goto Error;
    }

    totalSamples = stb_vorbis_stream_length_in_samples (vorbis);

    if (totalSamples <= 0)
    {
        outError = "stb_vorbis_stream_length_in_samples returned non-positive";
        hr = E_FAIL;
        goto Error;
    }

    outPcm.resize (static_cast<size_t> (totalSamples)
                   * static_cast<size_t> (outChannels));

    {
        int decoded = stb_vorbis_get_samples_short_interleaved (
            vorbis,
            static_cast<int> (outChannels),
            outPcm.data (),
            static_cast<int> (outPcm.size ()));

        if (decoded <= 0)
        {
            outError = "stb_vorbis_get_samples_short_interleaved returned 0";
            hr = E_FAIL;
            goto Error;
        }

        outPcm.resize (static_cast<size_t> (decoded)
                       * static_cast<size_t> (outChannels));
    }

Error:
    if (vorbis != nullptr)
    {
        stb_vorbis_close (vorbis);
    }

    if (FAILED (hr))
    {
        outPcm.clear ();
    }

    return hr;
}
