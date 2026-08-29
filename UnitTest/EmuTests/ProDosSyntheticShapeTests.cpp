#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/ProDosSkeleton.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSyntheticShapeTests
//
//  Two ProDOS shapes that NO REAL VOLUME HERE CAN REACH, constructed on purpose
//  so neither reads as a coverage gap.
//
//      Tree storage        A tree exists only when a file needs more than 256
//                          data blocks. The three fixture disks are 280-block
//                          volumes whose largest file is 60 blocks, so no tree
//                          is on them and none could be.
//
//      Random-access text  All 24 text files across the fixture volumes have an
//                          auxiliary type of 0, meaning sequential. None sets a
//                          record length, so the random-access form -- where the
//                          auxiliary type IS the record size and unwritten
//                          records are sparse holes -- has no real sample.
//
//  A constructed fixture is the right tool for exactly this and the wrong tool
//  for the question the oracle suite answers. Real disks say whether the reader
//  understands the format; a synthetic one cannot, having been built by the same
//  understanding it would be testing. What it CAN do is place a chosen structure
//  where nothing else will, which is what these two are for.
//
//  THE LAYOUT CONSTANTS BELOW ARE DECLARED HERE RATHER THAN BORROWED from
//  ProDosSkeleton, deliberately. Building the fixture out of the same constants
//  the reader uses would make a wrong offset agree with itself and pass. These
//  come from the format, so the reader has to meet them.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosSyntheticShapeTests)
{
public:

    static constexpr size_t  kBlockSize    = 512;
    static constexpr int     kDirKeyBlock  = 2;
    static constexpr int     kBitmapBlock  = 6;
    static constexpr int     kFirstFree    = 7;
    static constexpr size_t  kFirstEntry   = 0x04;
    static constexpr size_t  kEntryLength  = 0x27;

    //  File-entry fields, relative to the entry.
    static constexpr size_t  kEntOffTypeName   = 0x00;
    static constexpr size_t  kEntOffName       = 0x01;
    static constexpr size_t  kEntOffFileType   = 0x10;
    static constexpr size_t  kEntOffKeyPointer = 0x11;
    static constexpr size_t  kEntOffBlocksUsed = 0x13;
    static constexpr size_t  kEntOffEof        = 0x15;
    static constexpr size_t  kEntOffAccess     = 0x1E;
    static constexpr size_t  kEntOffAuxType    = 0x1F;
    static constexpr size_t  kEntOffHeaderPtr  = 0x25;

    static constexpr Byte    kStorageSapling   = 0x20;
    static constexpr Byte    kStorageTree      = 0x30;
    static constexpr Byte    kAccessDefault    = 0xC3;
    static constexpr Byte    kTypeText         = 0x04;
    static constexpr Byte    kTypeBinary       = 0x06;

    //  Header-entry field, relative to the volume directory header.
    static constexpr size_t  kHdrOffFileCount  = 0x21;

    //  Pointers in an index block are split: 256 low bytes, then 256 high.
    static constexpr size_t  kIndexHighHalf    = 256;

    static void PutByte (vector<Byte> & volume, int block, size_t offset, Byte value)
    {
        volume[ProDosSkeleton::BlockByteOffset (block, offset)] = value;
    }

    static void PutWord (vector<Byte> & volume, int block, size_t offset, Word value)
    {
        PutByte (volume, block, offset,     (Byte) (value & 0xFF));
        PutByte (volume, block, offset + 1, (Byte) (value >> 8));
    }

    //  Set bit = free, MSB of byte 0 = block 0.
    static void MarkBlockUsed (vector<Byte> & volume, Word block)
    {
        size_t  at   = ProDosSkeleton::BlockByteOffset (kBitmapBlock, (size_t) (block / 8));
        Byte    mask = (Byte) (0x80 >> (block % 8));

        volume[at] = (Byte) (volume[at] & ~mask);
    }

    //  Hands out consecutive blocks from the first free one, keeping the bitmap
    //  honest so the volume stays internally consistent.
    static Word Take (vector<Byte> & volume, Word & cursor)
    {
        Word  block = cursor;

        Assert::IsTrue (block < ProDosSkeleton::kTotalBlocks,
            L"the fixture must fit the volume it is built on");

        MarkBlockUsed (volume, block);
        cursor++;

        return block;
    }

    //  One file entry in the volume directory's key block, in the slot after
    //  the header. One file per fixture keeps the entry arithmetic out of the
    //  way of what is being tested.
    static void WriteEntry (vector<Byte>       & volume,
                            const std::string  & name,
                            Byte                 storage,
                            Byte                 fileType,
                            Word                 keyBlock,
                            Word                 blocksUsed,
                            uint32_t             eof,
                            Word                 auxType)
    {
        size_t  header = kFirstEntry;
        size_t  at     = kFirstEntry + kEntryLength;
        size_t  i      = 0;

        PutByte (volume, kDirKeyBlock, at + kEntOffTypeName,
                 (Byte) (storage | (Byte) name.size()));

        for (i = 0; i < name.size(); i++)
        {
            PutByte (volume, kDirKeyBlock, at + kEntOffName + i, (Byte) name[i]);
        }

        PutByte (volume, kDirKeyBlock, at + kEntOffFileType, fileType);
        PutWord (volume, kDirKeyBlock, at + kEntOffKeyPointer, keyBlock);
        PutWord (volume, kDirKeyBlock, at + kEntOffBlocksUsed, blocksUsed);

        PutByte (volume, kDirKeyBlock, at + kEntOffEof,     (Byte) (eof & 0xFF));
        PutByte (volume, kDirKeyBlock, at + kEntOffEof + 1, (Byte) ((eof >> 8) & 0xFF));
        PutByte (volume, kDirKeyBlock, at + kEntOffEof + 2, (Byte) ((eof >> 16) & 0xFF));

        PutByte (volume, kDirKeyBlock, at + kEntOffAccess, kAccessDefault);
        PutWord (volume, kDirKeyBlock, at + kEntOffAuxType, auxType);
        PutWord (volume, kDirKeyBlock, at + kEntOffHeaderPtr, (Word) kDirKeyBlock);

        PutWord (volume, kDirKeyBlock, header + kHdrOffFileCount, 1);
    }

    //  An empty formatted volume to build on. Formatting itself is another
    //  suite's subject; here it only supplies a valid directory and bitmap.
    static vector<Byte> MakeEmptyVolume()
    {
        vector<Byte>  volume ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

        AssertSucceeded (ProDosSkeleton::Write (volume, "SYNTH"));

        return volume;
    }

    //  A byte value unique to each position in the file, so a traversal that
    //  returns the right NUMBER of blocks in the wrong ORDER is still caught.
    static Byte PatternFor (int dataBlockIndex)
    {
        return (Byte) ((dataBlockIndex * 7 + 3) & 0xFF);
    }

    TEST_METHOD (Tree_FileSpanningMoreThan256Blocks_ReadsInOrderAndToItsEof)
    {
        // The shape no fixture disk holds. A tree's key block is a MASTER index
        // of index blocks, each of which addresses up to 256 data blocks -- so
        // block 256 of the file is reached through the master's second entry,
        // and the whole structure is one level deeper than a sapling's.
        //
        // 257 data blocks is the smallest file that requires it: one more than a
        // single index block can address. Building it larger would test nothing
        // further and would not fit a 280-block volume.
        //
        // The per-block pattern is what makes the ordering claim real. A reader
        // that gathered the right count of blocks in the wrong sequence would
        // return a file of exactly the right length.
        constexpr int  kFullIndexBlocks = 256;
        constexpr int  kTailBytes       = 300;

        vector<Byte>  volume     = MakeEmptyVolume();
        Word          cursor     = kFirstFree;
        Word          master     = 0;
        Word          firstIndex = 0;
        Word          lastIndex  = 0;
        uint32_t      eof        = (uint32_t) kFullIndexBlocks * (uint32_t) kBlockSize + kTailBytes;
        int           i          = 0;
        FilePayload   got;

        master     = Take (volume, cursor);
        firstIndex = Take (volume, cursor);
        lastIndex  = Take (volume, cursor);

        PutByte (volume, master, 0, (Byte) (firstIndex & 0xFF));
        PutByte (volume, master, kIndexHighHalf, (Byte) (firstIndex >> 8));
        PutByte (volume, master, 1, (Byte) (lastIndex & 0xFF));
        PutByte (volume, master, 1 + kIndexHighHalf, (Byte) (lastIndex >> 8));

        for (i = 0; i < kFullIndexBlocks; i++)
        {
            Word    data = Take (volume, cursor);
            size_t  b    = 0;

            PutByte (volume, firstIndex, (size_t) i, (Byte) (data & 0xFF));
            PutByte (volume, firstIndex, (size_t) i + kIndexHighHalf, (Byte) (data >> 8));

            for (b = 0; b < kBlockSize; b++)
            {
                PutByte (volume, data, b, PatternFor (i));
            }
        }

        {
            Word    tail = Take (volume, cursor);
            size_t  b    = 0;

            PutByte (volume, lastIndex, 0, (Byte) (tail & 0xFF));
            PutByte (volume, lastIndex, kIndexHighHalf, (Byte) (tail >> 8));

            for (b = 0; b < kBlockSize; b++)
            {
                PutByte (volume, tail, b, PatternFor (kFullIndexBlocks));
            }
        }

        WriteEntry (volume, "BIGFILE", kStorageTree, kTypeBinary, master,
                    (Word) (cursor - kFirstFree), eof, 0x2000);

        // A tree really does need this much of the volume, which is why the
        // fixture disks cannot carry one.
        Assert::IsTrue (cursor > 256, L"the fixture must exceed one index block's reach");

        {
            ProDosVolume  built (volume);

            AssertSucceeded (built.Read (FilePath::Parse ("BIGFILE"), got));
        }

        Assert::AreEqual ((size_t) eof, got.bytes.size(),
            L"a tree file's length is its EOF, not a multiple of its blocks");

        for (i = 0; i <= kFullIndexBlocks; i++)
        {
            size_t  at = (size_t) i * kBlockSize;

            Assert::AreEqual (PatternFor (i), got.bytes[at],
                L"data blocks must arrive in file order, across the master index too");
        }

        // The last block is truncated to the EOF, so its final byte is the
        // 300th rather than the 512th.
        Assert::AreEqual (PatternFor (kFullIndexBlocks), got.bytes[got.bytes.size() - 1]);

        Assert::IsTrue (VolumeKind::ProDos == VolumeImage::DetectFilesystem (volume),
            L"and the volume is still recognizably ProDOS");
    }

    TEST_METHOD (Tree_MasterIndexHoleReadsAsZeros_NotAsAShortFile)
    {
        // A tree whose master index has an empty entry is a sparse file, not a
        // damaged one: the 256 blocks that entry would have addressed are holes
        // and read as zeros. Returning a short file instead would silently drop
        // 128 KB from the middle of one.
        constexpr int  kHoleBlocks = 256;

        vector<Byte>  volume = MakeEmptyVolume();
        Word          cursor = kFirstFree;
        Word          master = Take (volume, cursor);
        Word          index  = Take (volume, cursor);
        Word          data   = Take (volume, cursor);
        uint32_t      eof    = (uint32_t) kHoleBlocks * (uint32_t) kBlockSize + 16;
        size_t        b      = 0;
        FilePayload   got;

        // Master entry 0 left ZERO -- the hole. Entry 1 addresses real content.
        PutByte (volume, master, 1, (Byte) (index & 0xFF));
        PutByte (volume, master, 1 + kIndexHighHalf, (Byte) (index >> 8));

        PutByte (volume, index, 0, (Byte) (data & 0xFF));
        PutByte (volume, index, kIndexHighHalf, (Byte) (data >> 8));

        for (b = 0; b < kBlockSize; b++)
        {
            PutByte (volume, data, b, (Byte) 0xC1);
        }

        WriteEntry (volume, "SPARSE", kStorageTree, kTypeBinary, master, 3, eof, 0x2000);

        {
            ProDosVolume  reader (volume);

            AssertSucceeded (reader.Read (FilePath::Parse ("SPARSE"), got));
        }

        Assert::AreEqual ((size_t) eof, got.bytes.size(),
            L"a sparse file is its full declared length, holes included");

        Assert::AreEqual (Byte (0), got.bytes[0], L"the hole reads as zeros");
        Assert::AreEqual (Byte (0), got.bytes[kHoleBlocks * kBlockSize - 1]);

        Assert::AreEqual (Byte (0xC1), got.bytes[kHoleBlocks * kBlockSize],
            L"and the content after it is where the file says it is");
    }

    TEST_METHOD (RandomAccessText_AuxTypeIsARecordLength_NotALoadAddress)
    {
        // The trap this exists to catch. ProDOS stores a binary's load address
        // and a text file's record length in the SAME directory field, and a
        // reader that surfaces the field without regard to the type reports a
        // 128-byte record length as a load address of $0080 -- a number that
        // looks entirely plausible and is meaningless.
        //
        // No real fixture can catch it: all 24 text files across the fixture
        // volumes have an auxiliary type of 0, so a reader that copied the field
        // into the load address unconditionally would report $0000 for every one
        // of them and be indistinguishable from a correct one.
        constexpr Word  kRecordLength = 128;

        vector<Byte>  volume = MakeEmptyVolume();
        Word          cursor = kFirstFree;
        Word          index  = Take (volume, cursor);
        Word          first  = Take (volume, cursor);
        Word          last   = Take (volume, cursor);
        uint32_t      eof    = 4 * (uint32_t) kBlockSize;
        size_t        b      = 0;
        FilePayload   got;

        // A sapling whose index has holes at records 1 and 2 -- the shape a
        // random-access file takes when its middle records were never written.
        PutByte (volume, index, 0, (Byte) (first & 0xFF));
        PutByte (volume, index, kIndexHighHalf, (Byte) (first >> 8));
        PutByte (volume, index, 3, (Byte) (last & 0xFF));
        PutByte (volume, index, 3 + kIndexHighHalf, (Byte) (last >> 8));

        for (b = 0; b < kBlockSize; b++)
        {
            PutByte (volume, first, b, (Byte) 0xC1);
            PutByte (volume, last,  b, (Byte) 0xDA);
        }

        WriteEntry (volume, "RECORDS", kStorageSapling, kTypeText, index, 3, eof,
                    kRecordLength);

        {
            ProDosVolume  reader (volume);

            AssertSucceeded (reader.Read (FilePath::Parse ("RECORDS"), got));
        }

        Assert::AreEqual (kTypeText, got.type, L"ProDOS type $04 is TXT");

        Assert::IsTrue (got.hasAuxType, L"the auxiliary type is present and reported");
        Assert::AreEqual (kRecordLength, got.auxType, L"and carries the record length");

        Assert::IsFalse (got.hasLoadAddress,
            L"a text file does not load anywhere -- its auxiliary type is a record size");

        Assert::AreEqual ((size_t) eof, got.bytes.size(),
            L"unwritten records are holes within the file, not the end of it");

        Assert::AreEqual (Byte (0xC1), got.bytes[0]);
        Assert::AreEqual (Byte (0), got.bytes[kBlockSize],
            L"an unwritten record reads as zeros");
        Assert::AreEqual (Byte (0xDA), got.bytes[3 * kBlockSize],
            L"and the record after the holes is where its index entry says");
    }
};
