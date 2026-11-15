#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





static constexpr int   s_kTracks         = SectorSource::kTracks;
static constexpr int   s_kSectors        = SectorSource::kSectors;
static constexpr int   s_kBytes          = SectorSource::kBytes;
static constexpr int   s_kBlocks         = SectorSource::kBlocks;
static constexpr int   s_kBootTracks     = 3;
static constexpr int   s_kCatalogTrack   = 0x01;
static constexpr int   s_kCatalogSector  = 0x02;
static constexpr int   s_kBitmap         = 0x38;
static constexpr int   s_kBitmapBytes    = 4;
static constexpr int   s_kFirstEntry     = 0x0B;
static constexpr int   s_kEntrySize      = 0x23;
static constexpr int   s_kEntries        = 7;
static constexpr Byte  s_kDeleted        = 0xFF;
static constexpr Byte  s_kUp             = 0x01;
static constexpr Byte  s_kDown           = 0xFF;
static constexpr int   s_kKeyBlock       = 2;
static constexpr int   s_kBitmapPointer  = 0x27;
static constexpr int   s_kBootBlocks     = 2;
static constexpr int   s_kBitsPerByte    = 8;
static constexpr Byte  s_kFirstBlockBit  = 0x80;
static constexpr int   s_kHighestVolume  = 0xFE;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::Plan
//
//  Repair is offered only for one volume of a standard file system whose
//  verdict is Bad or Suspect and which has changes to make. The plan for a
//  volume that can't be repaired has its reason and no changes.
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan DiskRepair::Plan (const DiskAnalysis & analysis, const RepairOptions & options)
{
    RepairPlan                     plan;
    Work                           work    { analysis, options, {}, {}, {}, 0, plan };
    SectorSource                   source  (analysis);
    std::unique_ptr<SectorSource>  patched;
    FileMap                        after;
    int                            t       = 0;
    int                            s       = 0;
    int                            volume  = analysis.summary.commonVolume;



    plan.before = analysis.verdict;
    work.volume = (volume >= 1 && volume <= s_kHighestVolume) ? volume : Dos33MapReader::kStandardVolume;
    work.sectors.assign (static_cast<size_t> (s_kTracks * s_kSectors * s_kBytes), 0);
    work.isReadable.assign (static_cast<size_t> (s_kTracks * s_kSectors), false);

    for (t = 0; t < s_kTracks; t++)
    {
        for (s = 0; s < s_kSectors; s++)
        {
            const SectorSource::Sector &  sector = source.GetDos33 (t, s);

            if (SectorSource::IsReadable (sector.result) && sector.bytes != nullptr)
            {
                std::copy (sector.bytes, sector.bytes + s_kBytes, work.sectors.begin() + (t * s_kSectors + s) * s_kBytes);
                work.isReadable[t * s_kSectors + s] = true;
            }
        }
    }

    work.original = work.sectors;

    if (analysis.verdict == FileSystemVerdict::None)
    {
        plan.reason = L"The disk has no standard file system, so it isn't repaired";
    }
    else if (analysis.fileMaps.size() != 1)
    {
        plan.reason = L"The disk has more than one volume, so it isn't repaired";
    }
    else if (analysis.verdict == FileSystemVerdict::Ok)
    {
        plan.reason = L"The file system has no damage to repair";
    }
    else
    {
        switch (analysis.fileMaps[0].fileSystem)
        {
            case MapFileSystem::Dos33:  PlanDos33  (work, analysis.fileMaps[0]); break;
            case MapFileSystem::ProDos: PlanProDos (work, analysis.fileMaps[0]); break;

            case MapFileSystem::Pascal:
                CheckChains (work, analysis.fileMaps[0]);

                if (plan.reason.empty())
                {
                    PlanCounts (work, analysis.fileMaps[0]);
                }

                break;

            default:
                plan.reason = L"Repair is for DOS 3.3, ProDOS and Apple Pascal volumes";
                break;
        }
    }

    CollectSectors (work);

    if (plan.reason.empty() && plan.changes.empty())
    {
        plan.reason = L"None of the findings can be repaired by changing the bookkeeping";
    }

    if (plan.reason.empty() && BuildMap (work, patched, after))
    {
        plan.after = after.verdict;
    }

    plan.canRepair = plan.reason.empty();

    if (!plan.canRepair)
    {
        plan.changes.clear();
        plan.sectors.clear();
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::Apply
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::Apply (const RepairPlan & plan, vector<Byte> & inOutSectors)
{
    for (const RepairSector & sector : plan.sectors)
    {
        size_t  at = static_cast<size_t> ((sector.track * s_kSectors + sector.sector) * s_kBytes);

        if (at + s_kBytes <= inOutSectors.size())
        {
            std::copy (sector.bytes.begin(), sector.bytes.end(), inOutSectors.begin() + static_cast<ptrdiff_t> (at));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanDos33
//
//  The header first, then the catalog relinked; the volume is read again
//  through both, and its counts and bitmap set from the files it then has.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanDos33 (Work & work, const FileMap & map)
{
    std::unique_ptr<SectorSource>  source;
    FileMap                        relinked;
    bool                           isRead   = false;



    if (!map.isCatalogStandard)
    {
        work.plan.reason = L"The catalog isn't standard, so it isn't repaired";
    }

    if (work.plan.reason.empty())
    {
        PlanHeader (work, map);
        PlanRelink (work, map);
    }

    if (work.plan.reason.empty())
    {
        isRead = BuildMap (work, source, relinked) && relinked.fileSystem == MapFileSystem::Dos33;

        if (!isRead)
        {
            work.plan.reason = L"The volume can't be read once its catalog is relinked, so it isn't repaired";
        }
    }

    if (work.plan.reason.empty())
    {
        CheckChains (work, relinked);
    }

    if (work.plan.reason.empty())
    {
        PlanCounts      (work, relinked);
        PlanDos33Bitmap (work, relinked);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanHeader
//
//  A lost header is set to DOS's whole; otherwise only a next
//  track to allocate that is off the disk, or a direction that isn't up or
//  down, is set to track 17 moving up.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanHeader (Work & work, const FileMap & map)
{
    Byte *  vtoc = GetDos33 (work, Dos33MapReader::kVtocTrack, 0);



    if (map.isHeaderLost)
    {
        Dos33MapReader::MakeStandardHeader (static_cast<Byte> (work.volume), vtoc);
        work.plan.changes.push_back (L"Set the VTOC's header to DOS 3.3's values");
    }
    else if (vtoc[Dos33MapReader::kNextTrack] >= s_kTracks || (vtoc[Dos33MapReader::kDirection] != s_kUp && vtoc[Dos33MapReader::kDirection] != s_kDown))
    {
        vtoc[Dos33MapReader::kNextTrack] = static_cast<Byte> (Dos33MapReader::kVtocTrack);
        vtoc[Dos33MapReader::kDirection] = s_kUp;
        work.plan.changes.push_back (L"Set the VTOC's next track to allocate to track 17, moving up");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanRelink
//
//  A catalog whose chain loops, runs off the disk, or ends while a sector
//  below the last has entries in use, is relinked from sector $F down
//  through every believable sector, when at least four of them are linked
//  as DOS's INIT links them. A catalog sector that can't be read stops the
//  repair.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanRelink (Work & work, const FileMap & map)
{
    SectorSource  source   (work.analysis);
    vector<int>   path;
    int           linked   = 0;
    size_t        i        = 0;
    bool          isEarly  = false;
    bool          isPrefix = true;
    int           next     = 0;
    Byte *        vtoc     = GetDos33 (work, Dos33MapReader::kVtocTrack, 0);
    Byte *        at       = nullptr;



    if (!map.isCatalogComplete && map.unreadableCell >= 0 && !IsReadable (work, map.unreadableCell / s_kSectors, map.unreadableCell % s_kSectors))
    {
        work.plan.reason = std::format (L"The catalog's sector at track {}, sector ${:X} can't be read, so it isn't repaired",
                                        map.unreadableCell / s_kSectors, map.unreadableCell % s_kSectors);
        return;
    }

    Dos33MapReader::GetInitCatalog (source, path, linked);

    for (i = 0; i < map.catalogCells.size() && i < path.size(); i++)
    {
        isPrefix = isPrefix && map.catalogCells[i] == Dos33MapReader::GetCell (Dos33MapReader::kVtocTrack, path[i]);
    }

    for (i = map.catalogCells.size(); map.isCatalogComplete && isPrefix && i < path.size(); i++)
    {
        at = GetDos33 (work, Dos33MapReader::kVtocTrack, path[i]);

        for (int e = 0; e < s_kEntries; e++)
        {
            Byte  track = at[s_kFirstEntry + e * s_kEntrySize];

            isEarly = isEarly || (track != 0 && track != s_kDeleted && map.cells[Dos33MapReader::GetCell (Dos33MapReader::kVtocTrack, path[i])].owners.empty());
        }
    }

    if (map.isCatalogComplete && !isEarly)
    {
        return;
    }

    if (linked < Dos33MapReader::kInitLinkedMin || vtoc[s_kCatalogTrack] != Dos33MapReader::kVtocTrack || vtoc[s_kCatalogSector] != Dos33MapReader::kCatalogSector)
    {
        work.plan.reason = L"The catalog can't be relinked: fewer than four of its sectors on track 17 are linked in the order DOS's INIT links them";
        return;
    }

    for (i = 0; i < path.size(); i++)
    {
        at   = GetDos33 (work, Dos33MapReader::kVtocTrack, path[i]);
        next = (i + 1 < path.size()) ? path[i + 1] : -1;

        if (next >= 0 && (at[s_kCatalogTrack] != Dos33MapReader::kVtocTrack || at[s_kCatalogSector] != next))
        {
            at[s_kCatalogTrack]  = static_cast<Byte> (Dos33MapReader::kVtocTrack);
            at[s_kCatalogSector] = static_cast<Byte> (next);
            work.plan.changes.push_back (std::format (L"Link catalog sector ${:X} to sector ${:X}", path[i], next));
        }
        else if (next < 0 && (at[s_kCatalogTrack] != 0 || at[s_kCatalogSector] != 0))
        {
            at[s_kCatalogTrack]  = 0;
            at[s_kCatalogSector] = 0;
            work.plan.changes.push_back (std::format (L"End the catalog at sector ${:X}", path[i]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanDos33Bitmap
//
//  Each sector off tracks 0 to 2 and 17 marked used when a file uses it and
//  free when none does, unless the user turns off freeing them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanDos33Bitmap (Work & work, const FileMap & map)
{
    Byte *       vtoc       = GetDos33 (work, Dos33MapReader::kVtocTrack, 0);
    vector<int>  markedUsed (map.files.size(), 0);
    int          unowned    = 0;
    int          t          = 0;
    int          s          = 0;



    for (t = s_kBootTracks; t < s_kTracks; t++)
    {
        for (s = 0; t != Dos33MapReader::kVtocTrack && s < s_kSectors; s++)
        {
            const MapCell &  cell    = map.cells[Dos33MapReader::GetCell (t, s)];
            Byte &           bits    = vtoc[s_kBitmap + t * s_kBitmapBytes + (s >= s_kBitsPerByte ? 0 : 1)];
            Byte             mask    = static_cast<Byte> (1 << (s % s_kBitsPerByte));
            bool             isUsed  = (bits & mask) == 0;
            bool             isOwned = !cell.owners.empty();

            if (isOwned && !isUsed)
            {
                bits = static_cast<Byte> (bits & ~mask);
                markedUsed[cell.owners[0]]++;
            }
            else if (!isOwned && isUsed && !work.options.isKeepingUnowned)
            {
                bits = static_cast<Byte> (bits | mask);
                unowned++;
            }
        }
    }

    AddBitmapChanges (work, map, markedUsed, unowned, 0, L"sector");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanProDos
//
//  The directory counts, then the bitmap: the boot blocks, the directory and
//  the bitmap itself and every block a file uses marked used, and every
//  other block free unless the user turns off freeing them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanProDos (Work & work, const FileMap & map)
{
    vector<int>  markedUsed (map.files.size(), 0);
    int     unowned     = 0;
    int     structures  = 0;
    int     bitmapAt    = 0;
    int     track       = 0;
    int     sector      = 0;
    int     b           = 0;
    Byte  * bits        = nullptr;
    Byte    mask        = 0;
    bool    isUsed      = false;
    bool    isOwned     = false;
    bool    isStructure = false;



    CheckChains (work, map);

    if (work.plan.reason.empty())
    {
        PlanCounts (work, map);

        bitmapAt = *GetBlockByte (work, s_kKeyBlock, s_kBitmapPointer, track, sector) | (*GetBlockByte (work, s_kKeyBlock, s_kBitmapPointer + 1, track, sector) << s_kBitsPerByte);

        if (bitmapAt <= s_kKeyBlock || bitmapAt >= s_kBlocks)
        {
            work.plan.reason = L"The volume bitmap's block is off the volume, so it isn't repaired";
        }
    }

    for (b = 0; work.plan.reason.empty() && b < s_kBlocks; b++)
    {
        const MapCell &  cell = map.cells[b];

        bits     = GetBlockByte (work, bitmapAt, b / s_kBitsPerByte, track, sector);
        mask     = static_cast<Byte> (s_kFirstBlockBit >> (b % s_kBitsPerByte));
        isUsed   = (*bits & mask) == 0;
        isOwned  = !cell.owners.empty();
        isStructure = b < s_kBootBlocks || cell.role == SectorRole::BootArea || cell.role == SectorRole::VtocOrKeyBlock ||
                   cell.role == SectorRole::CatalogOrDirectory || cell.role == SectorRole::VolumeBitmap;

        if ((isOwned || isStructure) && !isUsed)
        {
            *bits = static_cast<Byte> (*bits & ~mask);

            if (isOwned)
            {
                markedUsed[cell.owners[0]]++;
            }
            else
            {
                structures++;
            }
        }
        else if (!isOwned && !isStructure && isUsed && !work.options.isKeepingUnowned)
        {
            *bits = static_cast<Byte> (*bits | mask);
            unowned++;
        }
    }

    if (work.plan.reason.empty())
    {
        AddBitmapChanges (work, map, markedUsed, unowned, structures, L"block");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::PlanCounts
//
//  Each count an entry or a directory header has that differs, set to the
//  count of the chain or of the entries.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::PlanCounts (Work & work, const FileMap & map)
{
    Byte *  at     = nullptr;
    Byte *  high   = nullptr;
    int     track  = 0;
    int     sector = 0;



    for (const RecordedCount & count : map.counts)
    {
        if (map.fileSystem == MapFileSystem::Dos33)
        {
            at   = GetDos33 (work, count.cell / s_kSectors, count.cell % s_kSectors) + count.offset;
            high = at + 1;
        }
        else
        {
            at   = GetBlockByte (work, count.cell, count.offset, track, sector);
            high = GetBlockByte (work, count.cell, count.offset + 1, track, sector);
        }

        *at   = static_cast<Byte> (count.actual);
        *high = static_cast<Byte> (count.actual >> s_kBitsPerByte);
        work.plan.changes.push_back (std::format (L"Set {} to {}", count.what, count.actual));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::CheckChains
//
//  A chain that loops, runs off the volume, can't be read or shares a
//  sector with another file can't be set right without changing a file's
//  data, so the volume isn't repaired; the reason is the first such
//  finding.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::CheckChains (Work & work, const FileMap & map)
{
    for (const Finding & finding : map.findings)
    {
        bool  isBlocking = finding.kind == FindingKind::FileChainBroken || finding.kind == FindingKind::FileCrossLinked || finding.kind == FindingKind::PascalEntryOutOfRange;

        if (work.plan.reason.empty() && isBlocking)
        {
            work.plan.reason = L"A file's data can't be recovered by changing the bookkeeping. " + TextEncoding::Utf8ToWide (finding.detail) + L".";
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::AddBitmapChanges
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::AddBitmapChanges (Work & work, const FileMap & map, const vector<int> & markedUsed, int unowned, int structures, LPCWSTR unit)
{
    auto  plural = [] (int count) { return (count == 1) ? L"" : L"s"; };



    for (size_t f = 0; f < markedUsed.size(); f++)
    {
        if (markedUsed[f] > 0)
        {
            work.plan.changes.push_back (std::format (L"Mark {} {}{} {} uses as used in the bitmap", markedUsed[f], unit, plural (markedUsed[f]), map.files[f].path));
        }
    }

    if (structures > 0)
    {
        work.plan.changes.push_back (std::format (L"Mark {} boot, directory and bitmap {}{} as used in the bitmap", structures, unit, plural (structures)));
    }

    if (unowned > 0)
    {
        work.plan.changes.push_back (std::format (L"Mark {} {}{} that no file uses as free in the bitmap", unowned, unit, plural (unowned)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::CollectSectors
//
//  Each sector the plan changed, whole; a change to a sector that couldn't
//  be read stops the repair, since its other bytes aren't known.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::CollectSectors (Work & work)
{
    RepairSector  out;
    size_t        at  = 0;



    for (int index = 0; index < s_kTracks * s_kSectors; index++)
    {
        at = static_cast<size_t> (index * s_kBytes);

        if (std::equal (work.sectors.begin() + at, work.sectors.begin() + at + s_kBytes, work.original.begin() + at))
        {
            continue;
        }

        if (!work.isReadable[index] && work.plan.reason.empty())
        {
            work.plan.reason = std::format (L"Track {}, sector ${:X} can't be read, so it isn't repaired", index / s_kSectors, index % s_kSectors);
        }

        out.track  = index / s_kSectors;
        out.sector = index % s_kSectors;
        std::copy (work.sectors.begin() + at, work.sectors.begin() + at + s_kBytes, out.bytes.begin());
        work.plan.sectors.push_back (out);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::BuildMap
//
//  The volume as the plan so far would write it: the analysis's sectors,
//  with each changed one read from the plan's buffer instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskRepair::BuildMap (const Work & work, std::unique_ptr<SectorSource> & outSource, FileMap & outMap)
{
    vector<FileMap>  maps;
    size_t           at       = 0;
    bool             isMapped = false;



    outSource = std::make_unique<SectorSource> (work.analysis);

    for (int index = 0; index < s_kTracks * s_kSectors; index++)
    {
        at = static_cast<size_t> (index * s_kBytes);

        if (!std::equal (work.sectors.begin() + at, work.sectors.begin() + at + s_kBytes, work.original.begin() + at))
        {
            outSource->SetDos33 (index / s_kSectors, index % s_kSectors, work.sectors.data() + at);
        }
    }

    maps     = FileMapBuilder::Build (*outSource, work.volume);
    isMapped = maps.size() == 1 && maps[0].notMapped == NotMappedReason::None;

    if (isMapped)
    {
        outMap = std::move (maps[0]);
    }

    return isMapped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::GetDos33
//
////////////////////////////////////////////////////////////////////////////////

Byte * DiskRepair::GetDos33 (Work & work, int track, int sector)
{
    return work.sectors.data() + (track * s_kSectors + sector) * s_kBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::GetBlockByte
//
//  A byte of a ProDOS or Pascal block in the DOS-order buffer, and the
//  track and DOS sector that has it.
//
////////////////////////////////////////////////////////////////////////////////

Byte * DiskRepair::GetBlockByte (Work & work, int block, int offset, int & outTrack, int & outSector)
{
    int  physical = SectorSource::GetBlockPhysical (block, offset / s_kBytes);



    outTrack  = block / SectorSource::kBlocksPerTrack;
    outSector = NibblizationLayer::GetDosFileIndexForPhysicalSector (physical);

    return GetDos33 (work, outTrack, outSector) + offset % s_kBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::IsReadable
//
////////////////////////////////////////////////////////////////////////////////

bool DiskRepair::IsReadable (const Work & work, int track, int sector)
{
    return work.isReadable[track * s_kSectors + sector];
}
