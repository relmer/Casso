#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout
//
//  DOS MASTER volumes: DOS 3.3 volumes of one size in a run of blocks that a
//  ProDOS volume has marked in use and that no file of it has. A disk filled
//  with them has the ProDOS directory inside the first DOS volume.
//
//  The search is CiderPress II's, under its Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class DosMasterLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static HRESULT  FindRegion (IBlockStore & store, uint32_t & outFirstBlock, uint32_t & outBlockCount);
    static HRESULT  TryVolumes (IBlockStore             & store,
                                uint32_t                  firstBlock,
                                uint32_t                  blockCount,
                                uint32_t                  volumeBlocks,
                                std::vector<Partition>  & outPartitions);
};