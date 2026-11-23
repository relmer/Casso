#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeTests
//
//  ProDosVolume: listing, reading and the integrity report, over a buffer
//  or a block store, and the check made before a result is returned.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeTests)
{
public:

    TEST_METHOD (Volume_Enumerate_ReportsVolumeNameFilesAndFreeSpace)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume ("MYDISK");
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
        vector<Byte>  vol     = ProDosVolumeFixtures::MakeVolume();
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
        vector<Byte>  vol     = ProDosVolumeFixtures::MakeVolume();
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
        vector<Byte>           vol     = ProDosVolumeFixtures::MakeVolume();
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
        vector<Byte>           vol     = ProDosVolumeFixtures::MakeVolume();
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


    TEST_METHOD (Volume_OverAStore_ListsA32MbVolumeFromItsDirectoryAndBitmap)
    {
        vector<Byte>       vol ((size_t) 65535 * ProDosSkeleton::kBlockByteSize, 0);
        vector<Byte>       result;
        FilePayload        payload = ProDosVolumeFixtures::MakeBinaryPayload (20000, 0x2000);
        FilePayload        back;
        VolumeListing      listing;

        AssertSucceeded (ProDosSkeleton::Write (vol, "BIG"));
        AssertSucceeded (ProDosVolume (vol).Write (FilePath::Parse ("PROG"), payload, result));

        MemoryBlockStore   store (result);
        BlockImage         image (store);
        ProDosVolume       volume (image);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG"), listing.entries[0].name);
        Assert::IsTrue (volume.GetBytesLoaded() < (size_t) 64 * ProDosSkeleton::kBlockByteSize);

        AssertSucceeded (volume.Read (FilePath::Parse ("PROG"), back));
        Assert::IsTrue (back.bytes == payload.bytes);
    }


    //  An entry written by ProDOS 8 has zero there, which describes no
    //  case at all, and its name reads back as the directory holds it.
    TEST_METHOD (Volume_Enumerate_AnEntryWithNoCaseWord_ReadsAsUpperCase)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   written;
        FilePayload    payload;
        VolumeListing  listing;
        size_t         wordAt  = 0;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (200, 'x');

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("Desk.Accs"), payload, written));
        }

        wordAt = ProDosSkeleton::GetBlockByteOffset (ProDosVolumeFixtures::kDirKeyBlock, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + 0x1C);

        written[wordAt]     = 0;
        written[wordAt + 1] = 0;

        {
            ProDosVolume  volume (written);

            AssertSucceeded (volume.Enumerate (listing));

            Assert::AreEqual (std::string ("DESK.ACCS"), listing.entries[0].name);
        }
    }


    //  A subdirectory lists what it holds, under the volume's own name, and a
    //  file inside it reads by its path at the length its entry records. A
    //  directory that is not there is refused.
    TEST_METHOD (Volume_EnumerateDirectory_ListsAndReadsInsideASubdirectory)
    {
        vector<Byte>   disk      = ProDosVolumeFixtures::LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
        ProDosVolume   volume (disk);
        VolumeListing  root;
        VolumeListing  inside;
        VolumeListing  missing;
        FilePayload    payload;
        std::string    directory;
        std::string    file;
        uint32_t       fileBytes = 0;
        size_t         i         = 0;

        AssertSucceeded (volume.Enumerate (root));

        for (i = 0; i < root.entries.size() && directory.empty(); i++)
        {
            if (root.entries[i].isDirectory)
            {
                directory = root.entries[i].name;
            }
        }

        Assert::IsFalse (directory.empty(), L"this disk must have a subdirectory");

        AssertSucceeded  (volume.EnumerateDirectory (FilePath::Parse (directory), inside));
        Assert::IsFalse  (inside.entries.empty(), L"The subdirectory lists what it holds");
        Assert::AreEqual (root.volumeName, inside.volumeName, L"under the volume's own name");

        for (i = 0; i < inside.entries.size() && file.empty(); i++)
        {
            if (!inside.entries[i].isDirectory)
            {
                file      = inside.entries[i].name;
                fileBytes = inside.entries[i].eofBytes;
            }
        }

        if (!file.empty())
        {
            AssertSucceeded  (volume.Read (FilePath::Parse (directory + "/" + file), payload));
            Assert::AreEqual ((size_t) fileBytes, payload.bytes.size(), L"A file inside it reads at its recorded length");
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND), volume.EnumerateDirectory (FilePath::Parse ("NO.SUCH.DIR"), missing));
    }


    TEST_METHOD (Volume_PreCommitCheck_AResultClaimingABlockTwice_IsRefusedAsOurOwnDefect)
    {
        // Correct code cannot produce a result that fails this, which is what
        // the check is for -- so the only way to exercise the refusal is to hand
        // it one on purpose.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           survivor;
        Word                   shared = 0;
        vector<Byte>           input  = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>           result = ProDosVolumeFixtures::MakeVolumeWithCrossLinkedFiles (survivor, shared);
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

        vector<Byte>           input  = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>           result;
        vector<Byte>           handedBack;
        VolumeIntegrityReport  before;
        Word                   key    = 0;
        Word                   stolen = 0;
        HRESULT                hr     = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (input, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        {
            ProDosVolume  volume (input);

            AssertSucceeded (volume.BuildIntegrityReport (before));
        }

        result = input;
        key    = ProDosVolumeFixtures::KeyPointerOf (result, 2, 1);
        stolen = ProDosVolumeFixtures::IndexPointerAt (result, key, 1);

        // A block the directory still references, handed back to the pool.
        ProDosVolumeFixtures::MarkFree (result, stolen, true);

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
        vector<Byte>           vol     = ProDosVolumeFixtures::MakeVolumeWithCrossLinkedFiles (survivor, shared);
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
};
