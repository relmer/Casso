#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosModifiedDateTests
//
//  The modification stamp a ProDOS directory record carries, surfaced on
//  every listed entry; and its absence on DOS 3.3, which records none.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ProDosModifiedDateTests)
{
public:

    static constexpr const char *  kDos33Fixture  = "Cassque/dos33.dsk";
    static constexpr const char *  kProDosFixture = "Cassque/prodos.po";

    //  1984-08-17 12:34, the stamp the fixture script writes on every entry.
    static constexpr int64_t  kFixtureStampUnix = 461594040;

    //  Where the first file record and its modification stamp sit, restated
    //  from the format reference rather than borrowed from the volume.
    static constexpr int     kDirKeyBlock    = 2;
    static constexpr size_t  kOffFirstEntry  = 0x04;
    static constexpr size_t  kEntryLength    = 0x27;
    static constexpr size_t  kEntOffModified = 0x21;
    static constexpr size_t  kStampBytes     = 4;



    static void LoadSectors (const char * fixture, vector<Byte> & outSectors)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        SectorDecodeReport  report;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, fixture, outSectors, report));
    }



    TEST_METHOD (ProDos_EveryEntryCarriesTheStamp)
    {
        vector<Byte>   sectors;
        VolumeListing  listing;

        LoadSectors (kProDosFixture, sectors);

        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::IsTrue (!listing.entries.empty());

        for (const FileEntry & entry : listing.entries)
        {
            Assert::IsTrue   (entry.hasModified, L"every fixture entry was stamped");
            Assert::AreEqual (kFixtureStampUnix, entry.modifiedUnix);
        }
    }



    TEST_METHOD (Dos33_EntriesCarryNoDate)
    {
        vector<Byte>   sectors;
        VolumeListing  listing;

        LoadSectors (kDos33Fixture, sectors);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::IsTrue (!listing.entries.empty());

        for (const FileEntry & entry : listing.entries)
        {
            Assert::IsFalse (entry.hasModified);
        }
    }



    TEST_METHOD (ProDos_AZeroDateReadsAsNoDate)
    {
        vector<Byte>   sectors;
        VolumeListing  listing;
        size_t         firstEntry = 0;
        size_t         i          = 0;

        LoadSectors (kProDosFixture, sectors);

        //  HELLO is the first record after the volume header.
        firstEntry = ProDosSkeleton::GetBlockByteOffset (kDirKeyBlock, kOffFirstEntry + kEntryLength);

        for (i = 0; i < kStampBytes; i++)
        {
            sectors[firstEntry + kEntOffModified + i] = 0;
        }

        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::AreEqual (std::string ("HELLO"), listing.entries[0].name);
        Assert::IsFalse  (listing.entries[0].hasModified);
        Assert::IsTrue   (listing.entries[1].hasModified, L"only the cleared entry loses its date");
    }



    TEST_METHOD (TryToUnixTime_AppliesTheCenturyPivotAndRefusesNonsense)
    {
        int64_t  unix = 0;

        //  2005-01-01 00:00: a two-digit year below the pivot is this century.
        Assert::IsTrue   (ProDosVolume::TryToUnixTime ((Word) ((5 << 9) | (1 << 5) | 1), 0, unix));
        Assert::AreEqual ((int64_t) 1104537600, unix);

        //  1984-08-17 12:34, as the fixture is stamped.
        Assert::IsTrue   (ProDosVolume::TryToUnixTime ((Word) ((84 << 9) | (8 << 5) | 17), (Word) ((12 << 8) | 34), unix));
        Assert::AreEqual (kFixtureStampUnix, unix);

        //  A thirteenth month, and no date at all.
        Assert::IsFalse (ProDosVolume::TryToUnixTime ((Word) ((84 << 9) | (13 << 5) | 1), 0, unix));
        Assert::IsFalse (ProDosVolume::TryToUnixTime (0, 0, unix));
    }
};
