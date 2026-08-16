#pragma once

#include "Pch.h"

#include "IVolume.h"
#include "ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume
//
//  A DOS 3.3 filesystem over a flat sector buffer.
//
//  Nothing like this existed: the tree had a ProDOS reader and most of a ProDOS
//  writer, and on the DOS 3.3 side only a routine that emits one hardcoded
//  greeting file. Extraction is the first thing a developer migrating off Apple
//  II disks needs, so this is the reader that story rests on.
//
//  The structures are simple by comparison -- a flat catalog chained through
//  track 17, a track/sector list per file, a free bitmap in the VTOC, no
//  subdirectories and no tree. Geometry constants come from Dos33Skeleton
//  rather than being restated here, so the format is described in one place.
//
//  Addressing for the integrity pass is unit = track * 16 + sector, giving 560
//  units for a 35-track disk.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33Volume : public IVolume
{
public:
    //  Holds the buffer by reference and never modifies it. Every mutating
    //  operation produces a complete new buffer instead.
    explicit Dos33Volume (const vector<Byte> & sectors);

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    //  DOS 3.3 file types, as stored in the catalog's type byte below the lock
    //  bit. Exposed because callers name types on the command line.
    static constexpr Byte  kTypeText        = 0x00;
    static constexpr Byte  kTypeInteger     = 0x01;
    static constexpr Byte  kTypeApplesoft   = 0x02;
    static constexpr Byte  kTypeBinary      = 0x04;
    static constexpr Byte  kTypeRelocatable = 0x10;

    //  Set in the type byte when the file is locked. Placement over a locked
    //  file is refused, matching how the guest behaves.
    static constexpr Byte  kLockedBit       = 0x80;

    static constexpr uint32_t  kUnitCount = 560;   // 35 tracks x 16 sectors

private:
    //  One catalog record, plus where it was found so a rewrite can address it.
    struct RawEntry
    {
        int       catalogTrack  = 0;
        int       catalogSector = 0;
        size_t    entryOffset   = 0;
        int       tsListTrack   = 0;
        int       tsListSector  = 0;
        Byte      typeByte      = 0;
        uint16_t  sectorCount   = 0;
        bool      isDeleted     = false;
        string    name;
    };

    //  Catalog geometry. The VTOC's own offsets live in Dos33Skeleton.
    static constexpr size_t  kEntryBase        = 0x0B;
    static constexpr size_t  kEntrySize        = 0x23;
    static constexpr int     kEntriesPerSector = 7;
    static constexpr size_t  kNameBytes        = 30;

    static constexpr size_t  kEntOffTsTrack     = 0x00;
    static constexpr size_t  kEntOffTsSector    = 0x01;
    static constexpr size_t  kEntOffType        = 0x02;
    static constexpr size_t  kEntOffName        = 0x03;
    static constexpr size_t  kEntOffSectorCount = 0x21;

    //  A track byte of $FF marks a deleted entry; $00 means the catalog has no
    //  further entries at all.
    static constexpr Byte    kEntryDeleted = 0xFF;
    static constexpr Byte    kEntryUnused  = 0x00;

    //  Track/sector list geometry.
    static constexpr size_t  kTsOffNextTrack  = 0x01;
    static constexpr size_t  kTsOffNextSector = 0x02;
    static constexpr size_t  kTsOffFirstPair  = 0x0C;
    static constexpr int     kTsPairsPerList  = 122;

    //  Payload headers DOS 3.3 stores inside the file's own data.
    static constexpr size_t  kBinaryHeaderBytes    = 4;   // load address + length
    static constexpr size_t  kTokenizedHeaderBytes = 2;   // length only

    static uint32_t  ToUnit (int track, int sector);

    Byte  ReadByte (int track, int sector, size_t offset) const;

    //  Walks the catalog chain. Damage is appended rather than thrown, and
    //  `outFullyParsed` reports whether anything had to be skipped.
    void  CollectEntries (vector<RawEntry>    & outEntries,
                          vector<std::string> & outDamage,
                          bool                & outFullyParsed) const;

    //  Gathers a file's data sectors by walking its track/sector list under a
    //  guard, so a corrupted chain cannot loop forever.
    bool  CollectDataSectors (const RawEntry           & entry,
                              vector<uint32_t>         & outUnits,
                              ChainWalkGuard           & guard) const;

    //  Strips whatever header the file type carries, and reports the load
    //  address when the type records one.
    static void  ApplyTypeHeader (Byte typeByte, vector<Byte> & inOutBytes, FilePayload & outPayload);

    const vector<Byte> &  m_sectors;
};
