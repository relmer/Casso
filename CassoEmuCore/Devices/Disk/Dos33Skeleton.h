#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton
//
//  Pure DOS 3.3 filesystem structures for a fresh 143,360-byte sector buffer.
//  Write lays down an INIT-compatible empty volume: VTOC at track 17 sector 0
//  (catalog chain T17 S15 -> S1, tracks 0-2 + 17 allocated in the free
//  bitmap), every other sector zero. InstallDos copies the DOS image
//  (tracks 0-2) from a caller-supplied System Master sector image so the disk
//  boots; Dos33FileWriter adds the minimal HELLO greeting a booted DOS runs.
//
//  The buffer is always in DOS 3.3 LOGICAL sector layout (track-major,
//  256-byte sectors); format ordering is applied later by the builder.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33Skeleton
{
public:
    //  Empty formatted volume (data-only). Buffer must be kImageByteSize.
    static HRESULT  Write (vector<Byte> & buffer, Byte volumeNumber);

    //  Copies tracks 0-2 verbatim from the System Master image.
    static HRESULT  InstallDos (vector<Byte> & buffer, const vector<Byte> & masterSectors);

private:
    //  DOS 3.3 on-disk geometry (Beneath Apple DOS ch. 4). The VTOC lives at
    //  T17 S0; the catalog chains T17 S15 down to S1.
    static constexpr int   kVtocTrack             = 17;
    static constexpr int   kCatalogFirstSector    = 15;
    static constexpr int   kVtocOffCatalogTrack   = 0x01;   // -> 17
    static constexpr int   kVtocOffCatalogSector  = 0x02;   // -> 15
    static constexpr int   kVtocOffDosRelease     = 0x03;   // -> 3
    static constexpr int   kVtocOffVolumeNumber   = 0x06;
    static constexpr int   kVtocOffMaxTsPairs     = 0x27;   // -> 122
    static constexpr int   kVtocOffLastAllocTrack = 0x30;   // allocation hint
    static constexpr int   kVtocOffAllocDirection = 0x31;   // +1 outward
    static constexpr int   kVtocOffTrackCount     = 0x34;   // -> 35
    static constexpr int   kVtocOffSectorCount    = 0x35;   // -> 16
    static constexpr int   kVtocOffSectorBytesLo  = 0x36;   // -> 0x00
    static constexpr int   kVtocOffSectorBytesHi  = 0x37;   // -> 0x01 (256)
    static constexpr int   kVtocOffFreeBitmap     = 0x38;   // 4 bytes / track
    static constexpr int   kCatalogOffNextTrack   = 0x01;
    static constexpr int   kCatalogOffNextSector  = 0x02;
    static constexpr Byte  kDosRelease            = 3;
    static constexpr Byte  kMaxTsPairs            = 122;
    static constexpr int   kDosImageTracks        = 3;      // tracks 0-2 carry DOS

    //  Byte offset of (track, logical sector) in the DOS 3.3-ordered buffer.
    static size_t  SectorOffset (int track, int sector);
};





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileWriter
//
//  Writes a file into a Dos33Skeleton-formatted buffer: catalog entry,
//  track/sector list, data sectors, VTOC bitmap kept honest. Today that is
//  only the minimal Applesoft HELLO greeting a bootable disk needs.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33FileWriter
{
public:
    static HRESULT  WriteHello (vector<Byte> & buffer);
};
