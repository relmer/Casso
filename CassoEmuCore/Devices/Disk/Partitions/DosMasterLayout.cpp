#include "Pch.h"

#include "Devices/Disk/Partitions/DosMasterLayout.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





//  The volume sizes DOS MASTER makes: 140K, 160K, 200K and 400K.
static constexpr uint32_t  s_kVolumeSizes[]     = { 280, 320, 400, 800 };
static constexpr uint32_t  s_kMaxProDosBlocks   = 65535;
static constexpr uint32_t  s_kDiskBlocks        = 1600;

//  The blocks a filled 800K disk has outside the run: the ProDOS volume
//  structure, and with it 28K of storage for a bootable disk.
static constexpr uint32_t  s_kStructureBlocks   = 7;
static constexpr uint32_t  s_kBootStorageBlocks = 56;

static constexpr uint32_t  s_kLargeVolumeBlocks = 800;
static constexpr int       s_kSectorsPerTrack   = 16;
static constexpr int       s_kLargeSectors      = 32;





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::TryDetect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT   hr         = S_OK;
    uint32_t  firstBlock = 0;
    uint32_t  blockCount = 0;



    outPartitions.clear();

    hr = FindRegion (store, firstBlock, blockCount);
    CHR (hr);

    BAIL_OUT_IF (blockCount == 0, S_OK);

    for (uint32_t volumeBlocks : s_kVolumeSizes)
    {
        if (outPartitions.empty())
        {
            hr = TryVolumes (store, firstBlock, blockCount, volumeBlocks, outPartitions);
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::FindRegion
//
//  The last run of blocks of a ProDOS volume that are in use in its bitmap
//  and are part of no file or directory. `outBlockCount` is zero when the
//  store isn't a ProDOS volume or has no such run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::FindRegion (IBlockStore & store, uint32_t & outFirstBlock, uint32_t & outBlockCount)
{
    HRESULT                   hr           = S_OK;
    HRESULT                   hrReport     = S_OK;
    uint32_t                  total        = store.GetBlockCount();
    uint32_t                  volumeBlocks = 0;
    uint32_t                  units        = 0;
    uint32_t                  first        = 0;
    uint32_t                  last         = 0;
    uint32_t                  count        = 0;
    uint32_t                  block        = 0;
    VolumeKind                kind         = VolumeKind::Unknown;
    bool                      isHfs        = false;
    std::vector<Byte>         bytes;
    std::unique_ptr<IVolume>  volume;
    VolumeIntegrityReport     report;



    outFirstBlock = 0;
    outBlockCount = 0;

    BAIL_OUT_IF (total <= PartitionLayout::kFloppyBlocks || total > s_kMaxProDosBlocks, S_OK);

    hr = PartitionLayout::ProbeBlockVolume (store, 0, total, kind, isHfs, volumeBlocks);
    CHR (hr);

    BAIL_OUT_IF (kind != VolumeKind::ProDos, S_OK);

    hr = PartitionLayout::ReadBlocks (store, 0, total, bytes);
    CHR (hr);

    volume = VolumeImage::CreateVolume (VolumeKind::ProDos, bytes);
    BAIL_OUT_IF (volume == nullptr, S_OK);

    hrReport = volume->BuildIntegrityReport (report);
    BAIL_OUT_IF (FAILED (hrReport), S_OK);

    units = report.GetUnitCount();

    for (block = total - 1; block > 0 && first == 0; block--)
    {
        bool  inRun = block < units && report.IsAllocatedInFreeMap (block) && !report.IsClaimed (block);

        if (inRun && last == 0)
        {
            last = block;
        }
        else if (!inRun && last != 0)
        {
            first = block + 1;
        }
    }

    BAIL_OUT_IF (first == 0, S_OK);

    count = last - first + 1;

    if (total == s_kDiskBlocks && (count == s_kDiskBlocks - s_kStructureBlocks || count == s_kDiskBlocks - s_kStructureBlocks - s_kBootStorageBlocks))
    {
        first = 0;
        count = s_kDiskBlocks;
    }

    outFirstBlock = first;
    outBlockCount = count;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosMasterLayout::TryVolumes
//
//  The run as volumes of one size, when the run is a multiple of the size and
//  every one of them is a DOS volume.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosMasterLayout::TryVolumes (IBlockStore             & store,
                                     uint32_t                  firstBlock,
                                     uint32_t                  blockCount,
                                     uint32_t                  volumeBlocks,
                                     std::vector<Partition>  & outPartitions)
{
    HRESULT                 hr              = S_OK;
    std::vector<Partition>  volumes;
    int                     sectorsPerTrack = (volumeBlocks < s_kLargeVolumeBlocks) ? s_kSectorsPerTrack : s_kLargeSectors;
    int                     tracks          = (int) volumeBlocks / (sectorsPerTrack / 2);
    bool                    allDos          = true;
    uint32_t                i               = 0;



    outPartitions.clear();

    BAIL_OUT_IF (blockCount < volumeBlocks || blockCount % volumeBlocks != 0, S_OK);

    for (i = 0; i < blockCount / volumeBlocks && allDos; i++)
    {
        Partition  volume;

        volume.index      = (int) i + 1;
        volume.firstBlock = firstBlock + i * volumeBlocks;
        volume.blockCount = volumeBlocks;
        volume.layout     = PartitionLayoutKind::DosMaster;
        volume.volumeKind = VolumeKind::Dos33;
        volume.dosSectors = true;

        hr = PartitionLayout::IsDosVolume (store, volume, tracks, sectorsPerTrack, allDos);
        CHR (hr);

        volumes.push_back (std::move (volume));
    }

    if (allDos)
    {
        outPartitions = std::move (volumes);
    }

Error:
    return hr;
}