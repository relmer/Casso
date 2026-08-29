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

    //  Delete, plus an account of what it freed, what it refused to free, and
    //  the conditions that bound the answer. The interface form forwards here
    //  and throws the account away; anything reporting to a user wants it.
    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    //  The self-check every computed write and delete runs over its own output,
    //  and the ONLY way a computed buffer reaches a caller. Refuses a buffer
    //  that disagrees with itself in a way the buffer it was computed from did
    //  not, and hands it over otherwise.
    //
    //  The check and the hand-off are one operation on purpose. A check sitting
    //  BESIDE the assignment can be deleted while the assignment stays, and
    //  nothing observable changes -- the suite cannot catch that, because no
    //  input makes correct code produce a bad buffer. Routing the hand-off
    //  through the check means removing it cannot be silent.
    //
    //  A refusal means OUR writer produced a bad image, not that the user asked
    //  for something impossible, so it answers E_UNEXPECTED and asserts -- the
    //  one condition in this layer that is a defect rather than a verdict.
    //
    //  Public because a refusal is only reachable by handing it a result that is
    //  deliberately wrong.
    static HRESULT  HandBackVerifiedResult (const VolumeIntegrityReport  & before,
                                            const vector<Byte>           & result,
                                            vector<Byte>                 & outBuffer);

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

    //  Where DOS stashes the original track byte when it tombstones an entry.
    static constexpr size_t  kEntOffLastNameByte = 0x20;

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

    //  Offset in the list of the first data sector it describes, so a walker
    //  that joins the chain partway knows where it is.
    static constexpr size_t  kTsOffSectorOffsetLo = 0x05;
    static constexpr size_t  kTsOffSectorOffsetHi = 0x06;

    //  Free bitmap: four bytes per track, of which two are used. Byte 0 holds
    //  sectors 15..8 in bits 7..0 and byte 1 holds sectors 7..0; a SET bit
    //  means free.
    static constexpr size_t  kBitmapBytesPerTrack = 4;
    static constexpr int     kBitmapSplitSector   = 8;

    //  High-ASCII space, which is what a short name is padded out with.
    static constexpr Byte    kNamePad = 0xA0;

    //  Where the name of the program a booted DOS runs is kept: inside the DOS
    //  image on track 1, NOT in the catalog. Thirty bytes of high ASCII padded
    //  with high-ASCII spaces, the same width a catalog name field has.
    //
    //  Measured rather than recalled. Scanning the stock System Master for the
    //  high-ASCII spelling of its greeting name finds exactly two occurrences on
    //  the whole disk: this one and the catalog entry naming the file. The
    //  field's own end is not visible in the data -- the bytes after it are pad
    //  bytes too -- so the width comes from the catalog name field, which the
    //  DOS code reading it shares.
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;

    static uint32_t  ToUnit    (int track, int sector);
    static int       GetTrack  (uint32_t unit);
    static int       GetSector (uint32_t unit);

    //  Order in which free sectors are handed out: outward from the catalog
    //  track first, then inward, which is the order DOS's own allocation hint
    //  describes. Never yields the catalog track.
    static int       GetAllocationTrack (int index);

    //  Credits the volume's own structures -- the tracks DOS occupies and the
    //  catalog track -- to the reserved volume owner. No catalog entry names
    //  them, and a report that left them unowned would call every healthy
    //  volume inconsistent.
    static void  ClaimVolumeStructures  (VolumeIntegrityReport & outReport);

    static bool  IsFreeInBitmap  (const vector<Byte> & buffer, int track, int sector);
    static void  SetFreeInBitmap (vector<Byte> & buffer, int track, int sector, bool isFree);
    static void  WriteByteAt     (vector<Byte> & buffer, int track, int sector, size_t offset, Byte value);

    //  30 bytes of high ASCII, space-padded. False when the name is one DOS
    //  could not open again.
    static bool  TryEncodeCatalogName (const std::string & name, vector<Byte> & outBytes);

    //  Rebuilds whatever header the type stores inside the file's own data --
    //  the exact inverse of ApplyTypeHeader, so a read followed by a write
    //  reproduces the stored form.
    static HRESULT  ComposeStoredBytes (const FilePayload & payload, vector<Byte> & outStored);

    static void  ZeroUnits        (vector<Byte> & buffer, const vector<uint32_t> & units);
    static void  PlaceFile        (vector<Byte>            & buffer,
                                   const vector<uint32_t>  & units,
                                   size_t                    dataSectorCount,
                                   const vector<Byte>      & stored);
    static void  WriteCatalogEntry (vector<Byte>        & buffer,
                                    int                   catalogTrack,
                                    int                   catalogSector,
                                    size_t                entryOffset,
                                    uint32_t              listUnit,
                                    Byte                  typeByte,
                                    uint16_t              sectorCount,
                                    const vector<Byte>  & nameBytes);

    Byte  ReadByte (int track, int sector, size_t offset) const;

    //  Whether the tracks DOS occupies hold anything at all. A volume formatted
    //  without an operating system reserves them and leaves them blank, and
    //  patching a greeting name into blank space would report success for
    //  something no boot could ever read.
    bool  HasDosImage() const;

    //  Case-insensitive, because the catalog is upper case and the caller's
    //  shell is not.
    static bool  TryFindEntry (const vector<RawEntry>  & entries,
                               const std::string       & leaf,
                               uint16_t                & outOwner);

    //  A catalog slot is reusable only when its track byte says never-used or
    //  deleted. AN ENTRY OCCUPYING NO SECTORS IS STILL AN ENTRY -- twenty of
    //  the sixty-three on Merlin's own disk are exactly that, drawing headings
    //  in CATALOG -- so the sector count must not be consulted here.
    bool  TryFindFreeCatalogSlot (int & outTrack, int & outSector, size_t & outOffset) const;

    //  Hands out `count` sectors, or none at all.
    bool  TryAllocateUnits (const VolumeIntegrityReport  & report,
                            size_t                         count,
                            vector<uint32_t>             & outUnits) const;

    //  Lays down a file whose name the catalog does not already hold, over the
    //  buffer this volume was constructed with. Replacement stages the removal
    //  into a working buffer and calls this on a volume over that, so one code
    //  path places every file and a replacement cannot half-happen.
    HRESULT  AddFile (const vector<Byte>  & nameBytes,
                           Byte                  typeByte,
                           const vector<Byte>  & stored,
                           vector<Byte>        & outBuffer) const;

    //  Turns what a delete decided into text a user can act on.
    static void  AppendDeleteWarnings (DeleteOutcome & inOutOutcome);

    //  Walks the catalog chain. Damage is appended rather than thrown, and
    //  `outFullyParsed` reports whether anything had to be skipped.
    void  CollectEntries (vector<RawEntry>    & outEntries,
                          vector<std::string> & outDamage,
                          bool                & outFullyParsed) const;

    //  Gathers a file's data sectors by walking its track/sector list under a
    //  guard, so a corrupted chain cannot loop forever. The list sectors come
    //  back separately: a reader wants only the data, while everything that
    //  accounts for space -- the integrity pass, and through it delete -- needs
    //  the lists too, since they are as much the file's footprint as its bytes.
    bool  CollectDataSectors (const RawEntry           & entry,
                              vector<uint32_t>         & outUnits,
                              vector<uint32_t>         & outListUnits,
                              ChainWalkGuard           & guard) const;

    //  Strips whatever header the file type carries, and reports the load
    //  address when the type records one.
    static void  ApplyTypeHeader (Byte typeByte, vector<Byte> & inOutBytes, FilePayload & outPayload);

    const vector<Byte> &  m_sectors;
};
