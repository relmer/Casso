#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScannerTests
//
//  The integrity pass and the pre-commit check, over in-memory volumes the
//  skeleton writer formats.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33IntegrityScannerTests)
{
public:

    static constexpr Byte      kVolumeNumber   = 254;
    static constexpr uint32_t  kUnitCount      = 560;
    static constexpr uint32_t  kReservedUnits  = 4 * 16;
    static constexpr int       kHelloListTrack = 18;
    static constexpr int       kHelloListSec   = 15;
    static constexpr int       kHelloDataSec   = 14;

    static vector<Byte> MakeVolumeWithHello()
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));
        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        return buffer;
    }

    static VolumeIntegrityReport Scan (const vector<Byte> & buffer)
    {
        Dos33SectorAccess      access (buffer);
        Dos33IntegrityScanner  scanner (access);
        VolumeIntegrityReport  report;

        AssertSucceeded (scanner.BuildIntegrityReport (report));

        return report;
    }

    static void MarkFree (vector<Byte> & buffer, int track, int sector)
    {
        Dos33SectorAccess  access (buffer);
        size_t             at = access.GetGeometry().GetBitmapByteOffset (track, sector);

        buffer[at] = (Byte) (buffer[at] | access.GetGeometry().GetBitmapMask (sector));
    }

    TEST_METHOD (BuildIntegrityReport_AHealthyVolume_IsClean)
    {
        vector<Byte>           buffer = MakeVolumeWithHello();
        VolumeIntegrityReport  report = Scan (buffer);

        Assert::IsTrue (report.IsClean());
        Assert::IsTrue (report.IsCatalogFullyParsed());
        Assert::AreEqual (kReservedUnits, (uint32_t) report.GetClaimsOf (VolumeIntegrityReport::kVolumeOwner).size(),
                          L"the volume owner claims the reserved tracks");
        Assert::AreEqual (size_t (2), report.GetClaimsOf (0).size(), L"HELLO's list and data sector");
    }

    TEST_METHOD (BuildIntegrityReport_AReferencedSectorMarkedFree_IsReported)
    {
        vector<Byte>           buffer = MakeVolumeWithHello();
        VolumeIntegrityReport  report;

        MarkFree (buffer, kHelloListTrack, kHelloDataSec);

        report = Scan (buffer);

        Assert::IsFalse (report.IsClean());
        Assert::AreEqual (size_t (1), report.GetClaimedButFree().size());
    }

    TEST_METHOD (BuildIntegrityReport_ABufferOfTheWrongSize_IsRejected)
    {
        vector<Byte>           buffer (NibbleFormat::kSectorByteSize, 0);
        Dos33SectorAccess      access (buffer);
        Dos33IntegrityScanner  scanner (access);
        VolumeIntegrityReport  report;

        Assert::AreEqual (E_INVALIDARG, scanner.BuildIntegrityReport (report));
    }

    TEST_METHOD (HandBackVerifiedResult_AnUnchangedVolume_IsCopiedOut)
    {
        vector<Byte>           buffer = MakeVolumeWithHello();
        VolumeIntegrityReport  before = Scan (buffer);
        vector<Byte>           handedBack;

        AssertSucceeded (Dos33IntegrityScanner::HandBackVerifiedResult (before, buffer, handedBack));

        Assert::IsTrue (handedBack == buffer);
    }

    TEST_METHOD (HandBackVerifiedResult_AResultWorseThanItsInput_IsRejected)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           buffer = MakeVolumeWithHello();
        vector<Byte>           result = buffer;
        VolumeIntegrityReport  before = Scan (buffer);
        vector<Byte>           handedBack;

        MarkFree (result, kHelloListTrack, kHelloListSec);

        Assert::AreEqual (E_UNEXPECTED, Dos33IntegrityScanner::HandBackVerifiedResult (before, result, handedBack));
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }
};





