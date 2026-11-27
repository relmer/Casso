#pragma once

#include "Pch.h"

class DiskCopy;
class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures
//
//  In-memory 16-sector and 13-sector disks for the format stamp tests, and
//  edits that rewrite a track's prologs in its bits, as a protected disk's
//  are.
//
////////////////////////////////////////////////////////////////////////////////

class StampDiskFixtures
{
public:
    static constexpr int   kTracks       = 35;
    static constexpr Byte  kVolume       = 254;
    static constexpr int   kAllFields    = -1;

    static void  MakeSixteenSector   (DiskImage & outImage);
    static void  MakeThirteenSector  (DiskImage & outImage);
    static int   ReplaceProlog       (DiskImage & image, int track, std::span<const Byte> from, std::span<const Byte> to, int maxCount = kAllFields);

    static std::shared_ptr<const DiskCopy>  MakeCopy (const DiskImage & image);

private:
    static Byte  ReadByteAt  (const vector<Byte> & bits, size_t bitAt);
    static void  WriteByteAt (vector<Byte> & bits, size_t bitAt, Byte value);
};
