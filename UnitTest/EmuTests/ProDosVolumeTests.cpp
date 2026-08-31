#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/ProDosSkeleton.h"
#include "Devices/Disk/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeTests
//
//  ProDOS skeleton structural invariants, file writer / reader round-trips
//  and bitmap-directory coherence, and boot install placement over synthetic
//  payload bytes. No host fixture files — synthetic volumes throughout.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeTests)
{
public:

    static constexpr size_t  kKeyBlockEntry = 0x04;

    //  Where the volume directory starts. The skeleton keeps its own copy
    //  private, and the tests that predate this one spell it as a literal.
    static constexpr int     kDirKeyBlock   = 2;

    //  Directory geometry, restated here because the skeleton keeps its own
    //  copies private and a test that reads bytes has to name offsets.
    static constexpr size_t  kEntryLength      = 0x27;
    static constexpr size_t  kEntOffTypeName   = 0x00;
    static constexpr size_t  kEntOffName       = 0x01;
    static constexpr size_t  kEntOffKeyPointer = 0x11;
    static constexpr size_t  kEntOffBlocksUsed = 0x13;
    static constexpr size_t  kEntOffEof        = 0x15;
    static constexpr size_t  kEntOffAccess     = 0x1E;

    static constexpr size_t  kBlockBytes       = 512;
    static constexpr size_t  kPointersPerIndex = 256;

    static constexpr Byte    kStorageSeedling = 0x10;
    static constexpr Byte    kStorageSapling  = 0x20;
    static constexpr Byte    kStorageTree     = 0x30;

    //  Blocks 0-1 boot, 2-5 the volume directory, 6 the bitmap.
    static constexpr uint32_t  kFreeOnBlankVolume = 280 - 7;

    //  A payload that needs a THIRD index level's worth of pointers -- 258 data
    //  blocks, so the master index holds two entries and the second one is
    //  reached only by a writer that grew past a sapling. 257 * 512 + 100.
    static constexpr size_t  kTreePayloadBytes = 131'684;
    static constexpr size_t  kTreeDataBlocks   = 258;
    static constexpr size_t  kTreeTotalBlocks  = 261;   // + two index + one master

    //  Exactly 256 data blocks: the largest file that is still a sapling.
    static constexpr size_t  kSaplingMaxBytes  = 131'072;
    static constexpr size_t  kSaplingMaxBlocks = 257;   // + one index

    static vector<Byte> MakeVolume (const std::string & name = "NEWDISK")
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0xEE);

        AssertSucceeded (ProDosSkeleton::Write (buffer, name));

        return buffer;
    }

    static Byte At (const vector<Byte> & buffer, int block, size_t offset)
    {
        return buffer[ProDosSkeleton::BlockByteOffset (block, offset)];
    }

    static Word WordAt (const vector<Byte> & buffer, int block, size_t offset)
    {
        return (Word) (At (buffer, block, offset) | (At (buffer, block, offset + 1) << 8));
    }


    TEST_METHOD (SkeletonConstants_MatchGeometry)
    {
        // 280 blocks x 512 bytes == the 143,360-byte 5.25" sector image.
        Assert::AreEqual (NibblizationLayer::kImageByteSize,
                          ProDosSkeleton::kTotalBlocks * 512);
    }


    TEST_METHOD (BlockByteOffset_CoversTheWholeImageExactlyOnce)
    {
        // Every (block, offset) must land on a distinct buffer byte and the
        // union must tile the full image — the interleave map is a bijection.
        std::vector<char>  seen (NibblizationLayer::kImageByteSize, 0);
        int                block  = 0;
        size_t             offset = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            for (offset = 0; offset < 512; offset++)
            {
                size_t  at = ProDosSkeleton::BlockByteOffset (block, offset);

                Assert::IsTrue (at < (size_t) NibblizationLayer::kImageByteSize);
                Assert::AreEqual ((char) 0, seen[at]);
                seen[at] = 1;
            }
        }
    }


    TEST_METHOD (Write_RejectsWrongBufferSize)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  undersized (512, 0);



        AssertFailed (ProDosSkeleton::Write (undersized, "NEWDISK"));
        expect.RequireCount (1);
    }


    TEST_METHOD (Write_RejectsInvalidVolumeNames)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);



        AssertFailed (ProDosSkeleton::Write (buffer, ""));
        AssertFailed (ProDosSkeleton::Write (buffer, "1DISK"));
        AssertFailed (ProDosSkeleton::Write (buffer, "BAD NAME"));
        AssertFailed (ProDosSkeleton::Write (buffer, "WAYTOOLONGDISKNAME"));

        expect.RequireCount (4);
    }


    TEST_METHOD (KeyBlock_CarriesTheVolumeHeader)
    {
        vector<Byte>  vol = MakeVolume();

        // Storage type 0xF, name length 7, name NEWDISK uppercased.
        Assert::AreEqual ((Byte) 0xF7, At (vol, 2, kKeyBlockEntry + 0x00));

        Assert::AreEqual ((Byte) 'N', At (vol, 2, kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'E', At (vol, 2, kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) 'W', At (vol, 2, kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D', At (vol, 2, kKeyBlockEntry + 0x04));
        Assert::AreEqual ((Byte) 'I', At (vol, 2, kKeyBlockEntry + 0x05));
        Assert::AreEqual ((Byte) 'S', At (vol, 2, kKeyBlockEntry + 0x06));
        Assert::AreEqual ((Byte) 'K', At (vol, 2, kKeyBlockEntry + 0x07));

        Assert::AreEqual ((Byte) 0xC3, At (vol, 2, kKeyBlockEntry + 0x1E));  // access
        Assert::AreEqual ((Byte) 0x27, At (vol, 2, kKeyBlockEntry + 0x1F));  // entry length
        Assert::AreEqual ((Byte) 0x0D, At (vol, 2, kKeyBlockEntry + 0x20));  // entries/block

        Assert::AreEqual ((Word) 0,   WordAt (vol, 2, kKeyBlockEntry + 0x21));  // file count
        Assert::AreEqual ((Word) 6,   WordAt (vol, 2, kKeyBlockEntry + 0x23));  // bitmap ptr
        Assert::AreEqual ((Word) 280, WordAt (vol, 2, kKeyBlockEntry + 0x25));  // total blocks
    }


    TEST_METHOD (Write_UppercasesTheVolumeName)
    {
        vector<Byte>  vol = MakeVolume ("My.Disk");

        Assert::AreEqual ((Byte) 0xF7, At (vol, 2, kKeyBlockEntry + 0x00));
        Assert::AreEqual ((Byte) 'M',  At (vol, 2, kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'Y',  At (vol, 2, kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) '.',  At (vol, 2, kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D',  At (vol, 2, kKeyBlockEntry + 0x04));
    }


    TEST_METHOD (DirectoryChain_LinksBlocks2Through5)
    {
        vector<Byte>  vol = MakeVolume();

        Assert::AreEqual ((Word) 0, WordAt (vol, 2, 0x00));  // key has no prev
        Assert::AreEqual ((Word) 3, WordAt (vol, 2, 0x02));

        Assert::AreEqual ((Word) 2, WordAt (vol, 3, 0x00));
        Assert::AreEqual ((Word) 4, WordAt (vol, 3, 0x02));

        Assert::AreEqual ((Word) 3, WordAt (vol, 4, 0x00));
        Assert::AreEqual ((Word) 5, WordAt (vol, 4, 0x02));

        Assert::AreEqual ((Word) 4, WordAt (vol, 5, 0x00));
        Assert::AreEqual ((Word) 0, WordAt (vol, 5, 0x02));  // chain ends
    }


    TEST_METHOD (Bitmap_MarksBlocks0Through6UsedAndTheRestFree)
    {
        vector<Byte>  vol = MakeVolume();
        size_t        i   = 0;

        // Byte 0: blocks 0-6 used (clear), block 7 free (set).
        Assert::AreEqual ((Byte) 0x01, At (vol, 6, 0));

        // Bytes 1-34: blocks 8-279 all free. 280 blocks is exactly 35 bytes.
        for (i = 1; i < 35; i++)
        {
            Assert::AreEqual ((Byte) 0xFF, At (vol, 6, i));
        }

        // The rest of the bitmap block is dead space and stays zero.
        for (i = 35; i < 512; i++)
        {
            Assert::AreEqual ((Byte) 0x00, At (vol, 6, i));
        }
    }


    static vector<Byte> MakePattern (size_t count)
    {
        vector<Byte>  bytes (count);
        size_t        i = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) ((i * 7 + 3) & 0xFF);
        }

        return bytes;
    }

    static int CountFreeBlocks (const vector<Byte> & vol)
    {
        int  free  = 0;
        int  block = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            Byte  b    = At (vol, 6, (size_t) (block / 8));
            Byte  mask = (Byte) (0x80 >> (block % 8));

            if ((b & mask) != 0) { free++; }
        }

        return free;
    }

    //  A byte that names the data BLOCK it belongs to, so a mis-ordered index
    //  shows up as wrong content rather than merely a wrong length. Deliberately
    //  not linear in the block number: any linear stamp repeats every 256
    //  blocks, which is exactly where a tree's second index block begins.
    static Byte BlockStamp (size_t blockIndex)
    {
        return (Byte) ((blockIndex * 31 + (blockIndex >> 8) * 101 + 7) & 0xFF);
    }

    static FilePayload MakeBinaryPayload (size_t length, Word loadAddress)
    {
        FilePayload  payload;
        size_t       i = 0;

        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = loadAddress;
        payload.hasLoadAddress = true;

        payload.bytes.resize (length);

        for (i = 0; i < length; i++)
        {
            payload.bytes[i] = BlockStamp (i / kBlockBytes);
        }

        return payload;
    }

    static size_t EntryOffset (int slot)
    {
        return kKeyBlockEntry + (size_t) slot * kEntryLength;
    }

    static Word KeyPointerOf (const vector<Byte> & vol, int dirBlock, int slot)
    {
        return WordAt (vol, dirBlock, EntryOffset (slot) + kEntOffKeyPointer);
    }

    //  One pointer out of an index block: 256 low bytes then the 256 matching
    //  high bytes, which is the layout a pair-per-slot reader gets wrong only
    //  for blocks numbered above 255.
    static Word IndexPointerAt (const vector<Byte> & vol, int indexBlock, size_t slot)
    {
        return (Word) (At (vol, indexBlock, slot)
                    | (At (vol, indexBlock, slot + kPointersPerIndex) << 8));
    }

    static uint32_t FreeBlocks (const vector<Byte> & vol)
    {
        ProDosVolume   volume (vol);
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        return listing.freeUnits;
    }

    //  What a computed result must not disagree about.
    //
    //  IsClean is deliberately NOT the assertion even though a healthy ProDOS
    //  volume happens to satisfy it -- the skeleton claims its own boot,
    //  directory and bitmap blocks under a reserved owner, where the DOS 3.3
    //  side leaves them allocated and unclaimed. Relying on that would still be
    //  wrong here: a CORRECT delete that leaks reports blocks allocated with
    //  nothing owning them, which is precisely the outcome the leak rule
    //  requires, so a whole-volume yes/no would refuse it. Compare the result
    //  against the input on the specific sets instead.
    static void AssertEditLeftTheVolumeConsistent (const vector<Byte> & before, const vector<Byte> & after)
    {
        ProDosVolume           wasVolume (before);
        ProDosVolume           isVolume (after);
        VolumeIntegrityReport  was;
        VolumeIntegrityReport  is;

        AssertSucceeded (wasVolume.BuildIntegrityReport (was));
        AssertSucceeded (isVolume.BuildIntegrityReport (is));

        Assert::AreEqual (size_t (0), is.GetCrossLinked().size(),
            L"the edit must not leave a block claimed by two entries");
        Assert::AreEqual (size_t (0), is.GetClaimedButFree().size(),
            L"the edit must not leave a referenced block marked free");
        Assert::AreEqual (was.GetUnfollowableChains().size(), is.GetUnfollowableChains().size(),
            L"the edit must not break a structure that was whole");
        Assert::AreEqual (was.GetAllocatedButUnclaimed().size(), is.GetAllocatedButUnclaimed().size(),
            L"the edit must not leak space, nor reclaim space it did not own");
    }

    //  Two files that both claim one data block, made by pointing the first
    //  file's second index slot at the second file's second data block. The
    //  block the first file used to own is left allocated and unclaimed, which
    //  is what a cross-link costs even before anything is deleted.
    static vector<Byte> MakeVolumeWithCrossLinkedFiles (vector<Byte> & outSurvivorBytes,
                                                        Word         & outSharedBlock)
    {
        vector<Byte>  vol   = MakeVolume();
        vector<Byte>  a     = MakePattern (600);
        vector<Byte>  b     = MakePattern (600);
        Word          keyA  = 0;
        Word          keyB  = 0;
        size_t        i     = 0;

        for (i = 0; i < b.size(); i++) { b[i] = (Byte) ~b[i]; }

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FILEA", 0x06, 0x2000, a));
        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FILEB", 0x06, 0x2000, b));

        keyA = KeyPointerOf (vol, 2, 1);
        keyB = KeyPointerOf (vol, 2, 2);

        outSharedBlock = IndexPointerAt (vol, keyB, 1);

        Assert::IsTrue (outSharedBlock != 0, L"the fixture must have a second data block to share");

        vol[ProDosSkeleton::BlockByteOffset (keyA, 1)] = (Byte) (outSharedBlock & 0xFF);
        vol[ProDosSkeleton::BlockByteOffset (keyA, 1 + kPointersPerIndex)] =
            (Byte) (outSharedBlock >> 8);

        outSurvivorBytes = b;

        return vol;
    }

    //  A sapling whose index block names a block the volume does not have.
    //  Refusing to delete it would strand every block it holds.
    static vector<Byte> MakeVolumeWithDamagedIndexFile()
    {
        vector<Byte>  vol     = MakeVolume();
        vector<Byte>  payload = MakePattern (600);
        Word          key     = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BROKEN", 0x06, 0x2000, payload));

        key = KeyPointerOf (vol, 2, 1);

        vol[ProDosSkeleton::BlockByteOffset (key, 1)]                     = 0xF4;   // 500
        vol[ProDosSkeleton::BlockByteOffset (key, 1 + kPointersPerIndex)] = 0x01;

        return vol;
    }


    TEST_METHOD (FileWriter_SeedlingRoundTripsThroughTheReader)
    {
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  payload  = MakePattern (300);
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0x2000, payload));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", readBack, fileType, auxType));

        Assert::IsTrue (payload == readBack, L"seedling data must round-trip");
        Assert::AreEqual ((Byte) 0xFF,   fileType);
        Assert::AreEqual ((Word) 0x2000, auxType);
    }


    TEST_METHOD (FileWriter_SaplingRoundTripsThroughTheReader)
    {
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  payload  = MakePattern (10'873);   // 22 data blocks
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BASIC.SYSTEM", 0xFF, 0x2000, payload));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "basic.system", readBack, fileType, auxType));

        Assert::IsTrue (payload == readBack, L"sapling data must round-trip, case-insensitive lookup");
    }


    TEST_METHOD (FileWriter_TwoFilesShareNoBlocks)
    {
        vector<Byte>  vol        = MakeVolume();
        vector<Byte>  a          = MakePattern (2'000);
        vector<Byte>  b          = MakePattern (3'000);
        vector<Byte>  backA;
        vector<Byte>  backB;
        Byte          ft         = 0;
        Word          aux        = 0;
        int           freeBefore = CountFreeBlocks (vol);
        int           freeAfter  = 0;



        // Distinct content so cross-contamination cannot cancel out.
        for (size_t i = 0; i < b.size(); i++) { b[i] = (Byte) ~b[i]; }

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, a));
        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BASIC.SYSTEM", 0xFF, 0, b));

        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", backA, ft, aux));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "BASIC.SYSTEM", backB, ft, aux));

        Assert::IsTrue (a == backA, L"first file intact after the second write");
        Assert::IsTrue (b == backB, L"second file intact");

        // Bitmap accounting: 4+1 blocks for a (sapling), 6+1 for b.
        freeAfter = CountFreeBlocks (vol);
        Assert::AreEqual (freeBefore - 12, freeAfter, L"exactly the allocated blocks left the bitmap");

        // The header counts both files.
        Assert::AreEqual ((Word) 2, WordAt (vol, 2, kKeyBlockEntry + 0x21));
    }


    TEST_METHOD (FileWriter_RejectsBadArguments)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  vol       = MakeVolume();
        vector<Byte>  undersized (100, 0);
        vector<Byte>  payload    = MakePattern (10);
        vector<Byte>  oversized  = MakePattern (257 * 512);



        AssertFailed (ProDosFileWriter::WriteFile (undersized, "PRODOS", 0xFF, 0, payload));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "", 0xFF, 0, payload));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, {}));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, oversized));

        expect.RequireCount (4);
    }


    TEST_METHOD (Reader_MissingFileFailsCleanly)
    {
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertFailed (ProDosReader::ExtractFile (vol, "NOSUCH", readBack, fileType, auxType));
        Assert::IsTrue (readBack.empty());
    }


    TEST_METHOD (Reader_KeyPointerOutsideTheVolume_IsRefusedNotFollowed)
    {
        // Block numbers index straight into the sector buffer through
        // BlockByteOffset, which multiplies out without checking anything, so a
        // block past the volume does not read wrong data -- it reads past the
        // end of the buffer entirely. Block 60000 lands roughly 30 MB beyond a
        // 140 KB image.
        //
        // This is reachable today, not only through the new volume layer:
        // InstallBoot extracts PRODOS and BASIC.SYSTEM from a downloaded master
        // for bootable-disk creation.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  payload (600, 0x5A);   // two blocks -- a sapling
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        size_t        entryAt  = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, payload));

        // Point the entry's key block far outside the volume.
        entryAt = ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x27 + 0x11);

        vol[entryAt]     = 0x60;
        vol[entryAt + 1] = 0xEA;   // 60000

        AssertFailed (ProDosReader::ExtractFile (vol, "VICTIM", readBack, fileType, auxType));
    }


    TEST_METHOD (Reader_IndexEntryOutsideTheVolume_IsRefusedNotFollowed)
    {
        // The same hazard one level down: the key block is valid but a pointer
        // inside it is not. Gathering it would read outside the buffer, and
        // returning what was gathered would hand back a truncated file that
        // looks whole.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  payload (600, 0x33);
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        Word          keyBlock = 0;
        size_t        entryAt  = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, payload));

        entryAt  = ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x27 + 0x11);
        keyBlock = (Word) (vol[entryAt] | (vol[entryAt + 1] << 8));

        // First data pointer in the index block -> outside the volume.
        vol[ProDosSkeleton::BlockByteOffset (keyBlock, 0)]       = 0x60;
        vol[ProDosSkeleton::BlockByteOffset (keyBlock, 0 + 256)] = 0xEA;

        AssertFailed (ProDosReader::ExtractFile (vol, "VICTIM", readBack, fileType, auxType));
    }


    TEST_METHOD (Volume_Enumerate_ReportsVolumeNameFilesAndFreeSpace)
    {
        vector<Byte>   vol     = MakeVolume ("MYDISK");
        vector<Byte>   payload (600, 0x41);
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x6000, payload));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::IsTrue (listing.hasVolumeName);
        Assert::AreEqual (string ("MYDISK"), listing.volumeName);
        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG"), listing.entries[0].name);
        Assert::AreEqual (Byte (0x06), listing.entries[0].type);

        // A binary's auxiliary type IS its load address; naming it as such
        // saves every caller from having to know that.
        Assert::IsTrue (listing.entries[0].hasLoadAddress);
        Assert::AreEqual (Word (0x6000), listing.entries[0].loadAddress);
        Assert::IsTrue (listing.entries[0].hasEofBytes);
        Assert::AreEqual (uint32_t (600), listing.entries[0].eofBytes);
        Assert::IsFalse (listing.entries[0].isLocked);
        Assert::IsTrue (listing.freeUnits > 0 && listing.freeUnits < 280);
    }


    TEST_METHOD (Volume_Read_ReturnsTheBytesAndTheLoadAddress)
    {
        vector<Byte>  vol     = MakeVolume();
        vector<Byte>  payload (600, 0x7E);
        FilePayload   got;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Read (FilePath::Parse ("PROG"), got));
        }

        Assert::AreEqual (size_t (600), got.bytes.size());
        Assert::AreEqual (Byte (0x7E), got.bytes[599]);
        Assert::IsTrue (got.hasLoadAddress);
        Assert::AreEqual (Word (0x2000), got.loadAddress);
        Assert::IsTrue (got.hasAuxType, L"ProDOS records an auxiliary type for every file");
    }


    TEST_METHOD (Volume_Read_MultiComponentPath_IsRefusedNotSilentlyTruncated)
    {
        vector<Byte>  vol     = MakeVolume();
        vector<Byte>  payload (600, 0x11);
        FilePayload   got;
        HRESULT       hr      = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload));

        {
            ProDosVolume  volume (vol);

            hr = volume.Read (FilePath::Parse ("UTIL/PROG"), got);
        }

        Assert::IsTrue (FAILED (hr),
            L"subdirectory traversal is not built, so a deeper path must be refused");
    }


    TEST_METHOD (Volume_IntegrityReport_CleanVolumeAgreesWithItsBitmap)
    {
        // The volume's own structures -- boot blocks, directory, bitmap -- are
        // claimed by the volume rather than by any file. Without that, every
        // volume would report its own metadata as space allocated to nobody.
        vector<Byte>           vol     = MakeVolume();
        vector<Byte>           payload (600, 0x22);
        VolumeIntegrityReport  report;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.BuildIntegrityReport (report));
        }

        Assert::IsTrue (report.IsCatalogFullyParsed());
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"no block may be claimed twice");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size(),
            L"nothing the directory references may be marked free");
        Assert::AreEqual (size_t (0), report.GetAllocatedButUnclaimed().size(),
            L"and nothing allocated may be unaccounted for");
        Assert::IsTrue (report.IsClean(), L"a freshly written volume is consistent");
    }


    TEST_METHOD (Volume_IntegrityReport_NamesTheBlocksAFileClaims)
    {
        // A sapling: one index block plus two data blocks.
        vector<Byte>           vol     = MakeVolume();
        vector<Byte>           payload (600, 0x33);
        VolumeIntegrityReport  report;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.BuildIntegrityReport (report));
        }

        Assert::AreEqual (size_t (3), report.GetClaimsOf (0).size(),
            L"the index block counts as the file's footprint too");
    }


    TEST_METHOD (Reader_DirectoryChainThatLoops_Terminates)
    {
        // A directory chain pointing back at itself. Without a guard this never
        // returns, which for a recovery tool is the worst failure of all: the
        // user cannot even see what went wrong.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;

        // Key block 2's next pointer -> itself.
        vol[ProDosSkeleton::BlockByteOffset (2, 0x02)]     = 0x02;
        vol[ProDosSkeleton::BlockByteOffset (2, 0x02 + 1)] = 0x00;

        AssertFailed (ProDosReader::ExtractFile (vol, "NOSUCH", readBack, fileType, auxType));
    }


    static vector<Byte> MakeSyntheticUsersDisk (vector<Byte> & outProdos, vector<Byte> & outBasic)
    {
        // A fabricated Users Disk: a real skeleton volume carrying a boot
        // pattern in blocks 0-1 and synthetic PRODOS / BASIC.SYSTEM files.
        vector<Byte>  users = MakeVolume ("USERS.DISK");
        size_t        i     = 0;

        for (i = 0; i < 1024; i++)
        {
            users[ProDosSkeleton::BlockByteOffset ((int) (i / 512), i % 512)] =
                (Byte) ((i * 13 + 1) & 0xFF);
        }

        outProdos = MakePattern (15'000);
        outBasic  = MakePattern (10'000);

        for (i = 0; i < outBasic.size(); i++)
        {
            outBasic[i] = (Byte) ~outBasic[i];
        }

        AssertSucceeded (ProDosFileWriter::WriteFile (users, "PRODOS", 0xFF, 0x2000, outProdos));
        AssertSucceeded (ProDosFileWriter::WriteFile (users, "BASIC.SYSTEM", 0xFF, 0x2000, outBasic));

        return users;
    }


    TEST_METHOD (InstallBoot_CopiesBootBlocksAndBothSystemFiles)
    {
        vector<Byte>  prodosBytes;
        vector<Byte>  basicBytes;
        vector<Byte>  users    = MakeSyntheticUsersDisk (prodosBytes, basicBytes);
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        size_t        i        = 0;



        AssertSucceeded (ProDosSkeleton::InstallBoot (vol, users));

        // Boot blocks 0-1 copied verbatim.
        for (i = 0; i < 1024; i++)
        {
            size_t  at = ProDosSkeleton::BlockByteOffset ((int) (i / 512), i % 512);

            if (vol[at] != users[at])
            {
                Assert::Fail (L"boot blocks must copy verbatim");
            }
        }

        // Both system files landed as real, readable files.
        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", readBack, fileType, auxType));
        Assert::IsTrue (prodosBytes == readBack, L"PRODOS must round-trip through the install");
        Assert::AreEqual ((Word) 0x2000, auxType);

        AssertSucceeded (ProDosReader::ExtractFile (vol, "BASIC.SYSTEM", readBack, fileType, auxType));
        Assert::IsTrue (basicBytes == readBack, L"BASIC.SYSTEM must round-trip through the install");

        // The volume name stayed the skeleton's own.
        Assert::AreEqual ((Byte) 0xF7, At (vol, 2, kKeyBlockEntry + 0x00));
    }


    TEST_METHOD (EverythingOutsideDirAndBitmapIsZero)
    {
        vector<Byte>  vol    = MakeVolume();
        int           block  = 0;
        size_t        offset = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            bool  structural = (block >= 2 && block <= 6);

            if (structural)
            {
                continue;
            }

            for (offset = 0; offset < 512; offset++)
            {
                if (At (vol, block, offset) != 0)
                {
                    wchar_t  msg[96] = {};

                    swprintf_s (msg, L"block %d offset %zu must be zero", block, offset);
                    Assert::Fail (msg);
                }
            }
        }
    }


    //  Volume bitmap: MSB of byte 0 is block 0, a SET bit means free.
    static void MarkFree (vector<Byte> & vol, uint32_t block, bool isFree)
    {
        size_t  at   = ProDosSkeleton::BlockByteOffset (6, (size_t) (block / 8));
        Byte    mask = (Byte) (0x80 >> (block % 8));

        vol[at] = isFree ? (Byte) (vol[at] | mask) : (Byte) (vol[at] & ~mask);
    }

    static vector<Byte> LoadFixture (const char * relativePath)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (relativePath, bytes));
        Assert::IsTrue (bytes.size() > 0, L"a fixture must not be empty");

        return bytes;
    }


    TEST_METHOD (Volume_Write_BinaryOntoAFreshVolume_ReadsBackByteForByte)
    {
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = MakeBinaryPayload (600, 0x6000);
        FilePayload    back;
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG"), listing.entries[0].name);
        Assert::AreEqual (ProDosVolume::kTypeBinary, listing.entries[0].type);
        Assert::IsFalse (listing.entries[0].isLocked);

        // Two data blocks plus the index block above them.
        Assert::AreEqual (uint32_t (3), listing.entries[0].sizeUnits);
        Assert::AreEqual (kFreeOnBlankVolume - 3, listing.freeUnits,
            L"exactly the blocks the file occupies must leave the free pool");

        AssertSucceeded (written.Read (FilePath::Parse ("PROG"), back));

        Assert::IsTrue (payload.bytes == back.bytes, L"the bytes read back must be the bytes placed");
        Assert::IsTrue (back.hasLoadAddress);
        Assert::AreEqual (Word (0x6000), back.loadAddress);

        AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Write_PutsTheLoadAddressInTheEntryAndNoHeaderInsideTheFile)
    {
        // The one thing the two filesystems disagree about. A DOS 3.3 binary
        // carries its load address and length in its own first four bytes, so
        // its stored size is the payload plus four. ProDOS records both in the
        // directory entry and stores no header at all -- measured on /MERLIN's
        // PARMS, whose EOF is 44 with a first stored byte of $3C and an
        // auxiliary type of $8000.
        //
        // A writer that carried the DOS 3.3 habit across would produce a file
        // four bytes long with four bytes of header at the front of it, and
        // nothing but this assertion would notice.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = MakeBinaryPayload (600, 0x6000);
        VolumeListing  listing;
        Word           key     = 0;
        Word           first   = 0;

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (uint32_t (600), listing.entries[0].eofBytes,
            L"the recorded length is the payload's own, not the payload plus a header");
        Assert::AreEqual (Word (0x6000), listing.entries[0].auxType,
            L"the load address lives in the auxiliary type on this filesystem");

        key   = KeyPointerOf (result, 2, 1);
        first = IndexPointerAt (result, key, 0);

        Assert::AreEqual (payload.bytes[0], At (result, first, 0),
            L"the file's first stored byte is the payload's first byte, not a header byte");
    }


    TEST_METHOD (Volume_Write_ProducesNothingWhenItRefuses_AndNeverTouchesTheInput)
    {
        vector<Byte>   vol      = MakeVolume();
        vector<Byte>   original;
        vector<Byte>   result;
        FilePayload    payload  = MakeBinaryPayload (600, 0x2000);
        HRESULT        hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload.bytes));

        original = vol;

        {
            ProDosVolume  volume (vol);

            hr = volume.Write (FilePath::Parse ("1BADNAME"), payload, result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME), hr);
        Assert::AreEqual (size_t (0), result.size(), L"a refused write produces nothing");
        Assert::IsTrue (vol == original, L"a refused write must not have touched the source image");
    }


    TEST_METHOD (Volume_Write_OverAnExistingName_ReplacesItRatherThanRefusing)
    {
        // The interim refusal this supersedes. What must never appear is a
        // second directory record of the same name, so the assertion is on the
        // count as much as on the contents.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        FilePayload    payload = MakeBinaryPayload (1'200, 0x6000);
        FilePayload    back;
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
            AssertSucceeded (written.Read (FilePath::Parse ("PROG"), back));
        }

        Assert::AreEqual (size_t (1), listing.entries.size(),
            L"one record of the name, never two");
        Assert::AreEqual (string ("PROG"), listing.entries[0].name);
        Assert::AreEqual (uint32_t (1'200), listing.entries[0].eofBytes,
            L"the replacement's length, not the length of what it replaced");
        Assert::AreEqual (uint32_t (4), listing.entries[0].sizeUnits,
            L"three data blocks and the index block above them");

        // Three blocks came back before four went out. Had the old file's space
        // merely been abandoned this would read kFreeOnBlankVolume - 7.
        Assert::AreEqual (kFreeOnBlankVolume - 4, listing.freeUnits,
            L"the replaced file's blocks return to the pool");
        Assert::AreEqual (Word (0x6000), listing.entries[0].auxType);

        Assert::IsTrue (payload.bytes == back.bytes,
            L"the bytes read back must be the bytes placed");

        AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Write_AReplacementThatCannotBeCompleted_LeavesTheOriginalWhereItWas)
    {
        // The failure replace exists to prevent. A removal APPLIED to the image
        // followed by a placement that fails loses the file outright: the record
        // is gone, its blocks are back in the pool, and nothing was written in
        // its place.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  original;
        vector<Byte>  result;
        vector<Byte>  existing = MakePattern (600);
        FilePayload   huge     = MakeBinaryPayload (60'000, 0x2000);
        FilePayload   back;
        HRESULT       hr       = S_OK;
        uint32_t      block    = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, existing));

        // Every block spoken for, so the replacement cannot fit even once the
        // original's three come back.
        for (block = 0; block < 280; block++)
        {
            MarkFree (vol, block, false);
        }

        original = vol;

        {
            ProDosVolume  volume (vol);

            hr = volume.Write (FilePath::Parse ("PROG"), huge, result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
        Assert::AreEqual (size_t (0), result.size(),
            L"a refused replacement must not hand back the half-done removal");
        Assert::IsTrue (vol == original, L"nor touch the image it was computed from");

        {
            ProDosVolume  after (vol);

            AssertSucceeded (after.Read (FilePath::Parse ("PROG"), back));
        }

        Assert::IsTrue (existing == back.bytes,
            L"the file it would have replaced must still be readable, byte for byte");
    }


    TEST_METHOD (Volume_Write_OverAFileTheAccessByteProtectsFromRemoval_IsRefused)
    {
        // A consequence of computing replacement as remove-then-place, recorded
        // deliberately: ProDOS gates writing and destroying on different bits,
        // and a replacement here releases the old file's blocks. So a file
        // marked writable but not destroyable is refused rather than rebuilt --
        // conservative while this layer cannot rewrite one in place.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  result;
        FilePayload   payload  = MakeBinaryPayload (600, 0x2000);
        size_t        accessAt = 0;
        HRESULT       hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        accessAt = ProDosSkeleton::BlockByteOffset (2, EntryOffset (1) + kEntOffAccess);

        // Write-enable set, destroy-enable clear.
        vol[accessAt] = 0x43;

        {
            ProDosVolume  volume (vol);

            hr = volume.Write (FilePath::Parse ("PROG"), payload, result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Write_OverALockedFile_IsRefusedForBeingLockedNotForExisting)
    {
        // The two refusals differ in what the user must do about them, and the
        // access byte can express write-protection without delete-protection,
        // so they are decided by different bits rather than by one flag.
        vector<Byte>   vol      = MakeVolume();
        vector<Byte>   result;
        FilePayload    payload  = MakeBinaryPayload (600, 0x2000);
        size_t         accessAt = 0;
        HRESULT        hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload.bytes));

        accessAt = ProDosSkeleton::BlockByteOffset (2, EntryOffset (1) + kEntOffAccess);

        // Destroy-enable set, write-enable clear: removable but not rewritable.
        vol[accessAt] = 0x81;

        {
            ProDosVolume  volume (vol);

            hr = volume.Write (FilePath::Parse ("PROG"), payload, result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Write_NamesTheGuestCouldNotOpenAgain_AreRefused)
    {
        // Validating a name being CREATED is not the printable-text check on
        // names being READ that was tried and reverted on the DOS 3.3 side.
        // Reading imposes no rule; a name ProDOS itself would reject is one
        // nobody can open again.
        vector<Byte>    vol      = MakeVolume();
        ProDosVolume    volume (vol);
        FilePayload     payload  = MakeBinaryPayload (600, 0x2000);
        vector<string>  rejected;
        size_t          i        = 0;

        rejected.push_back ("");
        rejected.push_back ("1PROG");
        rejected.push_back (" PROG");
        rejected.push_back ("PROG FILE");
        rejected.push_back ("PROG_FILE");
        rejected.push_back ("PROGRAMNAMETOOLONG");
        rejected.push_back ("SUB/PROG");

        Assert::IsTrue (rejected.size() > 0, L"the case list must not be empty");

        for (i = 0; i < rejected.size(); i++)
        {
            vector<Byte>  result;
            HRESULT       hr = volume.Write (FilePath::Parse (rejected[i]), payload, result);

            Assert::IsTrue (FAILED (hr), L"an unusable name must be refused");
            Assert::AreEqual (size_t (0), result.size());
        }
    }


    TEST_METHOD (Volume_Write_LowerCaseName_IsStoredTheWayTheGuestCanTypeIt)
    {
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = MakeBinaryPayload (600, 0x2000);
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("prog.a"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG.A"), listing.entries[0].name);
    }


    TEST_METHOD (Volume_Write_ABinaryWithNoLoadAddress_IsRefusedRatherThanDefaulted)
    {
        // $0000 is a legal load address, so a default would be
        // indistinguishable from an answer.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        FilePayload    payload;
        HRESULT        hr      = S_OK;

        payload.type  = ProDosVolume::kTypeBinary;
        payload.bytes = MakePattern (600);

        hr = volume.Write (FilePath::Parse ("PROG"), payload, result);

        Assert::IsTrue (FAILED (hr), L"a binary with nowhere to load must be refused");
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Write_ToAVolumeWithNoRoom_IsRefusedNamingDiskFull)
    {
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        FilePayload    payload = MakeBinaryPayload (600, 0x2000);
        uint32_t       block   = 0;
        HRESULT        hr      = S_OK;

        for (block = 0; block < 280; block++)
        {
            MarkFree (vol, block, false);
        }

        hr = volume.Write (FilePath::Parse ("PROG"), payload, result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
        Assert::AreEqual (size_t (0), result.size(),
            L"a partial allocation must not reach the caller");
    }


    TEST_METHOD (Volume_Write_NeverHandsOutABlockTheDirectoryStillClaims)
    {
        // A bitmap calling a block free while an entry still points at it is
        // exactly what a bad delete leaves behind. Handing that block to the
        // next file destroys the first one, and nothing says so until someone
        // reads it.
        vector<Byte>   vol      = MakeVolume();
        vector<Byte>   result;
        vector<Byte>   existing = MakePattern (600);
        FilePayload    payload  = MakeBinaryPayload (600, 0x2000);
        FilePayload    survivor;
        Word           key      = 0;
        Word           stolen   = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, existing));

        key    = KeyPointerOf (vol, 2, 1);
        stolen = IndexPointerAt (vol, key, 1);

        MarkFree (vol, stolen, true);

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Read (FilePath::Parse ("VICTIM"), survivor));
        }

        Assert::IsTrue (existing == survivor.bytes,
            L"the existing file must still be the file it was");
    }


    TEST_METHOD (Volume_Write_TheSaplingToTreeBoundary_GrowsExactlyWhereProDosDoes)
    {
        // One index block holds 256 pointers, so 256 data blocks is the largest
        // file that is still a sapling and 257 is the first that is not. A
        // writer that stopped at a sapling either refuses the second case or
        // -- worse -- writes 257 pointers into a block that holds 256, running
        // the last one into the high-byte half of the first.
        vector<Byte>   atLimit     = MakeVolume();
        vector<Byte>   pastLimit   = MakeVolume();
        vector<Byte>   sapling;
        vector<Byte>   tree;
        VolumeListing  saplingList;
        VolumeListing  treeList;

        {
            ProDosVolume  volume (atLimit);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           MakeBinaryPayload (kSaplingMaxBytes, 0x2000),
                                           sapling));
        }

        {
            ProDosVolume  volume (pastLimit);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           MakeBinaryPayload (kSaplingMaxBytes + 1, 0x2000),
                                           tree));
        }

        {
            ProDosVolume  written (sapling);

            AssertSucceeded (written.Enumerate (saplingList));
        }

        {
            ProDosVolume  written (tree);

            AssertSucceeded (written.Enumerate (treeList));
        }

        Assert::AreEqual (kStorageSapling, (Byte) (At (sapling, 2, EntryOffset (1)) & 0xF0),
            L"256 data blocks still fit one index block");
        Assert::AreEqual (uint32_t (kSaplingMaxBlocks), saplingList.entries[0].sizeUnits);

        Assert::AreEqual (kStorageTree, (Byte) (At (tree, 2, EntryOffset (1)) & 0xF0),
            L"one more data block requires a master index");
        Assert::AreEqual (uint32_t (kSaplingMaxBlocks + 3), treeList.entries[0].sizeUnits,
            L"one more data block, a second index block, and the master above them");
    }


    TEST_METHOD (Volume_Write_AFileNeedingATree_LaysOutAMasterIndexAndReadsBackWhole)
    {
        // No real volume in the fixture set can hold a tree: they are 280-block
        // disks and a tree needs more than 256 data blocks, so the largest file
        // any of them carries is 60. This shape has to be constructed, and what
        // that buys is a self-consistency check plus the byte layout below --
        // it cannot say whether Apple's ProDOS would agree, because nothing
        // here was written by it.
        vector<Byte>           vol     = MakeVolume();
        vector<Byte>           result;
        ProDosVolume           volume (vol);
        FilePayload            payload = MakeBinaryPayload (kTreePayloadBytes, 0x2000);
        FilePayload            back;
        VolumeListing          listing;
        VolumeIntegrityReport  report;
        Word                   master  = 0;
        Word                   index0  = 0;
        Word                   index1  = 0;

        AssertSucceeded (volume.Write (FilePath::Parse ("BIG"), payload, result));

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
            AssertSucceeded (written.Read (FilePath::Parse ("BIG"), back));
            AssertSucceeded (written.BuildIntegrityReport (report));
        }

        Assert::AreEqual (kStorageTree, (Byte) (At (result, 2, EntryOffset (1)) & 0xF0));
        Assert::AreEqual (uint32_t (kTreePayloadBytes), listing.entries[0].eofBytes);

        // Block accounting asserted against the integrity pass, not by reading
        // the entry back: the entry is what the writer said, the report is what
        // the volume actually references.
        Assert::AreEqual (uint32_t (kTreeTotalBlocks), listing.entries[0].sizeUnits);
        Assert::AreEqual (size_t (kTreeTotalBlocks), report.GetClaimsOf (0).size(),
            L"every index block is as much the file's footprint as its data");
        Assert::AreEqual (kFreeOnBlankVolume - (uint32_t) kTreeTotalBlocks, listing.freeUnits);
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size());
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size());
        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size());

        // The structure ProDOS documents: the key block is a master index of
        // index blocks, each of which holds up to 256 data pointers.
        master = KeyPointerOf (result, 2, 1);
        index0 = IndexPointerAt (result, master, 0);
        index1 = IndexPointerAt (result, master, 1);

        Assert::IsTrue (index0 != 0 && index1 != 0, L"258 data blocks need two index blocks");
        Assert::AreNotEqual (index0, index1);
        Assert::AreEqual (Word (0), IndexPointerAt (result, master, 2),
            L"and only two, so the third master slot stays a hole");

        Assert::AreEqual (payload.bytes[0], At (result, IndexPointerAt (result, index0, 0), 0),
            L"the first data block is reached master -> index -> data");

        Assert::AreEqual (kTreePayloadBytes, back.bytes.size(),
            L"the whole file must come back, not just the first index block's worth");

        // Each byte names the data block it belongs to. The last of these is
        // reachable only through the SECOND index block, which is the half a
        // sapling-only writer cannot produce.
        Assert::AreEqual (BlockStamp (255), back.bytes[255 * kBlockBytes]);
        Assert::AreEqual (BlockStamp (256), back.bytes[256 * kBlockBytes]);
        Assert::AreEqual (BlockStamp (257), back.bytes[257 * kBlockBytes]);

        AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Write_ASaplingSizedFile_MatchesTheShapeRealProDosGaveOne)
    {
        // The half of tree growth that real material CAN check. /APPLESOFT's
        // WHATSIT.A.Q was written in 1985 by software that had never heard of
        // this code: 29,798 bytes recorded as 60 blocks. Writing a file of the
        // same length here must arrive at the same block count, which is the
        // only available evidence that the overhead arithmetic -- data blocks
        // plus index blocks -- matches Apple's rather than merely itself.
        vector<Byte>   disk       = LoadFixture ("Disks/Merlin-proProdos2.33-b.dsk");
        vector<Byte>   vol        = MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        VolumeListing  real;
        VolumeListing  ours;
        size_t         i          = 0;
        bool           found      = false;
        uint32_t       realEof    = 0;
        uint32_t       realBlocks = 0;

        {
            ProDosVolume  onDisk (disk);

            AssertSucceeded (onDisk.Enumerate (real));
        }

        for (i = 0; i < real.entries.size(); i++)
        {
            if (real.entries[i].name == "WHATSIT.A.Q")
            {
                realEof    = real.entries[i].eofBytes;
                realBlocks = real.entries[i].sizeUnits;
                found      = true;
            }
        }

        Assert::IsTrue (found, L"the disk must carry the file this case is about");
        Assert::IsTrue (realEof > kBlockBytes, L"and it must be past seedling size");

        AssertSucceeded (volume.Write (FilePath::Parse ("SAME.SIZE"),
                                       MakeBinaryPayload (realEof, 0x2000), result));

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (ours));
        }

        Assert::AreEqual (realBlocks, ours.entries[0].sizeUnits,
            L"a file of the same length must occupy the same number of blocks real ProDOS used");
        Assert::AreEqual (kStorageSapling, (Byte) (At (result, 2, EntryOffset (1)) & 0xF0));
    }


    TEST_METHOD (Volume_Delete_AFileWithNoComplications_ReturnsExactlyItsBlocks)
    {
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), result, outcome));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
        }

        Assert::AreEqual (size_t (0), listing.entries.size(), L"the entry must be gone");
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"all three of its blocks return to the pool");
        Assert::AreEqual (size_t (3), outcome.freedUnits.size());
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (size_t (0), outcome.warnings.size(), L"a clean delete warns about nothing");
        Assert::IsTrue (outcome.catalogFullyParsed);

        // The volume header's own tally follows the directory it describes.
        Assert::AreEqual (Word (0), WordAt (result, 2, kKeyBlockEntry + 0x21));

        AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Delete_LeavesTheNameBehindButStopsResolvingIt)
    {
        // ProDOS's own tombstone: the storage-type nibble goes to zero and the
        // name stays, which is how an undelete tool finds the file again. The
        // consequence anything reading this volume must honor is that the name
        // is NOT what says a record is live -- resolving by name alone hands
        // back a file whose blocks have been given to somebody else.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        FilePayload    back;
        HRESULT        hr      = S_OK;
        Byte           typeLen = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), result));
        }

        typeLen = At (result, 2, EntryOffset (1) + kEntOffTypeName);

        Assert::AreEqual (Byte (0x00), (Byte) (typeLen & 0xF0), L"the storage type is cleared");
        Assert::AreEqual (Byte (0x04), (Byte) (typeLen & 0x0F), L"the name length survives");
        Assert::AreEqual (Byte ('P'), At (result, 2, EntryOffset (1) + kEntOffName),
            L"and so does the name itself");

        {
            ProDosVolume  written (result);

            hr = written.Read (FilePath::Parse ("PROG"), back);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), hr,
            L"a name left behind for an undelete tool must not still resolve");
    }


    TEST_METHOD (Volume_Delete_FreesOnlyWhatTheFileUniquelyOwns_AndReportsTheRestAsLeaked)
    {
        // The one failure here that damages a DIFFERENT file, and does it
        // silently: freeing a shared block succeeds, the volume looks healthy,
        // and the survivor is destroyed by the next allocation. Leaked space is
        // recoverable; a cross-linked free is not.
        vector<Byte>   survivorBytes;
        Word           shared     = 0;
        vector<Byte>   vol        = MakeVolumeWithCrossLinkedFiles (survivorBytes, shared);
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;
        FilePayload    survivor;
        uint32_t       freeBefore = FreeBlocks (vol);

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("FILEA"), result, outcome));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
            AssertSucceeded (written.Read (FilePath::Parse ("FILEB"), survivor));
        }

        Assert::AreEqual (size_t (2), outcome.freedUnits.size(),
            L"only the index block and the data block nothing else claims");
        Assert::AreEqual (size_t (1), outcome.leakedUnits.size(),
            L"the shared block is reported, not returned");
        Assert::AreEqual (uint32_t (shared), outcome.leakedUnits[0]);
        Assert::IsTrue (outcome.warnings.size() > 0, L"leaked space must be said out loud");

        Assert::AreEqual (freeBefore + 2, listing.freeUnits,
            L"three blocks were claimed and exactly two may come back");

        Assert::IsTrue (survivorBytes == survivor.bytes,
            L"the other file must still be whole, shared block included");

        AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Delete_AFileWhoseIndexIsDamaged_StillRemovesIt)
    {
        // Refusing would strand the volume: the entry could never be removed
        // and its blocks never reclaimed, on precisely the degraded disks this
        // exists to serve.
        vector<Byte>   vol     = MakeVolumeWithDamagedIndexFile();
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("BROKEN"), result, outcome));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
        }

        Assert::AreEqual (size_t (0), listing.entries.size(), L"a bad file must not be permanent");
        Assert::IsTrue (outcome.chainWasDamaged, L"the damage must be reported, not hidden");
        Assert::AreEqual (size_t (2), outcome.freedUnits.size(),
            L"what the walk reached is freed; what it never saw is not touched");
        Assert::IsTrue (outcome.warnings.size() > 0);
    }


    TEST_METHOD (Volume_Delete_ATreeFile_ReturnsItsIndexBlocksToo)
    {
        // A file needing a master index occupies those index blocks as surely
        // as it occupies its data. An accounting that saw only the key block
        // would leave the rest allocated with nothing owning them -- and not
        // even report them as leaked, because nothing knew they were the
        // file's.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   written;
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           MakeBinaryPayload (kTreePayloadBytes, 0x2000),
                                           written));
        }

        {
            ProDosVolume  volume (written);

            AssertSucceeded (volume.Delete (FilePath::Parse ("BIG"), result, outcome));
        }

        {
            ProDosVolume  after (result);

            AssertSucceeded (after.Enumerate (listing));
        }

        Assert::AreEqual (size_t (kTreeTotalBlocks), outcome.freedUnits.size(),
            L"258 data blocks, both index blocks, and the master above them");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"the volume must be as empty as it started");

        AssertEditLeftTheVolumeConsistent (written, result);
    }


    TEST_METHOD (Volume_Delete_AFileTheAccessByteProtectsFromRemoval_IsRefused)
    {
        // Delete is gated on destroy-enable, not on write-enable. The access
        // byte can carry one without the other, so a single "locked" flag would
        // be the wrong question in one direction or the other.
        vector<Byte>  vol      = MakeVolume();
        vector<Byte>  result;
        size_t        accessAt = 0;
        HRESULT       hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        accessAt = ProDosSkeleton::BlockByteOffset (2, EntryOffset (1) + kEntOffAccess);

        // Write-enable set, destroy-enable clear: rewritable but not removable.
        vol[accessAt] = 0x43;

        {
            ProDosVolume  volume (vol);

            hr = volume.Delete (FilePath::Parse ("PROG"), result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Delete_AMissingFileOrADeeperPath_IsRefused)
    {
        vector<Byte>  vol = MakeVolume();
        vector<Byte>  missing;
        vector<Byte>  deeper;
        ProDosVolume  volume (vol);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          volume.Delete (FilePath::Parse ("NOPE"), missing));
        Assert::AreEqual (size_t (0), missing.size());

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                          volume.Delete (FilePath::Parse ("UTIL/PROG"), deeper),
                          L"subdirectory traversal is not built, so a deeper path must be refused");
        Assert::AreEqual (size_t (0), deeper.size());
    }


    TEST_METHOD (Volume_Delete_FromAVolumeWhoseDirectoryDidNotFullyParse_WarnsDistinctly)
    {
        // The one case the unique-ownership rule can still lose data: an entry
        // that could not be read claims nothing observable, so a block it
        // shares looks unowned. The system must not claim otherwise.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   result;
        DeleteOutcome  outcome;
        size_t         i       = 0;
        bool           saidSo  = false;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        // The key block's next pointer aimed back at itself.
        vol[ProDosSkeleton::BlockByteOffset (2, 0x02)]     = 0x02;
        vol[ProDosSkeleton::BlockByteOffset (2, 0x02 + 1)] = 0x00;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), result, outcome));
        }

        Assert::IsFalse (outcome.catalogFullyParsed);
        Assert::IsTrue (outcome.warnings.size() > 0, L"an unbounded result must be reported as such");

        for (i = 0; i < outcome.warnings.size(); i++)
        {
            if (outcome.warnings[i].find ("directory did not parse") != string::npos)
            {
                saidSo = true;
            }
        }

        Assert::IsTrue (saidSo, L"the directory warning must be its own, not folded into another");
    }


    TEST_METHOD (Volume_Delete_ThenWrite_HandsTheReturnedBlocksAndTheSlotBackOut)
    {
        // What "returning space to the volume" has to mean in the end -- and on
        // this filesystem the directory slot is part of it, since the volume
        // directory cannot grow.
        vector<Byte>   vol     = MakeVolume();
        vector<Byte>   emptied;
        vector<Byte>   result;
        FilePayload    payload = MakeBinaryPayload (600, 0x2000);
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), emptied));
        }

        {
            ProDosVolume  afterDelete (emptied);

            AssertSucceeded (afterDelete.Write (FilePath::Parse ("NEXT"), payload, result));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
        }

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("NEXT"), listing.entries[0].name);
        Assert::AreEqual (kFreeOnBlankVolume - 3, listing.freeUnits,
            L"the new file fits in exactly the space the old one gave back");
        Assert::AreEqual (Byte ('N'), At (result, 2, EntryOffset (1) + kEntOffName),
            L"and in the directory slot it gave back, which cannot be grown");

        AssertEditLeftTheVolumeConsistent (emptied, result);
    }


    TEST_METHOD (Volume_Write_IntoABlockAPreviousFileUsedAsAnIndex_DoesNotInheritItsPointers)
    {
        // An index block still holding the previous occupant's pointers is read
        // as THIS file's structure, so the new file silently claims blocks it
        // was never given. The smaller second file is the case that matters:
        // its own pointers overwrite only the low slots and the stale ones
        // above them survive. The symptom is a claim on a block the bitmap
        // calls free, not a bad read, which is why the assertion is on the
        // integrity pass.
        vector<Byte>           vol     = MakeVolume();
        vector<Byte>           emptied;
        vector<Byte>           result;
        FilePayload            second  = MakeBinaryPayload (600, 0x2000);
        VolumeIntegrityReport  report;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FIRST", 0x06, 0x2000,
                                                      MakePattern (1'200)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("FIRST"), emptied));
        }

        {
            ProDosVolume  afterDelete (emptied);

            AssertSucceeded (afterDelete.Write (FilePath::Parse ("SECOND"), second, result));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.BuildIntegrityReport (report));
        }

        Assert::AreEqual (size_t (3), report.GetClaimsOf (0).size(),
            L"the new file must claim only the blocks it was given");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size(),
            L"a stale pointer shows up as a claim on a block the bitmap calls free");
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size());
    }


    TEST_METHOD (Volume_Delete_ASubdirectory_IsRefusedRatherThanOrphaningWhatIsInside)
    {
        // /MERLIN carries real subdirectories. This layer does not walk into
        // one, so the report credits it with its key block and nothing else --
        // deleting it would free that single block and silently orphan every
        // file beneath, while reporting a clean removal.
        vector<Byte>   disk   = LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
        vector<Byte>   result;
        ProDosVolume   volume (disk);
        VolumeListing  listing;
        size_t         i      = 0;
        bool           found  = false;
        HRESULT        hr     = S_OK;

        AssertSucceeded (volume.Enumerate (listing));

        for (i = 0; i < listing.entries.size() && !found; i++)
        {
            if (listing.entries[i].isDirectory)
            {
                hr    = volume.Delete (FilePath::Parse (listing.entries[i].name), result);
                found = true;
            }
        }

        Assert::IsTrue (found, L"this disk must carry the subdirectories the case is about");
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Delete_OnARealDisk_ReturnsExactlyWhatItSaysItReturned)
    {
        vector<Byte>   disk    = LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
        vector<Byte>   result;
        ProDosVolume   volume (disk);
        DeleteOutcome  outcome;
        VolumeListing  before;
        VolumeListing  after;
        size_t         chosen  = 0;
        size_t         i       = 0;
        bool           haveOne = false;

        AssertSucceeded (volume.Enumerate (before));

        for (i = 0; i < before.entries.size() && !haveOne; i++)
        {
            if (!before.entries[i].isDirectory && before.entries[i].sizeUnits > 0
                                               && !before.entries[i].isLocked)
            {
                chosen  = i;
                haveOne = true;
            }
        }

        Assert::IsTrue (haveOne,
            L"the disk must hold an unlocked file for this to be testing anything");

        AssertSucceeded (volume.Delete (FilePath::Parse (before.entries[chosen].name),
                                        result, outcome));

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (after));
        }

        Assert::AreEqual (before.entries.size() - 1, after.entries.size());
        Assert::AreEqual (before.freeUnits + (uint32_t) outcome.freedUnits.size(), after.freeUnits,
            L"the bitmap must move by exactly what the outcome claims");
        Assert::AreEqual (size_t (before.entries[chosen].sizeUnits), outcome.freedUnits.size(),
            L"and by exactly what the entry said the file occupied");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size(),
            L"nothing on this disk is cross-linked, so nothing should be left behind");
        Assert::IsTrue (outcome.catalogFullyParsed);
    }


    TEST_METHOD (Volume_PreCommitCheck_AResultClaimingABlockTwice_IsRefusedAsOurOwnDefect)
    {
        // Correct code cannot produce a result that fails this, which is what
        // the check is for -- so the only way to exercise the refusal is to hand
        // it one on purpose.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           survivor;
        Word                   shared = 0;
        vector<Byte>           input  = MakeVolume();
        vector<Byte>           result = MakeVolumeWithCrossLinkedFiles (survivor, shared);
        vector<Byte>           handedBack;
        ProDosVolume           volume (input);
        VolumeIntegrityReport  before;
        HRESULT                hr     = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        hr = ProDosVolume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr,
            L"a bad buffer from our own writer is a defect, not a verdict on input");
        Assert::AreEqual (size_t (0), handedBack.size(),
            L"and a refused buffer must not reach the caller at all");
        expect.RequireCount (1);
    }


    TEST_METHOD (Volume_PreCommitCheck_AResultWhoseBitmapDisagreesWithItsDirectory_IsRefused)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeVolume();
        vector<Byte>           result;
        vector<Byte>           handedBack;
        VolumeIntegrityReport  before;
        Word                   key    = 0;
        Word                   stolen = 0;
        HRESULT                hr     = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (input, "PROG", 0x06, 0x2000, MakePattern (600)));

        {
            ProDosVolume  volume (input);

            AssertSucceeded (volume.BuildIntegrityReport (before));
        }

        result = input;
        key    = KeyPointerOf (result, 2, 1);
        stolen = IndexPointerAt (result, key, 1);

        // A block the directory still references, handed back to the pool.
        MarkFree (result, stolen, true);

        hr = ProDosVolume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }


    TEST_METHOD (Volume_PreCommitCheck_ADeleteThatLeaks_IsAcceptedThoughTheResultIsNotClean)
    {
        // The leak rule and a whole-volume IsClean gate are in direct conflict.
        // Declining to free a cross-linked block is required, and it leaves
        // space allocated with nothing claiming it -- not clean, and exactly
        // right. That is why the check compares against the input rather than
        // asking one yes/no about the result.
        vector<Byte>           survivor;
        Word                   shared  = 0;
        vector<Byte>           vol     = MakeVolumeWithCrossLinkedFiles (survivor, shared);
        vector<Byte>           result;
        vector<Byte>           handedBack;
        VolumeIntegrityReport  before;
        VolumeIntegrityReport  after;
        DeleteOutcome          outcome;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.BuildIntegrityReport (before));
            AssertSucceeded (volume.Delete (FilePath::Parse ("FILEA"), result, outcome));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.BuildIntegrityReport (after));
        }

        Assert::AreEqual (size_t (1), outcome.leakedUnits.size(),
            L"the fixture must actually produce a leak");
        Assert::IsFalse (after.IsClean(),
            L"space allocated with nothing claiming it is what the leak rule detects");

        AssertSucceeded (ProDosVolume::HandBackVerifiedResult (before, result, handedBack));

        Assert::IsTrue (handedBack == result, L"an accepted buffer is handed over unchanged");
    }


    TEST_METHOD (Volume_Write_OntoARealDisk_LeavesEveryExistingEntryWhereItWas)
    {
        // /MERLIN is 1985 material with subdirectories, locked files and 21
        // free blocks. Placement on it must add one entry and take exactly the
        // blocks the new file needs.
        //
        // The comparison is by MEMBERSHIP, not by position, and that is a
        // finding rather than a convenience: this disk's volume directory has
        // an inactive record sitting BEFORE an active one, so ProDOS's reuse of
        // slots in place leaves a listing that is not densely packed. A new
        // file lands in the hole and every later entry keeps its slot while
        // moving one place down the listing.
        vector<Byte>   disk    = LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
        vector<Byte>   result;
        ProDosVolume   volume (disk);
        FilePayload    payload = MakeBinaryPayload (600, 0x0300);
        VolumeListing  before;
        VolumeListing  after;
        FilePayload    sample;
        size_t         i       = 0;
        size_t         j       = 0;

        AssertSucceeded (volume.Enumerate (before));
        AssertSucceeded (volume.Write (FilePath::Parse ("CASSOTEST"), payload, result));

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (after));
        }

        Assert::IsTrue (before.entries.size() > 0, L"the disk must carry entries");
        Assert::AreEqual (before.entries.size() + 1, after.entries.size());
        Assert::AreEqual (before.freeUnits - 3, after.freeUnits);

        for (i = 0; i < before.entries.size(); i++)
        {
            bool  survived = false;

            for (j = 0; j < after.entries.size(); j++)
            {
                if (after.entries[j].name == before.entries[i].name
                 && after.entries[j].sizeUnits == before.entries[i].sizeUnits)
                {
                    survived = true;
                }
            }

            Assert::IsTrue (survived, L"no existing entry may be displaced or resized");
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Read (FilePath::Parse ("CASSOTEST"), sample));
        }

        Assert::IsTrue (payload.bytes == sample.bytes);
    }

    //
    //  ------------------------------------------------------------------
    //  The startup program, which on this filesystem is a POSITION rather than
    //  a stored name: the kernel launches the first system program its walk of
    //  the volume directory reaches.
    //  ------------------------------------------------------------------
    //

    //  A system program. What the bytes are does not matter here -- which
    //  record the walk reaches first is the whole subject -- but the type does.
    static FilePayload MakeSystemPayload (size_t length)
    {
        FilePayload  payload = MakeBinaryPayload (length, 0x2000);

        payload.type = ProDosVolume::kTypeSystem;

        return payload;
    }

    static size_t IndexOfEntry (const VolumeListing & listing, const std::string & name)
    {
        size_t  i = 0;

        for (i = 0; i < listing.entries.size(); i++)
        {
            if (listing.entries[i].name == name)
            {
                return i;
            }
        }

        Assert::Fail (L"the listing must carry the entry being located");

        return 0;
    }

    //  Every buffer byte one directory record occupies. A record is NOT
    //  contiguous in the image -- a ProDOS block is two 256-byte halves that sit
    //  apart in a DOS-ordered buffer -- so the range has to be mapped rather
    //  than assumed.
    static void AddRecordBytes (std::vector<char>  & inOutAllowed,
                                int                  dirBlock,
                                int                  slot)
    {
        size_t  i = 0;

        for (i = 0; i < kEntryLength; i++)
        {
            inOutAllowed[ProDosSkeleton::BlockByteOffset (dirBlock, EntryOffset (slot) + i)] = 1;
        }
    }

    vector<Byte> MakeVolumeWithTwoSystemPrograms()
    {
        vector<Byte>  vol = MakeVolume();
        vector<Byte>  one;
        vector<Byte>  two;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("FIRST.SYSTEM"),
                                           MakeSystemPayload (600), one));
        }

        {
            ProDosVolume  volume (one);

            AssertSucceeded (volume.Write (FilePath::Parse ("SECOND.SYSTEM"),
                                           MakeSystemPayload (600), two));
        }

        return two;
    }

    TEST_METHOD (Volume_SetStartupProgram_PutsTheChosenProgramInFrontOfEveryOtherSystemFile)
    {
        // The reorder is the mechanism, so the assertion is about ORDER -- and
        // about what did not move. Every other record keeps its bytes, which is
        // checked by requiring every differing byte in the whole image to fall
        // inside one of the two records that were swapped.
        vector<Byte>       before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume       volume (before);
        vector<Byte>       after;
        VolumeListing      listedBefore;
        VolumeListing      listedAfter;
        std::vector<char>  allowed (NibblizationLayer::kImageByteSize, 0);
        size_t             differing = 0;
        size_t             i         = 0;

        AssertSucceeded (volume.Enumerate (listedBefore));

        Assert::AreEqual (size_t (0), IndexOfEntry (listedBefore, "FIRST.SYSTEM"),
            L"the volume must start out launching the other one, or nothing is being moved");

        AssertSucceeded (volume.SetStartupProgram (FilePath::Parse ("SECOND.SYSTEM"), after));

        {
            ProDosVolume  written (after);

            AssertSucceeded (written.Enumerate (listedAfter));
        }

        Assert::AreEqual (size_t (0), IndexOfEntry (listedAfter, "SECOND.SYSTEM"),
            L"the chosen program must now be the first record the walk reaches");

        Assert::AreEqual (listedBefore.entries.size(), listedAfter.entries.size(),
            L"and no entry may be gained or lost by reordering two of them");

        Assert::AreEqual (listedBefore.freeUnits, listedAfter.freeUnits,
            L"a swap allocates and frees nothing");

        AddRecordBytes (allowed, kDirKeyBlock, 1);
        AddRecordBytes (allowed, kDirKeyBlock, 2);

        for (i = 0; i < before.size(); i++)
        {
            if (before[i] != after[i])
            {
                differing++;

                Assert::AreEqual ((char) 1, allowed[i],
                    L"nothing outside the two swapped records may change -- not a block "
                    L"pointer, not the bitmap, not another file's entry");
            }
        }

        Assert::IsTrue (differing > 0, L"and the records must actually have been swapped");
    }

    TEST_METHOD (Volume_SetStartupProgram_AProgramAlreadyInFront_LeavesTheVolumeByteForByteAsItWas)
    {
        // Nothing to do is a legitimate outcome here, and rewriting the same
        // arrangement back would make an unnecessary commit look like an edit.
        vector<Byte>  before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume  volume (before);
        vector<Byte>  after;

        AssertSucceeded (volume.SetStartupProgram (FilePath::Parse ("FIRST.SYSTEM"), after));

        Assert::IsTrue (after == before,
            L"a program that is already first leaves the volume exactly as it was");
    }

    TEST_METHOD (Volume_SetStartupProgram_AFileThatIsNotASystemProgram_IsRefused)
    {
        // A BIN file cannot be launched by the boot path at any position, so
        // reordering the directory would produce a disk that looks configured
        // and boots to whatever the next system program happens to be.
        vector<Byte>  vol = MakeVolume();
        vector<Byte>  written;
        vector<Byte>  after;
        HRESULT       hr  = S_OK;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("PROG"),
                                           MakeBinaryPayload (600, 0x6000), written));
        }

        {
            ProDosVolume  volume (written);

            hr = volume.SetStartupProgram (FilePath::Parse ("PROG"), after);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE), hr);
        Assert::AreEqual (size_t (0), after.size(), L"and a refusal produces nothing");
    }

    TEST_METHOD (Volume_SetStartupProgram_AFileTheVolumeDoesNotHold_IsRefusedAndProducesNothing)
    {
        vector<Byte>  before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume  volume (before);
        vector<Byte>  after;
        HRESULT       hr     = S_OK;

        hr = volume.SetStartupProgram (FilePath::Parse ("NOSUCH.SYSTEM"), after);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), hr);
        Assert::AreEqual (size_t (0), after.size());
    }

    TEST_METHOD (Volume_SetStartupProgram_TheKernelItself_IsRefusedRatherThanMovedToTheFront)
    {
        // PRODOS is a SYS file like any other and is the one file that must
        // never be nominated: the boot block loads it by name, and a volume
        // whose startup program was the kernel would ask a running ProDOS to
        // load ProDOS. Asserted against a disk that shipped, where the kernel
        // is the first SYS record on the volume.
        vector<Byte>   disk = LoadFixture ("Disks/Merlin-proProdos2.33-b.dsk");
        ProDosVolume   volume (disk);
        vector<Byte>   after;
        VolumeListing  listing;
        HRESULT        hr   = S_OK;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (0), IndexOfEntry (listing, "PRODOS"),
            L"the kernel is the first record on this disk, which is what makes it the "
            L"one a naive rule would leave alone and a wrong rule would move");

        hr = volume.SetStartupProgram (FilePath::Parse ("PRODOS"), after);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE), hr);
        Assert::AreEqual (size_t (0), after.size());
    }

    TEST_METHOD (Volume_SetStartupProgram_OnARealDisk_OvertakesTheProgramItWouldHaveLaunched)
    {
        // /APPLESOFT boots PRODOS and then BASIC.SYSTEM. A newly placed system
        // program must end up ahead of BASIC.SYSTEM and BEHIND nothing -- while
        // the kernel keeps the slot it has always had, since the boot block
        // finds it by name and moving it would be a change nobody asked for.
        vector<Byte>   disk = LoadFixture ("Disks/Merlin-proProdos2.33-b.dsk");
        vector<Byte>   written;
        vector<Byte>   after;
        VolumeListing  before;
        VolumeListing  listed;

        {
            ProDosVolume  volume (disk);

            AssertSucceeded (volume.Enumerate (before));
            AssertSucceeded (volume.Write (FilePath::Parse ("CASSO.SYSTEM"),
                                           MakeSystemPayload (60), written));
        }

        {
            ProDosVolume  volume (written);

            AssertSucceeded (volume.SetStartupProgram (FilePath::Parse ("CASSO.SYSTEM"), after));
        }

        {
            ProDosVolume  volume (after);

            AssertSucceeded (volume.Enumerate (listed));
        }

        Assert::IsTrue (IndexOfEntry (listed, "CASSO.SYSTEM")
                      < IndexOfEntry (listed, "BASIC.SYSTEM"),
            L"the placed program must precede the one the disk used to launch");

        Assert::AreEqual (IndexOfEntry (before, "PRODOS"), IndexOfEntry (listed, "PRODOS"),
            L"and the kernel must not have been shuffled to make room for it");

        Assert::AreEqual (before.entries.size() + 1, listed.entries.size(),
            L"with every entry the disk shipped with still on it");
    }
};
