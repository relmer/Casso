#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout
//
//  A CFFA card: ProDOS or HFS volumes at fixed places with no map. The first
//  four are 32 MB slots. The space after them is one or two 1 GB volumes, as
//  the GS/OS driver of firmware 1.x lays out, or up to nine more 32 MB slots,
//  as CFFA 2.0's firmware maps up to 13, whichever has more recognized
//  volumes. Slots after the last recognized volume are left out.
//  docs/file-formats/cffa-layout.md is the layout.
//
////////////////////////////////////////////////////////////////////////////////

class CffaLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static constexpr uint32_t  kSlotBlocks  = 65536;
    static constexpr uint32_t  kLargeBlocks = 2 * 1024 * 1024;

    //  Slots from a block, and whether each has a recognized volume.
    struct Reading
    {
        std::vector<Partition>  slots;
        std::vector<bool>       recognized;
        uint32_t                next  = 0;
        int                     found = 0;
    };

    static HRESULT  Read (IBlockStore & store, uint32_t first, uint32_t slotBlocks, int slotCount, Reading & outReading);
};
