#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"

class SectorSource;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairOptions / RepairSector / RepairPlan
//
//  Repair's options, a sector it writes (by track and DOS logical
//  sector, all 256 bytes), and the plan: whether the volume can be repaired
//  and why not, a line for each change, the sectors, and the verdict before
//  and after.
//
////////////////////////////////////////////////////////////////////////////////

struct RepairOptions
{
    bool  isKeepingUnowned = false;
};


struct RepairSector
{
    static constexpr int  kBytes = 256;

    int                         track  = 0;
    int                         sector = 0;
    std::array<Byte, kBytes>    bytes  = {};
};


struct RepairPlan
{
    bool                   canRepair = false;
    std::wstring           reason;
    vector<std::wstring>   changes;
    vector<RepairSector>   sectors;
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
    static RepairPlan  Plan  (const DiskAnalysis & analysis, const RepairOptions & options);

    //  The plan's sectors written over a 140 KB volume in DOS logical order.
    static void        Apply (const RepairPlan & plan, vector<Byte> & inOutSectors);

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

    static void   PlanDos33        (Work & work, const FileMap & map);
    static void   PlanHeader       (Work & work, const FileMap & map);
    static void   PlanRelink       (Work & work, const FileMap & map);
    static void   PlanDos33Bitmap  (Work & work, const FileMap & map);
    static void   PlanProDos       (Work & work, const FileMap & map);
    static void   PlanCounts       (Work & work, const FileMap & map);
    static void   CheckChains      (Work & work, const FileMap & map);
    static void   AddBitmapChanges (Work & work, const FileMap & map, const vector<int> & markedUsed, int unowned, int structures, LPCWSTR unit);
    static void   CollectSectors   (Work & work);

    static bool   BuildMap         (const Work & work, std::unique_ptr<SectorSource> & outSource, FileMap & outMap);
    static Byte * GetDos33         (Work & work, int track, int sector);
    static Byte * GetBlockByte     (Work & work, int block, int offset, int & outTrack, int & outSector);
    static bool   IsReadable       (const Work & work, int track, int sector);
};
