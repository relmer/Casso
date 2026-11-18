#include "Pch.h"

#include "Devices/Disk/Partitions/UniDosLayout.h"





static constexpr uint32_t  s_kDiskBlocks         = 1600;
static constexpr uint32_t  s_kVolumeBlocks       = 800;
static constexpr int       s_kTracks             = 50;
static constexpr int       s_kSectorsPerTrack    = 32;
static constexpr int       s_kVolumeCount        = 2;

//  The block of the first volume's VTOC, track 17 sector 0, and the catalog
//  sector AmDOS has there. UniDOS has sector 31.
static constexpr uint32_t  s_kVtocBlock          = 17 * s_kSectorsPerTrack / 2;
static constexpr size_t    s_kVtocOffCatalog     = 0x02;
static constexpr Byte      s_kAmDosCatalogSector = 15;





////////////////////////////////////////////////////////////////////////////////
//
//  UniDosLayout::TryDetect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UniDosLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  vtoc;



    outPartitions.clear();

    BAIL_OUT_IF (store.GetBlockCount() != s_kDiskBlocks, S_OK);

    hr = TryPair (store, false, outPartitions);
    CHR (hr);

    if (outPartitions.empty())
    {
        hr = TryPair (store, true, outPartitions);
        CHR (hr);
    }
    else
    {
        hr = PartitionLayout::ReadBlocks (store, s_kVtocBlock, 1, vtoc);
        CHR (hr);

        if (vtoc[s_kVtocOffCatalog] == s_kAmDosCatalogSector)
        {
            for (Partition & partition : outPartitions)
            {
                partition.layout = PartitionLayoutKind::AmDos;
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UniDosLayout::TryPair
//
//  Both volumes, when both are DOS volumes of 50 tracks and 32 sectors.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UniDosLayout::TryPair (IBlockStore & store, bool halves, std::vector<Partition> & outPartitions)
{
    HRESULT                 hr     = S_OK;
    std::vector<Partition>  volumes;
    bool                    allDos = true;
    int                     i      = 0;



    outPartitions.clear();

    for (i = 0; i < s_kVolumeCount && allDos; i++)
    {
        Partition  volume;

        volume.index      = i + 1;
        volume.volumeKind = VolumeKind::Dos33;
        volume.dosSectors = true;

        if (halves)
        {
            volume.blockCount = s_kDiskBlocks;
            volume.layout     = PartitionLayoutKind::OzDos;
            volume.span       = (i == 0) ? PartitionSpan::FirstHalves : PartitionSpan::SecondHalves;
        }
        else
        {
            volume.firstBlock = (uint32_t) i * s_kVolumeBlocks;
            volume.blockCount = s_kVolumeBlocks;
            volume.layout     = PartitionLayoutKind::UniDos;
        }

        hr = PartitionLayout::IsDosVolume (store, volume, s_kTracks, s_kSectorsPerTrack, allDos);
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