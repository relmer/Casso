#pragma once

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec
//
//  Reads RIFF/WAVE recordings into mono samples and writes 16-bit mono PCM.
//  Reads integer PCM at 8, 16, 24 and 32 bits, IEEE float at 32 and 64 bits,
//  and WAVE_FORMAT_EXTENSIBLE wrapping either, in one or two channels. Works
//  on byte buffers only; the caller does the file I/O.
//
////////////////////////////////////////////////////////////////////////////////

class WavCodec
{
public:
    static constexpr uint32_t  kWrittenBitsPerSample = 16;

    static bool     IsWav  (std::span<const Byte> bytes);
    static HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error);
    static void     Encode (const TapeAudio & audio, std::vector<Byte> & bytes);

private:
    struct Format
    {
        Word      tag           = 0;
        Word      channels      = 0;
        uint32_t  sampleRate    = 0;
        Word      blockAlign    = 0;
        Word      bitsPerSample = 0;
    };

    static constexpr size_t    kRiffHeaderSize    = 12;      // "RIFF", size, "WAVE"
    static constexpr size_t    kChunkHeaderSize   = 8;       // id, size
    static constexpr size_t    kFmtMinSize        = 16;
    static constexpr size_t    kFmtExtensibleSize = 40;
    static constexpr size_t    kSubFormatOffset   = 24;      // within the fmt body
    static constexpr Word      kTagPcm            = 1;
    static constexpr Word      kTagFloat          = 3;
    static constexpr Word      kTagExtensible     = 0xFFFE;
    static constexpr uint32_t  kCanonicalHeader   = 44;
    static constexpr uint32_t  kFmtPcmSize        = 16;
    static constexpr double    kFullScale16       = 32768.0;
    static constexpr size_t    kTagLength         = 4;
    static constexpr Word      kBits8             = 8;
    static constexpr Word      kBits16            = 16;
    static constexpr Word      kBits24            = 24;
    static constexpr Word      kBits32            = 32;
    static constexpr Word      kBits64            = 64;

    static HRESULT   ReadFormat    (std::span<const Byte> body, Format & format, std::string & error);
    static HRESULT   CheckFormat   (const Format & format, std::string & error);
    static void      ReadSamples   (std::span<const Byte> data, const Format & format, TapeAudio & audio);
    static float     ReadSample    (std::span<const Byte> data, size_t at, const Format & format);
    static uint32_t  ReadLittle32  (std::span<const Byte> bytes, size_t at);
    static Word      ReadLittle16  (std::span<const Byte> bytes, size_t at);
    static void      WriteLittle32 (std::vector<Byte> & bytes, uint32_t value);
    static void      WriteLittle16 (std::vector<Byte> & bytes, Word value);
    static void      WriteTag      (std::vector<Byte> & bytes, const char * pszTag);
};
