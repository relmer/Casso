#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockMap.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepair
//
//  The file system verdict of a volume of blocks from its map, and the
//  repair of a ProDOS or SOS volume's bookkeeping of any size: the bitmap
//  set from the files and the volume's structures, the blocks past the end
//  of the volume marked used, and each directory's file count set to its
//  active entries. A file's data is never changed, so a volume with a
//  block in two files, a file whose blocks can't be followed, or a
//  directory that can't be read isn't repaired. The changes have the same
//  keys as DiskRepair's. `first` is the volume's first block in the image,
//  for the block numbers in the reasons. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockRepair
{
public:
    static constexpr uint32_t  kBlockBytes = 512;

    using Block = std::array<Byte, kBlockBytes>;

    static FileSystemVerdict  GetVerdict (const BlockMap & map);

    static RepairPlan  Plan       (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options);
    static RepairPlan  PlanChosen (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options, const std::set<std::wstring> & chosen);

private:
    static constexpr uint32_t  kKeyBlock      = 2;
    static constexpr uint32_t  kBootBlocks    = 2;
    static constexpr uint32_t  kBitmapPointer = 0x27;
    static constexpr uint32_t  kTotalBlocks   = 0x29;
    static constexpr uint32_t  kBitsPerBlock  = 4096;
    static constexpr int       kBitsPerByte   = 8;
    static constexpr Byte      kFirstBlockBit = 0x80;

    struct Work
    {
        const BlockImage           & image;
        const BlockMap             & map;
        uint32_t                     first;
        const RepairOptions        & options;
        RepairPlan                 & plan;
        std::map<uint32_t, Block>    original;
        std::map<uint32_t, Block>    changed;
    };

    static void      CheckBlocking (Work & work);
    static void      PlanCounts    (Work & work);
    static void      PlanBitmap    (Work & work);
    static void      AddChange     (Work & work, const std::wstring & key, const std::wstring & text, const std::wstring & problem);
    static void      CollectBlocks (Work & work);
    static Byte    & GetByte       (Work & work, uint32_t block, uint32_t offset);
    static uint32_t  GetWord       (Work & work, uint32_t block, uint32_t offset);
    static bool      IsSkipped     (const Work & work, const std::wstring & key) { return work.options.skipped.contains (key); }
};
