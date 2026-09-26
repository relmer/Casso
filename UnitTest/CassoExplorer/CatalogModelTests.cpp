#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModelTests
//
//  Rows for every fixture entry, and the order each column puts them in.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CatalogModelTests)
{
public:

    using Column = CatalogModel::Column;



    static void LoadRows (const char * fixture, VolumeKind kind, std::vector<CatalogRow> & outRows)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        VolumeListing       listing;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, fixture, sectors, report));

        if (kind == VolumeKind::Dos33)
        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }
        else
        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
        }

        CatalogModel::FromListing (listing, kind, outRows);

        Assert::IsTrue (!outRows.empty(), L"the fixture must list");
    }



    static const CatalogRow & FindRow (const std::vector<CatalogRow> & rows, const wchar_t * name)
    {
        for (const CatalogRow & row : rows)
        {
            if (row.name == name)
            {
                return row;
            }
        }

        Assert::Fail (L"row not found");

        return rows[0];
    }



    TEST_METHOD (Dos33Rows_LetterTypesSectorSizesAndBinaryAddress)
    {
        std::vector<CatalogRow>  rows;

        LoadRows ("CassoExplorer/dos33.dsk", VolumeKind::Dos33, rows);

        Assert::AreEqual ((size_t) 7, rows.size());

        Assert::AreEqual (std::wstring (L"A"),  FindRow (rows, L"HELLO").typeText);
        Assert::AreEqual (std::wstring (L"T"),  FindRow (rows, L"NOTES").typeText);
        Assert::AreEqual (std::wstring (L"B"),  FindRow (rows, L"PICTURE").typeText);
        Assert::AreEqual (std::wstring (L"I"),  FindRow (rows, L"INTPROG").typeText);

        //  DOS 3.3 records sectors, not bytes: 34 sectors for the picture.
        Assert::AreEqual ((uint64_t) 34 * 256, FindRow (rows, L"PICTURE").sizeBytes);

        //  A binary's load address comes from its first two bytes, since the
        //  catalog does not record one; other types leave the column blank.
        Assert::IsFalse (FindRow (rows, L"PICTURE").addressText.empty());
        Assert::IsTrue  (FindRow (rows, L"HELLO").addressText.empty());
        Assert::IsFalse (FindRow (rows, L"PICTURE").hasModified);
        Assert::IsFalse (FindRow (rows, L"PICTURE").locked);
        Assert::IsFalse (FindRow (rows, L"PICTURE").isDirectory);
    }



    TEST_METHOD (ProDosRows_MnemonicsExactSizesAddressesDirectoriesAndDates)
    {
        std::vector<CatalogRow>  rows;

        LoadRows ("CassoExplorer/prodos.po", VolumeKind::ProDos, rows);

        Assert::AreEqual ((size_t) 7, rows.size());

        Assert::AreEqual (std::wstring (L"BAS"),   FindRow (rows, L"HELLO").typeText);
        Assert::AreEqual (std::wstring (L"TXT"),   FindRow (rows, L"NOTES").typeText);
        Assert::AreEqual (std::wstring (L"BIN"),   FindRow (rows, L"PICTURE").typeText);
        Assert::AreEqual (std::wstring (L"DIR"),   FindRow (rows, L"SUBDIR").typeText);

        Assert::AreEqual ((uint64_t) 8192,          FindRow (rows, L"PICTURE").sizeBytes);
        Assert::AreEqual (std::wstring (L"$2000"),  FindRow (rows, L"PICTURE").addressText);
        Assert::AreEqual (std::wstring (L"$0801"),  FindRow (rows, L"HELLO").addressText);
        Assert::IsTrue   (FindRow (rows, L"SUBDIR").isDirectory);
        Assert::IsTrue   (FindRow (rows, L"NOTES").hasModified);
        Assert::AreEqual ((int64_t) 461594040,      FindRow (rows, L"NOTES").modifiedUnix);
    }



    TEST_METHOD (HostRows_ExtensionTypeFolderAndDate)
    {
        FileSystemEntry  file;
        FileSystemEntry  folder;
        CatalogRow       fileRow;
        CatalogRow       folderRow;

        file.name         = L"game.dsk";
        file.sizeBytes    = 143360;
        file.modifiedUnix = 1700000000;

        folder.name     = L"Disks";
        folder.isFolder = true;

        fileRow   = CatalogModel::FromHostEntry (file, true, 3);
        folderRow = CatalogModel::FromHostEntry (folder, false, 4);

        Assert::AreEqual (std::wstring (L"DSK"),    fileRow.typeText);
        Assert::AreEqual ((uint64_t) 143360,        fileRow.sizeBytes);
        Assert::IsTrue   (fileRow.isDiskImage);
        Assert::IsTrue   (fileRow.hasModified);
        Assert::AreEqual ((size_t) 3,               fileRow.sourceIndex);

        Assert::AreEqual (std::wstring (L"Folder"), folderRow.typeText);
        Assert::IsTrue   (folderRow.isDirectory);
        Assert::IsFalse  (folderRow.hasModified);
    }



    TEST_METHOD (Sort_EveryColumnBothDirections_FoldersFirst)
    {
        std::vector<CatalogRow>  rows;
        size_t                   i    = 0;

        LoadRows ("CassoExplorer/prodos.po", VolumeKind::ProDos, rows);

        CatalogModel::Sort (rows, Column::Name, false);
        Assert::AreEqual (std::wstring (L"SUBDIR"), rows[0].name, L"the directory leads");
        Assert::AreEqual (std::wstring (L"DHIRES"), rows[1].name);
        Assert::AreEqual (std::wstring (L"PICTURE"), rows[6].name);

        CatalogModel::Sort (rows, Column::Name, true);
        Assert::AreEqual (std::wstring (L"SUBDIR"), rows[0].name, L"the directory still leads");
        Assert::AreEqual (std::wstring (L"PICTURE"), rows[1].name);
        Assert::AreEqual (std::wstring (L"DHIRES"), rows[6].name);

        CatalogModel::Sort (rows, Column::Size, false);
        for (i = 2; i < rows.size(); i++)
        {
            Assert::IsTrue (rows[i - 1].sizeBytes <= rows[i].sizeBytes, L"ascending size");
        }

        CatalogModel::Sort (rows, Column::Size, true);
        Assert::AreEqual (std::wstring (L"DHIRES"), rows[1].name, L"the largest file follows the directory");

        CatalogModel::Sort (rows, Column::Type, false);
        Assert::AreEqual (std::wstring (L"HELLO"), rows[1].name, L"BAS sorts before BIN");

        CatalogModel::Sort (rows, Column::Address, true);
        Assert::AreEqual (std::wstring (L"$2000"), rows[1].addressText);

        CatalogModel::Sort (rows, Column::Locked, false);
        CatalogModel::Sort (rows, Column::Modified, false);
        Assert::AreEqual ((size_t) 7, rows.size());
    }



    TEST_METHOD (Sort_DrivesGoByTheirLetter)
    {
        std::vector<CatalogRow>  rows (2);

        rows[0].name     = L"Alpha (D:)";
        rows[0].hostPath = L"D:\\";
        rows[1].name     = L"Zulu (C:)";
        rows[1].hostPath = L"C:\\";

        for (CatalogRow & row : rows)
        {
            row.isDirectory = true;
            row.isDrive     = true;
        }

        CatalogModel::Sort (rows, CatalogModel::Column::Name, false);

        Assert::AreEqual (std::wstring (L"Zulu (C:)"), rows[0].name);
    }


    TEST_METHOD (SourceIndex_SurvivesASort)
    {
        std::vector<CatalogRow>  rows;
        std::vector<CatalogRow>  original;

        LoadRows ("CassoExplorer/prodos.po", VolumeKind::ProDos, rows);

        original = rows;

        CatalogModel::Sort (rows, Column::Name, true);

        for (const CatalogRow & row : rows)
        {
            Assert::AreEqual (original[row.sourceIndex].name, row.name);
        }
    }



    //  A DOS 3.3 heading entry: "A", backspaces, then its text. Control
    //  characters are displayed in caret form and returned as muted ranges, one per run;
    //  a typed caret and letter, and every other character, pass through
    //  unmuted.
    TEST_METHOD (DisplayName_ShowsControlCharactersInCaretFormAndMutesThem)
    {
        std::wstring                      name = std::wstring (L"A") + (wchar_t) 0x08 + (wchar_t) 0x08 + L"^H" + (wchar_t) 0x7F;
        std::vector<std::pair<int, int>>  ranges;

        Assert::AreEqual (std::wstring (L"A^H^H^H^?"), CatalogModel::GetDisplayName (name, ranges));
        Assert::AreEqual ((size_t) 2, ranges.size());
        Assert::AreEqual (1, ranges[0].first);
        Assert::AreEqual (5, ranges[0].second, L"the two backspaces are one run");
        Assert::AreEqual (7, ranges[1].first,  L"the typed ^H between them is not muted");
        Assert::AreEqual (9, ranges[1].second);

        Assert::AreEqual (std::wstring (L"MERLIN.X"), CatalogModel::GetDisplayName (L"MERLIN.X", ranges));
        Assert::IsTrue   (ranges.empty());
    }
};
