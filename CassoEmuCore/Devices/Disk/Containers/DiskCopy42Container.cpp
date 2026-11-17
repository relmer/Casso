#include "Pch.h"

#include "Devices/Disk/Containers/DiskCopy42Container.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::IsDiskCopy42
//
//  The marker word at $52, a name length that fits its field, and data and tag
//  sizes that add up to the file's.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCopy42Container::IsDiskCopy42 (std::span<const Byte> bytes)
{
    uint64_t  dataSize = 0;
    uint64_t  tagSize  = 0;



    if (bytes.size() < kHeaderSize)
    {
        return false;
    }

    dataSize = ReadBig32 (bytes, kOffDataSize);
    tagSize  = ReadBig32 (bytes, kOffTagSize);

    return bytes[kOffPrivate] == (Byte) (kPrivateMarker >> 8)
        && bytes[kOffPrivate + 1] == (Byte) (kPrivateMarker & 0xFF)
        && bytes[0] < kNameBytes
        && kHeaderSize + dataSize + tagSize == bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::Parse
//
//  A data checksum that doesn't match is reported in the header rather than
//  refused, so the image can be read and isn't written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCopy42Container::Parse (
    std::span<const Byte>   bytes,
    DiskCopy42Header      & outHeader,
    std::vector<Byte>     & outData,
    std::string           & outError)
{
    HRESULT  hr       = S_OK;
    bool     isImage  = IsDiskCopy42 (bytes);
    size_t   nameLen  = 0;
    size_t   tagStart = 0;



    outHeader = DiskCopy42Header();
    outData.clear();

    CBRFEx (isImage, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The file is not a Disk Copy 4.2 image.");

    nameLen = bytes[0];

    outHeader.name.assign (bytes.begin() + 1, bytes.begin() + 1 + nameLen);

    outHeader.dataSize     = ReadBig32 (bytes, kOffDataSize);
    outHeader.tagSize      = ReadBig32 (bytes, kOffTagSize);
    outHeader.dataChecksum = ReadBig32 (bytes, kOffDataSum);
    outHeader.tagChecksum  = ReadBig32 (bytes, kOffTagSum);
    outHeader.diskFormat   = bytes[kOffDiskFormat];
    outHeader.formatByte   = bytes[kOffFormatByte];

    tagStart = kHeaderSize + outHeader.dataSize;

    outData.assign (bytes.begin() + kHeaderSize, bytes.begin() + tagStart);
    outHeader.tags.assign (bytes.begin() + tagStart, bytes.end());

    outHeader.isDataChecksumBad = ComputeChecksum (outData) != outHeader.dataChecksum;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::Build
//
//  Both checksums are computed again from the bytes written.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCopy42Container::Build (const DiskCopy42Header & header, std::span<const Byte> data, std::vector<Byte> & outBytes)
{
    size_t                 nameLen = std::min (header.name.size(), kNameBytes - 1);
    std::span<const Byte>  tags (header.tags);
    uint32_t               tagSum  = tags.size() > kTagSumSkip ? ComputeChecksum (tags.subspan (kTagSumSkip)) : 0;



    outBytes.assign (kHeaderSize, 0);

    outBytes[0] = (Byte) nameLen;
    std::copy_n (header.name.begin(), nameLen, outBytes.begin() + 1);

    WriteBig32 (outBytes, kOffDataSize, (uint32_t) data.size());
    WriteBig32 (outBytes, kOffTagSize,  (uint32_t) tags.size());
    WriteBig32 (outBytes, kOffDataSum,  ComputeChecksum (data));
    WriteBig32 (outBytes, kOffTagSum,   tagSum);

    outBytes[kOffDiskFormat]  = header.diskFormat;
    outBytes[kOffFormatByte]  = header.formatByte;
    outBytes[kOffPrivate]     = (Byte) (kPrivateMarker >> 8);
    outBytes[kOffPrivate + 1] = (Byte) (kPrivateMarker & 0xFF);

    outBytes.insert (outBytes.end(), data.begin(), data.end());
    outBytes.insert (outBytes.end(), tags.begin(), tags.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::ComputeChecksum
//
//  Each big-endian word is added to the sum, and the sum is rotated right one
//  bit after each add.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskCopy42Container::ComputeChecksum (std::span<const Byte> bytes)
{
    uint32_t  sum = 0;
    size_t    i   = 0;



    for (i = 0; i + 1 < bytes.size(); i += 2)
    {
        sum += (uint32_t) ((bytes[i] << 8) | bytes[i + 1]);
        sum  = std::rotr (sum, 1);
    }

    return sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::ReadBig32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskCopy42Container::ReadBig32 (std::span<const Byte> bytes, size_t at)
{
    return ((uint32_t) bytes[at] << 24)
         | ((uint32_t) bytes[at + 1] << 16)
         | ((uint32_t) bytes[at + 2] << 8)
         | (uint32_t) bytes[at + 3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy42Container::WriteBig32
//
////////////////////////////////////////////////////////////////////////////////

void DiskCopy42Container::WriteBig32 (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    bytes[at]     = (Byte) (value >> 24);
    bytes[at + 1] = (Byte) ((value >> 16) & 0xFF);
    bytes[at + 2] = (Byte) ((value >> 8) & 0xFF);
    bytes[at + 3] = (Byte) (value & 0xFF);
}
