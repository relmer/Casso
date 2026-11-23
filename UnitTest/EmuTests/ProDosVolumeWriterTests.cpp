#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriterTests
//
//  ProDosVolume writes: files placed as seedling, sapling and tree, the case
//  of a file's name, replacing a file, directories created, and full volumes.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeWriterTests)
{
public:

    static constexpr Byte    kStorageSapling  = 0x20;
    static constexpr Byte    kStorageTree     = 0x30;

    //  Exactly 256 data blocks: the largest file that is still a sapling.
    static constexpr size_t  kSaplingMaxBytes  = 131'072;
    static constexpr size_t  kSaplingMaxBlocks = 257;   // + one index


    TEST_METHOD (Volume_Write_BinaryOntoAFreshVolume_ReadsBackByteForByte)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x6000);
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
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume - 3, listing.freeUnits,
            L"exactly the blocks the file occupies must leave the free pool");

        AssertSucceeded (written.Read (FilePath::Parse ("PROG"), back));

        Assert::IsTrue (payload.bytes == back.bytes, L"the bytes read back must be the bytes placed");
        Assert::IsTrue (back.hasLoadAddress);
        Assert::AreEqual (Word (0x6000), back.loadAddress);

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (vol, result);
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
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x6000);
        VolumeListing  listing;
        Word           key     = 0;
        Word           first   = 0;

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (uint32_t (600), listing.entries[0].eofBytes,
            L"the recorded length is the payload's own, not the payload plus a header");
        Assert::AreEqual (Word (0x6000), listing.entries[0].auxType,
            L"the load address lives in the auxiliary type on this filesystem");

        key   = ProDosVolumeFixtures::KeyPointerOf (result, 2, 1);
        first = ProDosVolumeFixtures::IndexPointerAt (result, key, 0);

        Assert::AreEqual (payload.bytes[0], ProDosVolumeFixtures::At (result, first, 0),
            L"the file's first stored byte is the payload's first byte, not a header byte");
    }


    TEST_METHOD (Volume_Write_ProducesNothingWhenItRefuses_AndNeverTouchesTheInput)
    {
        vector<Byte>   vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   original;
        vector<Byte>   result;
        FilePayload    payload  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
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
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (1'200, 0x6000);
        FilePayload    back;
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

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
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume - 4, listing.freeUnits,
            L"the replaced file's blocks return to the pool");
        Assert::AreEqual (Word (0x6000), listing.entries[0].auxType);

        Assert::IsTrue (payload.bytes == back.bytes,
            L"the bytes read back must be the bytes placed");

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Write_AReplacementThatCannotBeCompleted_LeavesTheOriginalWhereItWas)
    {
        // The failure replace exists to prevent. A removal APPLIED to the image
        // followed by a placement that fails loses the file outright: the record
        // is gone, its blocks are back in the pool, and nothing was written in
        // its place.
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  original;
        vector<Byte>  result;
        vector<Byte>  existing = ProDosVolumeFixtures::MakePattern (600);
        FilePayload   huge     = ProDosVolumeFixtures::MakeBinaryPayload (60'000, 0x2000);
        FilePayload   back;
        HRESULT       hr       = S_OK;
        uint32_t      block    = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, existing));

        // Every block spoken for, so the replacement cannot fit even once the
        // original's three come back.
        for (block = 0; block < 280; block++)
        {
            ProDosVolumeFixtures::MarkFree (vol, block, false);
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
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  result;
        FilePayload   payload  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        size_t        accessAt = 0;
        HRESULT       hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        accessAt = ProDosSkeleton::GetBlockByteOffset (2, ProDosVolumeFixtures::EntryOffset (1) + ProDosVolumeFixtures::kEntOffAccess);

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
        vector<Byte>   vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        FilePayload    payload  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        size_t         accessAt = 0;
        HRESULT        hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, payload.bytes));

        accessAt = ProDosSkeleton::GetBlockByteOffset (2, ProDosVolumeFixtures::EntryOffset (1) + ProDosVolumeFixtures::kEntOffAccess);

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
        vector<Byte>    vol      = ProDosVolumeFixtures::MakeVolume();
        ProDosVolume    volume (vol);
        FilePayload     payload  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
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


    //  The directory stores upper case, which is what the guest can type, and
    //  the GS/OS case word beside it records what was typed, so the listing
    //  reads back the way the name was written.
    TEST_METHOD (Volume_Write_LowerCaseName_IsStoredUpperCaseAndListsAsTyped)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        ProDosVolume   written (result);
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("prog.a"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("prog.a"), listing.entries[0].name);

        //  Upper case on the disk, where ProDOS 8 reads it.
        Assert::AreEqual ((Byte) 'P', ProDosVolumeFixtures::At (result, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + ProDosVolumeFixtures::kEntOffName));
        Assert::AreEqual ((Byte) 'R', ProDosVolumeFixtures::At (result, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + ProDosVolumeFixtures::kEntOffName + 1));
    }


    TEST_METHOD (Volume_Write_ABinaryWithNoLoadAddress_IsRefusedRatherThanDefaulted)
    {
        // $0000 is a legal load address, so a default would be
        // indistinguishable from an answer.
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        FilePayload    payload;
        HRESULT        hr      = S_OK;

        payload.type  = ProDosVolume::kTypeBinary;
        payload.bytes = ProDosVolumeFixtures::MakePattern (600);

        hr = volume.Write (FilePath::Parse ("PROG"), payload, result);

        Assert::IsTrue (FAILED (hr), L"a binary with nowhere to load must be refused");
        Assert::AreEqual (size_t (0), result.size());
    }


    TEST_METHOD (Volume_Write_ToAVolumeWithNoRoom_IsRefusedNamingDiskFull)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        ProDosVolume   volume (vol);
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        uint32_t       block   = 0;
        HRESULT        hr      = S_OK;

        for (block = 0; block < 280; block++)
        {
            ProDosVolumeFixtures::MarkFree (vol, block, false);
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
        vector<Byte>   vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        vector<Byte>   existing = ProDosVolumeFixtures::MakePattern (600);
        FilePayload    payload  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        FilePayload    survivor;
        Word           key      = 0;
        Word           stolen   = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, existing));

        key    = ProDosVolumeFixtures::KeyPointerOf (vol, 2, 1);
        stolen = ProDosVolumeFixtures::IndexPointerAt (vol, key, 1);

        ProDosVolumeFixtures::MarkFree (vol, stolen, true);

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
        vector<Byte>   atLimit     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   pastLimit   = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   sapling;
        vector<Byte>   tree;
        VolumeListing  saplingList;
        VolumeListing  treeList;

        {
            ProDosVolume  volume (atLimit);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           ProDosVolumeFixtures::MakeBinaryPayload (kSaplingMaxBytes, 0x2000),
                                           sapling));
        }

        {
            ProDosVolume  volume (pastLimit);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           ProDosVolumeFixtures::MakeBinaryPayload (kSaplingMaxBytes + 1, 0x2000),
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

        Assert::AreEqual (kStorageSapling, (Byte) (ProDosVolumeFixtures::At (sapling, 2, ProDosVolumeFixtures::EntryOffset (1)) & 0xF0),
            L"256 data blocks still fit one index block");
        Assert::AreEqual (uint32_t (kSaplingMaxBlocks), saplingList.entries[0].sizeUnits);

        Assert::AreEqual (kStorageTree, (Byte) (ProDosVolumeFixtures::At (tree, 2, ProDosVolumeFixtures::EntryOffset (1)) & 0xF0),
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
        vector<Byte>           vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>           result;
        ProDosVolume           volume (vol);
        FilePayload            payload = ProDosVolumeFixtures::MakeBinaryPayload (ProDosVolumeFixtures::kTreePayloadBytes, 0x2000);
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

        Assert::AreEqual (kStorageTree, (Byte) (ProDosVolumeFixtures::At (result, 2, ProDosVolumeFixtures::EntryOffset (1)) & 0xF0));
        Assert::AreEqual (uint32_t (ProDosVolumeFixtures::kTreePayloadBytes), listing.entries[0].eofBytes);

        // Block accounting asserted against the integrity pass, not by reading
        // the entry back: the entry is what the writer said, the report is what
        // the volume actually references.
        Assert::AreEqual (uint32_t (ProDosVolumeFixtures::kTreeTotalBlocks), listing.entries[0].sizeUnits);
        Assert::AreEqual (size_t (ProDosVolumeFixtures::kTreeTotalBlocks), report.GetClaimsOf (0).size(),
            L"every index block is as much the file's footprint as its data");
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume - (uint32_t) ProDosVolumeFixtures::kTreeTotalBlocks, listing.freeUnits);
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size());
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size());
        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size());

        // The structure ProDOS documents: the key block is a master index of
        // index blocks, each of which holds up to 256 data pointers.
        master = ProDosVolumeFixtures::KeyPointerOf (result, 2, 1);
        index0 = ProDosVolumeFixtures::IndexPointerAt (result, master, 0);
        index1 = ProDosVolumeFixtures::IndexPointerAt (result, master, 1);

        Assert::IsTrue (index0 != 0 && index1 != 0, L"258 data blocks need two index blocks");
        Assert::AreNotEqual (index0, index1);
        Assert::AreEqual (Word (0), ProDosVolumeFixtures::IndexPointerAt (result, master, 2),
            L"and only two, so the third master slot stays a hole");

        Assert::AreEqual (payload.bytes[0], ProDosVolumeFixtures::At (result, ProDosVolumeFixtures::IndexPointerAt (result, index0, 0), 0),
            L"the first data block is reached master -> index -> data");

        Assert::AreEqual (ProDosVolumeFixtures::kTreePayloadBytes, back.bytes.size(),
            L"the whole file must come back, not just the first index block's worth");

        // Each byte names the data block it belongs to. The last of these is
        // reachable only through the SECOND index block, which is the half a
        // sapling-only writer cannot produce.
        Assert::AreEqual (ProDosVolumeFixtures::BlockStamp (255), back.bytes[255 * ProDosVolumeFixtures::kBlockBytes]);
        Assert::AreEqual (ProDosVolumeFixtures::BlockStamp (256), back.bytes[256 * ProDosVolumeFixtures::kBlockBytes]);
        Assert::AreEqual (ProDosVolumeFixtures::BlockStamp (257), back.bytes[257 * ProDosVolumeFixtures::kBlockBytes]);

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Write_ASaplingSizedFile_MatchesTheShapeRealProDosGaveOne)
    {
        // The half of tree growth that real material CAN check. /APPLESOFT's
        // WHATSIT.A.Q was written in 1985 by software that had never heard of
        // this code: 29,798 bytes recorded as 60 blocks. Writing a file of the
        // same length here must arrive at the same block count, which is the
        // only available evidence that the overhead arithmetic -- data blocks
        // plus index blocks -- matches Apple's rather than merely itself.
        vector<Byte>   disk       = ProDosVolumeFixtures::LoadFixture ("Disks/Merlin-proProdos2.33-b.dsk");
        vector<Byte>   vol        = ProDosVolumeFixtures::MakeVolume();
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
        Assert::IsTrue (realEof > ProDosVolumeFixtures::kBlockBytes, L"and it must be past seedling size");

        AssertSucceeded (volume.Write (FilePath::Parse ("SAME.SIZE"),
                                       ProDosVolumeFixtures::MakeBinaryPayload (realEof, 0x2000), result));

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (ours));
        }

        Assert::AreEqual (realBlocks, ours.entries[0].sizeUnits,
            L"a file of the same length must occupy the same number of blocks real ProDOS used");
        Assert::AreEqual (kStorageSapling, (Byte) (ProDosVolumeFixtures::At (result, 2, ProDosVolumeFixtures::EntryOffset (1)) & 0xF0));
    }


    //  GS/OS Technical Note #8: the name is stored upper case and the two bytes
    //  ProDOS 8 called version and min_version record its case, bit 15 marking
    //  the word as describing the name at all.
    TEST_METHOD (Volume_Write_MixedCaseName_StoresUpperCaseAndTheCaseWord)
    {
        vector<Byte>   vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   written;
        FilePayload    payload;
        FilePayload    read;
        VolumeListing  listing;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (200, 'x');

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("Desk.Accs"), payload, written));
        }

        //  The entry itself: upper case, and the word the note's own example
        //  gives for this name.
        Assert::AreEqual ((Byte) 'D', ProDosVolumeFixtures::At (written, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + ProDosVolumeFixtures::kEntOffName));
        Assert::AreEqual ((Byte) 'E', ProDosVolumeFixtures::At (written, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + ProDosVolumeFixtures::kEntOffName + 1));
        Assert::AreEqual ((Word) 0xB9C0, ProDosVolumeFixtures::WordAt (written, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + 0x1C));

        {
            ProDosVolume  volume (written);

            AssertSucceeded (volume.Enumerate (listing));

            Assert::AreEqual (size_t (1), listing.entries.size());
            Assert::AreEqual (std::string ("Desk.Accs"), listing.entries[0].name,
                L"the listing shows the name the way it was typed");

            //  Matching pays no attention to case, so either form reaches it.
            AssertSucceeded (volume.Read (FilePath::Parse ("DESK.ACCS"), read));
            AssertSucceeded (volume.Read (FilePath::Parse ("desk.accs"), read));
        }
    }


    TEST_METHOD (Volume_CreateDirectory_MakesADirectoryThatHoldsFiles)
    {
        vector<Byte>   vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   made;
        vector<Byte>   filled;
        FilePayload    payload;
        FilePayload    read;
        VolumeListing  root;
        VolumeListing  inside;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (300, 'Z');

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL"), made));
        }

        //  The volume header's tally counts the new directory.
        Assert::AreEqual ((Word) 1, ProDosVolumeFixtures::WordAt (made, ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + 0x21));

        {
            ProDosVolume  volume (made);

            AssertSucceeded (volume.Enumerate (root));

            Assert::AreEqual (size_t (1), root.entries.size());
            Assert::AreEqual (std::string ("UTIL"), root.entries[0].name);
            Assert::IsTrue   (root.entries[0].isDirectory);

            AssertSucceeded  (volume.EnumerateDirectory (FilePath::Parse ("UTIL"), inside));
            Assert::AreEqual (size_t (0), inside.entries.size(), L"a new directory holds nothing");

            AssertSucceeded (volume.Write (FilePath::Parse ("UTIL/NOTES"), payload, filled));
        }

        {
            ProDosVolume  volume (filled);

            AssertSucceeded (volume.EnumerateDirectory (FilePath::Parse ("UTIL"), inside));

            Assert::AreEqual (size_t (1), inside.entries.size());
            Assert::AreEqual (std::string ("NOTES"), inside.entries[0].name);

            AssertSucceeded (volume.Read (FilePath::Parse ("UTIL/NOTES"), read));
            Assert::IsTrue   (payload.bytes == read.bytes, L"the file reads back byte for byte");
        }
    }


    TEST_METHOD (Volume_CreateDirectory_InsideASubdirectory_Nests)
    {
        vector<Byte>   first = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   second;
        vector<Byte>   third;
        VolumeListing  inside;

        {
            ProDosVolume  volume (first);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL"), second));
        }

        {
            ProDosVolume  volume (second);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL/DEEP"), third));
        }

        {
            ProDosVolume  volume (third);

            AssertSucceeded (volume.EnumerateDirectory (FilePath::Parse ("UTIL"), inside));

            Assert::AreEqual (size_t (1), inside.entries.size());
            Assert::AreEqual (std::string ("DEEP"), inside.entries[0].name);
            Assert::IsTrue   (inside.entries[0].isDirectory);
        }
    }


    TEST_METHOD (Volume_CreateDirectory_ARepeatOrAMissingParent_IsRefused)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  made;
        vector<Byte>  again;
        vector<Byte>  deeper;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL"), made));
        }

        ProDosVolume  volume (made);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
                          volume.CreateDirectory (FilePath::Parse ("UTIL"), again));
        Assert::AreEqual (size_t (0), again.size());

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND),
                          volume.CreateDirectory (FilePath::Parse ("NOPE/DEEP"), deeper),
                          L"the directory above it has to be there first");
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
        vector<Byte>           vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>           emptied;
        vector<Byte>           result;
        FilePayload            second  = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        VolumeIntegrityReport  report;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FIRST", 0x06, 0x2000,
                                                      ProDosVolumeFixtures::MakePattern (1'200)));

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
        vector<Byte>   disk    = ProDosVolumeFixtures::LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
        vector<Byte>   result;
        ProDosVolume   volume (disk);
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x0300);
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


    //  The volume directory is its four blocks, 51 entries, as ProDOS 8 has
    //  it; a subdirectory takes another block when full.
    TEST_METHOD (Volume_AFullVolumeDirectory_TakesNoMore_AndASubdirectoryGrows)
    {
        vector<Byte>   disk    = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   next;
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (10, 0x6000);
        VolumeListing  listed;
        int            i       = 0;

        for (i = 0; i < 51; i++)
        {
            AssertSucceeded (ProDosVolume (disk).Write (FilePath::Parse (std::format ("F{}", i)), payload, next));
            disk = next;
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE), ProDosVolume (disk).Write (FilePath::Parse ("ONEMORE"), payload, next));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE), ProDosVolume (disk).CreateDirectory (FilePath::Parse ("DIR"), next));

        disk = ProDosVolumeFixtures::MakeVolume();
        AssertSucceeded (ProDosVolume (disk).CreateDirectory (FilePath::Parse ("SUB"), next));
        disk = next;

        for (i = 0; i < 20; i++)
        {
            AssertSucceeded (ProDosVolume (disk).Write (FilePath::Parse (std::format ("SUB/F{}", i)), payload, next));
            disk = next;
        }

        AssertSucceeded (ProDosVolume (disk).EnumerateDirectory (FilePath::Parse ("SUB"), listed));
        Assert::AreEqual ((size_t) 20, listed.entries.size(), L"past the subdirectory's first block");
    }
};
