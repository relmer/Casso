#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout
//
//  The blocks of an Apple Pascal volume, from the Apple II Pascal Operating
//  System Reference Manual: the boot blocks 0 and 1, the directory from block
//  2 to the block before the volume entry's end block, and each file in the
//  run of blocks in its entry. The directory's entries are 26 bytes and
//  run across its blocks, the volume's entry first. Pascal has no free map: a
//  block no entry has is free.
//
////////////////////////////////////////////////////////////////////////////////

class PascalBlockLayout
{
public:
    static constexpr uint32_t  kBlockBytes     = 512;
    static constexpr uint32_t  kDirectoryBlock = 2;

    static bool  IsFound  (const BlockImage & image, uint32_t & outBlocks);
    static void  Walk     (const BlockImage & image, BlockMap & outMap);
    static void  Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields);

private:
    static constexpr uint32_t  kEntryBytes         = 26;
    static constexpr uint32_t  kMaxFiles           = 77;
    static constexpr uint32_t  kFirstBlock         = 0;
    static constexpr uint32_t  kLastBlock          = 2;
    static constexpr uint32_t  kKind               = 4;
    static constexpr uint32_t  kVolumeName         = 6;
    static constexpr uint32_t  kVolumeBlocks       = 14;
    static constexpr uint32_t  kFileCount          = 16;
    static constexpr uint32_t  kFileName           = 6;
    static constexpr uint32_t  kLastByte           = 22;
    static constexpr uint32_t  kModified           = 24;
    static constexpr uint32_t  kVolumeNameMax      = 7;
    static constexpr uint32_t  kFileNameMax        = 15;
    static constexpr uint32_t  kMaxDirectoryBlocks = 8;

    static Byte          GetByte (const BlockImage & image, size_t at);
    static Word          GetWord (const BlockImage & image, size_t at) { return (Word) (GetByte (image, at) | (GetByte (image, at + 1) << 8)); }
    static std::wstring  GetName (const BlockImage & image, size_t at, uint32_t maxLength);
};
