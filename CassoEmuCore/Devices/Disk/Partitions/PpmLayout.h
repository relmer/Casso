#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout
//
//  A Pascal ProFile Manager region: Apple Pascal volumes inside the blocks
//  of the ProDOS file PASCAL.AREA, with a map in the region's first two
//  blocks. docs/file-formats/ppm-layout.md is the layout.
//
////////////////////////////////////////////////////////////////////////////////

class PpmLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static constexpr uint32_t  kMaxProDosBlocks = 65535;
    static constexpr uint32_t  kRootBlock       = 2;
    static constexpr uint32_t  kMapBlocks       = 2;
    static constexpr int       kMaxVolumes      = 31;

    //  The region's place, from its directory entry.
    struct Region
    {
        uint32_t  first        = 0;
        uint32_t  count        = 0;
        uint32_t  volumeBlocks = 0;
    };

    static HRESULT  FindRegion  (IBlockStore & store, Region & outRegion);
    static bool     IsAreaEntry (const std::vector<Byte> & block, size_t entry);
    static HRESULT  ListVolumes (IBlockStore & store, const Region & region, const std::vector<Byte> & map, std::vector<Partition> & outPartitions);
};
