#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/ProDosBlockLayout.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetVerdict
//
//  Bad for a block in two files, a block a file uses that the bitmap marks
//  free, a file count that differs, a file whose blocks can't be followed or
//  a directory that can't be read; Possible inconsistencies for a block in
//  no file that the bitmap marks used.
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
//  Repair is offered only for a ProDOS or SOS volume whose verdict is Bad or
//  Possible inconsistencies and which has changes to make. The plan for a
//  volume that can't be repaired has its reason and no changes.
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockRepair::Plan (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options)
{
    RepairPlan  plan;
    Work        work  { image, map, first, options, plan, {}, {} };



    plan.before = GetVerdict (map);

    if (map.fileSystem == BlockFileSystem::None)
    {
        plan.reason = L"The volume has no standard file system.";
    }
    else if (map.fileSystem != BlockFileSystem::ProDos && map.fileSystem != BlockFileSystem::Sos)
    {
        plan.reason = L"Only ProDOS and SOS volumes can be repaired in block mode.";
    }
    else if (plan.before == FileSystemVerdict::Ok)
    {
        plan.reason = L"The file system has no damage.";
    }
    else
    {
        CheckBlocking (work);

        if (plan.reason.empty())
        {
            PlanCounts (work);
            PlanBitmap (work);
        }
    }

    CollectBlocks (work);

    if (plan.reason.empty() && plan.changes.empty())
    {
        plan.reason = options.skipped.empty() ? L"None of the damage is in the volume's bookkeeping." : L"No changes are selected.";
    }

    plan.canRepair = plan.reason.empty();

    if (!plan.canRepair)
    {
        plan.changes.clear();
        plan.blocks.clear();
    }

    return plan;
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
//  BlockRepair::CheckBlocking
//
//  A block in two files, a file whose blocks can't be followed, or a
//  directory that can't be read can't be set right without changing a
//  file's data, so the volume isn't repaired.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::CheckBlocking (Work & work)
{
    uint32_t  block = 0;



    for (block = 0; work.plan.reason.empty() && block < work.map.GetBlockCount(); block++)
    {
        if (work.map.blocks[block].role == SectorRole::CrossLinked)
        {
            work.plan.reason = L"Repairing this would change a file's data: block " + NumberFormat::FormatInteger ((int64_t) (work.first + block)) + L" is in more than one file.";
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
//  Each directory file count that differs, set to the directory's active
//  entries.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanCounts (Work & work)
{
    std::wstring  key;



    for (const BlockCount & count : work.map.counts)
    {
        key = std::format (L"count:{}:{}", count.keyBlock, count.offset);

        if (IsSkipped (work, key))
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
//  BlockRepair::PlanBitmap
//
//  The boot blocks, the directories, the bitmap and every block a file uses
//  marked used, every other block free unless the user turns off freeing
//  them, and the bits past the end of the volume in the bitmap's last block
//  marked used, so ProDOS doesn't allocate them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::PlanBitmap (Work & work)
{
    const BlockMap  & map        = work.map;
    uint32_t          bitmap     = GetWord (work, kKeyBlock, kBitmapPointer);
    uint32_t          total      = GetWord (work, kKeyBlock, kTotalBlocks);
    uint32_t          bitmaps    = (total + kBitsPerBlock - 1) / kBitsPerBlock;
    uint32_t          covered    = (std::min) (total, map.GetBlockCount());
    vector<int>       markedUsed (map.files.size(), 0);
    int               structures = 0;
    int               unowned    = 0;
    int               pastEnd    = 0;
    bool              isFreeing  = !work.options.isKeepingUnowned && !IsSkipped (work, L"unowned");
    bool              isMarking  = !IsSkipped (work, L"structures");
    bool              isEnding   = !IsSkipped (work, L"pastend");
    uint32_t          block      = 0;
    Byte            * bits       = nullptr;
    Byte              mask       = 0;
    bool              isUsed     = false;
    bool              isOwned    = false;
    bool              isNeeded   = false;



    auto  plural = [] (int count) { return (count == 1) ? L"" : L"s"; };
    auto  verb   = [] (int count) { return (count == 1) ? L"is" : L"are"; };
    auto  locate = [&work, &bits, &mask, bitmap] (uint32_t at)
    {
        bits = &GetByte (work, bitmap + at / kBitsPerBlock, (at % kBitsPerBlock) / kBitsPerByte);
        mask = (Byte) (kFirstBlockBit >> (at % kBitsPerByte));
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

        isUsed   = (*bits & mask) == 0;
        isOwned  = !entry.owners.empty();
        isNeeded = isOwned || entry.structure != BlockStructure::None || block < kBootBlocks;

        if (isNeeded && !isUsed && isOwned && !IsSkipped (work, L"used:" + map.files[entry.owners[0]]))
        {
            *bits = (Byte) (*bits & ~mask);
            markedUsed[entry.owners[0]]++;
        }
        else if (isNeeded && !isUsed && !isOwned && isMarking)
        {
            *bits = (Byte) (*bits & ~mask);
            structures++;
        }
        else if (!isNeeded && isUsed && isFreeing)
        {
            *bits = (Byte) (*bits | mask);
            unowned++;
        }
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

    for (size_t f = 0; f < markedUsed.size(); f++)
    {
        int                   count = markedUsed[f];
        const std::wstring  & path  = map.files[f];
        std::wstring          n     = NumberFormat::FormatInteger (count);

        if (count > 0)
        {
            AddChange (work, L"used:" + path, L"Mark " + n + L" block" + plural (count) + L" of " + path + L" as used in the bitmap",
                       n + L" block" + plural (count) + L" used by " + path + L" " + verb (count) + L" marked free");
        }
    }

    if (structures > 0)
    {
        AddChange (work, L"structures", L"Mark " + NumberFormat::FormatInteger (structures) + L" boot, directory and bitmap block" + plural (structures) + L" as used in the bitmap",
                   NumberFormat::FormatInteger (structures) + L" boot, directory and bitmap block" + plural (structures) + L" " + verb (structures) + L" marked free");
    }

    if (unowned > 0)
    {
        AddChange (work, L"unowned", L"Mark " + NumberFormat::FormatInteger (unowned) + L" block" + plural (unowned) + L" not in any file as free in the bitmap",
                   NumberFormat::FormatInteger (unowned) + L" block" + plural (unowned) + L" not in any file " + verb (unowned) + L" marked used");
    }

    if (pastEnd > 0)
    {
        AddChange (work, L"pastend", L"Mark " + NumberFormat::FormatInteger (pastEnd) + L" block" + plural (pastEnd) + L" past the end of the volume as used in the bitmap",
                   NumberFormat::FormatInteger (pastEnd) + L" block" + plural (pastEnd) + L" past the end of the volume " + verb (pastEnd) + L" marked free");
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
//  BlockRepair::CollectBlocks
//
//  Each block the plan changed, whole, with its bytes before.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepair::CollectBlocks (Work & work)
{
    RepairBlock  out;



    for (const auto & [block, bytes] : work.changed)
    {
        if (bytes == work.original[block])
        {
            continue;
        }

        out.block    = block;
        out.original = work.original[block];
        out.bytes    = bytes;

        work.plan.blocks.push_back (out);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetByte
//
//  A byte of a block as the plan so far would write it. A block is read the
//  first time one of its bytes is, and a copy of its bytes before is saved.
//
////////////////////////////////////////////////////////////////////////////////

Byte & BlockRepair::GetByte (Work & work, uint32_t block, uint32_t offset)
{
    auto   found = work.changed.find (block);
    Block  bytes = {};



    if (found == work.changed.end())
    {
        ProDosBlockLayout::ReadBlock (work.image, block, bytes);

        work.original[block] = bytes;
        found = work.changed.emplace (block, bytes).first;
    }

    return found->second[offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair::GetWord
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockRepair::GetWord (Work & work, uint32_t block, uint32_t offset)
{
    return GetByte (work, block, offset) | ((uint32_t) GetByte (work, block, offset + 1) << kBitsPerByte);
}
