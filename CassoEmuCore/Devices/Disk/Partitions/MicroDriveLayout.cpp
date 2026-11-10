#include "Pch.h"

#include "Devices/Disk/Partitions/MicroDriveLayout.h"





static constexpr Byte      s_kMagicLow        = 0xCA;
static constexpr Byte      s_kMagicHigh       = 0xCC;
static constexpr size_t    s_kOffFirstCount   = 0x0C;
static constexpr size_t    s_kOffSecondCount  = 0x0D;
static constexpr size_t    s_kOffFirstStarts  = 0x20;
static constexpr size_t    s_kOffFirstSizes   = 0x40;
static constexpr size_t    s_kOffSecondStarts = 0x80;
static constexpr size_t    s_kOffSecondSizes  = 0xA0;
static constexpr int       s_kChunkMax        = 8;

//  The high byte of a size is not part of it.
static constexpr uint32_t  s_kSizeMask        = 0x00FFFFFF;





////////////////////////////////////////////////////////////////////////////////
//
//  MicroDriveLayout::TryDetect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MicroDriveLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT            hr          = S_OK;
    std::vector<Byte>  map;
    bool               found       = false;
    int                firstCount  = 0;
    int                secondCount = 0;



    outPartitions.clear();

    BAIL_OUT_IF (store.GetBlockCount() == 0, S_OK);

    hr = PartitionLayout::ReadBlocks (store, 0, 1, map);
    CHR (hr);

    found       = map[0] == s_kMagicLow && map[1] == s_kMagicHigh;
    firstCount  = map[s_kOffFirstCount];
    secondCount = map[s_kOffSecondCount];

    BAIL_OUT_IF (!found || firstCount > s_kChunkMax || secondCount > s_kChunkMax || firstCount + secondCount == 0, S_OK);

    hr = AppendChunk (store, map, firstCount, s_kOffFirstStarts, s_kOffFirstSizes, outPartitions);
    CHR (hr);

    hr = AppendChunk (store, map, secondCount, s_kOffSecondStarts, s_kOffSecondSizes, outPartitions);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MicroDriveLayout::AppendChunk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MicroDriveLayout::AppendChunk (IBlockStore              & store,
                                       const std::vector<Byte>  & map,
                                       int                        count,
                                       size_t                     offStarts,
                                       size_t                     offSizes,
                                       std::vector<Partition>   & ioPartitions)
{
    HRESULT  hr = S_OK;
    int      i  = 0;



    for (i = 0; i < count; i++)
    {
        uint32_t  start = PartitionLayout::ReadLong (map, offStarts + (size_t) i * sizeof (uint32_t));
        uint32_t  size  = PartitionLayout::ReadLong (map, offSizes  + (size_t) i * sizeof (uint32_t)) & s_kSizeMask;

        hr = PartitionLayout::AppendBlockPartition (store, PartitionLayoutKind::MicroDrive, start, size, "", ioPartitions);
        CHR (hr);
    }

Error:
    return hr;
}