#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMapTests
//
//  The VTOC free bitmap and allocation from it, over an in-memory formatted
//  volume and an integrity report built by the test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33FreeMapTests)
{
public:

    static constexpr Byte      kVolumeNumber      = 254;
    static constexpr uint32_t  kFreeOnBlankVolume = 31 * 16;
    static constexpr uint32_t  kUnitCount         = 560;
    static constexpr int       kFirstTrack        = 18;
    static constexpr int       kLastSector        = 15;

    static vector<Byte> MakeFormattedVolume()
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));

        return buffer;
    }

    TEST_METHOD (CountFreeUnits_OnABlankVolume_LeavesOutTheReservedTracks)
    {
        vector<Byte>       buffer = MakeFormattedVolume();
        Dos33SectorAccess  access (buffer);
        Dos33FreeMap       freeMap (access);

        Assert::AreEqual (kFreeOnBlankVolume, freeMap.CountFreeUnits());
        Assert::IsFalse (freeMap.IsFree (Dos33Skeleton::kVtocTrack, 0));
        Assert::IsTrue  (freeMap.IsFree (kFirstTrack, 0));
    }

    TEST_METHOD (SetFreeInBitmap_ChangesTheCopyOnly)
    {
        vector<Byte>       buffer = MakeFormattedVolume();
        vector<Byte>       result = buffer;
        Dos33SectorAccess  access (buffer);
        Dos33FreeMap       freeMap (access);

        freeMap.SetFreeInBitmap (result, kFirstTrack, 0, false);

        Assert::IsTrue (freeMap.IsFree (kFirstTrack, 0), L"the source bitmap is left alone");

        {
            Dos33SectorAccess  resultAccess (result);
            Dos33FreeMap       resultMap (resultAccess);

            Assert::IsFalse (resultMap.IsFree (kFirstTrack, 0));
        }
    }

    TEST_METHOD (TryAllocateUnits_StartsNextToTheCatalogTrack)
    {
        vector<Byte>           buffer = MakeFormattedVolume();
        Dos33SectorAccess      access (buffer);
        Dos33FreeMap           freeMap (access);
        VolumeIntegrityReport  report;
        vector<uint32_t>       units;

        report.Reset (kUnitCount);

        Assert::IsTrue (freeMap.TryAllocateUnits (report, 2, units));
        Assert::AreEqual (size_t (2), units.size());
        Assert::AreEqual (access.ToUnit (kFirstTrack, kLastSector),     units[0]);
        Assert::AreEqual (access.ToUnit (kFirstTrack, kLastSector - 1), units[1]);
    }

    TEST_METHOD (TryAllocateUnits_PassesOverASectorTheCatalogClaims)
    {
        vector<Byte>           buffer = MakeFormattedVolume();
        Dos33SectorAccess      access (buffer);
        Dos33FreeMap           freeMap (access);
        VolumeIntegrityReport  report;
        vector<uint32_t>       units;

        report.Reset (kUnitCount);
        report.AddClaim (access.ToUnit (kFirstTrack, kLastSector), 0);

        Assert::IsTrue (freeMap.TryAllocateUnits (report, 1, units));
        Assert::AreEqual (access.ToUnit (kFirstTrack, kLastSector - 1), units[0]);
    }

    TEST_METHOD (TryAllocateUnits_MoreThanTheVolumeHas_Fails)
    {
        vector<Byte>           buffer = MakeFormattedVolume();
        Dos33SectorAccess      access (buffer);
        Dos33FreeMap           freeMap (access);
        VolumeIntegrityReport  report;
        vector<uint32_t>       units;

        report.Reset (kUnitCount);

        Assert::IsFalse (freeMap.TryAllocateUnits (report, kFreeOnBlankVolume + 1, units));
    }
};





