#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Geometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::Make
//
////////////////////////////////////////////////////////////////////////////////

Dos33Geometry Dos33Geometry::Make (int tracks, int sectorsPerTrack)
{
    Dos33Geometry  geometry;



    geometry.tracks              = tracks;
    geometry.sectorsPerTrack     = sectorsPerTrack;
    geometry.bitmapBytesPerTrack = (tracks == kBasisTrackCount) ? 2 : 4;

    return geometry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::GetForSize
//
////////////////////////////////////////////////////////////////////////////////

Dos33Geometry Dos33Geometry::GetForSize (size_t bufferBytes)
{
    Dos33Geometry  thirteen = Make (35, 13);



    return (bufferBytes == thirteen.GetImageByteSize()) ? thirteen : Dos33Geometry();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::TryDetect
//
//  The VTOC's offset depends on the sector count, so the VTOC is read at the
//  offset of each count in turn. A match has that count in its sectors-per-
//  track byte, a track count that makes the buffer's size, 256-byte sectors,
//  and a catalog on track 17.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Geometry::TryDetect (const vector<Byte> & buffer, Dos33Geometry & outGeometry)
{
    static constexpr int  kSectorCounts[] = { 16, 13, 32 };



    bool  found = false;



    for (int sectors : kSectorCounts)
    {
        size_t         vtoc        = (size_t) kVtocTrack * (size_t) sectors * kSectorByteSize;
        int            tracks      = 0;
        bool           tracksOk    = false;
        Dos33Geometry  candidate;

        if (found || vtoc + kSectorByteSize > buffer.size())
        {
            continue;
        }

        tracks    = buffer[vtoc + kVtocOffTrackCount];
        tracksOk  = (tracks >= kMinTrackCount && tracks <= kMaxTrackCount) || tracks == kBasisTrackCount;
        candidate = Make (tracks, sectors);

        found = tracksOk
             && buffer[vtoc + kVtocOffSectorCount]   == (Byte) sectors
             && buffer[vtoc + kVtocOffCatalogTrack]  == (Byte) kVtocTrack
             && buffer[vtoc + kVtocOffCatalogSector] >= 1
             && buffer[vtoc + kVtocOffCatalogSector] <  (Byte) sectors
             && buffer[vtoc + kVtocOffSectorBytes]     == 0x00
             && buffer[vtoc + kVtocOffSectorBytes + 1] == 0x01
             && candidate.GetImageByteSize()         == buffer.size();

        if (found)
        {
            outGeometry = candidate;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::GetBitmapByteOffset
//
////////////////////////////////////////////////////////////////////////////////

size_t Dos33Geometry::GetBitmapByteOffset (int track, int sector) const
{
    int  width = bitmapBytesPerTrack * 8;
    int  bit   = sector + width - sectorsPerTrack;



    return GetSectorOffset (kVtocTrack, 0) + kVtocOffFreeBitmap
         + (size_t) track * (size_t) bitmapBytesPerTrack
         + (size_t) ((width - 1 - bit) / 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::GetBitmapMask
//
////////////////////////////////////////////////////////////////////////////////

Byte Dos33Geometry::GetBitmapMask (int sector) const
{
    int  bit = sector + bitmapBytesPerTrack * 8 - sectorsPerTrack;



    return (Byte) (1u << (bit % 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry::GetFreeTrackBytes
//
//  Bytes past bitmapBytesPerTrack are zero.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33Geometry::GetFreeTrackBytes (Byte outBytes[4]) const
{
    int  sector = 0;
    int  width  = bitmapBytesPerTrack * 8;



    outBytes[0] = outBytes[1] = outBytes[2] = outBytes[3] = 0;

    for (sector = 0; sector < sectorsPerTrack; sector++)
    {
        int  bit = sector + width - sectorsPerTrack;

        outBytes[(width - 1 - bit) / 8] = (Byte) (outBytes[(width - 1 - bit) / 8] | GetBitmapMask (sector));
    }
}
