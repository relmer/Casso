#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UniDosLayout
//
//  Two 400K DOS 3.3 volumes on an 800K disk, each 50 tracks of 32 sectors.
//  AmDOS and UniDOS have one volume in the first 800 blocks and one in the
//  last 800. OzDOS has one volume in the first half of every block and one in
//  the second half.
//
//  The layouts are from CiderPress II's notes, under its Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class UniDosLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static HRESULT  TryPair (IBlockStore & store, bool halves, std::vector<Partition> & outPartitions);
};