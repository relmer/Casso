#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout
//
//  A CFFA card: ProDOS or HFS volumes at fixed places with no map. The first
//  four are 32 MB each. The space after them is one or two 1 GB volumes or up
//  to four more of 32 MB, whichever has more recognizable volumes.
//
//  The search is CiderPress II's, under its Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class CffaLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static HRESULT  AppendSet (IBlockStore             & store,
                               uint32_t                  sliceBlocks,
                               int                       sliceCount,
                               uint32_t                & ioNextBlock,
                               std::vector<Partition>  & ioPartitions,
                               int                     & outRecognized);
};