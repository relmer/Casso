#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/WozMetadata.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRealDiskTests
//
//  Real disk images read with no stamp, with the stamp Casso builds for them,
//  and with a stamp whose marks are wrong on every track. A stamp may only
//  add sectors; a wrong one must change nothing.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampRealDiskTests)
{
public:

    static constexpr const char *  s_kImages[] =
    {
        "CassoExplorer/dos33.dsk",
        "CiderPress/woz/dos32master_2.woz",
        "CiderPress/rdos/rdos3-rss-save.woz",
        "CiderPress/rdos/rdos32-shiloh-save.woz",
        "CiderPress/rdos/rdos33-rw2000-save.woz",
    };

    static constexpr Byte  kWrongFirst = 0xDA;



    struct Read
    {
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        size_t              sectorBytes = 0;
        int                 perTrack    = 0;
    };



    static void ReadImage (const DiskImage & image, Read & outRead)
    {
        if (ThirteenSectorCodec::IsThirteenSector (image))
        {
            ThirteenSectorCodec::Denibblize (image, outRead.sectors, outRead.report);
            outRead.perTrack = ThirteenSectorCodec::kSectorsPerTrack;
        }
        else
        {
            Denibblizer::Denibblize (image, DiskFormat::Dsk, outRead.sectors, outRead.report);
            outRead.perTrack = 16;
        }

        outRead.sectorBytes = ThirteenSectorCodec::kSectorByteSize;
    }



    static void LoadImage (const char * name, DiskImage & outImage)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;



        HRESULT          hr = fixtures.OpenFixture (name, bytes);



        Assert::IsTrue (SUCCEEDED (hr), std::wstring (L"open ") .append (name, name + strlen (name)).c_str());

        hr = std::string_view (name).ends_with (".dsk") ? Nibblizer::NibblizeDsk (bytes, outImage) : WozLoader::Load (bytes, outImage);
        Assert::IsTrue (SUCCEEDED (hr), std::wstring (L"load ") .append (name, name + strlen (name)).c_str());
    }



    static void SetStamp (DiskImage & image, const FormatStamp & stamp)
    {
        WozMetadata  meta = image.GetWozMetadata();



        AssertSucceeded (FormatStampChunk::SetStamp (meta, stamp));
        image.SetWozMetadata (meta);
    }



    static FormatStamp BuildStamp (const DiskImage & image)
    {
        FormatStamp  stamp;



        FormatStampBuilder::Build (*DiskCopy::MakeFromImage (image, 1, "real.woz", 0, false), DecodeSettings::MakeStandard(), StampSource::Inspector, nullptr, "2026-11-23T00:00:00Z", stamp);

        return stamp;
    }



    //  Every sector the plain read verified reads the same with the stamp.
    static void AssertNoWorse (const Read & plain, const Read & stamped, const char * name)
    {
        std::wstring  label (name, name + strlen (name));



        for (int track = 0; track < plain.report.GetTrackCount(); track++)
        {
            Word  plainCoverage   = plain.report.GetCoverage (track);
            Word  stampedCoverage = stamped.report.GetCoverage (track);

            Assert::AreEqual ((int) plainCoverage, (int) (plainCoverage & stampedCoverage), label.c_str());

            for (int sector = 0; sector < plain.perTrack; sector++)
            {
                size_t  at = ((size_t) track * plain.perTrack + sector) * plain.sectorBytes;

                if ((plainCoverage & (1 << sector)) != 0)
                {
                    Assert::IsTrue (std::equal (plain.sectors.begin() + (ptrdiff_t) at, plain.sectors.begin() + (ptrdiff_t) (at + plain.sectorBytes), stamped.sectors.begin() + (ptrdiff_t) at), label.c_str());
                }
            }
        }
    }



    TEST_METHOD (EveryImage_ReadsNoWorseWithItsStamp)
    {
        int  checked = 0;



        for (const char * name : s_kImages)
        {
            DiskImage  image;
            Read       plain;
            Read       stamped;

            LoadImage (name, image);
            ReadImage (image, plain);

            SetStamp  (image, BuildStamp (image));
            ReadImage (image, stamped);

            AssertNoWorse (plain, stamped, name);
            checked++;
        }

        Assert::AreEqual ((int) std::size (s_kImages), checked);
    }



    TEST_METHOD (EveryImage_ReadsTheSameWithAWrongStamp)
    {
        int  checked = 0;



        for (const char * name : s_kImages)
        {
            DiskImage    image;
            Read         plain;
            Read         wrong;
            FormatStamp  stamp;

            LoadImage (name, image);
            ReadImage (image, plain);

            stamp = BuildStamp (image);

            for (StampTrack & track : stamp.tracks)
            {
                for (vector<Byte> & prolog : track.addressPrologs) { prolog[0] = kWrongFirst; }
                for (vector<Byte> & prolog : track.dataPrologs)    { prolog[0] = kWrongFirst; }
            }

            SetStamp  (image, stamp);
            ReadImage (image, wrong);

            Assert::IsTrue (plain.sectors == wrong.sectors, std::wstring (name, name + strlen (name)).c_str());

            for (int track = 0; track < plain.report.GetTrackCount(); track++)
            {
                Assert::IsTrue (plain.report.GetOutcome (track) == wrong.report.GetOutcome (track));
            }

            checked++;
        }

        Assert::AreEqual ((int) std::size (s_kImages), checked);
    }
};
