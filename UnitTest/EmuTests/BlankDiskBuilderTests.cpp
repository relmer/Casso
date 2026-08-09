#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/Dos33Skeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskBuilderTests
//
//  Spec 017: BlankDiskSpec validation matrix (FR-010), DOS 3.3 skeleton
//  structural invariants (R-004), and BlankDiskBuilder output determinism /
//  format guarantees (FR-011/FR-012). No host fixture files — every input is
//  built in memory.
//
//  Populated across T004 (matrix), T005 (skeleton), T006 (WOZ build),
//  T014 (full format matrix), T018 (payload installers, synthetic bytes).
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

    TEST_METHOD (SpecDefaults_MatchFr004)
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
