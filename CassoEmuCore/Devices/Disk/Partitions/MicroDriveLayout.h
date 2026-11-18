#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MicroDriveLayout
//
//  A ///SHH Systeme MicroDrive: a partition map in block 0, in two chunks of
//  up to eight partitions each.
//
//  The map's fields are from CiderPress II's MicroDrive notes, under its
//  Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class MicroDriveLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static HRESULT  AppendChunk (IBlockStore              & store,
                                 const std::vector<Byte>  & map,
                                 int                        count,
                                 size_t                     offStarts,
                                 size_t                     offSizes,
                                 std::vector<Partition>   & ioPartitions);
};