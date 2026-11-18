#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"

class SectorSource;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairOptions / RepairSector / RepairBlock / RepairFix / RepairChange /
//  RepairPlan
//
//  Repair's options, a sector it writes (by track and DOS logical sector,
//  all 256 bytes), a block of a volume of blocks it writes (its number in
//  the volume, its bytes before and after), a finding a change sets right,
//  a change, and the plan: whether the volume can be repaired and why not,
//  the changes, the sectors or blocks, and the verdict before and after.
//
//  A change's key identifies it from one plan to the next, so a change can
//  be left out by adding its key to RepairOptions::skipped.
//
////////////////////////////////////////////////////////////////////////////////

struct RepairOptions
{
    bool                    isKeepingUnowned = false;
    std::set<std::wstring>  skipped;
};


struct RepairSector
{
    static constexpr int  kBytes = 256;

    int                         track  = 0;
    int                         sector = 0;
    std::array<Byte, kBytes>    bytes  = {};
};


struct RepairBlock
{
    static constexpr int  kBytes = 512;

    uint32_t                    block    = 0;
    std::array<Byte, kBytes>    original = {};
    std::array<Byte, kBytes>    bytes    = {};
};


//  A finding matches when its kind is the fix's, and its map cell and detail
//  are the fix's where the fix sets them (a cell of -1 or an empty detail
//  matches any).
struct RepairFix
{
    FindingKind  kind   = FindingKind::FileCountDiffers;
    int          cell   = -1;
    std::string  detail;
};


//  A change: its key, what it does, the findings it sets right, and the
//  problem they are, in one line.
struct RepairChange
{
    std::wstring       key;
    std::wstring       text;
    vector<RepairFix>  fixes;
    std::wstring       problem;
};


struct RepairPlan
{
    bool                   canRepair = false;
    std::wstring           reason;
    vector<RepairChange>   changes;
    vector<RepairSector>   sectors;
    vector<RepairBlock>    blocks;
    FileSystemVerdict      before    = FileSystemVerdict::None;
    FileSystemVerdict      after     = FileSystemVerdict::None;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepair
//
//  The repair of a volume's bookkeeping from a disk analysis: a DOS 3.3
//  VTOC header set back to DOS's and its next track to allocate set onto
//  the disk, the catalog relinked from sector $F down, then the counts and
//  the bitmap set from the files; a ProDOS bitmap and directory file counts
//  from the directory tree; an Apple Pascal directory's file count. A
//  file's data and chain are never changed: a volume with a chain that
//  loops, runs off the disk, can't be read or shares a sector isn't
//  repaired, nor is a disk with no standard file system. Pure, with no I/O.
//
////////////////////////////////////////////////////////////////////////////////

class DiskRepair
{
public:
    //  The reason after a cross-link finding.
    static constexpr LPCWSTR  kCrossLinkReason = L"Repair can't fix a cross-link without changing a file's data.";

    static RepairPlan  Plan  (const DiskAnalysis & analysis, const RepairOptions & options);

    //  The plan with only the changes whose keys are chosen. A change that
    //  appears only once others are left out, such as a count read through a
    //  catalog that isn't relinked, is left out too.
    static RepairPlan  PlanChosen (const DiskAnalysis & analysis, const RepairOptions & options, const std::set<std::wstring> & chosen);

    //  The same for any planner: each change of its plan that isn't chosen is
    //  skipped, and the volume planned again.
    static RepairPlan  Narrow (const std::function<RepairPlan (const RepairOptions &)> & planner, const RepairOptions & options, const std::set<std::wstring> & chosen,
                               FileSystemVerdict before);

    //  The plan's sectors written over a 140 KB volume in DOS logical order.
    static void        Apply (const RepairPlan & plan, vector<Byte> & inOutSectors);

    static bool        IsFixing    (const RepairChange & change, const Finding & finding);

    //  The keys of the plan's changes that set the finding right.
    static std::set<std::wstring>  GetFixingKeys (const RepairPlan & plan, const Finding & finding);

    //  Every change's key.
    static std::set<std::wstring>  GetKeys       (const RepairPlan & plan);

private:
    struct Work
    {
        const DiskAnalysis &  analysis;
        const RepairOptions & options;
        vector<Byte>          sectors;
        vector<Byte>          original;
        vector<bool>          isReadable;
        int                   volume = 0;
        RepairPlan &          plan;
    };

    //  What a bitmap rebuild marked: for each file, the count of its sectors
    //  or blocks marked used and the findings that sets right; and the same
    //  for the volume's structures, and for those not in any file marked free.
    struct BitmapTally
    {
        vector<int>                markedUsed;
        vector<vector<RepairFix>>  usedFixes;
        int                        structures = 0;
        vector<RepairFix>          structureFixes;
        int                        unowned    = 0;
        vector<RepairFix>          unownedFixes;
    };

    static void   PlanDos33        (Work & work, const FileMap & map);
    static void   PlanHeader       (Work & work, const FileMap & map);
    static void   PlanRelink       (Work & work, const FileMap & map);
    static void   PlanDos33Bitmap  (Work & work, const FileMap & map);
    static void   PlanProDos       (Work & work, const FileMap & map);
    static void   PlanCounts       (Work & work, const FileMap & map);
    static void   CheckChains      (Work & work, const FileMap & map);
    static void   AddBitmapChanges (Work & work, const FileMap & map, const BitmapTally & tally, LPCWSTR unit, LPCWSTR structure);
    static void   AddChange        (Work & work, const FileMap & map, std::wstring key, std::wstring text, vector<RepairFix> fixes, const std::wstring & summary = std::wstring());
    static void   CollectSectors   (Work & work);

    static bool   BuildMap         (const Work & work, std::unique_ptr<SectorSource> & outSource, FileMap & outMap);
    static Byte * GetDos33         (Work & work, int track, int sector);
    static Byte * GetBlockByte     (Work & work, int block, int offset, int & outTrack, int & outSector);
    static bool   IsReadable       (const Work & work, int track, int sector);
    static bool   IsSkipped        (const Work & work, const std::wstring & key);
};
