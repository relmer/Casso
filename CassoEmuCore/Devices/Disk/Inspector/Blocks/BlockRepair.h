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
//  repair of its bookkeeping at any size:
//
//      ProDOS and SOS  the bitmap set from the files and the volume's
//                      structures, the blocks past the end of the volume
//                      marked used, and each directory's file count set to
//                      its active entries.
//      Apple Pascal    the directory's file count.
//      HFS             the volume bitmap, by allocation block, and the
//                      master directory block's free block count.
//      DOS 3.3         a volume inside another, such as half of an 800K
//                      UniDOS disk: the VTOC's bitmap, by sector.
//
//  A file's data is never changed, so a volume with a block in two files, a
//  file whose blocks can't be followed, or a directory that can't be read
//  isn't repaired, nor is an HFS volume whose catalog or extents overflow
//  file has more extents than the master directory block. The changes have
//  the same keys as DiskRepair's. `first` is the volume's first block in the
//  image, for the block numbers in the reasons. A DOS 3.3 plan has sectors,
//  and the others blocks. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockRepair
{
public:
    static constexpr uint32_t  kBlockBytes  = 512;
    static constexpr uint32_t  kSectorBytes = 256;

    using Block = std::array<Byte, kBlockBytes>;

    static FileSystemVerdict  GetVerdict (const BlockMap & map);

    static RepairPlan  Plan       (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options);
    static RepairPlan  PlanChosen (const BlockImage & image, const BlockMap & map, uint32_t first, const RepairOptions & options, const std::set<std::wstring> & chosen);

    //  A DOS 3.3 volume over its sectors in track and sector order.
    static RepairPlan  PlanSectors       (std::span<const Byte> sectors, const BlockMap & map, int sectorsPerTrack, const RepairOptions & options);
    static RepairPlan  PlanSectorsChosen (std::span<const Byte> sectors, const BlockMap & map, int sectorsPerTrack, const RepairOptions & options, const std::set<std::wstring> & chosen);

private:
    static constexpr uint32_t  kKeyBlock      = 2;
    static constexpr uint32_t  kBootBlocks    = 2;
    static constexpr uint32_t  kBitmapPointer = 0x27;
    static constexpr uint32_t  kTotalBlocks   = 0x29;
    static constexpr uint32_t  kBitsPerBlock  = 4096;
    static constexpr int       kBitsPerByte   = 8;
    static constexpr Byte      kFirstBit      = 0x80;

    //  HFS's master directory block.
    static constexpr uint32_t  kHfsBitmap         = 14;
    static constexpr uint32_t  kHfsAllocations    = 18;
    static constexpr uint32_t  kHfsAllocationSize = 20;
    static constexpr uint32_t  kHfsFreeBlocks     = 34;
    static constexpr uint32_t  kHfsExtentsSize    = 130;
    static constexpr uint32_t  kHfsExtentsExtent  = 134;
    static constexpr uint32_t  kHfsCatalogSize    = 146;
    static constexpr uint32_t  kHfsCatalogExtent  = 150;
    static constexpr uint32_t  kHfsExtents        = 3;

    //  DOS 3.3's VTOC.
    static constexpr int       kDosBootTracks = 3;
    static constexpr int       kVtocTrack     = 17;
    static constexpr uint32_t  kDosBitmap     = 0x38;
    static constexpr uint32_t  kDosTrackBytes = 4;

    struct Work
    {
        const BlockMap             & map;
        const RepairOptions        & options;
        RepairPlan                 & plan;
        const BlockImage           * image           = nullptr;
        std::span<const Byte>        sectors;
        int                          sectorsPerTrack = 0;
        uint32_t                     first           = 0;
        std::map<uint32_t, Block>    original;
        std::map<uint32_t, Block>    changed;
    };

    //  What a bitmap rebuild marked: the units of each file marked used,
    //  the volume's structures marked used, and the units in no file marked
    //  free.
    struct Tally
    {
        vector<int>  markedUsed;
        int          structures = 0;
        int          unowned    = 0;
    };

    static RepairPlan    Run              (Work & work);
    static void          CheckBlocking    (Work & work);
    static void          PlanCounts       (Work & work);
    static void          PlanProDosBitmap (Work & work);
    static void          PlanHfsBitmap    (Work & work);
    static void          PlanDosBitmap    (Work & work);
    static void          Mark             (Work & work, Tally & tally, int owner, bool isNeeded, Byte & bits, Byte mask, bool isFreeBit);
    static void          AddBitmapChanges (Work & work, const Tally & tally, LPCWSTR unit, LPCWSTR structure);
    static void          AddChange        (Work & work, const std::wstring & key, const std::wstring & text, const std::wstring & problem);
    static void          CollectUnits     (Work & work);
    static std::wstring  DescribeUnit     (const Work & work, uint32_t unit);
    static Byte        & GetByte          (Work & work, uint32_t unit, uint32_t offset);
    static uint32_t      GetWord          (Work & work, uint32_t unit, uint32_t offset);
    static uint32_t      GetBigWord       (Work & work, uint32_t unit, uint32_t offset);
    static uint32_t      GetBigLong       (Work & work, uint32_t unit, uint32_t offset) { return (GetBigWord (work, unit, offset) << 16) | GetBigWord (work, unit, offset + 2); }
    static bool          IsSkipped        (const Work & work, const std::wstring & key) { return work.options.skipped.contains (key); }
};
