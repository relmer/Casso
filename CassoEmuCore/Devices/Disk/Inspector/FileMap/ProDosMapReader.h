#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMapWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader
//
//  A ProDOS volume (FR-084, FR-086, FR-087): found by its volume directory
//  key block in block 2, then every directory entry at any depth: seedling,
//  sapling and tree files with the holes of a sparse file, both forks of a
//  forked file under its extended key block, subdirectories and what they
//  contain, and deleted entries; and the volume bitmap for what the volume
//  marks used. Every walk is bounded (FR-092).
//
////////////////////////////////////////////////////////////////////////////////

class ProDosMapReader
{
public:
    static constexpr int  kKeyBlock = 2;

    //  Whether block 2 is a volume directory key block, and the volume's
    //  size in blocks.
    static bool  IsFound (const SectorSource & source, int & outBlocks);
    static void  Read    (const SectorSource & source, FileMap & outMap);

private:
    using Block = std::array<Byte, 2 * SectorSource::kBytes>;

    struct Walk
    {
        FileMapWriter &       writer;
        const SectorSource &  source;
        std::set<int>         directoryBlocks;
        int                   depth        = 0;
        int                   directoryKey = 0;
        int                   parentBlock  = 0;
        int                   parentEntry  = 0;
    };

    static void  ReadDirectory  (Walk & walk, int keyBlock, const std::wstring & prefix, SectorRole role, int owner);
    static void  CheckFileCount (Walk & walk, int keyBlock, const std::wstring & prefix, int files, int active);
    static void  CheckParent    (Walk & walk, int keyBlock, const Block & header, const std::wstring & prefix, int parentBlock, int parentEntry);
    static void  CheckEntry     (Walk & walk, const Byte * entry, const std::wstring & path, int storage, int block, int offset);
    static void  CheckBitmapEnd (FileMapWriter & writer, const Block & bitmap, int total);
    static bool  IsValidName    (const Byte * entry);
    static void  ReadEntry      (Walk & walk, const Byte * entry, const std::wstring & prefix, int block, int offset);
    static void  ReadFork       (Walk & walk, int file, int storage, int key, uint32_t eof);
    static void  ReadIndex      (Walk & walk, int file, int index, int blocks, std::set<int> & visited);
    static bool  ReadChainBlock (Walk & walk, int file, int block, SectorRole role, std::set<int> & visited, Block & outBlock);

    static std::wstring  ReadName    (const Byte * entry);
    static std::wstring  GetTypeName (Byte type);
    static int           GetWord     (const Byte * bytes) { return bytes[0] | (bytes[1] << 8); }
};
