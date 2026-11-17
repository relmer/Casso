#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout
//
//  The sectors of a DOS 3.3 volume inside another, such as one of an 800K
//  UniDOS, AmDOS or OzDOS disk's two volumes, or a DOS Master partition,
//  from Beneath Apple DOS: the DOS image on tracks 0 to 2, the VTOC at track
//  17, sector 0, with the catalog's first sector and the free sector bitmap,
//  the catalog's chain of sectors of seven entries each, and each file's
//  chain of track and sector lists. The volume's tracks and sectors a track
//  are the VTOC's.
//
//  The map's units are sectors in track and sector order: unit = track *
//  sectors a track + sector. A field's link is a unit.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33SectorLayout
{
public:
    static constexpr uint32_t  kSectorBytes = 256;
    static constexpr int       kVtocTrack   = 17;

    //  The volume's tracks and sectors a track, from its VTOC, or false when
    //  the VTOC's aren't plausible.
    static bool  TryReadGeometry (std::span<const Byte> sectors, int sectorsPerTrack, int & outTracks, int & outSectors);

    static void  Walk     (std::span<const Byte> sectors, int tracks, int sectorsPerTrack, BlockMap & outMap);
    static void  Annotate (const BlockMap & map, uint32_t unit, int sectorsPerTrack, BlockBytes bytes, vector<BlockField> & outFields);

private:
    static constexpr uint32_t  kNextTrack       = 0x01;
    static constexpr uint32_t  kNextSector      = 0x02;
    static constexpr uint32_t  kDosRelease      = 0x03;
    static constexpr uint32_t  kVolumeNumber    = 0x06;
    static constexpr uint32_t  kMaxPairs        = 0x27;
    static constexpr uint32_t  kLastTrack       = 0x30;
    static constexpr uint32_t  kDirection       = 0x31;
    static constexpr uint32_t  kTracksPerDisk   = 0x34;
    static constexpr uint32_t  kSectorsPerTrack = 0x35;
    static constexpr uint32_t  kBytesPerSector  = 0x36;
    static constexpr uint32_t  kBitmap          = 0x38;
    static constexpr uint32_t  kBitmapBytes     = 4;
    static constexpr uint32_t  kFirstEntry      = 0x0B;
    static constexpr uint32_t  kEntryBytes      = 0x23;
    static constexpr uint32_t  kEntries         = 7;
    static constexpr uint32_t  kEntryType       = 0x02;
    static constexpr uint32_t  kEntryName       = 0x03;
    static constexpr uint32_t  kNameBytes       = 30;
    static constexpr uint32_t  kEntryLength     = 0x21;
    static constexpr uint32_t  kSectorOffset    = 0x05;
    static constexpr uint32_t  kFirstPair       = 0x0C;
    static constexpr uint32_t  kPairs           = 122;
    static constexpr Byte      kDeleted         = 0xFF;
    static constexpr int       kDosTracks       = 3;

    struct Walker
    {
        std::span<const Byte>  sectors;
        int                    tracks          = 0;
        int                    sectorsPerTrack = 0;
        BlockMap             & map;
    };

    static bool          IsInside    (const Walker & walker, int track, int sector);
    static uint32_t      GetUnit     (const Walker & walker, int track, int sector) { return (uint32_t) (track * walker.sectorsPerTrack + sector); }
    static const Byte  * GetSector   (const Walker & walker, uint32_t unit) { return walker.sectors.data() + (size_t) unit * kSectorBytes; }
    static void          Mark        (Walker & walker, uint32_t unit, SectorRole role, BlockStructure structure, int owner, uint32_t node = 0);
    static void          WalkCatalog (Walker & walker, int track, int sector);
    static void          WalkFile    (Walker & walker, int file, int track, int sector);
    static void          ReadBitmap  (Walker & walker);

    static void  AnnotateVtoc    (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list);
    static void  AnnotateCatalog (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list);
    static void  AnnotateTsList  (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list);
    static void  AddTrackSector  (const BlockMap & map, int sectorsPerTrack, uint32_t at, const std::wstring & label, BlockFieldList & list);
};
