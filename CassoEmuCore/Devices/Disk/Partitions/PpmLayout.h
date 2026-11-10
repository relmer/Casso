#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout
//
//  A Pascal ProFile Manager area: Apple Pascal volumes inside the blocks of
//  the ProDOS file PASCAL.AREA, with a map in the area's first two blocks.
//
//  The map's fields are from CiderPress II's PPM notes, under its Apache
//  license.
//
////////////////////////////////////////////////////////////////////////////////

class PpmLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static HRESULT  FindArea (IBlockStore & store, uint32_t & outFirstBlock, uint32_t & outBlockCount, uint32_t & outVolumeBlocks);
};