#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplePartitionMap
//
//  The Apple Partition Map of a hard disk or CD image, from Apple's Inside
//  Macintosh: Devices, "SCSI Manager": the driver descriptor record in block
//  0, signature 'ER', with the device's block size; then from block 1 one
//  partition map entry a block, signature 'PM', each with the map's length in
//  blocks, the partition's first block and length in the device's blocks,
//  and the partition's name and type. Numbers are big-endian.
//
////////////////////////////////////////////////////////////////////////////////

class ApplePartitionMap
{
public:
    static constexpr uint32_t  kBlockBytes = (uint32_t) IBlockStore::kBlockBytes;
    static constexpr uint32_t  kMaxEntries = 256;

    struct Entry
    {
        uint32_t      mapBlock   = 0;
        uint32_t      firstBlock = 0;
        uint32_t      blockCount = 0;
        std::wstring  name;
        std::wstring  type;
    };

    //  False, with no entries, for a store with no map. First block and
    //  count are in 512-byte blocks, and clipped to the store.
    static bool  TryRead (IBlockStore & store, vector<Entry> & outEntries);

    static bool  IsDescriptor (BlockBytes bytes);
    static bool  IsMapEntry   (BlockBytes bytes);

    static void  AnnotateDescriptor (BlockBytes bytes, vector<BlockField> & outFields);
    static void  AnnotateEntry      (BlockBytes bytes, uint32_t blockCount, vector<BlockField> & outFields);

private:
    static constexpr Word      kDescriptorSignature = 0x4552;   // 'ER'
    static constexpr Word      kEntrySignature      = 0x504D;   // 'PM'
    static constexpr uint32_t  kBlockSize           = 2;
    static constexpr uint32_t  kMapBlocks           = 4;
    static constexpr uint32_t  kFirstBlock          = 8;
    static constexpr uint32_t  kBlockCount          = 12;
    static constexpr uint32_t  kName                = 16;
    static constexpr uint32_t  kType                = 48;
    static constexpr uint32_t  kTextBytes           = 32;

    static uint32_t      ReadLong (BlockBytes bytes, uint32_t at);
    static std::wstring  ReadText (BlockBytes bytes, uint32_t at);
};
