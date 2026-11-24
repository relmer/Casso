#pragma once

#include "Pch.h"

#include "Update/Deflate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GzipFile
//
//  Reads and writes gzip (RFC 1952): a header with an optional name, raw
//  Deflate data, then the CRC-32 and length of the data. Writing compresses
//  the data with Deflate, at the level a file's header indicates when it is
//  written again.
//
////////////////////////////////////////////////////////////////////////////////

class GzipFile
{
public:
    static bool     IsGzip   (std::span<const Byte> bytes);
    static HRESULT  Expand   (std::span<const Byte> bytes, std::vector<Byte> & outData, std::string & outName, std::string & outError);
    static void     Build    (std::span<const Byte> data, const std::string & name, std::vector<Byte> & outBytes, int level = Deflate::kDefault);

    //  The Deflate level a gzip file was written at, from its extra flags:
    //  kBest for 2, kFastest for 4, otherwise kDefault.
    static int      GetLevel (std::span<const Byte> bytes);

private:
    static constexpr Byte    kId1        = 0x1F;
    static constexpr Byte    kId2        = 0x8B;
    static constexpr Byte    kDeflate    = 8;
    static constexpr Byte    kFlagCrc    = 0x02;
    static constexpr Byte    kFlagExtra  = 0x04;
    static constexpr Byte    kFlagName   = 0x08;
    static constexpr Byte    kFlagNote   = 0x10;
    static constexpr Byte    kExtraBest  = 2;
    static constexpr Byte    kExtraFast  = 4;
    static constexpr size_t  kExtraFlags = 8;
    static constexpr size_t  kHeaderSize = 10;
    static constexpr size_t  kTrailer    = 8;
};
