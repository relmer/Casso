#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeRenameTests
//
//  Rename on both filesystems, over the scratch fixtures. Each case reads the
//  file back under its new name and compares bytes, since a rename that only
//  changed the listing would pass anything weaker.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VolumeRenameTests)
{
public:

    static constexpr const char *  kDos33Fixture  = "Cassque/dos33.dsk";
    static constexpr const char *  kProDosFixture = "Cassque/prodos.po";

    //  The two catalog layouts, restated from the format references rather
    //  than borrowed from the volumes, so a test that finds an entry agrees
    //  with the code under test only where the format itself does.
    static constexpr int     kDos33CatalogTrack     = 17;
    static constexpr int     kDos33CatalogSector    = 15;
    static constexpr size_t  kDos33EntryBase        = 0x0B;
    static constexpr size_t  kDos33EntrySize        = 0x23;
    static constexpr size_t  kDos33EntOffType       = 0x02;
    static constexpr size_t  kDos33EntOffName       = 0x03;
    static constexpr int     kDos33EntriesPerSector = 7;

    static constexpr int     kProDosDirKeyBlock     = 2;
    static constexpr int     kProDosDirLastBlock    = 5;
    static constexpr size_t  kProDosOffFirstEntry   = 0x04;
    static constexpr size_t  kProDosEntryLength     = 0x27;
    static constexpr int     kProDosEntriesPerBlock = 13;
    static constexpr size_t  kProDosEntOffName      = 0x01;
    static constexpr size_t  kProDosEntOffAccess    = 0x1E;



    static void LoadSectors (const char * fixture, vector<Byte> & outSectors)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        SectorDecodeReport  report;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, fixture, outSectors, report));
    }



    static bool HasName (const VolumeListing & listing, const char * name)
    {
        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == name)
            {
                return true;
            }
        }

        return false;
    }



    //  The DOS 3.3 catalog entry for `name`, by scanning the first catalog
    //  sector the way DOS lays it out, so the test agrees with the volume
    //  only where the format itself agrees.
    static size_t FindDos33Entry (const vector<Byte> & sectors, const char * name)
    {
        size_t  sectorAt = Dos33Skeleton::GetSectorOffset (kDos33CatalogTrack, kDos33CatalogSector);
        int     slot     = 0;

        for (slot = 0; slot < kDos33EntriesPerSector; slot++)
        {
            size_t  at      = sectorAt + kDos33EntryBase + (size_t) slot * kDos33EntrySize;
            size_t  i       = 0;
            bool    matches = true;

            for (i = 0; name[i] != '\0' && matches; i++)
            {
                matches = sectors[at + kDos33EntOffName + i] == (Byte) (name[i] | 0x80);
            }

            if (matches && sectors[at + kDos33EntOffName + i] == 0xA0)
            {
                return at;
            }
        }

        Assert::Fail (L"catalog entry not found");

        return 0;
    }



    //  The ProDOS directory record for `name` in the volume directory.
    static size_t FindProDosEntry (const vector<Byte> & sectors, const char * name)
    {
        int  block = 0;
        int  n     = 0;

        for (block = kProDosDirKeyBlock; block <= kProDosDirLastBlock; block++)
        {
            for (n = (block == kProDosDirKeyBlock) ? 1 : 0; n < kProDosEntriesPerBlock; n++)
            {
                size_t  at  = ProDosSkeleton::GetBlockByteOffset (block,
                                  kProDosOffFirstEntry + (size_t) n * kProDosEntryLength);
                size_t  len = sectors[at] & 0x0F;

                if (len == strlen (name)
                 && memcmp (&sectors[at + kProDosEntOffName], name, len) == 0)
                {
                    return at;
                }
            }
        }

        Assert::Fail (L"directory entry not found");

        return 0;
    }



    //
    //  ------------------------------------------------------------------
    //  DOS 3.3
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Dos33_RenameRewritesTheCatalogInPlace)
    {
        vector<Byte>   sectors;
        vector<Byte>   renamed;
        FilePayload    before;
        FilePayload    after;
        VolumeListing  listing;

        LoadSectors (kDos33Fixture, sectors);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), before));
            AssertSucceeded (volume.Rename (FilePath::Parse ("HELLO"), "greeting", renamed));
        }

        {
            Dos33Volume  volume (renamed);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse ("GREETING"), after));
        }

        Assert::IsTrue   (HasName (listing, "GREETING"), L"the catalog stores the name in upper case");
        Assert::IsFalse  (HasName (listing, "HELLO"));
        Assert::AreEqual ((size_t) 7, listing.entries.size());
        Assert::IsTrue   (before.bytes == after.bytes, L"the file's sectors are untouched");
        Assert::IsTrue   (before.type == after.type);
    }



    TEST_METHOD (Dos33_RenameToTheSameNameRecasesIt)
    {
        vector<Byte>   sectors;
        vector<Byte>   renamed;
        VolumeListing  listing;

        LoadSectors (kDos33Fixture, sectors);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Rename (FilePath::Parse ("hello"), "Hello", renamed));
        }

        {
            Dos33Volume  volume (renamed);

            AssertSucceeded (volume.Enumerate (listing));
        }

        Assert::IsTrue (HasName (listing, "HELLO"));
    }



    TEST_METHOD (Dos33_RenameRefusesACollisionAnIllegalNameAndAMissingFile)
    {
        vector<Byte>  sectors;
        vector<Byte>  renamed;

        LoadSectors (kDos33Fixture, sectors);

        {
            Dos33Volume  volume (sectors);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
                              volume.Rename (FilePath::Parse ("NOTES"), "HELLO", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                              volume.Rename (FilePath::Parse ("NOTES"), "1BAD", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                              volume.Rename (FilePath::Parse ("NOTES"), "A,B", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                              volume.Rename (FilePath::Parse ("NOPE"), "FINE", renamed));
        }

        Assert::IsTrue (renamed.empty(), L"a refusal produces nothing");
    }



    TEST_METHOD (Dos33_RenameRefusesALockedFile)
    {
        vector<Byte>  sectors;
        vector<Byte>  renamed;
        size_t        entry = 0;

        LoadSectors (kDos33Fixture, sectors);

        entry = FindDos33Entry (sectors, "HELLO");
        sectors[entry + kDos33EntOffType] = (Byte) (sectors[entry + kDos33EntOffType] | Dos33Volume::kLockedBit);

        {
            Dos33Volume  volume (sectors);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
                              volume.Rename (FilePath::Parse ("HELLO"), "GREETING", renamed));
        }
    }



    //
    //  ------------------------------------------------------------------
    //  ProDOS
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (ProDos_RenameRewritesTheDirectoryInPlace)
    {
        vector<Byte>   sectors;
        vector<Byte>   renamed;
        FilePayload    before;
        FilePayload    after;
        VolumeListing  listing;

        LoadSectors (kProDosFixture, sectors);

        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), before));
            AssertSucceeded (volume.Rename (FilePath::Parse ("HELLO"), "greeting.bas", renamed));
        }

        {
            ProDosVolume  volume (renamed);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse ("GREETING.BAS"), after));
        }

        Assert::IsTrue   (HasName (listing, "GREETING.BAS"));
        Assert::IsFalse  (HasName (listing, "HELLO"));
        Assert::AreEqual ((size_t) 7, listing.entries.size());
        Assert::IsTrue   (before.bytes == after.bytes, L"the file's blocks are untouched");
        Assert::IsTrue   (before.auxType == after.auxType);
    }



    TEST_METHOD (ProDos_RenameRefusesACollisionAnIllegalNameAMissingFileAndADirectory)
    {
        vector<Byte>  sectors;
        vector<Byte>  renamed;

        LoadSectors (kProDosFixture, sectors);

        {
            ProDosVolume  volume (sectors);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
                              volume.Rename (FilePath::Parse ("NOTES"), "hello", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                              volume.Rename (FilePath::Parse ("NOTES"), "BAD NAME", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                              volume.Rename (FilePath::Parse ("NOTES"), "SIXTEENCHARACTRS", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                              volume.Rename (FilePath::Parse ("NOPE"), "FINE", renamed));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED),
                              volume.Rename (FilePath::Parse ("SUBDIR"), "OTHER", renamed));
        }

        Assert::IsTrue (renamed.empty(), L"a refusal produces nothing");
    }



    TEST_METHOD (ProDos_RenameRefusesAFileWithoutRenameEnable)
    {
        vector<Byte>  sectors;
        vector<Byte>  renamed;
        size_t        entry = 0;

        LoadSectors (kProDosFixture, sectors);

        entry = FindProDosEntry (sectors, "HELLO");
        sectors[entry + kProDosEntOffAccess] =
            (Byte) (sectors[entry + kProDosEntOffAccess] & ~ProDosVolume::kAccessRenameEnable);

        {
            ProDosVolume  volume (sectors);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
                              volume.Rename (FilePath::Parse ("HELLO"), "GREETING", renamed));
        }
    }
};
