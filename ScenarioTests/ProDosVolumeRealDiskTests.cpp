#include "Pch.h"

#include "../UnitTest/EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRealDiskTests
//
//  ProDosVolume over the Merlin ProDOS disks: placement, removal and the
//  startup program on volumes real ProDOS wrote.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeRealDiskTests)
{
public:

    static constexpr Byte  kStorageSapling = 0x20;

    static vector<Byte> LoadFixture (const char * relativePath)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (relativePath, bytes));
        Assert::IsTrue (bytes.size() > 0, L"a fixture must not be empty");

        return bytes;
    }

    //  A system program. What the bytes are does not matter here -- which
    //  record the walk reaches first is the whole subject -- but the type does.
    static FilePayload MakeSystemPayload (size_t length)
    {
        FilePayload  payload = ProDosVolumeFixtures::MakeBinaryPayload (length, 0x2000);

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


    TEST_METHOD (Volume_Write_ASaplingSizedFile_MatchesTheShapeRealProDosGaveOne)
    {
        // The half of tree growth that real material CAN check. /APPLESOFT's
        // WHATSIT.A.Q was written in 1985 by software that had never heard of
        // this code: 29,798 bytes recorded as 60 blocks. Writing a file of the
        // same length here must arrive at the same block count, which is the
        // only available evidence that the overhead arithmetic -- data blocks
        // plus index blocks -- matches Apple's rather than merely itself.
        vector<Byte>   disk       = LoadFixture ("Disks/Merlin-proProdos2.33-b.dsk");
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
};




