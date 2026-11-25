#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "CassoExplorer/Browser/BrowserSearchMatches.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatchesTests
//
//  A search's match inside a disk image found again in its image's catalog,
//  the matches among the selected rows, and whether the selection is matches.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserSearchMatchesTests)
{
public:

    static constexpr const char *     kImage        = "C:\\Disks\\vol.po";
    static constexpr const wchar_t *  kImageWide    = L"C:\\Disks\\vol.po";
    static constexpr size_t           kBlockBytes   = 512;
    static constexpr size_t           kVolumeBlocks = 1600;


    struct Rig
    {
        BrowserPartsFixture   fixture;
        BrowserSearchMatches  matches { fixture.parts };

        //  A volume with SEED in its root and GAMES/HELLO in a directory.
        Rig()
        {
            std::vector<Byte>  buffer (kVolumeBlocks * kBlockBytes, 0);
            std::vector<Byte>  next;
            FilePayload        payload;

            AssertSucceeded (ProDosSkeleton::Write (buffer, "BLOCKS"));

            payload.bytes.assign (300, 0x5A);
            payload.type           = ProDosVolume::kTypeBinary;
            payload.loadAddress    = 0x2000;
            payload.hasLoadAddress = true;
            AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("SEED"), payload, next));
            buffer = next;

            AssertSucceeded (ProDosVolume (buffer).CreateDirectory (FilePath::Parse ("GAMES"), next));
            buffer = next;

            AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("GAMES/HELLO"), payload, next));

            fixture.Seed (kImage, next);
        }

        //  The row a search lists for a file of the volume, by its catalog index.
        CatalogRow  MakeRow (size_t catalogIndex, const std::string & directory)
        {
            CatalogRow  row;

            row.imagePath         = kImageWide;
            row.imageCatalogIndex = catalogIndex;
            row.imageDirectory    = directory;

            return row;
        }

        //  The catalog index of the file in a directory of the volume.
        size_t  GetCatalogIndex (const std::string & directory, const std::string & name)
        {
            VolumeListing  listing;
            VolumeKind     kind = VolumeKind::Unknown;

            AssertSucceeded (fixture.operations.List (kImage, directory, listing, kind).hr);

            for (const FileEntry & entry : listing.entries)
            {
                if (entry.name == name)
                {
                    return entry.catalogIndex;
                }
            }

            Assert::Fail (L"The file is not in the volume");
            return 0;
        }
    };


    TEST_METHOD (RowWithNoImage_IsNotAMatch)
    {
        Rig                 rig;
        CatalogRow          row;
        BrowserSearchMatch  match;
        std::wstring        error;

        Assert::IsFalse (rig.matches.TryFindSearchMatch (row, match, error));
        Assert::IsTrue  (error.empty());
    }


    TEST_METHOD (FileInTheRoot_IsFoundByItsCatalogIndex)
    {
        Rig                 rig;
        BrowserSearchMatch  match;
        std::wstring        error;
        CatalogRow          row   = rig.MakeRow (rig.GetCatalogIndex (std::string(), "SEED"), std::string());

        Assert::IsTrue   (rig.matches.TryFindSearchMatch (row, match, error));
        Assert::AreEqual (std::string ("SEED"), match.entry.name);
        Assert::IsTrue   (match.kind == VolumeKind::ProDos);
        Assert::IsTrue   (match.directory.empty());
        Assert::AreEqual (std::wstring (kImageWide), match.imagePath);
    }


    TEST_METHOD (FileInADirectory_IsFoundInThatDirectory)
    {
        Rig                 rig;
        BrowserSearchMatch  match;
        std::wstring        error;
        CatalogRow          row   = rig.MakeRow (rig.GetCatalogIndex ("GAMES", "HELLO"), "GAMES");

        Assert::IsTrue   (rig.matches.TryFindSearchMatch (row, match, error));
        Assert::AreEqual (std::string ("HELLO"), match.entry.name);
        Assert::AreEqual (std::string ("GAMES"), match.directory);
    }


    TEST_METHOD (FileNoLongerInTheImage_IsReportedWithWhy)
    {
        Rig                 rig;
        BrowserSearchMatch  match;
        std::wstring        error;
        CatalogRow          row   = rig.MakeRow (9999, std::string());

        Assert::IsFalse  (rig.matches.TryFindSearchMatch (row, match, error));
        Assert::AreEqual (std::wstring (L"This file is no longer in its disk image."), error);
    }


    TEST_METHOD (ImageThatCannotBeRead_IsReportedWithWhy)
    {
        Rig                 rig;
        BrowserSearchMatch  match;
        std::wstring        error;
        CatalogRow          row;

        row.imagePath = L"C:\\Disks\\gone.po";

        Assert::IsFalse (rig.matches.TryFindSearchMatch (row, match, error));
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (RootCatalog_IsCached_ADirectoryIsNot)
    {
        Rig                 rig;
        VolumeListing       listing;
        VolumeKind          kind  = VolumeKind::Unknown;
        BrowserSearchMatch  match;
        std::wstring        error;

        Assert::IsTrue (rig.matches.TryFindSearchMatch (rig.MakeRow (rig.GetCatalogIndex ("GAMES", "HELLO"), "GAMES"), match, error));
        Assert::IsFalse (rig.fixture.model.TryGetCachedCatalog (kImageWide, listing, kind), L"A directory's catalog is read each time");

        Assert::IsTrue (rig.matches.TryFindSearchMatch (rig.MakeRow (rig.GetCatalogIndex (std::string(), "SEED"), std::string()), match, error));
        Assert::IsTrue (rig.fixture.model.TryGetCachedCatalog (kImageWide, listing, kind), L"The catalog of the volume itself is cached");
    }


    TEST_METHOD (SelectedMatches_SkipRowsOutOfRangeAndRowsThatAreNotMatches)
    {
        Rig                              rig;
        std::vector<BrowserSearchMatch>  found;
        CatalogRow                       plain;

        plain.name = L"readme.txt";

        rig.fixture.list.rows         = { rig.MakeRow (rig.GetCatalogIndex (std::string(), "SEED"), std::string()), plain };
        rig.fixture.list.selectedRows = { 0, 1, 7, -1 };

        rig.matches.GetSelectedSearchMatches (found);

        Assert::AreEqual ((size_t) 1, found.size());
        Assert::AreEqual (std::string ("SEED"), found[0].entry.name);
    }


    TEST_METHOD (SelectionIsMatches_WhenAnySelectedRowHasAnImage)
    {
        Rig         rig;
        CatalogRow  plain;

        plain.name = L"readme.txt";

        rig.fixture.list.rows = { plain, rig.MakeRow (0, std::string()) };

        rig.fixture.list.selectedRows = { 0, 5 };
        Assert::IsFalse (rig.matches.AreSelectedRowsSearchMatches(), L"A plain row and a row out of range");

        rig.fixture.list.selectedRows = { 0, 1 };
        Assert::IsTrue (rig.matches.AreSelectedRowsSearchMatches());
    }
};
