#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Header
//
//  The fields of a Disk Copy 4.2 image's 84-byte header, and the tag bytes
//  that follow its data, kept so a write puts them back.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskCopy42Header
{
    std::string        name;
    uint32_t           dataSize     = 0;
    uint32_t           tagSize      = 0;
    uint32_t           dataChecksum = 0;
    uint32_t           tagChecksum  = 0;
    Byte               diskFormat   = 1;
    Byte               formatByte   = 0x24;
    std::vector<Byte>  tags;

    //  The data checksum in the header didn't match the data, so the image
    //  opens read-only.
    bool               isDataChecksumBad = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container
//
//  Reads and writes Apple's Disk Copy 4.2 image: an 84-byte big-endian
//  header, the disk's data in block order, then its tag bytes. Each checksum
//  adds the bytes as big-endian words and rotates the sum right one bit after
//  each, and the tag checksum leaves out the first 12 tag bytes, as Disk Copy
//  computes it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCopy42Container
{
public:
    static constexpr size_t  kHeaderSize = 84;

    static bool      IsDiskCopy42    (std::span<const Byte> bytes);
    static HRESULT   Parse           (std::span<const Byte> bytes, DiskCopy42Header & outHeader, std::vector<Byte> & outData, std::string & outError);
    static void      Build           (const DiskCopy42Header & header, std::span<const Byte> data, std::vector<Byte> & outBytes);
    static uint32_t  ComputeChecksum (std::span<const Byte> bytes);

private:
    static constexpr size_t  kNameBytes      = 64;
    static constexpr size_t  kOffDataSize    = 64;
    static constexpr size_t  kOffTagSize     = 68;
    static constexpr size_t  kOffDataSum     = 72;
    static constexpr size_t  kOffTagSum      = 76;
    static constexpr size_t  kOffDiskFormat  = 80;
    static constexpr size_t  kOffFormatByte  = 81;
    static constexpr size_t  kOffPrivate     = 82;
    static constexpr Word    kPrivateMarker  = 0x0100;
    static constexpr size_t  kTagSumSkip     = 12;

    static uint32_t  ReadBig32  (std::span<const Byte> bytes, size_t at);
    static void      WriteBig32 (std::vector<Byte> & bytes, size_t at, uint32_t value);
};
