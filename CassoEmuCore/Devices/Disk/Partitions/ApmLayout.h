#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApmLayout
//
//  A hard disk or CD image with an Apple Partition Map. Each entry of the map
//  is a partition, except the map itself and free space, and the partition
//  has the entry's name. The map is read with ApplePartitionMap.
//
////////////////////////////////////////////////////////////////////////////////

class ApmLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);
};
