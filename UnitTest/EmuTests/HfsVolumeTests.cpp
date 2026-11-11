#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/BlockImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolumeTests
//
//  HFS volumes HfsWriter formats and changes, read back through HfsVolume,
//  and the catalog name order HfsBTree sorts by.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HfsVolumeTests)
{
public:

    static constexpr size_t  k800KSectors = 1600;
    static constexpr size_t  k64MBSectors = 131072;
    static constexpr Byte    kFill        = 0x5A;

    static vector<Byte> MakeVolume (size_t sectors = k800KSectors)
    {
        vector<Byte>  image;



        AssertSucceeded (HfsWriter::Format (image, sectors, "Untitled"));

        return image;
    }


    static FilePayload MakePayload (size_t bytes = 3000, Byte type = 0x06, Word auxType = 0x2000)
    {
        FilePayload  payload;



        payload.bytes.assign (bytes, kFill);
        payload.type       = type;
        payload.auxType    = auxType;
        payload.hasAuxType = true;

        return payload;
    }


    //  The image after `change`, which must succeed.
    template <typename Change>
    static vector<Byte> Apply (const vector<Byte> & image, Change change)
    {
        HfsVolume     volume (image);
        vector<Byte>  written;



        AssertSucceeded (change (volume, written));

        return written;
    }


    static VolumeListing List (const vector<Byte> & image, const std::string & directory = "")
    {
        HfsVolume      volume (image);
        VolumeListing  listing;



        if (directory.empty())
        {
            AssertSucceeded (volume.Enumerate (listing));
        }
        else
        {
            AssertSucceeded (volume.EnumerateDirectory (FilePath::Parse (directory), listing));
        }

        return listing;
    }


    TEST_METHOD (Format_MakesAnEmptyVolumeWithTheNameGiven)
    {
        vector<Byte>   image   = MakeVolume();
        VolumeListing  listing = List (image);



        Assert::IsTrue (HfsVolume::IsFound (BlockImage (image)));
        Assert::AreEqual (std::string ("Untitled"), listing.volumeName);
        Assert::AreEqual ((size_t) 0, listing.entries.size());
        Assert::IsTrue (listing.freeUnits > 0 && listing.freeUnits < listing.totalUnits);
    }


    TEST_METHOD (Format_KeepsAHardDiskUnder65536AllocationBlocks)
    {
        vector<Byte>  image = MakeVolume (k64MBSectors);
        Word          count = (Word) ((image[1024 + 18] << 8) | image[1024 + 19]);
        uint32_t      size  = ((uint32_t) image[1024 + 20] << 24) | ((uint32_t) image[1024 + 21] << 16) | ((uint32_t) image[1024 + 22] << 8) | image[1024 + 23];



        Assert::AreEqual ((uint32_t) 1024, size);
        Assert::IsTrue (count > 65000);
        Assert::AreEqual (std::string ("Untitled"), List (image).volumeName);
    }


    TEST_METHOD (Format_RejectsAnInvalidName)
    {
        vector<Byte>  image;
        HRESULT       colon = HfsWriter::Format (image, k800KSectors, "A:B");
        HRESULT       empty = HfsWriter::Format (image, k800KSectors, "");



        Assert::IsTrue (FAILED (colon));
        Assert::IsTrue (FAILED (empty));
        Assert::IsTrue (image.empty());
    }


    TEST_METHOD (Write_ReadsBackBothForksAndTheProDosType)
    {
        vector<Byte>  image   = MakeVolume();
        FilePayload   payload = MakePayload();
        FilePayload   read;



        payload.resourceFork.assign (700, 0x11);
        payload.hasResourceFork = true;

        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Picture"), payload, out); });

        HfsVolume  after (image);

        AssertSucceeded (after.Read (FilePath::Parse ("Picture"), read));
        Assert::IsTrue (read.bytes == payload.bytes);
        Assert::IsTrue (read.resourceFork == payload.resourceFork);
        Assert::AreEqual ((Byte) 0x06, read.type);
        Assert::AreEqual ((Word) 0x2000, read.auxType);
    }


    TEST_METHOD (Write_ReplacesAFileOfTheSameNameAndFreesItsBlocks)
    {
        vector<Byte>  image = MakeVolume();
        uint32_t      free  = List (image).freeUnits;



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Data"), MakePayload (40000), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("DATA"), MakePayload (1000), out); });

        VolumeListing  listing = List (image);

        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::IsTrue (listing.freeUnits + 4 >= free);
    }


    TEST_METHOD (Write_GrowsTheCatalogForManyFiles)
    {
        constexpr size_t  kFiles = 300;

        vector<Byte>  image = MakeVolume();
        FilePayload   read;



        for (size_t i = 0; i < kFiles; i++)
        {
            image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse (std::format ("File number {}", i)), MakePayload (100), out); });
        }

        Assert::AreEqual (kFiles, List (image).entries.size());

        HfsVolume  after (image);

        AssertSucceeded (after.Read (FilePath::Parse ("File number 299"), read));
        Assert::AreEqual ((size_t) 100, read.bytes.size());
    }


    TEST_METHOD (Write_FailsWhenTheVolumeIsFull)
    {
        vector<Byte>  image = MakeVolume();
        HfsVolume     volume (image);
        vector<Byte>  written;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), volume.Write (FilePath::Parse ("Huge"), MakePayload (900'000), written));
    }


    TEST_METHOD (CreateDirectory_HoldsFilesWrittenIntoIt)
    {
        vector<Byte>  image = MakeVolume();
        FilePayload   read;



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("Games"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("Games/Old"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Games/Old/Lode"), MakePayload(), out); });

        VolumeListing  root  = List (image);
        VolumeListing  games = List (image, "Games");
        VolumeListing  old   = List (image, "Games/Old");

        Assert::AreEqual ((size_t) 1, root.entries.size());
        Assert::IsTrue (root.entries[0].isDirectory);
        Assert::AreEqual ((size_t) 1, games.entries.size());
        Assert::AreEqual ((size_t) 1, old.entries.size());
        Assert::AreEqual (std::string ("Lode"), old.entries[0].name);

        HfsVolume  after (image);

        AssertSucceeded (after.Read (FilePath::Parse ("Games/Old/Lode"), read));
        Assert::AreEqual ((size_t) 3000, read.bytes.size());
    }


    TEST_METHOD (CreateDirectory_FailsWhenTheNameIsTaken)
    {
        vector<Byte>  image = MakeVolume();



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Games"), MakePayload(), out); });

        HfsVolume     volume (image);
        vector<Byte>  written;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS), volume.CreateDirectory (FilePath::Parse ("GAMES"), written));
    }


    TEST_METHOD (Delete_FreesTheFileAndRefusesALockedOne)
    {
        vector<Byte>   image   = MakeVolume();
        uint32_t       free    = List (image).freeUnits;
        FilePayload    locked  = MakePayload();
        DeleteOutcome  outcome;



        locked.access    = FilePayload::kAccessLocked;
        locked.hasAccess = true;

        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Open"), MakePayload (20000), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Shut"), locked, out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Delete (FilePath::Parse ("Open"), out, outcome); });

        Assert::IsFalse (outcome.freedUnits.empty());

        HfsVolume     volume (image);
        vector<Byte>  written;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), volume.Delete (FilePath::Parse ("Shut"), written));
        Assert::AreEqual ((size_t) 1, List (image).entries.size());
        Assert::IsTrue (List (image).freeUnits + 8 >= free);
    }


    TEST_METHOD (RemoveDirectory_RemovesEverythingBelowIt)
    {
        vector<Byte>          image = MakeVolume();
        uint32_t              free  = List (image).freeUnits;
        DirectoryRemovalPlan  plan;
        DeleteOutcome         outcome;



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("A"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("A/B"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("A/One"), MakePayload(), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("A/B/Two"), MakePayload(), out); });

        AssertSucceeded (HfsVolume (image).BuildRemovalPlan (FilePath::Parse ("A"), plan));
        Assert::AreEqual ((size_t) 4, plan.entries.size());
        Assert::IsTrue (plan.entries.back().isDirectory);

        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.RemoveDirectory (FilePath::Parse ("A"), false, out, outcome); });

        Assert::AreEqual ((size_t) 0, List (image).entries.size());
        Assert::AreEqual (free, List (image).freeUnits);
    }


    TEST_METHOD (Rename_ChangesTheNameAndKeepsTheContents)
    {
        vector<Byte>  image = MakeVolume();
        FilePayload   read;



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("Docs"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Docs/Letter"), MakePayload(), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Rename (FilePath::Parse ("Docs"), "Papers", out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Rename (FilePath::Parse ("Papers/Letter"), "Memo", out); });

        HfsVolume  after (image);

        AssertSucceeded (after.Read (FilePath::Parse ("Papers/Memo"), read));
        Assert::AreEqual ((size_t) 3000, read.bytes.size());
        Assert::AreEqual (std::string ("Papers"), List (image).entries[0].name);
    }


    TEST_METHOD (SetAttributes_ChangesTheTypeAndTheLock)
    {
        vector<Byte>    image = MakeVolume();
        FileAttributes  attributes;



        attributes.type      = 0xFF;
        attributes.hasType   = true;
        attributes.access    = FilePayload::kAccessLocked;
        attributes.hasAccess = true;

        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Start"), MakePayload(), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.SetAttributes (FilePath::Parse ("Start"), attributes, out); });

        VolumeListing  listing = List (image);

        Assert::AreEqual ((Byte) 0xFF, listing.entries[0].type);
        Assert::IsTrue (listing.entries[0].isLocked);
    }


    TEST_METHOD (IntegrityReport_IsCleanAfterChanges)
    {
        vector<Byte>           image = MakeVolume();
        VolumeIntegrityReport  report;
        DeleteOutcome          outcome;



        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.CreateDirectory (FilePath::Parse ("Stuff"), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("Stuff/A"), MakePayload (5000), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Write (FilePath::Parse ("B"), MakePayload (9000), out); });
        image = Apply (image, [&] (HfsVolume & volume, vector<Byte> & out) { return volume.Delete (FilePath::Parse ("Stuff/A"), out, outcome); });

        AssertSucceeded (HfsVolume (image).BuildIntegrityReport (report));
        Assert::IsTrue (report.IsClean());
    }


    TEST_METHOD (CompareNames_IgnoresCaseAndRanksAccentsAfterTheLetter)
    {
        auto  compare = [] (const std::string & a, const std::string & b)
        {
            return HfsBTree::CompareNames ((const Byte *) a.data(), a.size(), (const Byte *) b.data(), b.size());
        };



        Assert::AreEqual (0, compare ("Read Me", "READ ME"));
        Assert::IsTrue (compare ("Apple", "Banana") < 0);
        Assert::IsTrue (compare ("App", "Apple") < 0);
        Assert::IsTrue (compare ("e", "\x8E") < 0);      // e, then e acute
        Assert::IsTrue (compare ("\x8E", "f") < 0);
    }


    TEST_METHOD (MapFromProDos_FollowsTheFileTypeNote)
    {
        uint32_t  type    = 0;
        uint32_t  creator = 0;
        Byte      back    = 0;
        Word      aux     = 0;



        HfsWriter::MapFromProDos (0x04, 0, type, creator);
        Assert::AreEqual ((uint32_t) 0x54455854, type);

        HfsWriter::MapFromProDos (0x06, 0x2000, type, creator);
        Assert::AreEqual ((uint32_t) 0x70062000, type);
        Assert::AreEqual ((uint32_t) 0x70646F73, creator);

        HfsVolume::MapToProDos (type, creator, back, aux);
        Assert::AreEqual ((Byte) 0x06, back);
        Assert::AreEqual ((Word) 0x2000, aux);
    }
};
