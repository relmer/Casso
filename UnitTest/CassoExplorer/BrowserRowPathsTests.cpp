#include "Pch.h"

#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/TreeModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPathsTests
//
//  The location a row opens and the row's path, over rows built in memory
//  and a fake test of whether an image can be listed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserRowPathsTests)
{
public:

    struct Rig
    {
        std::vector<CatalogRow>  rows;
        std::vector<TreeNode>    rootChildren;

        BrowserRowPaths::Listing MakeListing (const Location & location, bool isVolume = false)
        {
            return BrowserRowPaths::Listing { location, rows, rootChildren, false, isVolume,
                                              [] (const std::wstring & path) { return path.ends_with (L".dsk"); } };
        }

        int AddRow (const wchar_t * name, bool isDirectory = false, bool isDiskImage = false)
        {
            CatalogRow  row;

            row.name        = name;
            row.isDirectory = isDirectory;
            row.isDiskImage = isDiskImage;
            row.sourceIndex = rows.size();

            rows.push_back (row);

            return (int) rows.size() - 1;
        }
    };



    TEST_METHOD (TryGetRowLocation_HostFolder_OpensFoldersAndListableImages)
    {
        Rig       rig;
        Location  target;
        int       folder = rig.AddRow (L"Sub", true);
        int       image  = rig.AddRow (L"game.dsk", false, true);
        int       file   = rig.AddRow (L"readme.txt");

        BrowserRowPaths::Listing  listing = rig.MakeListing (Location::MakeHostFolder (L"C:\\Disks"));



        Assert::IsTrue   (BrowserRowPaths::TryGetRowLocation (listing, folder, target));
        Assert::IsTrue   (target.kind == Location::Kind::HostFolder);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Sub"), target.path);

        Assert::IsTrue   (BrowserRowPaths::TryGetRowLocation (listing, image, target));
        Assert::IsTrue   (target.kind == Location::Kind::DiskImage);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.dsk"), target.path);

        Assert::IsFalse  (BrowserRowPaths::TryGetRowLocation (listing, file, target));
        Assert::IsFalse  (BrowserRowPaths::TryGetRowLocation (listing, -1, target));
    }



    TEST_METHOD (TryGetRowLocation_DirectoryInAnImage_OpensBelowTheOneListed)
    {
        Rig       rig;
        Location  target;
        int       directory = rig.AddRow (L"SUB", true);



        Assert::IsTrue   (BrowserRowPaths::TryGetRowLocation (rig.MakeListing (Location::MakeDiskDirectory (L"C:\\a.po", "TOP"), true), directory, target));
        Assert::IsTrue   (target.kind == Location::Kind::DiskDirectory);
        Assert::AreEqual (std::string ("TOP/SUB"), target.innerPath);
    }



    TEST_METHOD (TryGetRowPath_HostAndVolumeRows)
    {
        Rig           rig;
        std::wstring  path;
        int           row = rig.AddRow (L"readme.txt");



        Assert::IsTrue   (BrowserRowPaths::TryGetRowPath (rig.MakeListing (Location::MakeHostFolder (L"C:\\Disks")), row, path));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\readme.txt"), path);

        Assert::IsFalse  (BrowserRowPaths::TryGetRowPath (rig.MakeListing (Location::MakeHostFolder (L"C:\\Disks")), 5, path));
    }



    TEST_METHOD (GetEntryPath_PrefixesTheListedDirectory)
    {
        FileEntry  entry;



        entry.name = "HELLO";

        Assert::AreEqual (std::string ("HELLO"),     BrowserRowPaths::GetEntryPath (Location::MakeDiskImage (L"C:\\a.po"), entry));
        Assert::AreEqual (std::string ("SUB/HELLO"), BrowserRowPaths::GetEntryPath (Location::MakeDiskDirectory (L"C:\\a.po", "SUB"), entry));
    }



    TEST_METHOD (JoinPathAndParentFolder)
    {
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), BrowserRowPaths::JoinPath (L"C:\\Disks", L"a.dsk"));
        Assert::AreEqual (std::wstring (L"C:\\a.dsk"),        BrowserRowPaths::JoinPath (L"C:\\", L"a.dsk"));
        Assert::AreEqual (std::wstring (L"C:\\Disks"),        BrowserRowPaths::GetParentFolder (L"C:\\Disks\\Sub"));
        Assert::IsTrue   (BrowserRowPaths::GetParentFolder (L"C:\\").empty());
        Assert::IsTrue   (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\MultiPart.hdv|#1.po"));
        Assert::IsFalse  (BrowserRowPaths::IsPartitionVolume (L"C:\\walk\\Masters.zip|Master.dsk"));
    }
};
