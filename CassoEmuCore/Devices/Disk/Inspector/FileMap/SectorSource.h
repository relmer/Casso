#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorSource
//
//  The sectors of a 35-track volume as the analysis decoded them (FR-084):
//  volume track N from quarter track N times four, each physical sector from
//  the first field in passing order with that number and an address field
//  that is good or not checked, with its 256 bytes and its result. A track
//  with no record, or a damaged one, has every sector missing. The DOS 3.3,
//  ProDOS and CP/M skews turn a system's own sector into a physical one,
//  through the same tables the analyzer gives each sector.
//
////////////////////////////////////////////////////////////////////////////////

class SectorSource
{
public:
    static constexpr int  kTracks          = 35;
    static constexpr int  kSectors         = 16;
    static constexpr int  kBytes           = 256;
    static constexpr int  kBlocksPerTrack  = 8;
    static constexpr int  kBlocks          = kTracks * kBlocksPerTrack;

    struct Sector
    {
        const Byte *   bytes  = nullptr;
        SectorResult   result = SectorResult::Missing;
    };

    explicit SectorSource (const DiskAnalysis & analysis);

    const Sector &  GetPhysical (int track, int physical) const;
    const Sector &  GetDos33    (int track, int logical) const  { return GetPhysical (track, GetDos33Physical (logical)); }
    const Sector &  GetCpm      (int track, int cpmSector) const { return GetPhysical (track, GetCpmPhysical (cpmSector)); }
    const Sector &  GetHalf     (int block, int half) const     { return GetPhysical (block / kBlocksPerTrack, GetBlockPhysical (block, half)); }

    //  A block's 512 bytes, or false when either half is bad or missing.
    bool  ReadBlock (int block, std::array<Byte, 2 * kBytes> & outBytes) const;

    //  Whether the tracks are mostly 13-sector, which the map does not read.
    bool  IsThirteenSector () const { return m_isThirteenSector; }

    static int  GetDos33Physical (int logical);
    static int  GetBlockPhysical (int block, int half);
    static int  GetCpmPhysical   (int cpmSector) { return (3 * cpmSector) % kSectors; }

    static bool  IsReadable (SectorResult result) { return result == SectorResult::Good || result == SectorResult::NotChecked; }

private:
    std::array<std::array<Sector, kSectors>, kTracks>  m_sectors           = {};
    bool                                               m_isThirteenSector  = false;
    Sector                                             m_missing;
};
