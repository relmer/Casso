#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FocusDriveLayout
//
//  A Parsons Engineering FocusDrive: a signature and a partition map in block
//  0, and the partition names in blocks 1 and 2.
//
//  The map's fields are from CiderPress II's FocusDrive notes, under its
//  Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class FocusDriveLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);
};