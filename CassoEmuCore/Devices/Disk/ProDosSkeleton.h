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
    //  logical-sector buffer. Public so the file writer / reader, the block
    //  commands and the builder's tests address blocks the same single way.
    static size_t   GetBlockByteOffset (int block, size_t offsetInBlock);

    static constexpr int     kBlockByteSize   = 512;

private:
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

    //  File-entry fields, relative to the entry.
    static constexpr size_t  kEntOffTypeName      = 0x00;
    static constexpr size_t  kEntOffName          = 0x01;
    static constexpr size_t  kEntOffFileType      = 0x10;
    static constexpr size_t  kEntOffKeyPointer    = 0x11;
    static constexpr size_t  kEntOffBlocksUsed    = 0x13;
    static constexpr size_t  kEntOffEof           = 0x15;
    static constexpr size_t  kEntOffAccess        = 0x1E;
    static constexpr size_t  kEntOffAuxType       = 0x1F;
    static constexpr size_t  kEntOffHeaderPointer = 0x25;

    //  The storage-type nibble of a directory record. Zero means the record is
    //  INACTIVE -- either never used or deleted -- and a deleted one keeps its
    //  name so an undelete tool can find it, which is exactly why the name
    //  cannot be what decides.
    static constexpr Byte    kStorageInactive = 0x00;
    static constexpr Byte    kStorageSeedling = 0x10;
    static constexpr Byte    kStorageSapling  = 0x20;
    static constexpr Byte    kStorageTree     = 0x30;
    static constexpr Byte    kStorageSubdir   = 0xD0;

    //  Block pointers in an index block: 256 low bytes, then the 256 matching
    //  high bytes. Numerically the same as a sector's byte count, which is a
    //  coincidence worth not relying on.
    static constexpr size_t  kPointersPerIndex = 256;

    friend class ProDosReader;
    friend class ProDosFileWriter;
    friend class ProDosVolume;

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

    //  A block number the volume could actually hold. Every pointer walked here
    //  comes off the disk, so none of them may be trusted: block numbers index
    //  directly into the sector buffer, and an out-of-range one reads far past
    //  its end rather than merely returning something wrong.
    static bool  IsBlockInRange (Word block);

private:
    //  False when the index block itself is unusable, so the caller reports an
    //  unreadable file instead of gathering garbage.
    static bool  AppendIndexedBlocks (const vector<Byte> & volume,
                                      Word                 indexBlock,
                                      vector<Word>       & dataBlocks);
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

private:
    static HRESULT  AllocateBlock (vector<Byte> & buffer, Word & outBlock);
};
