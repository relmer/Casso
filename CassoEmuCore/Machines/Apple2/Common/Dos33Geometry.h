#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Geometry
//
//  The track and sector counts of a DOS 3.x volume, and where a sector and its
//  bitmap bit are in the buffer. DOS 3.3 is 35 tracks of 16 sectors. DOS 3.2
//  is 35 tracks of 13. Larger volumes have 40 or 50 tracks, or 32 sectors per
//  track, and the Basis 108 layout is 80 tracks of 16 sectors with two bitmap
//  bytes per track where the others have four.
//
//  A track's bitmap bytes are one big-endian value whose highest bit is the
//  track's last sector, so sector S is bit S + width - sectorsPerTrack, where
//  width is 32 or 16.
//
//  The VTOC fields are from CiderPress II's DOS notes, under its Apache
//  license.
//
////////////////////////////////////////////////////////////////////////////////

struct Dos33Geometry
{
    static constexpr int     kVtocTrack            = 17;
    static constexpr size_t  kSectorByteSize       = 256;
    static constexpr size_t  kVtocOffCatalogTrack  = 0x01;
    static constexpr size_t  kVtocOffCatalogSector = 0x02;
    static constexpr size_t  kVtocOffTrackCount    = 0x34;
    static constexpr size_t  kVtocOffSectorCount   = 0x35;
    static constexpr size_t  kVtocOffSectorBytes   = 0x36;
    static constexpr size_t  kVtocOffFreeBitmap    = 0x38;
    static constexpr int     kBasisTrackCount      = 80;
    static constexpr int     kMinTrackCount        = 35;
    static constexpr int     kMaxTrackCount        = 50;

    int  tracks              = 35;
    int  sectorsPerTrack     = 16;
    int  bitmapBytesPerTrack = 4;

    //  The geometry with these counts. An 80-track volume has two bitmap
    //  bytes per track.
    static Dos33Geometry  Make (int tracks, int sectorsPerTrack);

    //  The geometry a buffer's VTOC indicates, when the VTOC is at that
    //  geometry's offset and the buffer has that geometry's size.
    static bool  TryDetect (const vector<Byte> & buffer, Dos33Geometry & outGeometry);

    //  The geometry of a buffer by its size alone: 13 sectors for a 116,480
    //  byte buffer, DOS 3.3's otherwise.
    static Dos33Geometry  GetForSize (size_t bufferBytes);

    size_t    GetSectorOffset     (int track, int sector) const { return ((size_t) track * (size_t) sectorsPerTrack + (size_t) sector) * kSectorByteSize; }
    size_t    GetImageByteSize    () const                      { return (size_t) tracks * (size_t) sectorsPerTrack * kSectorByteSize; }
    uint32_t  GetUnitCount        () const                      { return (uint32_t) (tracks * sectorsPerTrack); }
    uint32_t  ToUnit              (int track, int sector) const { return (uint32_t) (track * sectorsPerTrack + sector); }
    int       GetTrack            (uint32_t unit) const         { return (int) (unit / (uint32_t) sectorsPerTrack); }
    int       GetSector           (uint32_t unit) const         { return (int) (unit % (uint32_t) sectorsPerTrack); }
    int       GetLastSector       () const                      { return sectorsPerTrack - 1; }
    bool      IsThirteenSector    () const                      { return sectorsPerTrack == 13; }
    bool      IsStandard          () const                      { return tracks == 35 && sectorsPerTrack == 16; }

    //  Where a sector's bitmap bit is: the byte's offset in the buffer and
    //  the bit's mask in that byte.
    size_t    GetBitmapByteOffset (int track, int sector) const;
    Byte      GetBitmapMask       (int sector) const;

    //  The bitmap bytes of a track with every sector free.
    void      GetFreeTrackBytes   (Byte outBytes[4]) const;

    bool  operator== (const Dos33Geometry & other) const = default;
};
