#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueFixtureTests
//
//  The two scratch volumes every browser test reads. Each is loaded through
//  the same path the emulator's mount uses and its catalog counted, so a
//  fixture that stops loading fails here, by name, rather than in whichever
//  test happened to reach it first.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueFixtureTests)
{
public:

    static constexpr const char *  kDos33Image  = "Cassque/dos33.dsk";
    static constexpr const char *  kProDosImage = "Cassque/prodos.po";

    //  Seven files on the DOS 3.3 side; six files and one subdirectory on the
    //  ProDOS side, which has no Integer BASIC type to put.
    static constexpr size_t  kDos33Entries  = 7;
    static constexpr size_t  kProDosEntries = 7;



    static void LoadSectors (const char * fixture, VolumeKind expectedKind, vector<Byte> & outSectors)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        SectorDecodeReport  report;
        VolumeKind          kind    = VolumeKind::Unknown;



        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, fixture, outSectors, report));

        kind = VolumeImage::DetectFilesystem (outSectors);

        Assert::IsTrue (kind == expectedKind, L"the fixture must carry the filesystem its name claims");
    }



    TEST_METHOD (Dos33Fixture_LoadsAndListsEveryEntry)
    {
        vector<Byte>   sectors;
        VolumeListing  listing;



        LoadSectors (kDos33Image, VolumeKind::Dos33, sectors);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::AreEqual (kDos33Entries, listing.entries.size());
        Assert::IsTrue   (listing.damage.empty(), L"a fixture written by this tree must read cleanly");
        Assert::AreEqual ((Byte) 77, listing.volumeNumber);
    }



    TEST_METHOD (ProDosFixture_LoadsAndListsEveryEntry)
    {
        vector<Byte>   sectors;
        VolumeListing  listing;
        size_t         directories = 0;



        LoadSectors (kProDosImage, VolumeKind::ProDos, sectors);

        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::AreEqual (kProDosEntries, listing.entries.size());
        Assert::IsTrue   (listing.damage.empty(), L"a fixture written by this tree must read cleanly");
        Assert::AreEqual (std::string ("CASSQUE"), listing.volumeName);

        for (const FileEntry & entry : listing.entries)
        {
            directories += entry.isDirectory ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 1, directories, L"the ProDOS fixture carries one subdirectory");
    }
};
