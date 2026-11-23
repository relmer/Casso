#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemoverTests
//
//  ProDosVolume deletes: the blocks a file or directory returns to the
//  bitmap, damaged and shared files, and removal plans.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeRemoverTests)
{
public:

    static constexpr size_t  kEntOffTypeName   = 0x00;

    static uint32_t FreeBlocks (const vector<Byte> & vol)
    {
        ProDosVolume   volume (vol);
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        return listing.freeUnits;
    }

    //  A sapling whose index block names a block the volume does not have.
    //  Refusing to delete it would strand every block it holds.
    static vector<Byte> MakeVolumeWithDamagedIndexFile()
    {
        vector<Byte>  vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  payload = ProDosVolumeFixtures::MakePattern (600);
        Word          key     = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BROKEN", 0x06, 0x2000, payload));

        key = ProDosVolumeFixtures::KeyPointerOf (vol, 2, 1);

        vol[ProDosSkeleton::GetBlockByteOffset (key, 1)]                     = 0xF4;   // 500
        vol[ProDosSkeleton::GetBlockByteOffset (key, 1 + ProDosVolumeFixtures::kPointersPerIndex)] = 0x01;

        return vol;
    }


    TEST_METHOD (Volume_Delete_AFileWithNoComplications_ReturnsExactlyItsBlocks)
    {
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), result, outcome));
        }

        {
            ProDosVolume  written (result);

            AssertSucceeded (written.Enumerate (listing));
        }

        Assert::AreEqual (size_t (0), listing.entries.size(), L"the entry must be gone");
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume, listing.freeUnits,
            L"all three of its blocks return to the pool");
        Assert::AreEqual (size_t (3), outcome.freedUnits.size());
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (size_t (0), outcome.warnings.size(), L"a clean delete warns about nothing");
        Assert::IsTrue (outcome.catalogFullyParsed);

        // The volume header's own tally follows the directory it describes.
        Assert::AreEqual (Word (0), ProDosVolumeFixtures::WordAt (result, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x21));

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (vol, result);
    }


    TEST_METHOD (Volume_Delete_LeavesTheNameBehindButStopsResolvingIt)
    {
        // ProDOS's own tombstone: the storage-type nibble goes to zero and the
        // name stays, which is how an undelete tool finds the file again. The
        // consequence anything reading this volume must honor is that the name
        // is NOT what says a record is live -- resolving by name alone hands
        // back a file whose blocks have been given to somebody else.
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        FilePayload    back;
        HRESULT        hr      = S_OK;
        Byte           typeLen = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Delete (FilePath::Parse ("PROG"), result));
        }

        typeLen = ProDosVolumeFixtures::At (result, 2, ProDosVolumeFixtures::EntryOffset (1) + kEntOffTypeName);

        Assert::AreEqual (Byte (0x00), (Byte) (typeLen & 0xF0), L"the storage type is cleared");
        Assert::AreEqual (Byte (0x04), (Byte) (typeLen & 0x0F), L"the name length survives");
        Assert::AreEqual (Byte ('P'), ProDosVolumeFixtures::At (result, 2, ProDosVolumeFixtures::EntryOffset (1) + ProDosVolumeFixtures::kEntOffName),
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
        vector<Byte>   vol        = ProDosVolumeFixtures::MakeVolumeWithCrossLinkedFiles (survivorBytes, shared);
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

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (vol, result);
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
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   written;
        vector<Byte>   result;
        DeleteOutcome  outcome;
        VolumeListing  listing;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("BIG"),
                                           ProDosVolumeFixtures::MakeBinaryPayload (ProDosVolumeFixtures::kTreePayloadBytes, 0x2000),
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

        Assert::AreEqual (size_t (ProDosVolumeFixtures::kTreeTotalBlocks), outcome.freedUnits.size(),
            L"258 data blocks, both index blocks, and the master above them");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume, listing.freeUnits,
            L"the volume must be as empty as it started");

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (written, result);
    }


    TEST_METHOD (Volume_Delete_AFileTheAccessByteProtectsFromRemoval_IsRefused)
    {
        // Delete is gated on destroy-enable, not on write-enable. The access
        // byte can carry one without the other, so a single "locked" flag would
        // be the wrong question in one direction or the other.
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  result;
        size_t        accessAt = 0;
        HRESULT       hr       = S_OK;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        accessAt = ProDosSkeleton::GetBlockByteOffset (2, ProDosVolumeFixtures::EntryOffset (1) + ProDosVolumeFixtures::kEntOffAccess);

        // Write-enable set, destroy-enable clear: rewritable but not removable.
        vol[accessAt] = 0x43;

        {
            ProDosVolume  volume (vol);

            hr = volume.Delete (FilePath::Parse ("PROG"), result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }


    //  Builds the same tree the removal test uses: UTIL, UTIL/DEEP, and a file
    //  inside the deeper one.
    static vector<Byte> MakeTreeVolume()
    {
        vector<Byte>  first  = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  second;
        vector<Byte>  third;
        vector<Byte>  fourth;
        FilePayload   payload;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (300, 'Z');

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

            AssertSucceeded (volume.Write (FilePath::Parse ("UTIL/DEEP/NOTES"), payload, fourth));
        }

        return fourth;
    }


    TEST_METHOD (Volume_BuildRemovalPlan_ListsTheSubtreeDeepestFirst)
    {
        vector<Byte>          tree = MakeTreeVolume();
        ProDosVolume          volume (tree);
        DirectoryRemovalPlan  plan;

        AssertSucceeded (volume.BuildRemovalPlan (FilePath::Parse ("UTIL"), plan));

        //  Deepest first, the directory itself last.
        Assert::AreEqual (size_t (3), plan.entries.size());
        Assert::AreEqual (std::string ("UTIL/DEEP/NOTES"), plan.entries[0].path);
        Assert::AreEqual (std::string ("UTIL/DEEP"),       plan.entries[1].path);
        Assert::AreEqual (std::string ("UTIL"),            plan.entries[2].path);
        Assert::IsFalse  (plan.hasLockedEntries);
        Assert::IsTrue   (plan.blocksFreed >= 3);
    }


    TEST_METHOD (Volume_RemoveDirectory_TakesTheWholeSubtreeAndGivesItsSpaceBack)
    {
        vector<Byte>   vol        = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   step;
        vector<Byte>   emptied;
        FilePayload    payload;
        VolumeListing  root;
        DeleteOutcome  outcome;
        uint32_t       freeBefore = 0;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (300, 'Z');

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL"), step));
        }

        {
            ProDosVolume  volume (step);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL/DEEP"), vol));
        }

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("UTIL/DEEP/NOTES"), payload, step));
        }

        {
            ProDosVolume          volume (step);
            DirectoryRemovalPlan  plan;
            VolumeListing         before;

            AssertSucceeded (volume.Enumerate (before));

            freeBefore = before.freeUnits;

            AssertSucceeded (volume.BuildRemovalPlan (FilePath::Parse ("UTIL"), plan));

            //  Deepest first, the directory itself last.
            Assert::AreEqual (size_t (3), plan.entries.size());
            Assert::AreEqual (std::string ("UTIL/DEEP/NOTES"), plan.entries[0].path);
            Assert::AreEqual (std::string ("UTIL/DEEP"),       plan.entries[1].path);
            Assert::AreEqual (std::string ("UTIL"),            plan.entries[2].path);
            Assert::IsFalse  (plan.hasLockedEntries);
            Assert::IsTrue   (plan.blocksFreed >= 3);
        }

        {
            ProDosVolume  volume (step);

            AssertSucceeded (volume.RemoveDirectory (FilePath::Parse ("UTIL"), false, emptied, outcome));
        }

        {
            ProDosVolume  volume (emptied);

            AssertSucceeded (volume.Enumerate (root));

            Assert::AreEqual (size_t (0), root.entries.size(), L"the directory and everything below it is gone");
            Assert::IsTrue   (root.freeUnits > freeBefore, L"and its blocks are free again");
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND),
                              volume.EnumerateDirectory (FilePath::Parse ("UTIL"), root));
        }
    }


    TEST_METHOD (Volume_RemoveDirectory_ALockedEntryStopsItUnlessForced)
    {
        vector<Byte>   vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   step;
        vector<Byte>   emptied;
        FilePayload    payload;
        DeleteOutcome  outcome;
        size_t         accessAt = 0;

        payload.type = ProDosVolume::kTypeText;
        payload.bytes.assign (300, 'Z');

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("UTIL"), step));
        }

        {
            ProDosVolume  volume (step);

            AssertSucceeded (volume.Write (FilePath::Parse ("UTIL/NOTES"), payload, vol));
        }

        //  Clear destroy-enable on the file, which is the bit a removal is
        //  gated on. It is the first record of the directory's own key block.
        accessAt = ProDosSkeleton::GetBlockByteOffset (7, ProDosVolumeFixtures::kKeyBlockEntry + ProDosVolumeFixtures::kEntryLength + ProDosVolumeFixtures::kEntOffAccess);

        vol[accessAt] = 0x43;

        {
            ProDosVolume          volume (vol);
            DirectoryRemovalPlan  plan;

            AssertSucceeded (volume.BuildRemovalPlan (FilePath::Parse ("UTIL"), plan));

            Assert::IsTrue (plan.hasLockedEntries, L"the plan marks what is locked before anything is written");

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
                              volume.RemoveDirectory (FilePath::Parse ("UTIL"), false, emptied, outcome));
            Assert::AreEqual (size_t (0), emptied.size(), L"and nothing is written");

            AssertSucceeded (volume.RemoveDirectory (FilePath::Parse ("UTIL"), true, emptied, outcome));
        }

        {
            ProDosVolume   volume (emptied);
            VolumeListing  root;

            AssertSucceeded (volume.Enumerate (root));
            Assert::AreEqual (size_t (0), root.entries.size());
        }
    }


    TEST_METHOD (Volume_Delete_AMissingFileOrAMissingDirectory_IsRefused)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  missing;
        vector<Byte>  deeper;
        ProDosVolume  volume (vol);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          volume.Delete (FilePath::Parse ("NOPE"), missing));
        Assert::AreEqual (size_t (0), missing.size());

        //  The path walks its directories, and this volume has no UTIL, so the
        //  refusal is about the directory rather than about the name.
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND),
                          volume.Delete (FilePath::Parse ("UTIL/PROG"), deeper));
        Assert::AreEqual (size_t (0), deeper.size());
    }


    TEST_METHOD (Volume_Delete_FromAVolumeWhoseDirectoryDidNotFullyParse_WarnsDistinctly)
    {
        // The one case the unique-ownership rule can still lose data: an entry
        // that could not be read claims nothing observable, so a block it
        // shares looks unowned. The system must not claim otherwise.
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   result;
        DeleteOutcome  outcome;
        size_t         i       = 0;
        bool           saidSo  = false;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

        // The key block's next pointer aimed back at itself.
        vol[ProDosSkeleton::GetBlockByteOffset (2, 0x02)]     = 0x02;
        vol[ProDosSkeleton::GetBlockByteOffset (2, 0x02 + 1)] = 0x00;

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
        vector<Byte>   vol     = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>   emptied;
        vector<Byte>   result;
        FilePayload    payload = ProDosVolumeFixtures::MakeBinaryPayload (600, 0x2000);
        VolumeListing  listing;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PROG", 0x06, 0x2000, ProDosVolumeFixtures::MakePattern (600)));

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
        Assert::AreEqual (ProDosVolumeFixtures::kFreeOnBlankVolume - 3, listing.freeUnits,
            L"the new file fits in exactly the space the old one gave back");
        Assert::AreEqual (Byte ('N'), ProDosVolumeFixtures::At (result, 2, ProDosVolumeFixtures::EntryOffset (1) + ProDosVolumeFixtures::kEntOffName),
            L"and in the directory slot it gave back, which cannot be grown");

        ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (emptied, result);
    }


    TEST_METHOD (Volume_Delete_ASubdirectory_IsRefusedRatherThanOrphaningWhatIsInside)
    {
        // /MERLIN carries real subdirectories. This layer does not walk into
        // one, so the report credits it with its key block and nothing else --
        // deleting it would free that single block and silently orphan every
        // file beneath, while reporting a clean removal.
        vector<Byte>   disk   = ProDosVolumeFixtures::LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
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
        vector<Byte>   disk    = ProDosVolumeFixtures::LoadFixture ("Disks/Merlin-proProdos2.33-a.dsk");
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
};
