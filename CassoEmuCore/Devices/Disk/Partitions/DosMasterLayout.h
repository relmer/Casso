#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout
//
//  DOS MASTER volumes: DOS 3.3 volumes of one size in the last run of blocks
//  that a ProDOS volume marks in use with no file on them. A disk filled
//  with them has the ProDOS directory inside the first DOS volume.
//  docs/file-formats/dos-master-layout.md is the layout.
//
////////////////////////////////////////////////////////////////////////////////

class DosMasterLayout
{
public:
    //  The list is empty for a store that isn't this layout.
    static HRESULT  TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions);

private:
    static constexpr uint32_t  kMaxProDosBlocks = 65535;

    struct Size
    {
        int       tracks;
        int       sectorsPerTrack;
        uint32_t  blocks;
    };

    //  The run of blocks the bitmap marks in use: from `marked`, and from
    //  `first` with the blocks of a disk it fills, block 0.
    struct Run
    {
        uint32_t  volumeBlocks = 0;
        uint32_t  marked       = 0;
        uint32_t  first        = 0;
        uint32_t  count        = 0;
    };

    static constexpr Size  s_kSizes[] =
    {
        { 35, 16, 280 },
        { 40, 16, 320 },
        { 50, 16, 400 },
        { 50, 32, 800 },
    };

    static HRESULT  GetRun      (IBlockStore & store, Run & outRun);
    static bool     IsFree      (const std::vector<Byte> & bitmap, uint32_t block);
    static HRESULT  IsUnclaimed (IBlockStore & store, const Run & run, bool & outUnclaimed);
    static HRESULT  FillRun     (IBlockStore             & store,
                                 uint32_t                  first,
                                 uint32_t                  count,
                                 const Size              & size,
                                 std::vector<Partition>  & outPartitions);
};
