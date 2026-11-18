#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/ApplePartitionMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The driver descriptor record's and a partition map entry's fields, from
//  Inside Macintosh: Devices: offset, length and label. A length of 2 or 4
//  is a big-endian number, any other a run of text.
//
////////////////////////////////////////////////////////////////////////////////

struct PartitionMapField
{
    uint32_t  offset;
    uint32_t  length;
    LPCWSTR   label;
};


static constexpr PartitionMapField s_kDescriptorFields[] =
{
    {  0, 2, L"Driver descriptor: signature"     },
    {  2, 2, L"Driver descriptor: block size"    },
    {  4, 4, L"Driver descriptor: blocks"        },
    {  8, 2, L"Driver descriptor: device type"   },
    { 10, 2, L"Driver descriptor: device ID"     },
    { 12, 4, L"Driver descriptor: reserved"      },
    { 16, 2, L"Driver descriptor: drivers"       },
};


static constexpr PartitionMapField s_kEntryFields[] =
{
    {   0,  2, L"Partition map: signature"          },
    {   2,  2, L"Partition map: reserved"           },
    {   4,  4, L"Partition map: map blocks"         },
    {  12,  4, L"Partition map: partition blocks"   },
    {  16, 32, L"Partition map: partition name"     },
    {  48, 32, L"Partition map: partition type"     },
    {  80,  4, L"Partition map: first data block"   },
    {  84,  4, L"Partition map: data blocks"        },
    {  88,  4, L"Partition map: status"             },
    {  92,  4, L"Partition map: first boot block"   },
    {  96,  4, L"Partition map: boot code size"     },
    { 100,  4, L"Partition map: boot load address"  },
    { 108,  4, L"Partition map: boot entry point"   },
    { 116,  4, L"Partition map: boot code checksum" },
    { 120, 16, L"Partition map: processor"          },
};





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::IsDescriptor
//
////////////////////////////////////////////////////////////////////////////////

bool ApplePartitionMap::IsDescriptor (BlockBytes bytes)
{
    return bytes.size() >= kBlockBytes && ((bytes[0] << 8) | bytes[1]) == kDescriptorSignature;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::IsMapEntry
//
////////////////////////////////////////////////////////////////////////////////

bool ApplePartitionMap::IsMapEntry (BlockBytes bytes)
{
    return bytes.size() >= kBlockBytes && ((bytes[0] << 8) | bytes[1]) == kEntrySignature;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ApplePartitionMap::ReadLong (BlockBytes bytes, uint32_t at)
{
    return ((uint32_t) bytes[at] << 24) | ((uint32_t) bytes[at + 1] << 16) | ((uint32_t) bytes[at + 2] << 8) | (uint32_t) bytes[at + 3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::ReadText
//
//  A field of up to 32 characters, ended by a zero.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ApplePartitionMap::ReadText (BlockBytes bytes, uint32_t at)
{
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kLastPrintable  = 0x7E;



    std::wstring  text;
    uint32_t      i    = 0;



    for (i = 0; i < kTextBytes && bytes[at + i] != 0; i++)
    {
        text += (bytes[at + i] >= kFirstPrintable && bytes[at + i] <= kLastPrintable) ? (wchar_t) bytes[at + i] : L'?';
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::TryRead
//
//  A device block size other than 512 scales each entry's blocks to 512.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplePartitionMap::TryRead (IBlockStore & store, vector<Entry> & outEntries)
{
    HRESULT                        hr       = S_OK;
    std::array<Byte, kBlockBytes>  block    = {};
    uint32_t                       scale    = 1;
    uint32_t                       mapCount = 0;
    uint32_t                       i        = 0;
    Entry                          entry;
    uint32_t                       total    = store.GetBlockCount();
    bool                           isFound  = false;



    outEntries.clear();

    BAIL_OUT_IF (total < 2, S_OK);

    hr = store.ReadBlocks (0, 1, block);
    CHR (hr);

    isFound = IsDescriptor (block);
    BAIL_OUT_IF (!isFound, S_OK);

    scale = std::max<uint32_t> (1, (uint32_t) ((block[kBlockSize] << 8) | block[kBlockSize + 1]) / kBlockBytes);

    hr = store.ReadBlocks (1, 1, block);
    CHR (hr);

    isFound = IsMapEntry (block);
    BAIL_OUT_IF (!isFound, S_OK);

    mapCount = std::min<uint32_t> (ReadLong (block, kMapBlocks), kMaxEntries);

    for (i = 1; i <= mapCount && i < total; i++)
    {
        hr = store.ReadBlocks (i, 1, block);
        CHR (hr);

        isFound = IsMapEntry (block);

        if (!isFound)
        {
            break;
        }

        entry.mapBlock   = i;
        entry.firstBlock = ReadLong (block, kFirstBlock) * scale;
        entry.blockCount = ReadLong (block, kBlockCount) * scale;
        entry.name       = ReadText (block, kName);
        entry.type       = ReadText (block, kType);

        if (entry.firstBlock >= total)
        {
            continue;
        }

        entry.blockCount = std::min<uint32_t> (entry.blockCount, total - entry.firstBlock);

        outEntries.push_back (entry);
    }

Error:
    if (FAILED (hr))
    {
        outEntries.clear();
    }

    return !outEntries.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::AnnotateDescriptor
//
////////////////////////////////////////////////////////////////////////////////

void ApplePartitionMap::AnnotateDescriptor (BlockBytes bytes, vector<BlockField> & outFields)
{
    constexpr uint32_t  kWordBytes = 2;



    BlockFieldList  list (bytes, outFields);



    for (const PartitionMapField & field : s_kDescriptorFields)
    {
        if (field.length == kWordBytes)
        {
            list.AddBigWord (field.offset, field.label);
        }
        else
        {
            list.AddBigLong (field.offset, field.label);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap::AnnotateEntry
//
//  The partition's first block links to it.
//
////////////////////////////////////////////////////////////////////////////////

void ApplePartitionMap::AnnotateEntry (BlockBytes bytes, uint32_t blockCount, vector<BlockField> & outFields)
{
    constexpr uint32_t  kWordBytes = 2;
    constexpr uint32_t  kLongBytes = 4;



    BlockFieldList  list (bytes, outFields);
    uint32_t        first = list.GetBigLong (kFirstBlock);



    for (const PartitionMapField & field : s_kEntryFields)
    {
        if (field.length == kWordBytes)
        {
            list.AddBigWord (field.offset, field.label);
        }
        else if (field.length == kLongBytes)
        {
            list.AddBigLong (field.offset, field.label);
        }
        else
        {
            list.AddRun (field.offset, field.length, field.label, L"\"" + ReadText (bytes, field.offset) + L"\"");
        }
    }

    list.AddBigLink (kFirstBlock, kLongBytes, L"Partition map: first block", first < blockCount ? (int64_t) first : BlockField::kNoLink);
}
