#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskBuilderTests
//
//  BlankDiskSpec validation matrix, DOS 3.3 skeleton structural invariants,
//  and BlankDiskBuilder output determinism / format guarantees. No host
//  fixture files — every input is built in memory.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (BlankDiskBuilderTests)
{
public:

    static BlankDiskSpec MakeSpec (DiskFormat fmt, BlankDiskContents contents, bool bootable = false)
    {
        BlankDiskSpec  spec;

        spec.format   = fmt;
        spec.contents = contents;
        spec.bootable = bootable;

        return spec;
    }

    TEST_METHOD (SpecDefaults_AreTheImmediatelyUsableConfiguration)
    {
        BlankDiskSpec  spec;



        Assert::IsTrue (spec.format   == DiskFormat::Woz);
        Assert::IsTrue (spec.contents == BlankDiskContents::Dos33);
        Assert::IsFalse (spec.bootable);
        Assert::AreEqual ((int) NibblizationLayer::kDefaultVolume, (int) spec.volumeNumber);
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (spec));
    }

    TEST_METHOD (ValidateSpec_WozPairsWithEverything)
    {
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Woz, BlankDiskContents::Dos33)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Woz, BlankDiskContents::ProDos)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Woz, BlankDiskContents::Unformatted)));
    }

    TEST_METHOD (ValidateSpec_DskPairsWithDosOrRaw)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;



        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Dsk, BlankDiskContents::Dos33)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Dsk, BlankDiskContents::Unformatted)));
        AssertFailed    (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Dsk, BlankDiskContents::ProDos)));
        expect.RequireCount (1);
    }

    TEST_METHOD (ValidateSpec_PoPairsWithProDosOrRaw)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;



        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Po, BlankDiskContents::Unformatted)));
        AssertFailed    (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Po, BlankDiskContents::Dos33)));
        expect.RequireCount (1);
    }

    TEST_METHOD (ValidateSpec_DoIsNeverCreatable)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;



        AssertFailed (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Do, BlankDiskContents::Dos33)));
        AssertFailed (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Do, BlankDiskContents::Unformatted)));
        expect.RequireCount (2);
    }

    TEST_METHOD (ValidateSpec_BootableRequiresFormattedContents)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;



        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Woz, BlankDiskContents::Dos33,  true)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Po,  BlankDiskContents::ProDos, true)));
        AssertFailed    (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Woz, BlankDiskContents::Unformatted, true)));
        expect.RequireCount (1);
    }

    static vector<Byte> MakeDos33Buffer (Byte volume = NibblizationLayer::kDefaultVolume)
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0xEE);

        AssertSucceeded (Dos33Skeleton::Write (buffer, volume));

        return buffer;
    }

    static size_t Sector (int track, int sector)
    {
        return ((size_t) track * 16 + (size_t) sector) * 256;
    }

    TEST_METHOD (Dos33Skeleton_VtocFields)
    {
        vector<Byte>  buffer = MakeDos33Buffer (0x7B);
        size_t        vtoc   = Sector (17, 0);



        Assert::AreEqual ((Byte) 17,   buffer[vtoc + 0x01]);   // catalog track
        Assert::AreEqual ((Byte) 15,   buffer[vtoc + 0x02]);   // catalog sector
        Assert::AreEqual ((Byte) 3,    buffer[vtoc + 0x03]);   // DOS release
        Assert::AreEqual ((Byte) 0x7B, buffer[vtoc + 0x06]);   // volume number
        Assert::AreEqual ((Byte) 122,  buffer[vtoc + 0x27]);   // max TS pairs
        Assert::AreEqual ((Byte) 35,   buffer[vtoc + 0x34]);   // tracks
        Assert::AreEqual ((Byte) 16,   buffer[vtoc + 0x35]);   // sectors
        Assert::AreEqual ((Byte) 0x00, buffer[vtoc + 0x36]);   // 256 bytes/sector, LE
        Assert::AreEqual ((Byte) 0x01, buffer[vtoc + 0x37]);
    }

    TEST_METHOD (Dos33Skeleton_FreeBitmapReservesDosAndCatalogTracks)
    {
        vector<Byte>  buffer = MakeDos33Buffer();
        size_t        vtoc   = Sector (17, 0);
        int           track  = 0;



        for (track = 0; track < 35; track++)
        {
            size_t  entry     = vtoc + 0x38 + (size_t) track * 4;
            bool    allocated = (track <= 2) || (track == 17);
            Byte    expected  = allocated ? (Byte) 0x00 : (Byte) 0xFF;

            Assert::AreEqual (expected, buffer[entry],     L"bitmap byte 0");
            Assert::AreEqual (expected, buffer[entry + 1], L"bitmap byte 1");
            Assert::AreEqual ((Byte) 0, buffer[entry + 2], L"bitmap byte 2 always 0");
            Assert::AreEqual ((Byte) 0, buffer[entry + 3], L"bitmap byte 3 always 0");
        }
    }

    TEST_METHOD (Dos33Skeleton_CatalogChainLinksDownToSectorOne)
    {
        vector<Byte>  buffer = MakeDos33Buffer();
        int           sector = 0;



        for (sector = 15; sector >= 2; sector--)
        {
            size_t  cat = Sector (17, sector);

            Assert::AreEqual ((Byte) 17,           buffer[cat + 0x01]);
            Assert::AreEqual ((Byte) (sector - 1), buffer[cat + 0x02]);
        }

        // The last catalog sector terminates the chain.
        Assert::AreEqual ((Byte) 0, buffer[Sector (17, 1) + 0x01]);
        Assert::AreEqual ((Byte) 0, buffer[Sector (17, 1) + 0x02]);

        // Every catalog file entry is zero -> empty CATALOG.
        for (sector = 15; sector >= 1; sector--)
        {
            size_t  cat = Sector (17, sector);
            size_t  i   = 0;

            for (i = 0x0B; i < 256; i++)
            {
                Assert::AreEqual ((Byte) 0, buffer[cat + i]);
            }
        }
    }

    TEST_METHOD (Dos33Skeleton_EverythingOutsideTrack17IsZero)
    {
        vector<Byte>  buffer = MakeDos33Buffer();
        size_t        i      = 0;



        for (i = 0; i < buffer.size(); i++)
        {
            if (i >= Sector (17, 0) && i < Sector (18, 0))
            {
                continue;   // VTOC + catalog track checked elsewhere
            }

            Assert::AreEqual ((Byte) 0, buffer[i]);
        }
    }

    TEST_METHOD (Build_WozDos33_LoadsAndRoundTripsTheSkeleton)
    {
        BlankDiskSpec  spec;                          // defaults: WOZ + DOS 3.3
        vector<Byte>   woz;
        vector<Byte>   readBack;
        DiskImage      img;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));
        Assert::IsTrue (woz.size() > 8, L"WOZ output must be non-trivial");

        // The produced file is a valid WOZ the loader accepts...
        AssertSucceeded (WozLoader::Load (woz, img));
        Assert::IsTrue (img.GetTrackBitCount (0) > 0);

        // ...whose tracks denibblize back to the exact skeleton buffer, so
        // the GCR encoding of the fresh disk is bit-faithful.
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, readBack));

        vector<Byte>  expected (NibblizationLayer::kImageByteSize, 0);
        AssertSucceeded (Dos33Skeleton::Write (expected, spec.volumeNumber));

        Assert::IsTrue (expected == readBack, L"skeleton must survive the GCR round-trip");
    }

    TEST_METHOD (Build_WozOutputs_AreDeterministic)
    {
        BlankDiskSpec  spec;
        vector<Byte>   first;
        vector<Byte>   second;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, first));
        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, second));

        Assert::IsTrue (first == second, L"identical spec must build identical bytes");
    }

    TEST_METHOD (Build_WozNeverWriteProtected)
    {
        BlankDiskSpec  spec;
        vector<Byte>   woz;
        DiskImage      img;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));
        AssertSucceeded (WozLoader::Load (woz, img));

        // A new disk must not carry the image write-protect flag.
        Assert::IsFalse (img.GetWriteProtectInfo().imageFlag);
    }

    TEST_METHOD (Build_WozUnformatted_FullCapacityZeroBitTracks)
    {
        BlankDiskSpec  spec;
        vector<Byte>   woz;
        DiskImage      img;
        int            track = 0;



        spec.contents = BlankDiskContents::Unformatted;

        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));
        AssertSucceeded (WozLoader::Load (woz, img));

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            Assert::AreEqual (NibblizationLayer::kTrackBitCapacity,
                              img.GetTrackBitCount (track));
        }
    }

    TEST_METHOD (Build_MountsThroughTheStore)
    {
        BlankDiskSpec   spec;
        vector<Byte>    woz;
        DiskImageStore  store;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));
        AssertSucceeded (store.MountFromBytes (6, 0, "new-blank.woz", DiskFormat::Woz, woz));
        Assert::IsTrue (store.IsMounted (6, 0));
    }

    TEST_METHOD (Build_FailureLeavesOutputUntouched)
    {
        BlankDiskSpec  spec;
        vector<Byte>   out   = { 0xDE, 0xAD };



        // Valid spec, not-yet-implemented path (bootable): clean failure
        // with the output vector untouched.
        spec.bootable = true;

        AssertFailed (BlankDiskBuilder::Build (spec, BootPayload{}, out));
        Assert::AreEqual ((size_t) 2, out.size());
        Assert::AreEqual ((Byte) 0xDE, out[0]);
    }

    TEST_METHOD (Dos33Skeleton_RejectsWrongBufferSize)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;
        vector<Byte>                        tooSmall (100, 0);



        AssertFailed (Dos33Skeleton::Write (tooSmall, 254));
        expect.RequireCount (1);
    }

    TEST_METHOD (ValidateSpec_ProDosVolumeNameRules)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;
        BlankDiskSpec                       spec   = MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos);



        spec.volumeName = "NEWDISK";
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (spec));

        spec.volumeName = "A.LONG.OK.NAME1";      // 15 chars, legal set
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (spec));

        spec.volumeName = "";                     // empty
        AssertFailed (BlankDiskBuilder::ValidateSpec (spec));

        spec.volumeName = "1LEADINGDIGIT";        // must start with a letter
        AssertFailed (BlankDiskBuilder::ValidateSpec (spec));

        spec.volumeName = "HAS SPACE";            // illegal character
        AssertFailed (BlankDiskBuilder::ValidateSpec (spec));

        spec.volumeName = "WAY.TOO.LONG.NAME1";   // 17 chars
        AssertFailed (BlankDiskBuilder::ValidateSpec (spec));

        expect.RequireCount (4);
    }
};
