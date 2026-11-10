#include "Pch.h"

#include "Devices/Disk/Partitions/CffaLayout.h"





static constexpr uint32_t  s_kSliceBlocks = 65536;
static constexpr uint32_t  s_kLargeBlocks = 2097152;
static constexpr int       s_kSliceCount  = 4;
static constexpr int       s_kMoreSlices  = 9;
static constexpr int       s_kLargeCount  = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout::TryDetect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CffaLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT                 hr              = S_OK;
    uint32_t                total           = store.GetBlockCount();
    uint32_t                next            = 0;
    uint32_t                nextLarge       = 0;
    uint32_t                nextSlice       = 0;
    int                     recognized      = 0;
    int                     recognizedLarge = 0;
    int                     recognizedSlice = 0;
    int                     index           = 0;
    std::vector<Partition>  found;
    std::vector<Partition>  large;
    std::vector<Partition>  slices;



    outPartitions.clear();

    //  A store of one slice or less is a plain volume.
    BAIL_OUT_IF (total <= s_kSliceBlocks, S_OK);

    hr = AppendSet (store, s_kSliceBlocks, s_kSliceCount, next, found, recognized);
    CHR (hr);

    BAIL_OUT_IF (recognized == 0, S_OK);

    if (next < total)
    {
        nextLarge = next;
        nextSlice = next;

        hr = AppendSet (store, s_kLargeBlocks, s_kLargeCount, nextLarge, large, recognizedLarge);
        CHR (hr);

        hr = AppendSet (store, s_kSliceBlocks, s_kMoreSlices, nextSlice, slices, recognizedSlice);
        CHR (hr);

        if (recognizedSlice > recognizedLarge)
        {
            found.insert (found.end(), slices.begin(), slices.end());
        }
        else
        {
            found.insert (found.end(), large.begin(), large.end());
        }
    }

    for (Partition & partition : found)
    {
        partition.index = ++index;
    }

    outPartitions = std::move (found);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout::AppendSet
//
//  Up to `sliceCount` volumes of `sliceBlocks` from `ioNextBlock` on. The last
//  one is shorter when the store ends inside it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CffaLayout::AppendSet (IBlockStore             & store,
                               uint32_t                  sliceBlocks,
                               int                       sliceCount,
                               uint32_t                & ioNextBlock,
                               std::vector<Partition>  & ioPartitions,
                               int                     & outRecognized)
{
    HRESULT   hr    = S_OK;
    uint32_t  total = store.GetBlockCount();
    int       i     = 0;



    outRecognized = 0;

    for (i = 0; i < sliceCount && ioNextBlock < total; i++)
    {
        uint32_t  blocks = std::min (sliceBlocks, total - ioNextBlock);

        hr = PartitionLayout::AppendBlockPartition (store, PartitionLayoutKind::Cffa, ioNextBlock, blocks, "", ioPartitions);
        CHR (hr);

        if (ioPartitions.back().volumeKind == VolumeKind::ProDos || ioPartitions.back().isHfs)
        {
            outRecognized++;
        }

        ioNextBlock += blocks;
    }

Error:
    return hr;
}