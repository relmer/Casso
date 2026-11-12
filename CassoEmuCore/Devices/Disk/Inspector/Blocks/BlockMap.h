#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSystem / BlockStructure
//
//  The file systems block mode reads, and the structure a block is part of,
//  which selects how its bytes are annotated. A DOS 3.3 volume inside another
//  is mapped by the sector rather than the block.
//
////////////////////////////////////////////////////////////////////////////////

enum class BlockFileSystem
{
    None,
    ProDos,
    Sos,
    Pascal,
    Hfs,
    Dos33,
};


enum class BlockStructure
{
    None,
    BootLoader,
    ProDosDirectory,
    ProDosBitmap,
    ProDosIndex,
    ProDosMasterIndex,
    ProDosExtendedKey,
    PascalDirectory,
    HfsBootBlock,
    HfsMasterDirectory,
    HfsAlternateMasterDirectory,
    HfsVolumeBitmap,
    HfsNode,
    Dos33Vtoc,
    Dos33Catalog,
    Dos33TsList,
    FileData,
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapEntry
//
//  One 512-byte block of a volume: its role on the block map, the structure
//  it is part of, and the files that use it, as indexes into BlockMap::files.
//  For an HFS B-tree node, `tree` is the B-tree's file ID and `node` the
//  node's number in it; for a file's data, `node` is the block's number in
//  the file, from 0.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockMapEntry
{
    SectorRole      role      = SectorRole::Free;
    BlockStructure  structure = BlockStructure::None;
    vector<int>     owners;
    uint32_t        tree      = 0;
    uint32_t        node      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap
//
//  Every block of a volume, the paths of the files that use them, and for
//  HFS where the allocation blocks start and which block each B-tree node
//  is in. `hasFreeMap` is false for a volume with no free map, such as
//  Pascal's, where a block with no file is simply free.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockMap
{
    static constexpr uint32_t  kNoBlock = UINT32_MAX;

    BlockFileSystem        fileSystem          = BlockFileSystem::None;
    std::wstring           volumeName;
    vector<BlockMapEntry>  blocks;
    vector<std::wstring>   files;
    bool                   hasFreeMap          = false;
    bool                   isCatalogComplete   = true;
    uint32_t               allocationStart     = 0;
    uint32_t               blocksPerAllocation = 1;
    vector<uint32_t>       catalogNodes;
    vector<uint32_t>       extentsNodes;

    uint32_t  GetBlockCount () const { return (uint32_t) blocks.size(); }

    //  Every block a file uses, ascending.
    vector<uint32_t>  GetBlocksOf (int file) const;

    //  The paths of the files that use a block, one a line, or empty.
    std::wstring      DescribeOwners (uint32_t block) const;

    //  The block an HFS B-tree node is in, or kNoBlock.
    uint32_t          GetNodeBlock (uint32_t tree, uint32_t node) const;

    //  The more notable of two roles, for a block of several units: damage
    //  first, then the volume's structures, then files' data, then free
    //  space.
    static SectorRole  PickRole    (SectorRole a, SectorRole b);
    static int         GetPriority (SectorRole role);

    //  The first 512-byte block of an HFS allocation block.
    uint32_t          GetAllocationBlock (uint32_t allocationBlock) const { return allocationStart + allocationBlock * blocksPerAllocation; }
};
