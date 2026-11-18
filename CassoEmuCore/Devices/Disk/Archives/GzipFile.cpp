#include "Pch.h"

#include "Devices/Disk/Archives/GzipFile.h"
#include "Update/Inflate.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GzipFile::IsGzip
//
////////////////////////////////////////////////////////////////////////////////

bool GzipFile::IsGzip (std::span<const Byte> bytes)
{
    return bytes.size() >= kHeaderSize + kTrailer && bytes[0] == kId1 && bytes[1] == kId2 && bytes[2] == kDeflate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GzipFile::Expand
//
//  The optional fields are skipped in the order RFC 1952 sets: extra, name,
//  comment, header CRC. The data's CRC-32 and length must match the trailer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GzipFile::Expand (std::span<const Byte> bytes, std::vector<Byte> & outData, std::string & outName, std::string & outError)
{
    HRESULT   hr        = S_OK;
    bool      isGzip    = IsGzip (bytes);
    Byte      flags     = 0;
    size_t    at        = kHeaderSize;
    size_t    end       = 0;
    uint32_t  crc       = 0;
    uint32_t  length    = 0;
    bool      inside    = true;
    bool      matches   = false;



    outData.clear();
    outName.clear();

    CBRFEx (isGzip, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The file is not a gzip file.");

    flags = bytes[3];
    end   = bytes.size() - kTrailer;

    if (flags & kFlagExtra)
    {
        inside = at + 2 <= end;
        CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The gzip header runs past the data.");

        at += 2 + (size_t) (bytes[at] | (bytes[at + 1] << 8));
    }

    if (flags & kFlagName)
    {
        while (at < end && bytes[at] != 0)
        {
            outName.push_back ((char) bytes[at++]);
        }

        at++;
    }

    if (flags & kFlagNote)
    {
        while (at < end && bytes[at] != 0)
        {
            at++;
        }

        at++;
    }

    if (flags & kFlagCrc)
    {
        at += 2;
    }

    inside = at <= end;
    CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The gzip header runs past the data.");

    crc    = (uint32_t) bytes[end] | ((uint32_t) bytes[end + 1] << 8) | ((uint32_t) bytes[end + 2] << 16) | ((uint32_t) bytes[end + 3] << 24);
    length = (uint32_t) bytes[end + 4] | ((uint32_t) bytes[end + 5] << 8) | ((uint32_t) bytes[end + 6] << 16) | ((uint32_t) bytes[end + 7] << 24);

    hr = Inflate::Decompress (bytes.subspan (at, end - at), length, outData);
    CHRF (hr, outError = "The gzip data doesn't expand.");

    matches = outData.size() == length && ZipArchive::ComputeCrc32 (outData) == crc;
    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_CRC), outError = "The gzip data doesn't match its CRC.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GzipFile::Build
//
//  The data in stored blocks of up to 65,535 bytes, each a header byte, its
//  length and the length's complement.
//
////////////////////////////////////////////////////////////////////////////////

void GzipFile::Build (std::span<const Byte> data, const std::string & name, std::vector<Byte> & outBytes)
{
    constexpr Byte  kOsUnknown = 0xFF;



    size_t    at  = 0;
    uint32_t  crc = ZipArchive::ComputeCrc32 (data);



    outBytes = { kId1, kId2, kDeflate, (Byte) (name.empty() ? 0 : kFlagName), 0, 0, 0, 0, 0, kOsUnknown };

    if (!name.empty())
    {
        outBytes.insert (outBytes.end(), name.begin(), name.end());
        outBytes.push_back (0);
    }

    do
    {
        size_t  take = std::min (kMaxStored, data.size() - at);
        bool    last = at + take == data.size();

        outBytes.push_back (last ? 1 : 0);
        outBytes.push_back ((Byte) (take & 0xFF));
        outBytes.push_back ((Byte) (take >> 8));
        outBytes.push_back ((Byte) (~take & 0xFF));
        outBytes.push_back ((Byte) ((~take >> 8) & 0xFF));
        outBytes.insert (outBytes.end(), data.begin() + (ptrdiff_t) at, data.begin() + (ptrdiff_t) (at + take));

        at += take;
    }
    while (at < data.size());

    for (int shift = 0; shift < 32; shift += 8)
    {
        outBytes.push_back ((Byte) (crc >> shift));
    }

    for (int shift = 0; shift < 32; shift += 8)
    {
        outBytes.push_back ((Byte) ((uint32_t) data.size() >> shift));
    }
}
