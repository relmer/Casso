#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
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
static constexpr int   s_kBitsPerBitmapBlock = 4096;





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
        plan.reason = L"The disk has no standard file system.";
    }
    else if (FileSystemCheck::IsCopyProtected (analysis))
    {
        plan.reason = L"The disk is copy-protected.";
    }
    else if (analysis.fileMaps.size() != 1)
    {
        plan.reason = L"The disk has more than one volume.";
    }
    else if (analysis.verdict == FileSystemVerdict::Ok)
    {
        plan.reason = L"The file system has no damage.";
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

            //  A cross-link or a chain that can't be followed is the reason,
            //  as on a DOS 3.3, ProDOS or Apple Pascal volume.
            default:
                CheckChains (work, analysis.fileMaps[0]);

                if (plan.reason.empty())
                {
                    plan.reason = L"Only DOS 3.3, ProDOS and Apple Pascal volumes can be repaired.";
                }

                break;
        }
    }

    CollectSectors (work);

    if (plan.reason.empty() && plan.changes.empty())
    {
        plan.reason = options.skipped.empty() ? L"None of the damage is in the volume's bookkeeping." : L"No changes are selected.";
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
//  DiskRepair::PlanChosen
//
//  Each change of a plan that isn't chosen is skipped and the volume planned
//  again, until every change in the plan is chosen.
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan DiskRepair::PlanChosen (const DiskAnalysis & analysis, const RepairOptions & options, const std::set<std::wstring> & chosen)
{
    static constexpr int  kMaxPasses = 8;

    RepairOptions  narrowed = options;
    RepairPlan     plan     = Plan (analysis, narrowed);
    bool           isExtra  = true;



    for (int pass = 0; pass < kMaxPasses && isExtra && plan.canRepair; pass++)
    {
        isExtra = false;

        for (const RepairChange & change : plan.changes)
        {
            if (!chosen.contains (change.key))
            {
                narrowed.skipped.insert (change.key);
                isExtra = true;
            }
        }

        if (isExtra)
        {
            plan = Plan (analysis, narrowed);
        }
    }

    if (isExtra && plan.canRepair)
    {
        plan = RepairPlan();
        plan.before = analysis.verdict;
        plan.reason = L"The chosen changes can't be made without others.";
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::IsFixing
//
////////////////////////////////////////////////////////////////////////////////

bool DiskRepair::IsFixing (const RepairChange & change, const Finding & finding)
{
    return std::any_of (change.fixes.begin(), change.fixes.end(), [&finding] (const RepairFix & fix)
    {
        return fix.kind == finding.kind && (fix.cell < 0 || fix.cell == finding.mapCell) && (fix.detail.empty() || fix.detail == finding.detail);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::GetFixingKeys
//
////////////////////////////////////////////////////////////////////////////////

std::set<std::wstring> DiskRepair::GetFixingKeys (const RepairPlan & plan, const Finding & finding)
{
    std::set<std::wstring>  keys;



    for (const RepairChange & change : plan.changes)
    {
        if (IsFixing (change, finding))
        {
            keys.insert (change.key);
        }
    }

    return keys;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::GetKeys
//
////////////////////////////////////////////////////////////////////////////////

std::set<std::wstring> DiskRepair::GetKeys (const RepairPlan & plan)
{
    std::set<std::wstring>  keys;



    for (const RepairChange & change : plan.changes)
    {
        keys.insert (change.key);
    }

    return keys;
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
        work.plan.reason = L"The catalog isn't a standard DOS 3.3 catalog.";
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
            work.plan.reason = L"The volume can't be read with the relinked catalog.";
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



    if (map.isHeaderLost && !IsSkipped (work, L"header"))
    {
        Dos33MapReader::MakeStandardHeader (static_cast<Byte> (work.volume), vtoc);
        AddChange (work, L"header", L"Set the VTOC header to DOS 3.3's standard values", { { FindingKind::Dos33HeaderLost } });
    }
    else if (!map.isHeaderLost && !IsSkipped (work, L"next") &&
             (vtoc[Dos33MapReader::kNextTrack] >= s_kTracks || (vtoc[Dos33MapReader::kDirection] != s_kUp && vtoc[Dos33MapReader::kDirection] != s_kDown)))
    {
        vtoc[Dos33MapReader::kNextTrack] = static_cast<Byte> (Dos33MapReader::kVtocTrack);
        vtoc[Dos33MapReader::kDirection] = s_kUp;
        AddChange (work, L"next", L"Set the VTOC's next track to allocate to track 17, allocating upward", { { FindingKind::Dos33NextTrackOffDisk } });
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
    SectorSource       source   (work.analysis);
    vector<int>          path;
    vector<RepairFix>    fixes;
    std::wstring         key;
    int                  linked   = 0;
    size_t               i        = 0;
    bool                 isEarly  = false;
    bool                 isPrefix = true;
    int                  next     = 0;
    Byte               * vtoc     = GetDos33 (work, Dos33MapReader::kVtocTrack, 0);
    Byte               * at       = nullptr;



    if (!map.isCatalogComplete && map.unreadableCell >= 0 && !IsReadable (work, map.unreadableCell / s_kSectors, map.unreadableCell % s_kSectors))
    {
        work.plan.reason = std::format (L"Track {}, sector ${:X} of the catalog can't be read.",
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

    if (linked < Dos33MapReader::kInitLinkedMin)
    {
        work.plan.reason = std::format (L"The catalog can't be relinked: only {} of its sectors on track 17 are linked in INIT's order, and at least four are required.", linked);
        return;
    }

    if (vtoc[s_kCatalogTrack] != Dos33MapReader::kVtocTrack || vtoc[s_kCatalogSector] != Dos33MapReader::kCatalogSector)
    {
        work.plan.reason = L"The catalog isn't a standard DOS 3.3 catalog.";
        return;
    }

    //  A relink sets right a broken chain found on the catalog's track.
    for (const Finding & finding : map.findings)
    {
        if (finding.kind == FindingKind::FileChainBroken && finding.mapCell / s_kSectors == Dos33MapReader::kVtocTrack)
        {
            fixes.push_back ({ FindingKind::FileChainBroken, finding.mapCell });
        }
    }

    for (i = 0; i < path.size(); i++)
    {
        at   = GetDos33 (work, Dos33MapReader::kVtocTrack, path[i]);
        next = (i + 1 < path.size()) ? path[i + 1] : -1;
        key  = std::format (L"{}:{:X}", (next >= 0) ? L"link" : L"end", path[i]);

        if (IsSkipped (work, key))
        {
            continue;
        }

        if (next >= 0 && (at[s_kCatalogTrack] != Dos33MapReader::kVtocTrack || at[s_kCatalogSector] != next))
        {
            at[s_kCatalogTrack]  = static_cast<Byte> (Dos33MapReader::kVtocTrack);
            at[s_kCatalogSector] = static_cast<Byte> (next);
            AddChange (work, key, std::format (L"Link catalog sector ${:X} to sector ${:X}", path[i], next), fixes);
        }
        else if (next < 0 && (at[s_kCatalogTrack] != 0 || at[s_kCatalogSector] != 0))
        {
            at[s_kCatalogTrack]  = 0;
            at[s_kCatalogSector] = 0;
            AddChange (work, key, std::format (L"End the catalog at sector ${:X}", path[i]), fixes);
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
    vector<int>  structure  = map.catalogCells;
    BitmapTally  tally      { vector<int> (map.files.size(), 0), vector<vector<RepairFix>> (map.files.size()) };
    bool         isFreeing  = !work.options.isKeepingUnowned && !IsSkipped (work, L"unowned");
    bool         isMarking  = !IsSkipped (work, L"structures");
    int          t          = 0;
    int          s          = 0;



    for (t = s_kBootTracks; t < s_kTracks; t++)
    {
        for (s = 0; t != Dos33MapReader::kVtocTrack && s < s_kSectors; s++)
        {
            int              at      = Dos33MapReader::GetCell (t, s);
            const MapCell &  cell    = map.cells[at];
            Byte &           bits    = vtoc[s_kBitmap + t * s_kBitmapBytes + (s >= s_kBitsPerByte ? 0 : 1)];
            Byte             mask    = static_cast<Byte> (1 << (s % s_kBitsPerByte));
            bool             isUsed  = (bits & mask) == 0;
            bool             isOwned = !cell.owners.empty();

            if (isOwned && !isUsed && !IsSkipped (work, L"used:" + map.files[cell.owners[0]].path))
            {
                bits = static_cast<Byte> (bits & ~mask);
                tally.markedUsed[cell.owners[0]]++;
                tally.usedFixes[cell.owners[0]].push_back ({ FindingKind::FileOwnedMarkedFree, at });
            }
            else if (!isOwned && isUsed && isFreeing)
            {
                bits = static_cast<Byte> (bits | mask);
                tally.unowned++;
                tally.unownedFixes.push_back ({ FindingKind::FileAllocatedUnowned, at });
            }
        }
    }

    //  The VTOC and the catalog's sectors on track 17, which the bitmap
    //  rebuild skips, marked used when they are free.
    structure.push_back (Dos33MapReader::GetCell (Dos33MapReader::kVtocTrack, 0));

    for (int cell : structure)
    {
        Byte &  bits = vtoc[s_kBitmap + (cell / s_kSectors) * s_kBitmapBytes + (cell % s_kSectors >= s_kBitsPerByte ? 0 : 1)];
        Byte    mask = static_cast<Byte> (1 << (cell % s_kSectors % s_kBitsPerByte));

        if ((bits & mask) != 0 && isMarking)
        {
            bits = static_cast<Byte> (bits & ~mask);
            tally.structures++;
            tally.structureFixes.push_back ({ FindingKind::Dos33StructureMarkedFree, cell });
        }
    }

    AddBitmapChanges (work, map, tally, L"sector", L"VTOC and catalog");
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
    BitmapTally  tally      { vector<int> (map.files.size(), 0), vector<vector<RepairFix>> (map.files.size()) };
    bool    isFreeing   = !work.options.isKeepingUnowned && !IsSkipped (work, L"unowned");
    bool    isMarking   = !IsSkipped (work, L"structures");
    bool    isEnding    = !IsSkipped (work, L"pastend");
    int     pastEnd     = 0;
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
            work.plan.reason = std::format (L"The volume bitmap's block, {}, is outside the volume.", bitmapAt);
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

        if (isOwned && !isUsed && !IsSkipped (work, L"used:" + map.files[cell.owners[0]].path))
        {
            *bits = static_cast<Byte> (*bits & ~mask);
            tally.markedUsed[cell.owners[0]]++;
            tally.usedFixes[cell.owners[0]].push_back ({ FindingKind::FileOwnedMarkedFree, b });
        }
        else if (!isOwned && isStructure && !isUsed && isMarking)
        {
            *bits = static_cast<Byte> (*bits & ~mask);
            tally.structures++;
            tally.structureFixes.push_back ({ FindingKind::FileOwnedMarkedFree, b });
        }
        else if (!isOwned && !isStructure && isUsed && isFreeing)
        {
            *bits = static_cast<Byte> (*bits | mask);
            tally.unowned++;
            tally.unownedFixes.push_back ({ FindingKind::FileAllocatedUnowned, b });
        }
    }

    //  Blocks past the end of the volume, which the bitmap's last block has
    //  bits for, marked used, so ProDOS doesn't allocate them.
    for (b = s_kBlocks; work.plan.reason.empty() && isEnding && b < s_kBitsPerBitmapBlock; b++)
    {
        bits = GetBlockByte (work, bitmapAt, b / s_kBitsPerByte, track, sector);
        mask = static_cast<Byte> (s_kFirstBlockBit >> (b % s_kBitsPerByte));

        if ((*bits & mask) != 0)
        {
            *bits = static_cast<Byte> (*bits & ~mask);
            pastEnd++;
        }
    }

    if (work.plan.reason.empty())
    {
        AddBitmapChanges (work, map, tally, L"block", L"boot, directory and bitmap");
    }

    if (work.plan.reason.empty() && pastEnd > 0)
    {
        AddChange (work, L"pastend", std::format (L"Mark {} blocks past the end of the volume as used in the bitmap", pastEnd), { { FindingKind::ProDosBitmapPastEnd } });
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
    Byte *        at     = nullptr;
    Byte *        high   = nullptr;
    int           track  = 0;
    int           sector = 0;
    std::wstring  key;



    for (const RecordedCount & count : map.counts)
    {
        key = std::format (L"count:{}:{}", count.cell, count.offset);

        if (IsSkipped (work, key))
        {
            continue;
        }

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

        *at = static_cast<Byte> (count.actual);

        if (count.width > 1)
        {
            *high = static_cast<Byte> (count.actual >> s_kBitsPerByte);
        }

        AddChange (work, key, std::format (L"Set {} to {}", count.what, count.actual), { { count.kind, -1, count.finding } });
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
        bool  isBlocking = finding.kind == FindingKind::FileChainBroken || finding.kind == FindingKind::FileCrossLinked || finding.kind == FindingKind::PascalEntryOutOfRange ||
                            finding.kind == FindingKind::FileUsesStructure;

        if (work.plan.reason.empty() && isBlocking)
        {
            work.plan.reason = L"Repairing this would change a file's data: " + TextEncoding::Utf8ToWide (finding.detail) + (finding.detail.ends_with ('.') ? L"" : L".");
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::AddBitmapChanges
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::AddBitmapChanges (Work & work, const FileMap & map, const BitmapTally & tally, LPCWSTR unit, LPCWSTR structure)
{
    auto  plural = [] (int count) { return (count == 1) ? L"" : L"s"; };



    for (size_t f = 0; f < tally.markedUsed.size(); f++)
    {
        if (tally.markedUsed[f] > 0)
        {
            AddChange (work, L"used:" + map.files[f].path,
                       std::format (L"Mark {} {}{} of {} as used in the bitmap", tally.markedUsed[f], unit, plural (tally.markedUsed[f]), map.files[f].path), tally.usedFixes[f]);
        }
    }

    if (tally.structures > 0)
    {
        AddChange (work, L"structures", std::format (L"Mark {} {} {}{} as used in the bitmap", tally.structures, structure, unit, plural (tally.structures)), tally.structureFixes);
    }

    if (tally.unowned > 0)
    {
        AddChange (work, L"unowned", std::format (L"Mark {} {}{} not in any file as free in the bitmap", tally.unowned, unit, plural (tally.unowned)), tally.unownedFixes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::AddChange
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepair::AddChange (Work & work, std::wstring key, std::wstring text, vector<RepairFix> fixes)
{
    work.plan.changes.push_back ({ std::move (key), std::move (text), std::move (fixes) });
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
            work.plan.reason = std::format (L"Track {}, sector ${:X} can't be read.", index / s_kSectors, index % s_kSectors);
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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair::IsSkipped
//
////////////////////////////////////////////////////////////////////////////////

bool DiskRepair::IsSkipped (const Work & work, const std::wstring & key)
{
    return work.options.skipped.contains (key);
}
