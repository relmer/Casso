#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/ProDosBlockLayout.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetVerdict
//
//  Bad for a block in two files, a block a file or the volume uses that the
//  free map marks free, a count that differs, a file whose blocks can't be
//  followed or a directory that can't be read; Possible inconsistencies for
//  a block in no file that the free map marks used.
//
////////////////////////////////////////////////////////////////////////////////

FileSystemVerdict BlockRepair::GetVerdict (const BlockMap & map)
{
    bool               isBad     = !map.counts.empty() || map.hasBrokenChains || !map.isCatalogComplete;
    bool               isSuspect = false;
    FileSystemVerdict  verdict   = FileSystemVerdict::None;



    for (const BlockMapEntry & entry : map.blocks)
    {
        isBad     = isBad || entry.role == SectorRole::CrossLinked || entry.role == SectorRole::OwnedMarkedFree;
        isSuspect = isSuspect || entry.role == SectorRole::AllocatedUnowned;
    }

    if (map.fileSystem != BlockFileSystem::None)
    {
        verdict = isBad ? FileSystemVerdict::Bad : isSuspect ? FileSystemVerdict::Suspect : FileSystemVerdict::Ok;
    }

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::Plan
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::Plan (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options)
{
    RepairPlan  plan;
    Work        work  { map, options, plan };



    work.image = &image;
    work.first = first;

    return Run (work);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanChosen
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::PlanChosen (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options, const std::set<std::wstring> & chosen)
{
    return DiskRepair::Narrow ([&image, &map, first] (const RepairOptions & narrowed) { return Plan (image, map, first, narrowed); }, options, chosen, GetVerdict (map));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanSectors
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::PlanSectors (std::span<const Byte> sectors, const BlockMap & map, int sectorsPerTrack, const RepairOptions & options)
{
    RepairPlan  plan;
    Work        work  { map, options, plan };



    work.sectors         = sectors;
    work.sectorsPerTrack = sectorsPerTrack;

    return Run (work);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanSectorsChosen
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::PlanSectorsChosen (std::span<const Byte> sectors, const BlockMap & map, int sectorsPerTrack, const RepairOptions & options, const std::set<std::wstring> & chosen)
{
    return DiskRepair::Narrow ([sectors, &map, sectorsPerTrack] (const RepairOptions & narrowed) { return PlanSectors (sectors, map, sectorsPerTrack, narrowed); },
                               options, chosen, GetVerdict (map));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::Run
//
//  Repair is offered only for a volume of a file system it knows whose
//  verdict is Bad or Possible inconsistencies and which has changes to
//  make. The plan for a volume that can't be repaired has its reason and no
//  changes.
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::Run (Work & work)
{
    RepairPlan  & plan     = work.plan;
    bool          isSector = work.image == nullptr;



    plan.before = GetVerdict (work.map);

    if (work.map.fileSystem == BlockFileSystem::None)
    {
        plan.reason = L"The volume has no standard file system.";
    }
    else if (isSector != (work.map.fileSystem == BlockFileSystem::Dos33))
    {
        plan.reason = L"The volume can't be repaired in block mode.";
    }
    else if (plan.before == FileSystemVerdict::Ok)
    {
        plan.reason = L"The file system has no damage.";
    }
    else
    {
        CheckBlocking (work);
    }

    if (plan.reason.empty())
    {
        switch (work.map.fileSystem)
        {
            case BlockFileSystem::ProDos:
            case BlockFileSystem::Sos:
                PlanCounts       (work);
                PlanProDosBitmap (work);
                break;

            case BlockFileSystem::Pascal:
                PlanCounts (work);
                break;

            case BlockFileSystem::Hfs:
                PlanHfsBitmap (work);
                break;

            case BlockFileSystem::Dos33:
                PlanDosBitmap (work);
                break;

            default:
                break;
        }
    }

    CollectUnits (work);

    if (plan.reason.empty() && plan.changes.empty())
    {
        plan.reason = work.options.skipped.empty() ? L"None of the damage is in the volume's bookkeeping." : L"No changes are selected.";
    }

    plan.canRepair = plan.reason.empty();

    if (!plan.canRepair)
    {
        plan.changes.clear();
        plan.blocks.clear();
        plan.sectors.clear();
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::CheckBlocking
//
//  A block in two files, a file whose blocks can't be followed, or a
//  directory that can't be read can't be set right without changing a
//  file's data, so the volume isn't repaired.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::CheckBlocking (Work & work)
{
    uint32_t  unit = 0;



    for (unit = 0; work.plan.reason.empty() && unit < work.map.GetBlockCount(); unit++)
    {
        if (work.map.blocks[unit].role == SectorRole::CrossLinked)
        {
            work.plan.reason = L"Repairing this would change a file's data: " + DescribeUnit (work, unit) + L" is in more than one file.";
        }
    }

    if (work.plan.reason.empty() && work.map.hasBrokenChains)
    {
        work.plan.reason = L"Repairing this would change a file's data: a file's blocks can't be followed to its end.";
    }

    if (work.plan.reason.empty() && !work.map.isCatalogComplete)
    {
        work.plan.reason = L"Some of the volume's directories can't be read.";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanCounts
//
//  Each directory file count that differs, set to the directory's entries.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanCounts (Work & work)
{
    std::wstring  key;



    for (const BlockCount & count : work.map.counts)
    {
        key = std::format (L"count:{}:{}", count.keyBlock, count.offset);

        if (count.isFreeCount || IsSkipped (work, key))
        {
            continue;
        }

        GetByte (work, count.keyBlock, count.offset)     = (Byte) count.actual;
        GetByte (work, count.keyBlock, count.offset + 1) = (Byte) (count.actual >> kBitsPerByte);

        AddChange (work, key, L"Set " + count.what + L" to " + NumberFormat::FormatInteger (count.actual), count.finding);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanProDosBitmap
//
//  The boot blocks, the directories, the bitmap and every block a file uses
//  marked used, every other block free unless the user turns off freeing
//  them, and the bits past the end of the volume in the bitmap's last block
//  marked used, so ProDOS doesn't allocate them. A set bit is a free block,
//  the first in the high bit.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanProDosBitmap (Work & work)
{
    const BlockMap  & map      = work.map;
    uint32_t          bitmap   = GetWord (work, kKeyBlock, kBitmapPointer);
    uint32_t          total    = GetWord (work, kKeyBlock, kTotalBlocks);
    uint32_t          bitmaps  = (total + kBitsPerBlock - 1) / kBitsPerBlock;
    uint32_t          covered  = (std::min) (total, map.GetBlockCount());
    Tally             tally    { vector<int> (map.files.size(), 0) };
    int               pastEnd  = 0;
    bool              isEnding = !IsSkipped (work, L"pastend");
    uint32_t          block    = 0;
    Byte            * bits     = nullptr;
    Byte              mask     = 0;



    auto  locate = [&work, &bits, &mask, bitmap] (uint32_t at)
    {
        bits = &GetByte (work, bitmap + at / kBitsPerBlock, (at % kBitsPerBlock) / kBitsPerByte);
        mask = (Byte) (kFirstBit >> (at % kBitsPerByte));
    };



    if (bitmap <= kKeyBlock || total == 0 || bitmap + bitmaps > map.GetBlockCount())
    {
        work.plan.reason = L"The volume bitmap's block, " + NumberFormat::FormatInteger ((int64_t) (work.first + bitmap)) + L", is outside the volume.";
        return;
    }

    for (block = 0; block < covered; block++)
    {
        const BlockMapEntry  & entry = map.blocks[block];

        locate (block);
        Mark (work, tally, entry.owners.empty() ? -1 : entry.owners[0], !entry.owners.empty() || entry.structure != BlockStructure::None || block < kBootBlocks, *bits, mask, true);
    }

    for (block = total; isEnding && block < bitmaps * kBitsPerBlock; block++)
    {
        locate (block);

        if ((*bits & mask) != 0)
        {
            *bits = (Byte) (*bits & ~mask);
            pastEnd++;
        }
    }

    AddBitmapChanges (work, tally, L"block", L"boot, directory and bitmap");

    if (pastEnd > 0)
    {
        AddChange (work, L"pastend", L"Mark " + NumberFormat::FormatInteger (pastEnd) + (pastEnd == 1 ? L" block" : L" blocks") + L" past the end of the volume as used in the bitmap",
                   NumberFormat::FormatInteger (pastEnd) + (pastEnd == 1 ? L" block past the end of the volume is" : L" blocks past the end of the volume are") + L" marked free");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanHfsBitmap
//
//  An allocation block is in use when a file, a B-tree or the volume uses
//  any of its blocks. A set bit is an allocation block in use, the first in
//  the high bit. The free block count is then set to the free allocation
//  blocks the bitmap has.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanHfsBitmap (Work & work)
{
    const BlockMap  & map         = work.map;
    uint32_t          bitmap      = GetBigWord (work, kKeyBlock, kHfsBitmap);
    uint32_t          allocations = GetBigWord (work, kKeyBlock, kHfsAllocations);
    uint64_t          size        = GetBigLong (work, kKeyBlock, kHfsAllocationSize);
    uint32_t          bitmaps     = (allocations + kBitsPerBlock - 1) / kBitsPerBlock;
    Tally             tally       { vector<int> (map.files.size(), 0) };
    uint32_t          recorded    = GetBigWord (work, kKeyBlock, kHfsFreeBlocks);
    uint32_t          free        = 0;
    uint32_t          a           = 0;
    uint32_t          k           = 0;
    uint32_t          block       = 0;
    int               owner       = -1;
    bool              isNeeded    = false;
    Byte            * bits        = nullptr;
    Byte              mask        = 0;
    std::wstring      problem;



    //  A B-tree with more than three extents continues in the extents
    //  overflow file, and the map doesn't have its other blocks.
    for (const auto & [sizeAt, extentAt] : { std::pair<uint32_t, uint32_t> (kHfsExtentsSize, kHfsExtentsExtent), std::pair<uint32_t, uint32_t> (kHfsCatalogSize, kHfsCatalogExtent) })
    {
        uint64_t  covered = 0;

        for (k = 0; k < kHfsExtents; k++)
        {
            covered += GetBigWord (work, kKeyBlock, extentAt + k * 4 + 2) * size;
        }

        if (covered < GetBigLong (work, kKeyBlock, sizeAt))
        {
            work.plan.reason = L"The catalog file or the extents overflow file has more than three extents, so its blocks aren't all known.";
            return;
        }
    }

    if (bitmap <= kKeyBlock || allocations == 0 || bitmap + bitmaps > map.GetBlockCount())
    {
        work.plan.reason = L"The volume bitmap's block, " + NumberFormat::FormatInteger ((int64_t) (work.first + bitmap)) + L", is outside the volume.";
        return;
    }

    for (a = 0; a < allocations; a++)
    {
        owner    = -1;
        isNeeded = false;

        for (k = 0; k < map.blocksPerAllocation; k++)
        {
            block = map.GetAllocationBlock (a) + k;

            if (block < map.GetBlockCount())
            {
                const BlockMapEntry  & entry = map.blocks[block];

                owner    = (owner < 0 && !entry.owners.empty()) ? entry.owners[0] : owner;
                isNeeded = isNeeded || !entry.owners.empty() || entry.structure != BlockStructure::None;
            }
        }

        bits = &GetByte (work, bitmap + a / kBitsPerBlock, (a % kBitsPerBlock) / kBitsPerByte);
        mask = (Byte) (kFirstBit >> (a % kBitsPerByte));

        Mark (work, tally, owner, isNeeded, *bits, mask, false);

        free += (*bits & mask) == 0 ? 1 : 0;
    }

    AddBitmapChanges (work, tally, L"allocation block", L"volume structure");

    if (free != recorded && !IsSkipped (work, L"freecount"))
    {
        GetByte (work, kKeyBlock, kHfsFreeBlocks)     = (Byte) (free >> kBitsPerByte);
        GetByte (work, kKeyBlock, kHfsFreeBlocks + 1) = (Byte) free;

        for (const BlockCount & count : map.counts)
        {
            problem = count.isFreeCount ? count.finding : problem;
        }

        AddChange (work, L"freecount", L"Set the free block count to " + NumberFormat::FormatInteger (free),
                   problem.empty() ? L"The free block count is " + NumberFormat::FormatInteger (recorded) + L", and the repaired bitmap has " + NumberFormat::FormatInteger (free) + L" free"
                                   : problem);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::PlanDosBitmap
//
//  Each sector off tracks 0 to 2 and 17 marked used when a file uses it and
//  free when none does, unless the user turns off freeing them, then the
//  VTOC and the catalog's sectors marked used. Four bytes a track, a set
//  bit a free sector, the highest sector in the first byte's high bit.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanDosBitmap (Work & work)
{
    const BlockMap  & map    = work.map;
    int               spt    = work.sectorsPerTrack;
    int               tracks = (int) map.GetBlockCount() / spt;
    int               bytes  = spt / kBitsPerByte;
    uint32_t          vtoc   = (uint32_t) (kVtocTrack * spt);
    Tally             tally  { vector<int> (map.files.size(), 0) };
    int               track  = 0;
    int               sector = 0;
    uint32_t          unit   = 0;
    Byte            * bits   = nullptr;
    Byte              mask   = 0;



    auto  locate = [&work, &bits, &mask, vtoc, bytes] (int atTrack, int atSector)
    {
        bits = &GetByte (work, vtoc, kDosBitmap + atTrack * kDosTrackBytes + (bytes - 1 - atSector / kBitsPerByte));
        mask = (Byte) (1 << (atSector % kBitsPerByte));
    };



    for (track = kDosBootTracks; track < tracks; track++)
    {
        for (sector = 0; track != kVtocTrack && sector < spt; sector++)
        {
            const BlockMapEntry  & entry = map.blocks[(size_t) track * spt + sector];

            locate (track, sector);
            Mark (work, tally, entry.owners.empty() ? -1 : entry.owners[0], !entry.owners.empty(), *bits, mask, true);
        }
    }

    for (unit = 0; unit < map.GetBlockCount(); unit++)
    {
        BlockStructure  structure = map.blocks[unit].structure;

        if (structure == BlockStructure::Dos33Vtoc || structure == BlockStructure::Dos33Catalog)
        {
            locate ((int) unit / spt, (int) unit % spt);
            Mark (work, tally, -1, true, *bits, mask, true);
        }
    }

    AddBitmapChanges (work, tally, L"sector", L"VTOC and catalog");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::Mark
//
//  A unit a file uses, or the volume's structures use, marked used; one in
//  no file marked free unless the user turns off freeing them. `isFreeBit`
//  is true when a set bit is a free unit.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::Mark (Work & work, Tally & tally, int owner, bool isNeeded, Byte & bits, Byte mask, bool isFreeBit)
{
    bool  isSet     = (bits & mask) != 0;
    bool  isUsed    = isSet != isFreeBit;
    bool  isFreeing = !work.options.isKeepingUnowned && !IsSkipped (work, L"unowned");
    Byte  used      = isFreeBit ? (Byte) (bits & ~mask) : (Byte) (bits | mask);
    Byte  freed     = isFreeBit ? (Byte) (bits | mask) : (Byte) (bits & ~mask);



    if (isNeeded && !isUsed && owner >= 0 && !IsSkipped (work, L"used:" + work.map.files[owner]))
    {
        bits = used;
        tally.markedUsed[owner]++;
    }
    else if (isNeeded && !isUsed && owner < 0 && !IsSkipped (work, L"structures"))
    {
        bits = used;
        tally.structures++;
    }
    else if (!isNeeded && isUsed && isFreeing)
    {
        bits = freed;
        tally.unowned++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::AddBitmapChanges
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::AddBitmapChanges (Work & work, const Tally & tally, LPCWSTR unit, LPCWSTR structure)
{
    auto  noun  = [unit] (int n) { return std::wstring (unit) + (n == 1 ? L"" : L"s"); };
    auto  count = [noun] (int n) { return NumberFormat::FormatInteger (n) + L" " + noun (n); };
    auto  verb  = [] (int n) { return (n == 1) ? L" is" : L" are"; };
    int   s     = tally.structures;



    for (size_t f = 0; f < tally.markedUsed.size(); f++)
    {
        int                   n    = tally.markedUsed[f];
        const std::wstring  & path = work.map.files[f];

        if (n > 0)
        {
            AddChange (work, L"used:" + path, L"Mark " + count (n) + L" of " + path + L" as used in the bitmap", count (n) + L" used by " + path + verb (n) + L" marked free");
        }
    }

    if (s > 0)
    {
        AddChange (work, L"structures", L"Mark " + NumberFormat::FormatInteger (s) + L" " + structure + L" " + noun (s) + L" as used in the bitmap",
                   NumberFormat::FormatInteger (s) + L" " + structure + L" " + noun (s) + verb (s) + L" marked free");
    }

    if (tally.unowned > 0)
    {
        AddChange (work, L"unowned", L"Mark " + count (tally.unowned) + L" not in any file as free in the bitmap", count (tally.unowned) + L" not in any file" + verb (tally.unowned) + L" marked used");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::AddChange
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::AddChange (Work & work, const std::wstring & key, const std::wstring & text, const std::wstring & problem)
{
    RepairChange  change;



    change.key     = key;
    change.text    = text;
    change.problem = problem;

    work.plan.changes.push_back (std::move (change));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::CollectUnits
//
//  Each block or sector the plan changed, whole. A block has its bytes
//  before; a sector has its track and sector.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::CollectUnits (Work & work)
{
    RepairBlock   block;
    RepairSector  sector;



    for (const auto & [unit, bytes] : work.changed)
    {
        if (bytes == work.original[unit])
        {
            continue;
        }

        if (work.image != nullptr)
        {
            block.block    = unit;
            block.original = work.original[unit];
            block.bytes    = bytes;

            work.plan.blocks.push_back (block);
        }
        else
        {
            sector.track  = (int) unit / work.sectorsPerTrack;
            sector.sector = (int) unit % work.sectorsPerTrack;
            std::copy (bytes.begin(), bytes.begin() + kSectorBytes, sector.bytes.begin());

            work.plan.sectors.push_back (sector);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::DescribeUnit
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepair::DescribeUnit (const Work & work, uint32_t unit)
{
    if (work.image == nullptr)
    {
        return L"track " + NumberFormat::FormatInteger ((int64_t) unit / work.sectorsPerTrack) + L", sector " + NumberFormat::FormatInteger ((int64_t) unit % work.sectorsPerTrack);
    }

    return L"block " + NumberFormat::FormatInteger ((int64_t) (work.first + unit));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetByte
//
//  A byte of a block or sector as the plan so far would write it. A unit is
//  read the first time one of its bytes is, and a copy of its bytes before
//  is saved.
//
////////////////////////////////////////////////////////////////////////////////

Byte & BlockRepair::GetByte (Work & work, uint32_t unit, uint32_t offset)
{
    auto    found = work.changed.find (unit);
    Block   bytes = {};
    size_t  at    = (size_t) unit * kSectorBytes;



    if (found == work.changed.end())
    {
        if (work.image != nullptr)
        {
            ProDosBlockLayout::ReadBlock (*work.image, unit, bytes);
        }
        else if (at + kSectorBytes <= work.sectors.size())
        {
            std::copy (work.sectors.begin() + (ptrdiff_t) at, work.sectors.begin() + (ptrdiff_t) (at + kSectorBytes), bytes.begin());
        }

        work.original[unit] = bytes;
        found = work.changed.emplace (unit, bytes).first;
    }

    return found->second[offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetWord
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockRepair::GetWord (Work & work, uint32_t unit, uint32_t offset)
{
    return GetByte (work, unit, offset) | ((uint32_t) GetByte (work, unit, offset + 1) << kBitsPerByte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetBigWord
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockRepair::GetBigWord (Work & work, uint32_t unit, uint32_t offset)
{
    return ((uint32_t) GetByte (work, unit, offset) << kBitsPerByte) | GetByte (work, unit, offset + 1);
}
