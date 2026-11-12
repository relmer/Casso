#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/Dos33SectorLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::TryReadGeometry
//
//  The VTOC is at track 17, sector 0 of a volume with `sectorsPerTrack`
//  sectors a track, the number the partition's layout has.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33SectorLayout::TryReadGeometry (std::span<const Byte> sectors, int sectorsPerTrack, int & outTracks, int & outSectors)
{
    constexpr int  kMinTracks = 18;
    constexpr int  kMaxTracks = 50;



    size_t        at   = (size_t) kVtocTrack * sectorsPerTrack * kSectorBytes;
    const Byte  * vtoc = nullptr;



    if (sectorsPerTrack <= 0 || at + kSectorBytes > sectors.size())
    {
        return false;
    }

    vtoc       = sectors.data() + at;
    outTracks  = vtoc[kTracksPerDisk];
    outSectors = vtoc[kSectorsPerTrack];

    return outTracks >= kMinTracks && outTracks <= kMaxTracks && outSectors == sectorsPerTrack
        && (size_t) outTracks * outSectors * kSectorBytes <= sectors.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33SectorLayout::IsInside (const Walker & walker, int track, int sector)
{
    return track > 0 && track < walker.tracks && sector >= 0 && sector < walker.sectorsPerTrack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::Mark
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::Mark (
    Walker          & walker,
    uint32_t          unit,
    SectorRole        role,
    BlockStructure    structure,
    int               owner,
    uint32_t          node)
{
    BlockMapEntry  * entry = nullptr;



    if (unit >= walker.map.GetBlockCount())
    {
        return;
    }

    entry = &walker.map.blocks[unit];

    if (entry->structure == BlockStructure::None)
    {
        entry->role      = role;
        entry->structure = structure;
        entry->node      = node;
    }

    if (owner >= 0 && std::find (entry->owners.begin(), entry->owners.end(), owner) == entry->owners.end())
    {
        entry->owners.push_back (owner);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::Walk
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::Walk (std::span<const Byte> sectors, int tracks, int sectorsPerTrack, BlockMap & outMap)
{
    Walker        walker { sectors, tracks, sectorsPerTrack, outMap };
    uint32_t      vtoc   = (uint32_t) (kVtocTrack * sectorsPerTrack);
    const Byte  * bytes  = nullptr;
    int           track  = 0;
    int           sector = 0;



    outMap            = BlockMap();
    outMap.fileSystem = BlockFileSystem::Dos33;
    outMap.hasFreeMap = true;
    outMap.blocks.assign ((size_t) tracks * sectorsPerTrack, BlockMapEntry());

    bytes = GetSector (walker, vtoc);

    outMap.volumeName = L"DOS 3.3 volume " + std::to_wstring (bytes[kVolumeNumber]);

    Mark (walker, vtoc, SectorRole::VtocOrKeyBlock, BlockStructure::Dos33Vtoc, -1);

    for (track = 0; track < kDosTracks && track < tracks; track++)
    {
        for (sector = 0; sector < sectorsPerTrack; sector++)
        {
            Mark (walker, GetUnit (walker, track, sector), SectorRole::BootArea, BlockStructure::BootLoader, -1, (uint32_t) track);
        }
    }

    WalkCatalog (walker, bytes[kNextTrack], bytes[kNextSector]);
    ReadBitmap  (walker);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::WalkCatalog
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::WalkCatalog (Walker & walker, int track, int sector)
{
    std::set<uint32_t>  visited;
    uint32_t            unit   = 0;
    const Byte        * bytes  = nullptr;
    const Byte        * entry  = nullptr;
    uint32_t            i      = 0;
    uint32_t            k      = 0;
    std::wstring        name;
    int                 file   = 0;



    while (IsInside (walker, track, sector))
    {
        unit = GetUnit (walker, track, sector);

        if (!visited.insert (unit).second)
        {
            walker.map.isCatalogComplete = false;
            break;
        }

        Mark (walker, unit, SectorRole::CatalogOrDirectory, BlockStructure::Dos33Catalog, -1);

        bytes = GetSector (walker, unit);

        for (i = 0; i < kEntries; i++)
        {
            entry = bytes + kFirstEntry + i * kEntryBytes;

            if (entry[0] == 0 || entry[0] == kDeleted)
            {
                continue;
            }

            name.clear();

            for (k = 0; k < kNameBytes; k++)
            {
                name += (wchar_t) (entry[kEntryName + k] & 0x7F);
            }

            name.erase (name.find_last_not_of (L' ') + 1);

            file = (int) walker.map.files.size();
            walker.map.files.push_back (name);

            WalkFile (walker, file, entry[0], entry[1]);
        }

        track  = bytes[kNextTrack];
        sector = bytes[kNextSector];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::WalkFile
//
//  The chain of track and sector lists, and each data sector they list,
//  numbered in the file by the list's sector offset.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::WalkFile (Walker & walker, int file, int track, int sector)
{
    std::set<uint32_t>  visited;
    uint32_t            unit   = 0;
    const Byte        * list   = nullptr;
    uint32_t            offset = 0;
    uint32_t            i      = 0;



    while (IsInside (walker, track, sector))
    {
        unit = GetUnit (walker, track, sector);

        if (!visited.insert (unit).second)
        {
            break;
        }

        Mark (walker, unit, SectorRole::IndexBlock, BlockStructure::Dos33TsList, file);

        list   = GetSector (walker, unit);
        offset = (uint32_t) (list[kSectorOffset] | (list[kSectorOffset + 1] << 8));

        for (i = 0; i < kPairs; i++)
        {
            if (IsInside (walker, list[kFirstPair + 2 * i], list[kFirstPair + 2 * i + 1]))
            {
                Mark (walker, GetUnit (walker, list[kFirstPair + 2 * i], list[kFirstPair + 2 * i + 1]), SectorRole::FileData, BlockStructure::FileData, file, offset + i);
            }
        }

        track  = list[kNextTrack];
        sector = list[kNextSector];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::ReadBitmap
//
//  Four bytes a track, a set bit a free sector, the highest sector in the
//  first byte's high bit. A sector in two files is cross-linked; a sector
//  the bitmap marks used with no file is allocated but unowned; a file's
//  sector the bitmap marks free is owned but marked free.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::ReadBitmap (Walker & walker)
{
    constexpr int  kBitsPerByte = 8;



    const Byte  * vtoc   = GetSector (walker, (uint32_t) (kVtocTrack * walker.sectorsPerTrack));
    int           bytes  = walker.sectorsPerTrack / kBitsPerByte;
    int           track  = 0;
    int           sector = 0;
    Byte          value  = 0;
    bool          isFree = false;



    for (track = 0; track < walker.tracks; track++)
    {
        for (sector = 0; sector < walker.sectorsPerTrack; sector++)
        {
            BlockMapEntry  & entry = walker.map.blocks[GetUnit (walker, track, sector)];

            value  = vtoc[kBitmap + track * kBitmapBytes + (bytes - 1 - sector / kBitsPerByte)];
            isFree = (value & (1 << (sector % kBitsPerByte))) != 0;

            if (entry.owners.size() > 1)
            {
                entry.role = SectorRole::CrossLinked;
            }
            else if (entry.structure == BlockStructure::None)
            {
                entry.role = isFree ? SectorRole::Free : SectorRole::AllocatedUnowned;
            }
            else if (isFree && !entry.owners.empty())
            {
                entry.role = SectorRole::OwnedMarkedFree;
            }
            else if (isFree && entry.structure == BlockStructure::BootLoader)
            {
                entry.role      = SectorRole::Free;
                entry.structure = BlockStructure::None;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::AddTrackSector
//
//  A track and sector pair, linked to its unit when the track isn't zero.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::AddTrackSector (const BlockMap & map, int sectorsPerTrack, uint32_t at, const std::wstring & label, BlockFieldList & list)
{
    Byte      track  = list.GetByte (at);
    Byte      sector = list.GetByte (at + 1);
    uint32_t  unit   = (uint32_t) (track * sectorsPerTrack + sector);
    int64_t   link   = (track != 0 && sector < sectorsPerTrack && unit < map.GetBlockCount()) ? (int64_t) unit : BlockField::kNoLink;



    list.AddBigLink (at, 2, label, link);
    list.SetLastValue (L"T" + std::to_wstring (track) + L" S" + std::to_wstring (sector));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::Annotate
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::Annotate (const BlockMap & map, uint32_t unit, int sectorsPerTrack, BlockBytes bytes, vector<BlockField> & outFields)
{
    BlockFieldList         list (bytes, outFields);
    const BlockMapEntry  & entry = map.blocks[unit];
    std::wstring           owner = entry.owners.empty() ? std::wstring() : map.files[entry.owners[0]];



    switch (entry.structure)
    {
        case BlockStructure::BootLoader:
            list.AddRun (0, kSectorBytes, L"DOS image, track " + std::to_wstring (entry.node));
            break;

        case BlockStructure::Dos33Vtoc:
            AnnotateVtoc (map, sectorsPerTrack, list);
            break;

        case BlockStructure::Dos33Catalog:
            AnnotateCatalog (map, sectorsPerTrack, list);
            break;

        case BlockStructure::Dos33TsList:
            AnnotateTsList (map, sectorsPerTrack, list);
            break;

        case BlockStructure::FileData:
            list.AddRun (0, kSectorBytes, L"Data, sector " + std::to_wstring (entry.node) + L" of the file", owner);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::AnnotateVtoc
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::AnnotateVtoc (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list)
{
    int  tracks = list.GetByte (kTracksPerDisk);
    int  track  = 0;



    AddTrackSector (map, sectorsPerTrack, kNextTrack, L"VTOC: first catalog sector", list);

    list.AddByte (kDosRelease,      L"VTOC: DOS release");
    list.AddByte (kVolumeNumber,    L"VTOC: volume number");
    list.AddByte (kMaxPairs,        L"VTOC: track and sector pairs a list");
    list.AddByte (kLastTrack,       L"VTOC: last track allocated");
    list.AddByte (kDirection,       L"VTOC: allocation direction");
    list.AddByte (kTracksPerDisk,   L"VTOC: tracks");
    list.AddByte (kSectorsPerTrack, L"VTOC: sectors a track");
    list.AddWord (kBytesPerSector,  L"VTOC: bytes a sector");

    for (track = 0; track < tracks && kBitmap + (track + 1) * kBitmapBytes <= kSectorBytes; track++)
    {
        list.AddRun (kBitmap + track * kBitmapBytes, kBitmapBytes, L"VTOC: free sectors on track " + std::to_wstring (track));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::AnnotateCatalog
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::AnnotateCatalog (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list)
{
    uint32_t      i    = 0;
    uint32_t      at   = 0;
    std::wstring  head;



    AddTrackSector (map, sectorsPerTrack, kNextTrack, L"Next catalog sector", list);

    for (i = 0; i < kEntries; i++)
    {
        at = kFirstEntry + i * kEntryBytes;

        if (list.GetByte (at) == 0)
        {
            continue;
        }

        head = L"Entry " + std::to_wstring (i + 1) + (list.GetByte (at) == kDeleted ? L" (deleted): " : L": ");

        AddTrackSector (map, sectorsPerTrack, at, head + L"track and sector list", list);

        list.AddByte (at + kEntryType,   head + L"file type and lock");
        list.AddName (at + kEntryName,   kNameBytes, head + L"name", true);
        list.AddWord (at + kEntryLength, head + L"sectors");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorLayout::AnnotateTsList
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorLayout::AnnotateTsList (const BlockMap & map, int sectorsPerTrack, BlockFieldList & list)
{
    uint32_t  offset = list.GetWord (kSectorOffset);
    uint32_t  i      = 0;



    AddTrackSector (map, sectorsPerTrack, kNextTrack, L"Next track and sector list", list);

    list.AddWord (kSectorOffset, L"Sector offset in the file");

    for (i = 0; i < kPairs; i++)
    {
        if (list.GetByte (kFirstPair + 2 * i) == 0)
        {
            continue;
        }

        AddTrackSector (map, sectorsPerTrack, kFirstPair + 2 * i, L"Data sector " + std::to_wstring (offset + i), list);
    }
}
