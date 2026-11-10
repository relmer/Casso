#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionLayout.h"

#include "Devices/Disk/Partitions/CffaLayout.h"
#include "Devices/Disk/Partitions/DosMasterLayout.h"
#include "Devices/Disk/Partitions/FocusDriveLayout.h"
#include "Devices/Disk/Partitions/MicroDriveLayout.h"
#include "Devices/Disk/Partitions/PpmLayout.h"
#include "Devices/Disk/Partitions/UniDosLayout.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::Detect
//
//  The layouts with a signature are checked first, then the ones found inside a
//  ProDOS volume, then the ones found by the volumes at fixed places.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::Detect (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<Partition> & outPartitions)
{
    typedef HRESULT (* Probe) (IBlockStore &, std::vector<Partition> &);

    static constexpr Probe  kProbes[] =
    {
        MicroDriveLayout::TryDetect,
        FocusDriveLayout::TryDetect,
        PpmLayout::TryDetect,
        DosMasterLayout::TryDetect,
        UniDosLayout::TryDetect,
        CffaLayout::TryDetect,
    };



    HRESULT  hr = S_OK;



    outKind = PartitionLayoutKind::None;
    outPartitions.clear();

    for (Probe probe : kProbes)
    {
        if (outPartitions.empty())
        {
            hr = probe (store, outPartitions);
            CHR (hr);
        }
    }

    if (!outPartitions.empty())
    {
        outKind = outPartitions.front().layout;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::ReadPartition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::ReadPartition (IBlockStore & store, const Partition & partition, std::vector<Byte> & outSectors)
{
    static constexpr size_t  kHalfBytes = IBlockStore::kBlockBytes / 2;



    HRESULT            hr     = S_OK;
    std::vector<Byte>  raw;
    size_t             offset = (partition.span == PartitionSpan::SecondHalves) ? kHalfBytes : 0;
    uint32_t           block  = 0;



    outSectors.clear();

    hr = ReadBlocks (store, partition.firstBlock, partition.blockCount, raw);
    CHR (hr);

    if (partition.span != PartitionSpan::WholeBlocks)
    {
        outSectors.resize ((size_t) partition.blockCount * kHalfBytes);

        for (block = 0; block < partition.blockCount; block++)
        {
            std::copy_n (raw.begin() + (ptrdiff_t) ((size_t) block * IBlockStore::kBlockBytes + offset),
                         kHalfBytes,
                         outSectors.begin() + (ptrdiff_t) ((size_t) block * kHalfBytes));
        }
    }
    else if (!partition.dosSectors && partition.blockCount == kFloppyBlocks)
    {
        VolumeImage::ProDosFileToDosLogical (raw, outSectors);
    }
    else
    {
        outSectors = std::move (raw);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::WritePartition
//
//  A partition of half blocks is written over the blocks as they are read, so
//  the other volume's halves are written back as they were.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::WritePartition (IBlockStore & store, const Partition & partition, const std::vector<Byte> & sectors)
{
    static constexpr size_t  kHalfBytes = IBlockStore::kBlockBytes / 2;



    HRESULT            hr        = S_OK;
    std::vector<Byte>  raw;
    bool               halves    = partition.span != PartitionSpan::WholeBlocks;
    size_t             unitBytes = halves ? kHalfBytes : IBlockStore::kBlockBytes;
    size_t             offset    = (partition.span == PartitionSpan::SecondHalves) ? kHalfBytes : 0;
    bool               sized     = sectors.size() == (size_t) partition.blockCount * unitBytes;
    uint32_t           block     = 0;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));
    BAIL_OUT_IF (partition.blockCount == 0, S_OK);

    if (halves)
    {
        hr = ReadBlocks (store, partition.firstBlock, partition.blockCount, raw);
        CHR (hr);

        for (block = 0; block < partition.blockCount; block++)
        {
            std::copy_n (sectors.begin() + (ptrdiff_t) ((size_t) block * kHalfBytes),
                         kHalfBytes,
                         raw.begin() + (ptrdiff_t) ((size_t) block * IBlockStore::kBlockBytes + offset));
        }
    }
    else if (!partition.dosSectors && partition.blockCount == kFloppyBlocks)
    {
        VolumeImage::DosLogicalToProDosFile (sectors, raw);
    }
    else
    {
        raw = sectors;
    }

    hr = store.WriteBlocks (partition.firstBlock, raw);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::GetLayoutName
//
////////////////////////////////////////////////////////////////////////////////

const char * PartitionLayout::GetLayoutName (PartitionLayoutKind kind)
{
    switch (kind)
    {
        case PartitionLayoutKind::Cffa:       return "CFFA";
        case PartitionLayoutKind::FocusDrive: return "FocusDrive";
        case PartitionLayoutKind::MicroDrive: return "MicroDrive";
        case PartitionLayoutKind::DosMaster:  return "DOS MASTER";
        case PartitionLayoutKind::AmDos:      return "AmDOS";
        case PartitionLayoutKind::UniDos:     return "UniDOS";
        case PartitionLayoutKind::OzDos:      return "OzDOS";
        case PartitionLayoutKind::Ppm:        return "Pascal ProFile Manager";
        default:                              return "";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::ReadBlocks (IBlockStore & store, uint32_t first, uint32_t count, std::vector<Byte> & outBytes)
{
    HRESULT  hr     = S_OK;
    bool     inside = (uint64_t) first + count <= store.GetBlockCount();



    outBytes.clear();

    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));
    BAIL_OUT_IF (count == 0, S_OK);

    outBytes.resize ((size_t) count * IBlockStore::kBlockBytes);

    hr = store.ReadBlocks (first, count, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word PartitionLayout::ReadWord (const std::vector<Byte> & bytes, size_t offset)
{
    return (Word) (bytes[offset] | (bytes[offset + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PartitionLayout::ReadLong (const std::vector<Byte> & bytes, size_t offset)
{
    return (uint32_t) bytes[offset]
         | ((uint32_t) bytes[offset + 1] << 8)
         | ((uint32_t) bytes[offset + 2] << 16)
         | ((uint32_t) bytes[offset + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::AppendBlockPartition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::AppendBlockPartition (IBlockStore             & store,
                                               PartitionLayoutKind       layout,
                                               uint32_t                  firstBlock,
                                               uint32_t                  blockCount,
                                               const std::string       & name,
                                               std::vector<Partition>  & ioPartitions)
{
    HRESULT    hr           = S_OK;
    Partition  partition;
    uint32_t   total        = store.GetBlockCount();
    uint32_t   available    = (firstBlock < total) ? total - firstBlock : 0;
    uint32_t   volumeBlocks = 0;



    partition.index      = (int) ioPartitions.size() + 1;
    partition.firstBlock = firstBlock;
    partition.blockCount = std::min (blockCount, available);
    partition.layout     = layout;
    partition.damaged    = blockCount > available;
    partition.name       = name;

    hr = ProbeBlockVolume (store, partition.firstBlock, partition.blockCount, partition.volumeKind, partition.isHfs, volumeBlocks);
    CHR (hr);

    ioPartitions.push_back (std::move (partition));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::ProbeBlockVolume
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::ProbeBlockVolume (IBlockStore & store,
                                           uint32_t      firstBlock,
                                           uint32_t      blockCount,
                                           VolumeKind  & outKind,
                                           bool        & outIsHfs,
                                           uint32_t    & outVolumeBlocks)
{
    static constexpr uint32_t  kProbeBlocks = 3;



    HRESULT            hr = S_OK;
    std::vector<Byte>  blocks;



    outKind         = VolumeKind::Unknown;
    outIsHfs        = false;
    outVolumeBlocks = 0;

    BAIL_OUT_IF (blockCount < kProbeBlocks, S_OK);

    hr = ReadBlocks (store, firstBlock, kProbeBlocks, blocks);
    CHR (hr);

    if (IsProDosDirectory (blocks, blockCount, outVolumeBlocks))
    {
        outKind = VolumeKind::ProDos;
    }
    else
    {
        outIsHfs = IsHfsDirectory (blocks, blockCount, outVolumeBlocks);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::IsDosVolume
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionLayout::IsDosVolume (IBlockStore & store, const Partition & partition, int tracks, int sectorsPerTrack, bool & outIsDos)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  sectors;
    Dos33Geometry      geometry;



    outIsDos = false;

    hr = ReadPartition (store, partition, sectors);
    CHR (hr);

    outIsDos = Dos33Geometry::TryDetect (sectors, geometry)
            && geometry.tracks          == tracks
            && geometry.sectorsPerTrack == sectorsPerTrack;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::IsProDosDirectory
//
//  The volume directory header of block 2: a storage type of $F, a name of
//  letters, digits and periods that begins with a letter, the entry length
//  and entries per block of every ProDOS directory, and a block count and
//  bitmap pointer inside the run.
//
////////////////////////////////////////////////////////////////////////////////

bool PartitionLayout::IsProDosDirectory (const std::vector<Byte> & blocks, uint32_t blockCount, uint32_t & outVolumeBlocks)
{
    static constexpr size_t  kHeader          = 2 * IBlockStore::kBlockBytes + 0x04;
    static constexpr size_t  kOffEntryLength  = 0x1F;
    static constexpr size_t  kOffPerBlock     = 0x20;
    static constexpr size_t  kOffBitmap       = 0x23;
    static constexpr size_t  kOffTotalBlocks  = 0x25;
    static constexpr Byte    kEntryLength     = 0x27;
    static constexpr Byte    kEntriesPerBlock = 0x0D;



    Byte    typeLength = blocks[kHeader];
    size_t  nameLength = (size_t) (typeLength & 0x0F);
    Word    total      = ReadWord (blocks, kHeader + kOffTotalBlocks);
    Word    bitmap     = ReadWord (blocks, kHeader + kOffBitmap);
    bool    nameOk     = nameLength > 0;
    size_t  i          = 0;



    for (i = 0; i < nameLength; i++)
    {
        Byte  ch     = blocks[kHeader + 1 + i];
        bool  letter = ch >= 'A' && ch <= 'Z';
        bool  other  = (ch >= '0' && ch <= '9') || ch == '.';

        nameOk = nameOk && (letter || (other && i > 0));
    }

    outVolumeBlocks = total;

    return (typeLength & 0xF0) == 0xF0
        && nameOk
        && blocks[kHeader + kOffEntryLength] == kEntryLength
        && blocks[kHeader + kOffPerBlock]    == kEntriesPerBlock
        && total  > 0 && total  <= blockCount
        && bitmap > 0 && bitmap <  total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayout::IsHfsDirectory
//
//  The master directory block of block 2: the signature "BD", an allocation
//  block size that is a multiple of 512, and a volume that ends inside the
//  run. Its fields are big-endian.
//
////////////////////////////////////////////////////////////////////////////////

bool PartitionLayout::IsHfsDirectory (const std::vector<Byte> & blocks, uint32_t blockCount, uint32_t & outVolumeBlocks)
{
    static constexpr size_t  kMdb                = 2 * IBlockStore::kBlockBytes;
    static constexpr size_t  kOffAllocationCount = 0x12;
    static constexpr size_t  kOffAllocationBytes = 0x14;
    static constexpr size_t  kOffFirstAllocation = 0x1C;

    //  The boot blocks ahead of the volume's blocks, and the alternate master
    //  directory block and the unused block after them.
    static constexpr uint64_t  kBlocksOutside = 2;



    uint32_t  allocationCount = ((uint32_t) blocks[kMdb + kOffAllocationCount] << 8) | blocks[kMdb + kOffAllocationCount + 1];
    uint32_t  allocationBytes = ((uint32_t) blocks[kMdb + kOffAllocationBytes]     << 24)
                              | ((uint32_t) blocks[kMdb + kOffAllocationBytes + 1] << 16)
                              | ((uint32_t) blocks[kMdb + kOffAllocationBytes + 2] << 8)
                              |  (uint32_t) blocks[kMdb + kOffAllocationBytes + 3];
    uint32_t  firstAllocation = ((uint32_t) blocks[kMdb + kOffFirstAllocation] << 8) | blocks[kMdb + kOffFirstAllocation + 1];
    bool      signature       = blocks[kMdb] == 'B' && blocks[kMdb + 1] == 'D';
    bool      sized           = allocationBytes != 0 && allocationBytes % IBlockStore::kBlockBytes == 0;
    uint64_t  volumeBlocks    = (uint64_t) allocationCount * (allocationBytes / IBlockStore::kBlockBytes) + firstAllocation + kBlocksOutside;



    outVolumeBlocks = (signature && sized && volumeBlocks <= blockCount) ? (uint32_t) volumeBlocks : 0;

    return outVolumeBlocks != 0;
}