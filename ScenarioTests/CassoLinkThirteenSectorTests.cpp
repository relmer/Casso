#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/ProgramRig.h"
#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkThirteenSectorScenarioTests
//
//  13-sector disks over the link on an emulated //c, from CiderPress II's
//  samples, which aren't committed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkThirteenSectorScenarioTests)
{
public:

    static std::vector<Byte>  LoadFixture()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));

        return bytes;
    }

    //  The DOS 3.2 master read over the link: the sectors and tracks Casso
    //  decodes from the image.
    TEST_METHOD (TheDos32Master_IsReadOverTheLink_AsItDecodes)
    {
        FixtureProvider     fixtures;
        ProgramRig          rig (LoadFixture());
        std::vector<Byte>   woz;
        std::vector<Byte>   module;
        std::vector<Byte>   expected;
        std::vector<Byte>   read;
        SectorDecodeReport  expectedReport;
        SectorDecodeReport  readReport;
        CassoLinkStatus     status;



        AssertSucceeded (fixtures.OpenFixture ("CiderPress/woz/dos32master_2.woz", woz));
        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, woz));

        ThirteenSectorCodec::Denibblize (*rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0), expected, expectedReport);

        AssertSucceeded (LinkProgram::BuildThirteenSectorModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (CassoLinkThirteenSector::ReadDisk (*rig.host, 1, nullptr, nullptr, read, readReport, status));
        Assert::AreEqual ((Byte) 0, status.error);
        Assert::IsTrue (read == expected, L"The sectors read over the link");

        for (int track = 0; track < ThirteenSectorCodec::kTrackCount; track++)
        {
            Assert::AreEqual ((int) expectedReport.GetOutcome (track), (int) readReport.GetOutcome (track), std::format (L"track {}", track).c_str());
        }
    }
};
