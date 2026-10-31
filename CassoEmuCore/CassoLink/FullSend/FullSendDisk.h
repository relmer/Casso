#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk
//
//  The values a full send computes from a disk's sectors, in DOS logical order
//  in an image-sized buffer with at least the tracks read so far: its
//  fingerprint, its whole-image hash and its image's file name.
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendDisk
{
public:
    static constexpr int  kDosCatalogTrack = 17;

    //  ProDOS's volume directory, blocks 2 to 5, is on track 0, so its
    //  fingerprint is track 0 alone. Every other disk uses track 0 and the DOS
    //  3.3 catalog track.
    static bool     IsCatalogOnTrackZero (const std::vector<Byte> & image);

    //  SHA-256 in lowercase hex.
    static HRESULT  ComputeFingerprint (const std::vector<Byte> & image, std::string & outHex);
    static HRESULT  ComputeHash        (std::span<const Byte> image, std::string & outHex);

    //  The volume's name on ProDOS or Apple Pascal, the first program's on
    //  DOS 3.3; empty when the catalog has neither.
    static std::string  GetCatalogTitle (const std::vector<Byte> & image);

    //  NNNN NAME, with the name cleaned by the host file name rules, or NNNN
    //  alone.
    static std::string  MakeImageTitle (int sequence, const std::string & catalogTitle);

    //  With a leading period: the one chosen, or .po for ProDOS and .dsk for
    //  any other.
    static std::string  GetImageExtension (const std::vector<Byte> & image, const std::string & chosen);

private:
    static bool         TryGetPascalVolume (const std::vector<Byte> & image, std::string & outName);
    static std::string  ToHex              (const std::vector<Byte> & digest);
};
