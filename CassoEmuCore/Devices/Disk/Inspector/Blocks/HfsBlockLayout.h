#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout
//
//  The blocks of an HFS volume, from Apple's Inside Macintosh: Files, "Data
//  Organization on Volumes": boot blocks 0 and 1, the master directory block
//  in block 2 and its copy in the second-to-last block, the volume bitmap
//  from drVBMSt, then the allocation blocks from drAlBlSt. The catalog and
//  extents overflow files are B-trees of 512-byte nodes, each node in one
//  block; their first three extents are in the master directory block.
//  A file's forks are its file record's first three extents and the rest in
//  the extents overflow file.
//
//  Numbers are big-endian. Every walk is bounded by the volume's size.
//
////////////////////////////////////////////////////////////////////////////////

class HfsBlockLayout
{
public:
    static constexpr uint32_t  kBlockBytes = 512;
    static constexpr uint32_t  kMdbBlock   = 2;

    using Block = std::array<Byte, kBlockBytes>;

    static void  Walk     (const BlockImage & image, BlockMap & outMap);
    static void  Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields);

private:
    //  Master directory block offsets.
    static constexpr uint32_t  kVbmStart      = 14;
    static constexpr uint32_t  kAllocBlocks   = 18;
    static constexpr uint32_t  kAllocSize     = 20;
    static constexpr uint32_t  kAllocStart    = 28;
    static constexpr uint32_t  kVolumeName    = 36;
    static constexpr uint32_t  kExtentsSize   = 130;
    static constexpr uint32_t  kExtentsExtent = 134;
    static constexpr uint32_t  kCatalogSize   = 146;
    static constexpr uint32_t  kCatalogExtent = 150;

    //  Node descriptor offsets and node types.
    static constexpr uint32_t  kForwardLink   = 0;
    static constexpr uint32_t  kBackLink      = 4;
    static constexpr uint32_t  kNodeType      = 8;
    static constexpr uint32_t  kNodeHeight    = 9;
    static constexpr uint32_t  kRecordCount   = 10;
    static constexpr uint32_t  kDescriptor    = 14;
    static constexpr Byte      kIndexNode     = 0x00;
    static constexpr Byte      kHeaderNode    = 0x01;
    static constexpr Byte      kMapNode       = 0x02;
    static constexpr Byte      kLeafNode      = 0xFF;

    //  Header record offsets, from the node's start.
    static constexpr uint32_t  kFirstLeaf     = kDescriptor + 10;

    //  Catalog record types and the offsets of a file record's fields.
    static constexpr Byte      kDirectory     = 1;
    static constexpr Byte      kFile          = 2;
    static constexpr Byte      kDirThread     = 3;
    static constexpr Byte      kFileThread    = 4;
    static constexpr uint32_t  kDirId         = 6;
    static constexpr uint32_t  kFileId        = 20;
    static constexpr uint32_t  kDataExtents   = 74;
    static constexpr uint32_t  kForkExtents   = 86;
    static constexpr uint32_t  kThreadParent  = 10;
    static constexpr uint32_t  kThreadName    = 14;
    static constexpr uint32_t  kValence       = 4;
    static constexpr uint32_t  kDataSize      = 26;
    static constexpr uint32_t  kForkSize      = 36;
    static constexpr uint32_t  kRootId        = 2;
    static constexpr uint32_t  kExtentCount   = 3;
    static constexpr uint32_t  kExtentBytes   = 4;
    static constexpr Byte      kDataFork      = 0x00;
    static constexpr Byte      kResourceFork  = 0xFF;
    static constexpr uint32_t  kMaxNodes      = 65536;
    static constexpr uint32_t  kMaxDepth      = 64;

    struct Extent
    {
        uint32_t  start = 0;
        uint32_t  count = 0;
    };

    struct Folder
    {
        uint32_t      parent = 0;
        std::wstring  name;
    };

    struct ForkRecord
    {
        uint32_t        fileId = 0;
        Byte            fork   = kDataFork;
        int             file   = -1;
        vector<Extent>  extents;
    };

    struct Walker
    {
        const BlockImage                         & image;
        BlockMap                                 & map;
        std::map<uint32_t, Folder>                 folders;
        vector<ForkRecord>                         forks;
        std::map<std::pair<uint32_t, Byte>, vector<std::pair<uint32_t, vector<Extent>>>>  overflow;
    };

    static void          ReadBlock    (const BlockImage & image, uint32_t block, Block & outBlock);
    static void          ReadExtents  (const Byte * at, vector<Extent> & outExtents);
    static void          MarkBlock    (BlockMap & map, uint32_t block, SectorRole role, BlockStructure structure, int owner, uint32_t tree = 0, uint32_t node = 0);
    static void          MapTree      (Walker & walker, const vector<Extent> & extents, uint32_t treeBytes, vector<uint32_t> & outNodes);
    static void          ReadLeaves   (Walker & walker, const vector<uint32_t> & nodes, const std::function<void (const Block &, uint32_t key, uint32_t data)> & visit);
    static void          ReadCatalog  (Walker & walker, const vector<uint32_t> & nodes);
    static void          ReadOverflow (Walker & walker, const vector<uint32_t> & nodes);
    static void          ClaimFork    (Walker & walker, const ForkRecord & fork);
    static std::wstring  GetPath      (const Walker & walker, uint32_t parent, const std::wstring & name);

    static void      AnnotateBoot         (BlockFieldList & list);
    static void      AnnotateMdb          (const BlockMap & map, BlockFieldList & list);
    static void      AnnotateNode         (const BlockMap & map, const BlockMapEntry & entry, BlockFieldList & list);
    static void      AnnotateHeader       (const BlockMap & map, uint32_t tree, BlockFieldList & list);
    static void      AnnotateRecord       (const BlockMap & map, uint32_t tree, Byte type, uint32_t number, uint32_t at, uint32_t end, BlockFieldList & list);
    static void      AnnotateExtentRecord (const BlockMap & map, uint32_t at, const std::wstring & head, BlockFieldList & list);
    static uint32_t  GetRecordOffset      (const BlockFieldList & list, uint32_t record);

    static std::wstring  DecodeName (const Byte * at, size_t length);
    static Word          GetWord    (const Byte * at) { return (Word) ((at[0] << 8) | at[1]); }
    static uint32_t      GetLong    (const Byte * at) { return ((uint32_t) GetWord (at) << 16) | GetWord (at + 2); }
};
