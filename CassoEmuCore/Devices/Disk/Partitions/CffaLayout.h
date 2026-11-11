#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout
//
//  A CFFA card: ProDOS or HFS volumes at fixed places with no map. The first
//  four are 32 MB each. The space after them is one or two 1 GB volumes, as
//  the GS/OS driver of firmware 1.x lays out, or up to nine more of 32 MB, as
//  CFFA 2.0's firmware maps up to 13, whichever has more recognizable
//  volumes. Slots after the last volume are left out.
//
//  The search is CiderPress II's, under its Apache license, extended to the
//  13 volumes of CFFA 2.0's manual.
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