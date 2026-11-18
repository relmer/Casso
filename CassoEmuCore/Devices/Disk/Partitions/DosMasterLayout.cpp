#include "Pch.h"

#include "Devices/Disk/Partitions/DosMasterLayout.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::GetRun
//
//  The last run of blocks the volume bitmap of the ProDOS volume at block 0
//  marks in use, or a count of 0. Only the directory and bitmap blocks are
//  read. A run from block 7 or 63 to the volume's last block starts at
//  block 0, as DOS MASTER lays out a disk it fills.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::GetRun (IBlockStore & store, Run & outRun)
{
    static constexpr size_t  kBitmapPointer = 0x27;



    HRESULT            hr           = S_OK;
    uint32_t           total        = store.GetBlockCount();
    uint32_t           volumeBlocks = 0;
    uint32_t           end          = 0;
    uint32_t           first        = 0;
    uint32_t           bitmapBlock  = 0;
    VolumeKind         kind         = VolumeKind::Unknown;
    bool               isHfs        = false;
    bool               proDos       = false;
    std::vector<Byte>  directory;
    std::vector<Byte>  bitmap;



    outRun = Run();

    hr = PartitionLayout::ProbeBlockVolume (store, 0, (std::min) (total, kMaxProDosBlocks), kind, isHfs, volumeBlocks);
    CHR (hr);

    proDos = kind == VolumeKind::ProDos && volumeBlocks > 0 && volumeBlocks <= total;
    BAIL_OUT_IF (!proDos, S_OK);

    hr = PartitionLayout::ReadBlocks (store, 2, 1, directory);
    CHR (hr);

    bitmapBlock = PartitionLayout::ReadWord (directory, kBitmapPointer);
    BAIL_OUT_IF (bitmapBlock == 0 || bitmapBlock >= volumeBlocks, S_OK);

    hr = PartitionLayout::ReadBlocks (store, bitmapBlock, (volumeBlocks + 4095) / 4096, bitmap);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    end = volumeBlocks;

    while (end > 0 && IsFree (bitmap, end - 1))
    {
        end--;
    }

    BAIL_OUT_IF (end == 0, S_OK);

    first = end - 1;

    while (first > 0 && !IsFree (bitmap, first - 1))
    {
        first--;
    }

    outRun.volumeBlocks = volumeBlocks;
    outRun.marked       = first;
    outRun.first        = ((first == 7 || first == 63) && end == volumeBlocks) ? 0 : first;
    outRun.count        = end - outRun.first;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::IsFree
//
//  A set bit in the ProDOS volume bitmap is a free block, block 0 at the
//  high bit of the first byte.
//
////////////////////////////////////////////////////////////////////////////////

bool DosMasterLayout::IsFree (const std::vector<Byte> & bitmap, uint32_t block)
{
    size_t  at = block / 8;



    return at < bitmap.size() && (bitmap[at] & (0x80 >> (block % 8))) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::IsUnclaimed
//
//  True when no file, directory or volume structure of the ProDOS volume
//  has a block of the run from where the bitmap marks it. The whole volume
//  is read for its files, so this is checked only once the run's DOS
//  volumes are found.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::IsUnclaimed (IBlockStore & store, const Run & run, bool & outUnclaimed)
{
    HRESULT                   hr     = S_OK;
    std::vector<Byte>         blocks;
    std::unique_ptr<IVolume>  volume;
    VolumeIntegrityReport     report;



    outUnclaimed = false;

    hr = PartitionLayout::ReadBlocks (store, 0, run.volumeBlocks, blocks);
    CHR (hr);

    volume = VolumeImage::CreateVolume (VolumeKind::ProDos, blocks);
    BAIL_OUT_IF (volume == nullptr, S_OK);

    hr = volume->BuildIntegrityReport (report);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    outUnclaimed = true;

    for (uint32_t block = run.marked; block < run.first + run.count && outUnclaimed; block++)
    {
        outUnclaimed = !report.IsClaimed (block);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::FillRun
//
//  The run as volumes of one size, when the size divides it and every one
//  of them is a DOS 3.3 volume of its tracks and sectors. Otherwise the list
//  is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::FillRun (IBlockStore             & store,
                                  uint32_t                  first,
                                  uint32_t                  count,
                                  const Size              & size,
                                  std::vector<Partition>  & outPartitions)
{
    HRESULT    hr    = S_OK;
    bool       isDos = false;
    Partition  partition;



    outPartitions.clear();

    BAIL_OUT_IF (count < size.blocks || count % size.blocks != 0, S_OK);

    for (uint32_t at = first; at < first + count; at += size.blocks)
    {
        partition            = Partition();
        partition.index      = (int) outPartitions.size() + 1;
        partition.firstBlock = at;
        partition.blockCount = size.blocks;
        partition.layout     = PartitionLayoutKind::DosMaster;
        partition.volumeKind = VolumeKind::Dos33;
        partition.dosSectors = true;

        hr = PartitionLayout::IsDosVolume (store, partition, size.tracks, size.sectorsPerTrack, isDos);
        CHR (hr);

        if (!isDos)
        {
            outPartitions.clear();
            break;
        }

        outPartitions.push_back (partition);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::TryDetect
//
//  The sizes are checked smallest first, and the first that fills the run
//  is the layout, when no file of the ProDOS volume has a block of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT  hr        = S_OK;
    Run      run;
    bool     unclaimed = false;



    outPartitions.clear();

    hr = GetRun (store, run);
    CHR (hr);

    for (const Size & size : s_kSizes)
    {
        if (run.count > 0 && outPartitions.empty())
        {
            hr = FillRun (store, run.first, run.count, size, outPartitions);
            CHR (hr);
        }
    }

    BAIL_OUT_IF (outPartitions.empty(), S_OK);

    hr = IsUnclaimed (store, run, unclaimed);
    CHR (hr);

    if (!unclaimed)
    {
        outPartitions.clear();
    }

Error:
    return hr;
}
