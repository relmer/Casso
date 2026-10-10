#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Finding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MapFileSystem / SectorRole / SectorResult
//
//  The file systems the map reads (FR-083), what each sector of a volume is
//  for (FR-086), and what reading it gave, worst last so the worse of a
//  block's two halves is the larger.
//
////////////////////////////////////////////////////////////////////////////////

enum class MapFileSystem
{
    None,
    Dos33,
    ProDos,
    Pascal,
    Cpm,
};


enum class SectorRole
{
    Free,
    BootArea,
    VtocOrKeyBlock,
    CatalogOrDirectory,
    UnusedCatalogSector,
    VolumeBitmap,
    Subdirectory,
    IndexBlock,
    FileData,
    BadBlocksFile,
    AllocatedUnowned,
    OwnedMarkedFree,
    CrossLinked,
    Count,
};


enum class SectorResult
{
    Good,
    NotChecked,
    Bad,
    Missing,
};


enum class ChainReason
{
    None,
    Loop,
    OutsideVolume,
    BadSector,
    MissingSector,
    TooLong,
};


enum class NotMappedReason
{
    None,
    NoFileSystem,
    ThirteenSector,
    OtherSize,
};





////////////////////////////////////////////////////////////////////////////////
//
//  MapCell / FilePlace / MappedFile / FileMap
//
//  A volume's sectors, one cell each: a DOS 3.3 logical sector, a ProDOS or
//  Pascal block, or a CP/M sector, by track and column, with its role, its
//  result and the files that own it. A file's sectors in file order, a hole
//  being a place with no cell; its chain's end; and for a deleted file how
//  many of the sectors its entry leads to are still free (FR-087, FR-088).
//
////////////////////////////////////////////////////////////////////////////////

struct MapCell
{
    SectorRole                   role    = SectorRole::Free;
    SectorResult                 result  = SectorResult::Missing;
    std::array<SectorResult, 2>  halves  = { SectorResult::Missing, SectorResult::Missing };
    vector<int>                  owners;
};


struct FilePlace
{
    int         cell = -1;
    SectorRole  role = SectorRole::FileData;
};


struct MappedFile
{
    static constexpr uint32_t  kChainBroken       = 1;
    static constexpr uint32_t  kCrossLinked       = 2;
    static constexpr uint32_t  kTouchesBadSectors = 4;

    std::wstring       path;
    std::wstring       type;
    bool               isLocked       = false;
    uint64_t           recordedSize   = 0;
    int                recordedCount  = -1;
    vector<FilePlace>  sectors;
    ChainReason        chainReason    = ChainReason::None;
    int                chainCell      = -1;
    bool               isDeleted      = false;
    int                stillFree      = 0;
    int                usedByOthers   = 0;
    uint32_t           states         = 0;

    int  GetUsedCount () const;
};


struct FileMap
{
    MapFileSystem       fileSystem        = MapFileSystem::None;
    std::wstring        volumeName;
    int                 tracks            = 35;
    int                 cellsPerTrack     = 16;
    vector<MapCell>     cells;
    vector<MappedFile>  files;
    bool                isCatalogComplete = true;
    int                 unreadableCell    = -1;
    NotMappedReason     notMapped         = NotMappedReason::None;
    int                 otherSize         = 0;
    std::wstring        likelyCause;
    bool                usesUnchecked     = false;
    vector<Finding>     findings;

    int  GetTrack  (int cell) const { return cell / cellsPerTrack; }
    int  GetColumn (int cell) const { return cell % cellsPerTrack; }

    //  The physical sector that holds a cell, or a half of a block; and the
    //  cell a physical sector holds, with which half of a block it is, or -1.
    int  GetPhysical (int cell, int half = 0) const;
    int  GetCellOf   (int track, int physical, int & outHalf) const;

    //  A sector's place in a file: its role there and its number among the
    //  file's sectors, counting from 1, or 0 when the file does not use it.
    int  GetPlaceInFile (int file, int cell, SectorRole & outRole) const;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapBuilder
//
//  Every volume a disk holds (FR-084, FR-085), each mapped from the sectors
//  the analysis decoded, never decoding a track again (FR-083); or one map
//  that only says why the disk is not mapped (FR-094) and gives each
//  sector's result by track and physical sector. Pure.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskAnalysis;

class FileMapBuilder
{
public:
    static vector<FileMap>  Build (const DiskAnalysis & analysis);
};
