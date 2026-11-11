#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree
//
//  An HFS catalog or extents overflow B-tree of 512-byte nodes. The layout
//  is from Inside Macintosh: Files. Node 0 is the header node, with the
//  tree's depth, root, first and last leaves, its node count and the map
//  of nodes in use. Map nodes follow it when the map overflows. Leaves
//  contain the records in key order, linked forward and back. Index nodes
//  above them contain the first key of each child and its node number.
//
//  A change is made by building the whole tree again from its records in
//  order, which a catalog of a floppy or a hard disk has few enough of. A
//  record is its key, from the key length byte, and its data.
//
//  Catalog keys sort by parent directory ID, then by name, compared without
//  case as the Finder compares them; extents keys by fork, file ID and first
//  allocation block.
//
////////////////////////////////////////////////////////////////////////////////

class HfsBTree
{
public:
    enum class Kind { Catalog, Extents };

    struct Record
    {
        std::vector<Byte>  key;     // the key length byte and the key
        std::vector<Byte>  data;
    };

    static constexpr size_t   kNodeBytes         = 512;
    static constexpr size_t   kDescriptorBytes   = 14;
    static constexpr size_t   kHeaderRecordBytes = 106;
    static constexpr size_t   kUserRecordBytes   = 128;
    static constexpr size_t   kHeaderMapBytes    = 256;
    static constexpr size_t   kMapNodeBytes      = 494;
    static constexpr Byte     kNodeIndex         = 0x00;
    static constexpr Byte     kNodeHeader        = 0x01;
    static constexpr Byte     kNodeMap           = 0x02;
    static constexpr Byte     kNodeLeaf          = 0xFF;
    static constexpr Byte     kCatalogKeyMax     = 0x25;
    static constexpr Byte     kExtentsKeyMax     = 0x07;

    //  Negative, zero or positive, as `a` sorts before, with or after `b`.
    static int  CompareCatalogKeys (const std::vector<Byte> & a, const std::vector<Byte> & b);
    static int  CompareExtentsKeys (const std::vector<Byte> & a, const std::vector<Byte> & b);
    static int  CompareNames       (const Byte * a, size_t aLength, const Byte * b, size_t bLength);

    static void  Sort (Kind kind, std::vector<Record> & ioRecords);

    //  The tree's nodes, `totalNodes` of them, for records already in order.
    //  Fails with ERROR_DISK_FULL when they don't fit.
    static HRESULT  Build (Kind kind, const std::vector<Record> & records, uint32_t totalNodes, std::vector<Byte> & outNodes);

    //  The nodes a tree of these records takes, the header and map nodes
    //  included.
    static uint32_t  CountNodes (Kind kind, const std::vector<Record> & records);

private:
    static Byte               FoldName       (Byte character);
    static size_t             GetRecordBytes (Kind kind, const Record & record, bool isIndex);
    static void               PutWord        (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static void               PutLong        (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static uint32_t           GetLong        (const std::vector<Byte> & bytes, size_t at);
    static std::vector<Byte>  MakeIndexKey   (Kind kind, const std::vector<Byte> & key);

    static void  PackLevel (Kind kind, const std::vector<Record> & records, bool isIndex, std::vector<std::vector<size_t>> & outNodes);
};
