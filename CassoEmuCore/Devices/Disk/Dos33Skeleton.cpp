#include "Pch.h"

#include "Dos33Skeleton.h"





//  DOS 3.3 on-disk geometry (Beneath Apple DOS ch. 4). The VTOC lives at
//  T17 S0; the catalog chains T17 S15 down to S1. Offsets within the VTOC:
static constexpr int   s_kVtocTrack             = 17;
static constexpr int   s_kCatalogFirstSector    = 15;
static constexpr int   s_kVtocOffCatalogTrack   = 0x01;   // -> 17
static constexpr int   s_kVtocOffCatalogSector  = 0x02;   // -> 15
static constexpr int   s_kVtocOffDosRelease     = 0x03;   // -> 3
static constexpr int   s_kVtocOffVolumeNumber   = 0x06;
static constexpr int   s_kVtocOffMaxTsPairs     = 0x27;   // -> 122
static constexpr int   s_kVtocOffLastAllocTrack = 0x30;   // allocation hint
static constexpr int   s_kVtocOffAllocDirection = 0x31;   // +1 outward
static constexpr int   s_kVtocOffTrackCount     = 0x34;   // -> 35
static constexpr int   s_kVtocOffSectorCount    = 0x35;   // -> 16
static constexpr int   s_kVtocOffSectorBytesLo  = 0x36;   // -> 0x00
static constexpr int   s_kVtocOffSectorBytesHi  = 0x37;   // -> 0x01 (256)
static constexpr int   s_kVtocOffFreeBitmap     = 0x38;   // 4 bytes / track
static constexpr int   s_kCatalogOffNextTrack   = 0x01;
static constexpr int   s_kCatalogOffNextSector  = 0x02;
static constexpr Byte  s_kDosRelease            = 3;
static constexpr Byte  s_kMaxTsPairs            = 122;
static constexpr int   s_kDosImageTracks        = 3;      // tracks 0-2 carry DOS





////////////////////////////////////////////////////////////////////////////////
//
//  SectorOffset
//
//  Byte offset of (track, logical sector) in the DOS 3.3-ordered buffer.
//
////////////////////////////////////////////////////////////////////////////////

static size_t SectorOffset (int track, int sector)
{
    return ((size_t) track  * NibblizationLayer::kSectorsPerTrack + (size_t) sector)
         * (size_t) NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton::Write
//
//  INIT-compatible empty volume (spec 017 R-004): the free bitmap reserves
//  tracks 0-2 (where DOS lives on a bootable disk -- kept allocated on a
//  data disk too, matching what every 1980s formatter produced) and the
//  catalog track 17; every other track is fully free. The catalog chain
//  links T17 S15 -> S14 -> ... -> S1 with all file entries zero, so the
//  guest lists a clean empty CATALOG (FR-005).
//
//  Free-bitmap encoding: 4 bytes per track at VTOC+0x38; a FREE 16-sector
//  track is FF FF 00 00 (sectors 15..8 in byte 0's bits 7..0, sectors 7..0
//  in byte 1), an allocated track is all zero.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Skeleton::Write (vector<Byte> & buffer, Byte volumeNumber)
{
    HRESULT  hr          = S_OK;
    size_t   vtoc        = 0;
    size_t   bufferBytes = buffer.size();
    int      track       = 0;
    int      sector      = 0;



    CBRA (bufferBytes == (size_t) NibblizationLayer::kImageByteSize);

    std::fill (buffer.begin(), buffer.end(), (Byte) 0);

    // VTOC (T17 S0)
    vtoc = SectorOffset (s_kVtocTrack, 0);

    buffer[vtoc + s_kVtocOffCatalogTrack]   = (Byte) s_kVtocTrack;
    buffer[vtoc + s_kVtocOffCatalogSector]  = (Byte) s_kCatalogFirstSector;
    buffer[vtoc + s_kVtocOffDosRelease]     = s_kDosRelease;
    buffer[vtoc + s_kVtocOffVolumeNumber]   = volumeNumber;
    buffer[vtoc + s_kVtocOffMaxTsPairs]     = s_kMaxTsPairs;
    buffer[vtoc + s_kVtocOffLastAllocTrack] = (Byte) (s_kVtocTrack + 1);
    buffer[vtoc + s_kVtocOffAllocDirection] = 1;
    buffer[vtoc + s_kVtocOffTrackCount]     = (Byte) NibblizationLayer::kTrackCount;
    buffer[vtoc + s_kVtocOffSectorCount]    = (Byte) NibblizationLayer::kSectorsPerTrack;
    buffer[vtoc + s_kVtocOffSectorBytesLo]  = 0x00;
    buffer[vtoc + s_kVtocOffSectorBytesHi]  = 0x01;

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        bool    allocated = (track < s_kDosImageTracks) || (track == s_kVtocTrack);
        size_t  entry     = vtoc + s_kVtocOffFreeBitmap + (size_t) track * 4;

        buffer[entry]     = allocated ? 0x00 : 0xFF;
        buffer[entry + 1] = allocated ? 0x00 : 0xFF;
    }

    // Catalog chain (T17 S15 -> S1)
    for (sector = s_kCatalogFirstSector; sector >= 1; sector--)
    {
        size_t  cat = SectorOffset (s_kVtocTrack, sector);

        if (sector > 1)
        {
            buffer[cat + s_kCatalogOffNextTrack]  = (Byte) s_kVtocTrack;
            buffer[cat + s_kCatalogOffNextSector] = (Byte) (sector - 1);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton::InstallDos
//
//  Implemented in T018.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Skeleton::InstallDos (vector<Byte> & buffer, const vector<Byte> & masterSectors)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (masterSectors);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileWriter::WriteHello
//
//  Implemented in T018.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33FileWriter::WriteHello (vector<Byte> & buffer)
{
    UNREFERENCED_PARAMETER (buffer);

    return E_NOTIMPL;
}
