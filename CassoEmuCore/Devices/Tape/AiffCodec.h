#pragma once

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec
//
//  Reads AIFF and AIFF-C recordings into mono samples: signed big-endian PCM
//  at 8, 16, 24 and 32 bits, plus AIFF-C's uncompressed "NONE" and its
//  little-endian 16-bit "sowt", in one or two channels. Works on byte buffers
//  only; the caller does the file I/O.
//
////////////////////////////////////////////////////////////////////////////////

class AiffCodec
{
public:
    static bool     IsAiff (std::span<const Byte> bytes);
    static HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error);

private:
    struct Format
    {
        Word      channels      = 0;
        uint32_t  frames        = 0;
        Word      bitsPerSample = 0;
        uint32_t  sampleRate    = 0;
        bool      isLittle      = false;
    };

    static constexpr size_t  kFormHeaderSize  = 12;      // "FORM", size, "AIFF" or "AIFC"
    static constexpr size_t  kChunkHeaderSize = 8;       // id, size
    static constexpr size_t  kCommSize        = 18;      // channels, frames, bits, 80-bit rate
    static constexpr size_t  kCommAifcSize    = 22;      // plus the compression type
    static constexpr size_t  kSsndHeaderSize  = 8;       // offset, block size
    static constexpr size_t  kTagLength       = 4;
    static constexpr Word    kBits8           = 8;
    static constexpr Word    kBits16          = 16;
    static constexpr Word    kBits24          = 24;
    static constexpr Word    kBits32          = 32;

    static HRESULT   ReadComm       (std::span<const Byte> body, bool isAifc, Format & format, std::string & error);
    static HRESULT   CheckFormat    (const Format & format, std::string & error);
    static void      ReadSamples    (std::span<const Byte> data, const Format & format, TapeAudio & audio);
    static float     ReadSample     (std::span<const Byte> data, size_t at, const Format & format);
    static double    ReadExtended80 (std::span<const Byte> bytes, size_t at);
    static uint32_t  ReadBig32      (std::span<const Byte> bytes, size_t at);
    static Word      ReadBig16      (std::span<const Byte> bytes, size_t at);
};
