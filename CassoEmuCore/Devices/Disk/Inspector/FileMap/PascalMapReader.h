#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMapWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader
//
//  An Apple Pascal volume (FR-085, FR-087, FR-093): found by its volume
//  header at the start of block 2, then each directory entry's run of blocks
//  from its first block up to its next; a .BAD file's blocks are the bad
//  blocks the file system recorded. Pascal keeps no bitmap and removes a
//  deleted file's entry, so a block no file owns is free and no file is
//  listed as deleted.
//
////////////////////////////////////////////////////////////////////////////////

class PascalMapReader
{
public:
    static constexpr int  kFirstDirectoryBlock = 2;
    static constexpr int  kDirectoryBlocks     = 4;
    static constexpr int  kMaxFiles            = 77;

    //  Whether block 2 begins with a volume header, and the volume's size in
    //  blocks.
    static bool  IsFound (const SectorSource & source, int & outBlocks);
    static void  Read    (const SectorSource & source, FileMap & outMap);

private:
    static bool          ReadDirectory (const SectorSource & source, vector<Byte> & outBytes, int & outBadBlock);
    static std::wstring  ReadName      (const Byte * name, int length);
    static std::wstring  GetKindName   (int kind);
    static int           GetWord       (const Byte * bytes) { return bytes[0] | (bytes[1] << 8); }
};
