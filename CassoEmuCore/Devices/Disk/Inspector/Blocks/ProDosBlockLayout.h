#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout
//
//  The blocks of a ProDOS or SOS volume, from Apple's ProDOS 8 Technical
//  Reference Manual, appendix B, and the SOS Reference Manual, where both
//  have the same directory format: the boot loader in blocks 0 and 1, the
//  volume directory's chain of blocks from block 2, the bitmap at the block
//  the volume header points to, and below each directory entry a seedling's
//  data block, a sapling's index block, a tree's master index block, an
//  extended file's key block with its two forks, or a subdirectory's chain.
//
//  Walk sets each block's role, structure and files. Annotate returns a
//  block's fields by its structure. Every walk is bounded by the volume's
//  size and a depth limit, so a damaged volume can't loop.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosBlockLayout
{
public:
    static constexpr uint32_t  kKeyBlock   = 2;
    static constexpr uint32_t  kBlockBytes = 512;

    using Block = std::array<Byte, kBlockBytes>;

    //  Whether block 2 has a volume directory header, and the volume's size
    //  in blocks.
    static bool  IsFound (const BlockImage & image, uint32_t & outBlocks);

    static void  Walk     (const BlockImage & image, BlockFileSystem fileSystem, BlockMap & outMap);
    static void  Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields);

    static void  ReadBlock (const BlockImage & image, uint32_t block, Block & outBlock);

private:
    static constexpr uint32_t  kEntryLength     = 0x27;
    static constexpr uint32_t  kEntriesPerBlock = 13;
    static constexpr uint32_t  kFirstEntry      = 0x04;
    static constexpr uint32_t  kPrevPointer     = 0x00;
    static constexpr uint32_t  kNextPointer     = 0x02;
    static constexpr uint32_t  kBitmapPointer   = 0x27;
    static constexpr uint32_t  kTotalBlocks     = 0x29;
    static constexpr uint32_t  kPointers        = 256;
    static constexpr uint32_t  kBitsPerBlock    = 4096;
    static constexpr int       kMaxDepth        = 32;
    static constexpr Byte      kSeedling        = 0x1;
    static constexpr Byte      kSapling         = 0x2;
    static constexpr Byte      kTree            = 0x3;
    static constexpr Byte      kExtended        = 0x5;
    static constexpr Byte      kSubdirectory    = 0xD;
    static constexpr Byte      kSubdirHeader    = 0xE;
    static constexpr Byte      kVolumeHeader    = 0xF;
    static constexpr uint32_t  kResourceFork    = 0x100;
    static constexpr uint32_t  kFinderInfo      = 0x08;
    static constexpr uint32_t  kFinderEntry     = 18;
    static constexpr Byte      kNameMask        = 0x0F;
    static constexpr uint32_t  kDateBytes       = 4;

    //  Directory entry and header offsets, from the entry's start.
    static constexpr uint32_t  kOffTypeName          = 0x00;
    static constexpr uint32_t  kOffName              = 0x01;
    static constexpr uint32_t  kOffSubdirMarker      = 0x10;
    static constexpr uint32_t  kOffFileType          = 0x10;
    static constexpr uint32_t  kOffKeyPointer        = 0x11;
    static constexpr uint32_t  kOffBlocksUsed        = 0x13;
    static constexpr uint32_t  kOffEof               = 0x15;
    static constexpr uint32_t  kOffCreated           = 0x18;
    static constexpr uint32_t  kOffVersion           = 0x1C;
    static constexpr uint32_t  kOffMinVersion        = 0x1D;
    static constexpr uint32_t  kOffAccess            = 0x1E;
    static constexpr uint32_t  kOffEntryLength       = 0x1F;
    static constexpr uint32_t  kOffAuxType           = 0x1F;
    static constexpr uint32_t  kOffEntriesPerBlock   = 0x20;
    static constexpr uint32_t  kOffFileCount         = 0x21;
    static constexpr uint32_t  kOffModified          = 0x21;
    static constexpr uint32_t  kOffBitmapPointer     = 0x23;
    static constexpr uint32_t  kOffParentPointer     = 0x23;
    static constexpr uint32_t  kOffTotalBlocks       = 0x25;
    static constexpr uint32_t  kOffHeaderPointer     = 0x25;
    static constexpr uint32_t  kOffParentEntryNumber = 0x25;
    static constexpr uint32_t  kOffParentEntryLength = 0x26;
    struct Walker
    {
        const BlockImage  & image;
        BlockMap          & map;
        std::set<uint32_t>  directoryBlocks;
    };

    static bool  IsInside      (const BlockMap & map, uint32_t block) { return block != 0 && block < map.GetBlockCount(); }
    static void  MarkBlock     (Walker & walker, uint32_t block, SectorRole role, BlockStructure structure, int owner, uint32_t node = 0);
    static void  WalkDirectory (Walker & walker, uint32_t keyBlock, const std::wstring & prefix, int owner, int depth);
    static void  WalkEntry     (Walker & walker, const Byte * entry, const std::wstring & prefix, int depth);
    static void  WalkFork      (Walker & walker, int file, Byte storage, uint32_t key);
    static void  WalkIndex     (Walker & walker, int file, uint32_t index, uint32_t & ioNode);

    static void  AnnotateDirectory    (const BlockMap & map, BlockFieldList & list);
    static void  AnnotateHeader       (const BlockMap & map, uint32_t at, BlockFieldList & list);
    static void  AnnotateEntry        (const BlockMap & map, uint32_t at, int number, BlockFieldList & list);
    static void  AnnotateIndex        (const BlockMap & map, bool isMaster, BlockFieldList & list);
    static void  AnnotateExtendedKey  (const BlockMap & map, BlockFieldList & list);
    static void  AnnotateBitmap       (uint32_t bitmapBlock, BlockFieldList & list);

    static std::wstring  ReadName (const Byte * entry);
    static Word          GetWord  (const Byte * bytes) { return (Word) (bytes[0] | (bytes[1] << 8)); }
};
