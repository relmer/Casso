#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/ProDosSkeleton.h"
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

    //  .do IS .dsk UNDER THE OTHER NAME, and Build has always treated the two
    //  as one container. Only the validator disagreed, so `disk create foo.do`
    //  -- a word the command line offers by name -- asserted on the way in.
    TEST_METHOD (ValidateSpec_DoPairsWithDosOrRaw_TheSameAsDsk)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;



        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Do, BlankDiskContents::Dos33)));
        AssertSucceeded (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Do, BlankDiskContents::Unformatted)));
        AssertFailed    (BlankDiskBuilder::ValidateSpec (MakeSpec (DiskFormat::Do, BlankDiskContents::ProDos)));
        expect.RequireCount (1);
    }

    //
    //  EVERY CONTAINER THE COMMAND LINE OFFERS MUST BE ONE THE BUILDER WILL
    //  WRITE, and the list is swept rather than restated so that a row added
    //  to it without a matching arm here fails this test instead of asserting
    //  in front of whoever typed the new word.
    //
    //  Raw media is the case every container has in common: it is the one
    //  contents that carries no sector order, so any container that cannot
    //  take it cannot be written at all.
    //
    TEST_METHOD (CheckSpec_AcceptsEveryContainerTheCommandLineAdvertises)
    {
        const DiskCommandRunner::ContainerName *  containers = nullptr;
        size_t                                    count      = 0;
        size_t                                    i          = 0;



        containers = DiskCommandRunner::AdvertisedContainers (count);

        Assert::IsTrue (count > 0, L"the command line advertises at least one container");

        for (i = 0; i < count; i++)
        {
            BlankDiskSpec  spec  = MakeSpec (containers[i].format, BlankDiskContents::Unformatted);
            std::string    which = containers[i].name;
            vector<Byte>   bytes;

            Assert::IsTrue (BlankDiskVerdict::Ok == BlankDiskBuilder::CheckSpec (spec),
                (std::wstring (L"the builder refuses a container the command line offers: ")
                     + std::wstring (which.begin(), which.end())).c_str());

            AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload(), bytes));
            Assert::IsFalse (bytes.empty(),
                (std::wstring (L"and writes nothing for: ")
                     + std::wstring (which.begin(), which.end())).c_str());
        }
    }

    //  THE VERDICT NAMES THE RULE, which is the whole reason it is not an
    //  HRESULT: `disk create` puts the broken rule into a sentence, and one
    //  failure code for all three made it recite all three.
    TEST_METHOD (CheckSpec_NamesTheRuleThatWasBroken)
    {
        BlankDiskSpec  badPairing  = MakeSpec (DiskFormat::Po,  BlankDiskContents::Dos33);
        BlankDiskSpec  badBootable = MakeSpec (DiskFormat::Woz, BlankDiskContents::Unformatted, true);
        BlankDiskSpec  badName     = MakeSpec (DiskFormat::Po,  BlankDiskContents::ProDos);



        badName.volumeName = "1LEADINGDIGIT";

        Assert::IsTrue (BlankDiskVerdict::ContentsNotInContainer  == BlankDiskBuilder::CheckSpec (badPairing));
        Assert::IsTrue (BlankDiskVerdict::BootableNeedsFilesystem == BlankDiskBuilder::CheckSpec (badBootable));
        Assert::IsTrue (BlankDiskVerdict::ProDosNameUnusable      == BlankDiskBuilder::CheckSpec (badName));
    }

    //  CheckSpec ANSWERS WITHOUT ASSERTING. It is what stands between user
    //  input and ValidateSpec's E_INVALIDARG, so it has to be callable on a
    //  spec that is wrong in every way at once.
    TEST_METHOD (CheckSpec_DoesNotAssertOnASpecItRefuses)
    {
        BlankDiskSpec  spec = MakeSpec (DiskFormat::Po, BlankDiskContents::Dos33, true);



        spec.volumeName = "";

        Assert::IsFalse (BlankDiskVerdict::Ok == BlankDiskBuilder::CheckSpec (spec));
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

    TEST_METHOD (Build_DskDos33_IsTheSkeletonBufferVerbatim)
    {
        BlankDiskSpec  spec     = MakeSpec (DiskFormat::Dsk, BlankDiskContents::Dos33);
        vector<Byte>   dsk;
        vector<Byte>   expected = MakeDos33Buffer();



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, dsk));

        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, dsk.size());
        Assert::IsTrue (expected == dsk, L"a .dsk is the DOS-order buffer itself");
    }


    TEST_METHOD (Build_UnformattedSectorImages_AreAllZeros)
    {
        vector<Byte>  dsk;
        vector<Byte>  po;
        vector<Byte>  zeros ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);



        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Dsk, BlankDiskContents::Unformatted), BootPayload{}, dsk));
        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Po, BlankDiskContents::Unformatted), BootPayload{}, po));

        Assert::IsTrue (zeros == dsk, L"unformatted .dsk is a zero sector image");
        Assert::IsTrue (zeros == po,  L"unformatted .po is a zero sector image");
    }


    TEST_METHOD (Build_PoProDos_LaysBlocksLinearly)
    {
        // In a .po file ProDOS blocks are stored in order, so block b begins
        // at byte b*512 regardless of the buffer's internal interleave.
        BlankDiskSpec  spec = MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos);
        vector<Byte>   po;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, po));
        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, po.size());

        // Key block 2: storage 0xF + name length 7, name NEWDISK.
        Assert::AreEqual ((Byte) 0xF7, po[2 * 512 + 0x04]);
        Assert::AreEqual ((Byte) 'N',  po[2 * 512 + 0x05]);
        Assert::AreEqual ((Byte) 'K',  po[2 * 512 + 0x0B]);

        // Key block links to 3; block 5 ends the chain.
        Assert::AreEqual ((Byte) 3, po[2 * 512 + 0x02]);
        Assert::AreEqual ((Byte) 0, po[5 * 512 + 0x02]);

        // Bitmap block 6: blocks 0-6 used, 7-279 free.
        Assert::AreEqual ((Byte) 0x01, po[6 * 512]);
        Assert::AreEqual ((Byte) 0xFF, po[6 * 512 + 1]);
        Assert::AreEqual ((Byte) 0xFF, po[6 * 512 + 34]);
        Assert::AreEqual ((Byte) 0x00, po[6 * 512 + 35]);
    }


    TEST_METHOD (Build_WozProDos_RoundTripsTheSkeleton)
    {
        BlankDiskSpec  spec = MakeSpec (DiskFormat::Woz, BlankDiskContents::ProDos);
        vector<Byte>   woz;
        vector<Byte>   readBack;
        DiskImage      img;



        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));
        AssertSucceeded (WozLoader::Load (woz, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, readBack));

        vector<Byte>  expected (NibblizationLayer::kImageByteSize, 0);
        AssertSucceeded (ProDosSkeleton::Write (expected, spec.volumeName));

        Assert::IsTrue (expected == readBack,
            L"the ProDOS skeleton must survive the GCR round-trip");
    }


    TEST_METHOD (Build_SectorFormats_AreDeterministic)
    {
        vector<Byte>  first;
        vector<Byte>  second;



        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Dsk, BlankDiskContents::Dos33), BootPayload{}, first));
        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Dsk, BlankDiskContents::Dos33), BootPayload{}, second));
        Assert::IsTrue (first == second);

        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos), BootPayload{}, first));
        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos), BootPayload{}, second));
        Assert::IsTrue (first == second);
    }


    TEST_METHOD (Build_SectorFormats_MountThroughTheStore)
    {
        DiskImageStore  store;
        vector<Byte>    dsk;
        vector<Byte>    po;



        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Dsk, BlankDiskContents::Dos33), BootPayload{}, dsk));
        AssertSucceeded (store.MountFromBytes (6, 0, "new-blank.dsk", DiskFormat::Dsk, dsk));
        Assert::IsTrue (store.IsMounted (6, 0));

        AssertSucceeded (BlankDiskBuilder::Build (
            MakeSpec (DiskFormat::Po, BlankDiskContents::ProDos), BootPayload{}, po));
        AssertSucceeded (store.MountFromBytes (6, 1, "new-blank.po", DiskFormat::Po, po));
        Assert::IsTrue (store.IsMounted (6, 1));
    }


    static vector<Byte> MakeSyntheticMaster()
    {
        // A fabricated System Master: recognizable pattern in tracks 0-2,
        // distinct junk elsewhere so a copy that over-reaches is caught.
        vector<Byte>  master (NibblizationLayer::kImageByteSize, 0x5A);
        size_t        i = 0;

        for (i = 0; i < (size_t) (3 * 16 * 256); i++)
        {
            master[i] = (Byte) ((i * 11 + 5) & 0xFF);
        }

        return master;
    }


    TEST_METHOD (InstallDos_CopiesTracks0Through2Verbatim)
    {
        vector<Byte>  buffer = MakeDos33Buffer();
        vector<Byte>  master = MakeSyntheticMaster();
        size_t        i      = 0;



        AssertSucceeded (Dos33Skeleton::InstallDos (buffer, master));

        for (i = 0; i < (size_t) (3 * 16 * 256); i++)
        {
            if (buffer[i] != master[i])
            {
                Assert::Fail (L"DOS image bytes must copy verbatim");
            }
        }

        // The VTOC is untouched: catalog pointer still T17 S15.
        Assert::AreEqual ((Byte) 17, buffer[Sector (17, 0) + 0x01]);
        Assert::AreEqual ((Byte) 15, buffer[Sector (17, 0) + 0x02]);
    }


    TEST_METHOD (WriteHello_CatalogTsListDataAndBitmapAreHonest)
    {
        vector<Byte>  buffer = MakeDos33Buffer();
        size_t        entry  = Sector (17, 15) + 0x0B;
        size_t        i      = 0;



        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        // Catalog entry: TS list at T18 S15, Applesoft, name HELLO padded
        // with high-ASCII spaces, two sectors long.
        Assert::AreEqual ((Byte) 18,   buffer[entry + 0x00]);
        Assert::AreEqual ((Byte) 15,   buffer[entry + 0x01]);
        Assert::AreEqual ((Byte) 0x02, buffer[entry + 0x02]);
        Assert::AreEqual ((Byte) 0xC8, buffer[entry + 0x03]);   // H
        Assert::AreEqual ((Byte) 0xCF, buffer[entry + 0x07]);   // O
        Assert::AreEqual ((Byte) 0xA0, buffer[entry + 0x08]);   // padding
        Assert::AreEqual ((Byte) 2,    buffer[entry + 0x21]);

        // TS list points at the one data sector.
        Assert::AreEqual ((Byte) 18, buffer[Sector (18, 15) + 0x0C]);
        Assert::AreEqual ((Byte) 14, buffer[Sector (18, 15) + 0x0D]);

        // Data sector: length 8, then the tokenized 10 REM.
        Assert::AreEqual ((Byte) 0x08, buffer[Sector (18, 14) + 0]);
        Assert::AreEqual ((Byte) 0xB2, buffer[Sector (18, 14) + 6]);

        // Bitmap: T18 loses sectors 15 and 14, keeps the rest.
        Assert::AreEqual ((Byte) 0x3F, buffer[Sector (17, 0) + 0x38 + 18 * 4]);
        Assert::AreEqual ((Byte) 0xFF, buffer[Sector (17, 0) + 0x38 + 18 * 4 + 1]);

        // Other tracks' bitmap entries are untouched.
        Assert::AreEqual ((Byte) 0xFF, buffer[Sector (17, 0) + 0x38 + 19 * 4]);

        // Nothing else on the file's track was touched.
        for (i = 0x0E; i < 256; i++)
        {
            Assert::AreEqual ((Byte) 0, buffer[Sector (18, 15) + i]);
        }
    }


    TEST_METHOD (Build_BootableDos33_InstallsDosAndHello)
    {
        BlankDiskSpec  spec  = MakeSpec (DiskFormat::Woz, BlankDiskContents::Dos33, true);
        BootPayload    payload;
        vector<Byte>   woz;
        vector<Byte>   readBack;
        DiskImage      img;
        size_t         entry = Sector (17, 15) + 0x0B;
        size_t         i     = 0;



        payload.dosMasterSectors = MakeSyntheticMaster();

        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, woz));
        AssertSucceeded (WozLoader::Load (woz, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, readBack));

        // The DOS image landed in tracks 0-2...
        for (i = 0; i < (size_t) (3 * 16 * 256); i++)
        {
            if (readBack[i] != payload.dosMasterSectors[i])
            {
                Assert::Fail (L"bootable disk must carry the master's DOS tracks");
            }
        }

        // ...and the HELLO greeting is cataloged.
        Assert::AreEqual ((Byte) 0xC8, readBack[entry + 0x03]);
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
