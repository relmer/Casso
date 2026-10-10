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
//  hold, and deleted entries; and the volume bitmap for what the volume
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
        int                   depth = 0;
    };

    static void  ReadDirectory (Walk & walk, int keyBlock, const std::wstring & prefix, SectorRole role, int owner);
    static void  ReadEntry     (Walk & walk, const Byte * entry, const std::wstring & prefix);
    static void  ReadFork      (Walk & walk, int file, int storage, int key, uint32_t eof);
    static void  ReadIndex     (Walk & walk, int file, int index, int blocks, std::set<int> & visited);
    static bool  ReadChainBlock (Walk & walk, int file, int block, SectorRole role, std::set<int> & visited, Block & outBlock);

    static std::wstring  ReadName    (const Byte * entry);
    static std::wstring  GetTypeName (Byte type);
    static int           GetWord     (const Byte * bytes) { return bytes[0] | (bytes[1] << 8); }
};
