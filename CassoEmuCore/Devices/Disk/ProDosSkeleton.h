#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton
//
//  Pure ProDOS volume structures for a fresh 143,360-byte sector buffer.
//  Write lays down the canonical minimal 280-block
//  volume: directory key block 2 chained through 5, volume bitmap at block 6
//  (blocks 0-6 used), boot blocks zeroed. InstallBoot copies boot blocks 0-1
//  from a caller-supplied ProDOS master image and writes PRODOS +
//  BASIC.SYSTEM as real files so the volume boots.
//
//  The buffer is kept in DOS 3.3 LOGICAL sector layout like every other
//  skeleton; ProDOS blocks map onto it through the standard block ->
//  track/sector pairing. Format ordering is applied later by the builder.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosSkeleton
{
public:
    static constexpr int  kTotalBlocks = 280;

    //  Empty formatted volume (data-only). Buffer must be kImageByteSize.
    static HRESULT  Write (vector<Byte> & buffer, const std::string & volumeName);

    //  Boot blocks + PRODOS + BASIC.SYSTEM from the master image.
    static HRESULT  InstallBoot (vector<Byte> & buffer, const vector<Byte> & usersDisk);

    //  Byte offset of a position inside a ProDOS block within the DOS 3.3
    //  logical-sector buffer. Public so the file writer / reader and the
    //  builder's tests address blocks the same single way.
    static size_t   BlockByteOffset (int block, size_t offsetInBlock);

private:
    static constexpr int     kBlockByteSize   = 512;
    static constexpr int     kBlocksPerTrack  = 8;
    static constexpr int     kDirKeyBlock     = 2;
    static constexpr int     kDirLastBlock    = 5;
    static constexpr int     kBitmapBlock     = 6;

    static constexpr Byte    kStorageVolumeDir = 0xF0;
    static constexpr Byte    kAccessDefault    = 0xC3;
    static constexpr Byte    kEntryLength      = 0x27;
    static constexpr Byte    kEntriesPerBlock  = 0x0D;

    //  Directory-block layout: link pointers, then the first entry.
    static constexpr size_t  kOffPrevBlock   = 0x00;
    static constexpr size_t  kOffNextBlock   = 0x02;
    static constexpr size_t  kOffFirstEntry  = 0x04;

    //  Volume-directory header fields, relative to the header entry.
    static constexpr size_t  kHdrOffTypeName        = 0x00;
    static constexpr size_t  kHdrOffName            = 0x01;
    static constexpr size_t  kHdrOffAccess          = 0x1E;
    static constexpr size_t  kHdrOffEntryLength     = 0x1F;
    static constexpr size_t  kHdrOffEntriesPerBlock = 0x20;
    static constexpr size_t  kHdrOffFileCount       = 0x21;
    static constexpr size_t  kHdrOffBitmapPointer   = 0x23;
    static constexpr size_t  kHdrOffTotalBlocks     = 0x25;

    static constexpr size_t  kVolumeNameBytes = 15;

    //  ProDOS block half -> DOS 3.3 logical sector within the track. Derived
    //  from the standard 2:1 ProDOS physical interleave composed with the
    //  DOS 3.3 physical-to-logical skew; row = block index within the track,
    //  column = 256-byte half.
    static constexpr int  kBlockHalfSector[kBlocksPerTrack][2] =
    {
        { 0, 14 }, { 13, 12 }, { 11, 10 }, { 9, 8 },
        { 7, 6 },  { 5, 4 },   { 3, 2 },   { 1, 15 },
    };

    static void  WriteWord (vector<Byte> & buffer, int block, size_t offsetInBlock, Word value);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader
//
//  Minimal ProDOS volume reader: walks the volume directory and extracts one
//  named file's contents (seedling / sapling / tree). Exists to pull PRODOS
//  and BASIC.SYSTEM out of the downloaded master image.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosReader
{
public:
    static HRESULT  ExtractFile (const vector<Byte> & volume,
                                 const std::string  & fileName,
                                 vector<Byte>       & outBytes,
                                 Byte               & outFileType,
                                 Word               & outAuxType);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter
//
//  Writes a file into a ProDosSkeleton-formatted buffer: directory entry,
//  seedling / sapling data blocks, volume bitmap kept honest.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosFileWriter
{
public:
    static HRESULT  WriteFile (vector<Byte>       & buffer,
                               const std::string  & fileName,
                               Byte                 fileType,
                               Word                 auxType,
                               const vector<Byte> & bytes);
};
