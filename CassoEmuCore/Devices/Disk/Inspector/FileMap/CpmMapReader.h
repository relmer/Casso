#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMapWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpmMapReader
//
//  A CP/M 2.2 volume on an Apple II disk (FR-085, FR-087, FR-088, FR-093):
//  its 64 directory entries in CP/M sectors $0 to $7 of track 3, each file
//  its user and name with its extents' blocks in extent order, a block of 0
//  a hole, and each 1 KB allocation block four CP/M sectors. The system
//  entry of user 31, whose blocks lie on tracks 0 to 2, is the system area,
//  not a file; a free entry that still holds a name and blocks is a deleted
//  file. CP/M keeps no bitmap, so a sector no file owns is free.
//
////////////////////////////////////////////////////////////////////////////////

class CpmMapReader
{
public:
    static constexpr int  kDirectoryTrack   = 3;
    static constexpr int  kDirectorySectors = 8;
    static constexpr int  kEntries          = 64;
    static constexpr int  kEntrySize        = 32;
    static constexpr int  kVolumeBlocks     = 128;
    static constexpr int  kSystemBlocks     = 140;
    static constexpr int  kSectorsPerBlock  = 4;
    static constexpr Byte kFree             = 0xE5;

    struct Entry
    {
        int                   index     = 0;
        int                   user      = 0;
        std::wstring          name;
        int                   extent    = 0;
        int                   records   = 0;
        bool                  isLocked  = false;
        std::array<Byte, 16>  blocks    = {};
    };

    //  Whether track 3's directory holds a CP/M volume (FR-085): a valid
    //  entry in use and no invalid one, or more than four valid ones in use;
    //  a directory of free entries alone is an empty volume.
    static bool  IsFound (const SectorSource & source);
    static void  Read    (const SectorSource & source, FileMap & outMap);

    //  The CP/M sector's cell for allocation block b's sector n.
    static int   GetBlockCell (int block, int sector);

private:
    static bool          ReadDirectory (const SectorSource & source, vector<Byte> & outBytes, int & outBadSector);
    static bool          IsValid       (const Byte * entry, bool & outIsInUse);
    static Entry         ReadEntry     (const Byte * entry, int index);
};
