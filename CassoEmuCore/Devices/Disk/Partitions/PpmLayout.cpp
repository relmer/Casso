#include "Pch.h"

#include "Devices/Disk/Partitions/PpmLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::IsAreaEntry
//
//  A root directory entry of storage type $4 and the name PASCAL.AREA.
//
////////////////////////////////////////////////////////////////////////////////

bool PpmLayout::IsAreaEntry (const std::vector<Byte> & block, size_t entry)
{
    static constexpr std::string_view  kName        = "PASCAL.AREA";
    static constexpr Byte              kPascalArea  = 0x04;



    Byte              typeLength = block[entry];
    std::string_view  name ((const char *) block.data() + entry + 1, typeLength & 0x0F);



    return (typeLength >> 4) == kPascalArea && name == kName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::FindRegion
//
//  The PASCAL.AREA entry of the root directory of the ProDOS volume at
//  block 0, its region the key pointer and the blocks used. The count is 0
//  without that entry, or when the region isn't inside the volume.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PpmLayout::FindRegion (IBlockStore & store, Region & outRegion)
{
    static constexpr size_t  kEntryBytes     = 0x27;
    static constexpr size_t  kEntriesAt      = 0x04;
    static constexpr int     kEntriesInBlock = 13;
    static constexpr size_t  kNextBlock      = 0x02;
    static constexpr size_t  kKeyPointer     = 0x11;
    static constexpr size_t  kBlocksUsed     = 0x13;



    HRESULT            hr     = S_OK;
    uint32_t           total  = store.GetBlockCount();
    uint32_t           next   = kRootBlock;
    uint32_t           read   = 0;
    VolumeKind         kind   = VolumeKind::Unknown;
    bool               isHfs  = false;
    bool               inside = false;
    size_t             entry  = 0;
    std::vector<Byte>  block;



    outRegion = Region();

    BAIL_OUT_IF (total > kMaxProDosBlocks, S_OK);

    hr = PartitionLayout::ProbeBlockVolume (store, 0, total, kind, isHfs, outRegion.volumeBlocks);
    CHR (hr);

    BAIL_OUT_IF (kind != VolumeKind::ProDos, S_OK);

    //  The directory's blocks, each read once at most.
    while (next != 0 && next < total && read < total && outRegion.count == 0)
    {
        hr = PartitionLayout::ReadBlocks (store, next, 1, block);
        CHR (hr);

        for (int i = 0; i < kEntriesInBlock && outRegion.count == 0; i++)
        {
            entry = kEntriesAt + (size_t) i * kEntryBytes;

            if (IsAreaEntry (block, entry))
            {
                outRegion.first = PartitionLayout::ReadWord (block, entry + kKeyPointer);
                outRegion.count = PartitionLayout::ReadWord (block, entry + kBlocksUsed);
            }
        }

        next = PartitionLayout::ReadWord (block, kNextBlock);
        read++;
    }

    inside = outRegion.first != 0 && outRegion.count >= kMapBlocks && outRegion.first + outRegion.count <= outRegion.volumeBlocks;

    if (!inside)
    {
        outRegion.count = 0;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::ListVolumes
//
//  Volumes 1 to the map's count, each with its description. A volume that
//  ends past the ProDOS volume, has a description of 16 bytes or more, or
//  starts before the one listed before it ends is left out.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PpmLayout::ListVolumes (IBlockStore & store, const Region & region, const std::vector<Byte> & map, std::vector<Partition> & outPartitions)
{
    static constexpr size_t  kInfoBytes        = 8;
    static constexpr size_t  kDescriptionsAt   = 0x100;
    static constexpr size_t  kDescriptionBytes = 16;
    static constexpr size_t  kVolumeCount      = 0x02;



    HRESULT            hr     = S_OK;
    int                count  = PartitionLayout::ReadWord (map, kVolumeCount);
    uint32_t           ended  = 0;
    uint32_t           first  = 0;
    uint32_t           length = 0;
    size_t             text   = 0;
    size_t             size   = 0;
    bool               listed = false;
    std::vector<Byte>  sectors;



    for (int volume = 1; volume <= count; volume++)
    {
        Partition  partition;



        first  = PartitionLayout::ReadWord (map, (size_t) volume * kInfoBytes);
        length = PartitionLayout::ReadWord (map, (size_t) volume * kInfoBytes + 2);
        text   = kDescriptionsAt + (size_t) volume * kDescriptionBytes;
        size   = map[text];
        listed = first >= ended && first + length <= region.volumeBlocks && size < kDescriptionBytes;

        if (!listed)
        {
            continue;
        }

        partition.index      = (int) outPartitions.size() + 1;
        partition.firstBlock = first;
        partition.blockCount = length;
        partition.layout     = PartitionLayoutKind::Ppm;
        partition.name.assign ((const char *) map.data() + text + 1, size);

        hr = PartitionLayout::ReadPartition (store, partition, sectors);
        CHR (hr);

        partition.volumeKind = VolumeImage::DetectFilesystem (sectors);
        ended                = first + length;

        outPartitions.push_back (std::move (partition));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::TryDetect
//
//  The region's map must have the signature and a volume count of 1 to 31.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PpmLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    static constexpr Byte    kSignature[]  = { 0x03, 'P', 'P', 'M' };
    static constexpr size_t  kSignatureAt  = 0x04;
    static constexpr size_t  kVolumeCount  = 0x02;



    HRESULT            hr     = S_OK;
    Region             region;
    std::vector<Byte>  map;
    bool               marked = false;
    int                count  = 0;



    outPartitions.clear();

    hr = FindRegion (store, region);
    CHR (hr);

    BAIL_OUT_IF (region.count == 0, S_OK);

    hr = PartitionLayout::ReadBlocks (store, region.first, kMapBlocks, map);
    CHR (hr);

    marked = std::equal (std::begin (kSignature), std::end (kSignature), map.begin() + kSignatureAt);
    count   = PartitionLayout::ReadWord (map, kVolumeCount);

    BAIL_OUT_IF (!marked || count < 1 || count > kMaxVolumes, S_OK);

    hr = ListVolumes (store, region, map, outPartitions);
    CHR (hr);

Error:
    return hr;
}
