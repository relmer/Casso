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
//  Copies tracks 0-2 verbatim from the System Master sector image -- the
//  same bytes DOS's own INIT lays down. The skeleton's free bitmap already
//  reserves those tracks, so the VTOC stays honest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Skeleton::InstallDos (vector<Byte> & buffer, const vector<Byte> & masterSectors)
{
    HRESULT  hr          = S_OK;
    size_t   bufferBytes = buffer.size();
    size_t   masterBytes = masterSectors.size();
    size_t   dosBytes    = SectorOffset (kDosImageTracks, 0);



    CBRA (bufferBytes == (size_t) NibblizationLayer::kImageByteSize);
    CBRAEx (masterBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    std::copy (masterSectors.begin(), masterSectors.begin() + dosBytes, buffer.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileWriter::WriteHello
//
//  The minimal greeting a bootable disk needs: an Applesoft HELLO holding
//  one REM line, so the master's boot-time RUN HELLO lands at a clean
//  prompt instead of FILE NOT FOUND. One catalog entry (first slot of
//  T17 S15), one track/sector list, one data sector; both sectors come off
//  track 18 (the VTOC allocation hint) and leave the free bitmap honest.
//
//  On-disk Applesoft format: two length bytes, then the tokenized program.
//  "10 REM" tokenizes to next-line pointer $0807, line number 10, the REM
//  token, an end-of-line zero, and the end-of-program zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33FileWriter::WriteHello (vector<Byte> & buffer)
{
    constexpr int   kFileTrack      = 18;
    constexpr int   kTsListSector   = 15;
    constexpr int   kDataSector     = 14;
    constexpr Byte  kTypeApplesoft  = 0x02;
    constexpr int   kCatalogEntry   = 0x0B;   // first entry in a catalog sector
    constexpr int   kNameBytes      = 30;

    HRESULT  hr          = S_OK;
    size_t   bufferBytes = buffer.size();
    size_t   catalog     = 0;
    size_t   tsList      = 0;
    size_t   data        = 0;
    size_t   bitmap      = 0;
    size_t   i           = 0;

    //  "HELLO" in high ASCII, padded with high-ASCII spaces below.
    const Byte  kName[5] = { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF };

    //  10 REM, tokenized, behind the two on-disk length bytes.
    const Byte  kProgram[10] = { 0x08, 0x00, 0x07, 0x08, 0x0A, 0x00, 0xB2, 0x00, 0x00, 0x00 };



    CBRA (bufferBytes == (size_t) NibblizationLayer::kImageByteSize);

    catalog = Dos33Skeleton::SectorOffset (Dos33Skeleton::kVtocTrack,
                                           Dos33Skeleton::kCatalogFirstSector);
    tsList  = Dos33Skeleton::SectorOffset (kFileTrack, kTsListSector);
    data    = Dos33Skeleton::SectorOffset (kFileTrack, kDataSector);
    bitmap  = Dos33Skeleton::SectorOffset (Dos33Skeleton::kVtocTrack, 0)
            + (size_t) Dos33Skeleton::kVtocOffFreeBitmap + (size_t) kFileTrack * 4;

    // Catalog entry: TS-list location, type, padded name, sector count 2.
    buffer[catalog + kCatalogEntry + 0x00] = (Byte) kFileTrack;
    buffer[catalog + kCatalogEntry + 0x01] = (Byte) kTsListSector;
    buffer[catalog + kCatalogEntry + 0x02] = kTypeApplesoft;

    for (i = 0; i < kNameBytes; i++)
    {
        buffer[catalog + kCatalogEntry + 0x03 + i] =
            (i < sizeof (kName)) ? kName[i] : (Byte) 0xA0;
    }

    buffer[catalog + kCatalogEntry + 0x21] = 2;
    buffer[catalog + kCatalogEntry + 0x22] = 0;

    // Track/sector list: no continuation, first data pair at 0x0C.
    buffer[tsList + 0x0C] = (Byte) kFileTrack;
    buffer[tsList + 0x0D] = (Byte) kDataSector;

    // Data sector: the tokenized program.
    for (i = 0; i < sizeof (kProgram); i++)
    {
        buffer[data + i] = kProgram[i];
    }

    // Free bitmap: sectors 15 and 14 of the file's track leave the pool
    // (byte 0 holds sectors 15..8 in bits 7..0).
    buffer[bitmap] = (Byte) (buffer[bitmap] & 0x3F);

Error:
    return hr;
}
