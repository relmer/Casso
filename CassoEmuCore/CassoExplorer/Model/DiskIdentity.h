#pragma once

#include "Pch.h"

class SectorDecodeReport;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity
//
//  What makes two disks or two files the same. A disk's identity is the
//  SHA-256 of its decoded volume buffer, so a `.dsk`, a `.po`, a 2IMG and a
//  WOZ of one disk match. A WOZ with a track its sectors don't decode, as a
//  protection track, has more than the buffer: `hasMore` is set. A file's
//  identity is the SHA-256 of its data fork together with its type.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIdentity
{
public:
    struct Volume
    {
        std::string  sha256;
        uint32_t     crc32   = 0;
        bool         hasMore = false;
    };

    static HRESULT  ComputeVolume (const std::vector<Byte> & sectors, const SectorDecodeReport & report, Volume & outVolume);
    static HRESULT  ComputeFile   (std::span<const Byte> data, std::string & outSha256);

    //  The key two files match by: the data's digest and the type.
    static std::string  GetFileKey (const std::string & sha256, Byte type);

    static std::string  FormatHex   (std::span<const Byte> bytes);
    static std::string  FormatCrc32 (uint32_t crc32);

private:
    static constexpr const char *  s_kHexDigits = "0123456789abcdef";
};
