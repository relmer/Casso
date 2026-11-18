#include "Pch.h"

#include "Devices/Disk/Partitions/PpmLayout.h"





static constexpr char      s_kAreaName[]        = "PASCAL.AREA";
static constexpr size_t    s_kAreaNameLen       = sizeof (s_kAreaName) - 1;
static constexpr Byte      s_kStoragePascalArea = 0x04;
static constexpr uint32_t  s_kMaxProDosBlocks   = 65535;

//  A ProDOS directory block and its entries.
static constexpr uint32_t  s_kRootBlock         = 2;
static constexpr size_t    s_kOffNextBlock      = 0x02;
static constexpr size_t    s_kOffFirstEntry     = 0x04;
static constexpr size_t    s_kEntryBytes        = 0x27;
static constexpr int       s_kEntriesPerBlock   = 13;
static constexpr size_t    s_kEntOffKeyPointer  = 0x11;
static constexpr size_t    s_kEntOffBlocksUsed  = 0x13;

//  The area's map: a count of volumes, a signature, an entry of a first
//  block and a length for each volume, and a description for each.
static constexpr uint32_t  s_kMapBlocks         = 2;
static constexpr size_t    s_kOffVolumeCount    = 0x02;
static constexpr size_t    s_kOffSignature      = 0x04;
static constexpr Byte      s_kSignature[]       = { 0x03, 'P', 'P', 'M' };
static constexpr int       s_kMaxVolumes        = 31;
static constexpr size_t    s_kInfoBytes         = 8;
static constexpr size_t    s_kInfoOffLength     = 2;
static constexpr size_t    s_kOffDescriptions   = 0x100;
static constexpr size_t    s_kDescriptionBytes  = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::TryDetect
//
//  Entry 0 of the map is the area. An entry that ends past the volume, has a
//  description longer than its field, or begins before the end of the one
//  ahead of it is left out.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PpmLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT            hr           = S_OK;
    std::vector<Byte>  map;
    std::vector<Byte>  sectors;
    uint32_t           areaBlock    = 0;
    uint32_t           areaBlocks   = 0;
    uint32_t           volumeBlocks = 0;
    uint32_t           priorEnd     = 0;
    int                count        = 0;
    int                i            = 0;
    bool               found        = false;



    outPartitions.clear();

    hr = FindArea (store, areaBlock, areaBlocks, volumeBlocks);
    CHR (hr);

    BAIL_OUT_IF (areaBlocks == 0, S_OK);

    hr = PartitionLayout::ReadBlocks (store, areaBlock, s_kMapBlocks, map);
    CHR (hr);

    found = std::equal (std::begin (s_kSignature), std::end (s_kSignature), map.begin() + s_kOffSignature);
    count = PartitionLayout::ReadWord (map, s_kOffVolumeCount);

    BAIL_OUT_IF (!found || count == 0 || count > s_kMaxVolumes, S_OK);

    for (i = 1; i <= count; i++)
    {
        uint32_t   start       = PartitionLayout::ReadWord (map, (size_t) i * s_kInfoBytes);
        uint32_t   length      = PartitionLayout::ReadWord (map, (size_t) i * s_kInfoBytes + s_kInfoOffLength);
        size_t     description = s_kOffDescriptions + (size_t) i * s_kDescriptionBytes;
        size_t     textLength  = map[description];
        Partition  volume;

        if (start + length > volumeBlocks || textLength >= s_kDescriptionBytes || start < priorEnd)
        {
            continue;
        }

        volume.index      = (int) outPartitions.size() + 1;
        volume.firstBlock = start;
        volume.blockCount = length;
        volume.layout     = PartitionLayoutKind::Ppm;
        volume.name.assign (map.begin() + (ptrdiff_t) description + 1, map.begin() + (ptrdiff_t) (description + 1 + textLength));

        hr = PartitionLayout::ReadPartition (store, volume, sectors);
        CHR (hr);

        volume.volumeKind = VolumeImage::DetectFilesystem (sectors);
        priorEnd          = start + length;

        outPartitions.push_back (std::move (volume));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PpmLayout::FindArea
//
//  The blocks of PASCAL.AREA, from its entry in the volume directory of a
//  ProDOS volume. `outBlockCount` is zero when the store isn't a ProDOS
//  volume or has no such file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PpmLayout::FindArea (IBlockStore & store, uint32_t & outFirstBlock, uint32_t & outBlockCount, uint32_t & outVolumeBlocks)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  block;
    uint32_t           total   = store.GetBlockCount();
    uint32_t           next    = s_kRootBlock;
    uint32_t           visited = 0;
    uint32_t           first   = 0;
    uint32_t           count   = 0;
    VolumeKind         kind    = VolumeKind::Unknown;
    bool               isHfs   = false;
    bool               found   = false;
    int                slot    = 0;



    outFirstBlock   = 0;
    outBlockCount   = 0;
    outVolumeBlocks = 0;

    BAIL_OUT_IF (total > s_kMaxProDosBlocks, S_OK);

    hr = PartitionLayout::ProbeBlockVolume (store, 0, total, kind, isHfs, outVolumeBlocks);
    CHR (hr);

    BAIL_OUT_IF (kind != VolumeKind::ProDos, S_OK);

    //  The bound on blocks visited ends a chain that loops.
    while (!found && next != 0 && next < total && visited < total)
    {
        hr = PartitionLayout::ReadBlocks (store, next, 1, block);
        CHR (hr);

        for (slot = 0; slot < s_kEntriesPerBlock && !found; slot++)
        {
            size_t  entry      = s_kOffFirstEntry + (size_t) slot * s_kEntryBytes;
            Byte    typeLength = block[entry];

            found = (typeLength >> 4) == s_kStoragePascalArea
                 && (size_t) (typeLength & 0x0F) == s_kAreaNameLen
                 && std::equal (s_kAreaName, s_kAreaName + s_kAreaNameLen, block.begin() + (ptrdiff_t) entry + 1);

            if (found)
            {
                first = PartitionLayout::ReadWord (block, entry + s_kEntOffKeyPointer);
                count = PartitionLayout::ReadWord (block, entry + s_kEntOffBlocksUsed);
            }
        }

        next = PartitionLayout::ReadWord (block, s_kOffNextBlock);
        visited++;
    }

    if (found && first != 0 && count >= s_kMapBlocks && first + count <= outVolumeBlocks)
    {
        outFirstBlock = first;
        outBlockCount = count;
    }

Error:
    return hr;
}