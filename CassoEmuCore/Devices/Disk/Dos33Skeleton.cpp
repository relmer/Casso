#include "Pch.h"

#include "Dos33Skeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton::SectorOffset
//
//  Byte offset of (track, logical sector) in the DOS 3.3-ordered buffer.
//
////////////////////////////////////////////////////////////////////////////////

size_t Dos33Skeleton::SectorOffset (int track, int sector)
{
    return ((size_t) track  * NibblizationLayer::kSectorsPerTrack + (size_t) sector)
         * (size_t) NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton::Write
//
//  INIT-compatible empty volume: the free bitmap reserves tracks 0-2 (where
//  DOS lives on a bootable disk -- kept allocated on a data disk too,
//  matching what every 1980s formatter produced) and the catalog track 17;
//  every other track is fully free. The catalog chain links T17 S15 -> S14
//  -> ... -> S1 with all file entries zero, so the guest lists a clean empty
//  CATALOG.
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
    vtoc = SectorOffset (kVtocTrack, 0);

    buffer[vtoc + kVtocOffCatalogTrack]   = (Byte) kVtocTrack;
    buffer[vtoc + kVtocOffCatalogSector]  = (Byte) kCatalogFirstSector;
    buffer[vtoc + kVtocOffDosRelease]     = kDosRelease;
    buffer[vtoc + kVtocOffVolumeNumber]   = volumeNumber;
    buffer[vtoc + kVtocOffMaxTsPairs]     = kMaxTsPairs;
    buffer[vtoc + kVtocOffLastAllocTrack] = (Byte) (kVtocTrack + 1);
    buffer[vtoc + kVtocOffAllocDirection] = 1;
    buffer[vtoc + kVtocOffTrackCount]     = (Byte) NibblizationLayer::kTrackCount;
    buffer[vtoc + kVtocOffSectorCount]    = (Byte) NibblizationLayer::kSectorsPerTrack;
    buffer[vtoc + kVtocOffSectorBytesLo]  = 0x00;
    buffer[vtoc + kVtocOffSectorBytesHi]  = 0x01;

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        bool    allocated = (track < kDosImageTracks) || (track == kVtocTrack);
        size_t  entry     = vtoc + kVtocOffFreeBitmap + (size_t) track * 4;

        buffer[entry]     = allocated ? 0x00 : 0xFF;
        buffer[entry + 1] = allocated ? 0x00 : 0xFF;
    }

    // Catalog chain (T17 S15 -> S1)
    for (sector = kCatalogFirstSector; sector >= 1; sector--)
    {
        size_t  cat = SectorOffset (kVtocTrack, sector);

        if (sector > 1)
        {
            buffer[cat + kCatalogOffNextTrack]  = (Byte) kVtocTrack;
            buffer[cat + kCatalogOffNextSector] = (Byte) (sector - 1);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton::InstallDos
//
//  Boot-payload install; not yet implemented.
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
//  HELLO greeting writer; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33FileWriter::WriteHello (vector<Byte> & buffer)
{
    UNREFERENCED_PARAMETER (buffer);

    return E_NOTIMPL;
}
