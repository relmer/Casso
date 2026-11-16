#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoImagesListing.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/ImageClassCache.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoImagesListingTests
//
//  The Casso node's list: the images of its folders in one list, each with
//  its folder and its last load, over an in-memory host.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoImagesListingTests)
{
public:

    static constexpr const wchar_t *  kGames = L"C:\\Games";
    static constexpr const wchar_t *  kWork  = L"C:\\Work";
    static constexpr uint64_t         k140K  = 143360;
    static constexpr uint64_t         k800K  = 819200;



    struct Host
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        ImageClassCache     cache;

        void Add (const std::wstring & path, uint64_t bytes)
        {
            std::string  narrow = TextEncoding::WideToUtf8 (path);



            AssertSucceeded (fs.WriteAllText (path, std::string ((size_t) bytes, '\0')));

            io.files[narrow]  = std::vector<Byte> ((size_t) bytes, 0);
            io.stamps[narrow] = FileStamp { bytes, 100 };
        }

        Host()
        {
            Add (L"C:\\Games\\karateka.dsk", k140K);
            Add (L"C:\\Games\\readme.txt",   10);
            Add (L"C:\\Work\\system.po",     k800K);
            Add (L"C:\\Work\\tape.wav",      5000);
        }
    };



    static const CatalogRow * Find (const std::vector<CatalogRow> & rows, const wchar_t * name)
    {
        for (const CatalogRow & row : rows)
        {
            if (row.name == name)
            {
                return &row;
            }
        }

        return nullptr;
    }



    TEST_METHOD (ImagesOfEveryFolderInOneList)
    {
        Host                         host;
        std::vector<CatalogRow>      rows;
        std::vector<DiskMru::Entry>  recent = { { L"C:\\Games\\karateka.dsk", 200 } };



        CassoImagesListing::Build (host.fs, host.io, host.cache, { kGames, kWork }, recent, rows);

        Assert::AreEqual ((size_t) 3, rows.size(), L"two disks and a tape, and no text file");
        Assert::IsNull    (Find (rows, L"readme.txt"));
        Assert::IsNotNull (Find (rows, L"karateka.dsk"));
        Assert::AreEqual (std::wstring (kGames),                   Find (rows, L"karateka.dsk")->folderPath);
        Assert::AreEqual (std::wstring (L"C:\\Work\\system.po"),   Find (rows, L"system.po")->hostPath);
        Assert::AreEqual ((int64_t) 200,                           Find (rows, L"karateka.dsk")->lastUsedUnix);
        Assert::AreEqual ((int64_t) 0,                             Find (rows, L"system.po")->lastUsedUnix);
        Assert::AreEqual ((int) ImageFormFactor::Floppy35,         (int) Find (rows, L"system.po")->imageClass.formFactor);
        Assert::AreEqual ((int) ImageFormFactor::Tape,             (int) Find (rows, L"tape.wav")->imageClass.formFactor);
    }



    TEST_METHOD (MissingFolderIsSkipped)
    {
        Host                     host;
        std::vector<CatalogRow>  rows;



        CassoImagesListing::Build (host.fs, host.io, host.cache, { L"C:\\Gone", kGames }, {}, rows);

        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::IsNotNull (Find (rows, L"karateka.dsk"));
    }



    TEST_METHOD (OverlappingFoldersListAnImageOnce)
    {
        Host                     host;
        std::vector<CatalogRow>  rows;



        CassoImagesListing::Build (host.fs, host.io, host.cache, { kGames, L"c:\\games\\" }, {}, rows);

        Assert::AreEqual ((size_t) 1, rows.size());
    }



    TEST_METHOD (LastUsedSortsNewestFirstThenByName)
    {
        std::vector<CatalogRow>  rows(4);
        std::wstring             order;



        rows[0].name = L"zork.dsk";     rows[0].lastUsedUnix = 0;
        rows[1].name = L"choplifter";   rows[1].lastUsedUnix = 300;
        rows[2].name = L"aztec.dsk";    rows[2].lastUsedUnix = 0;
        rows[3].name = L"lode runner";  rows[3].lastUsedUnix = 500;

        CatalogModel::Sort (rows, CatalogModel::Column::LastUsed, true);

        for (const CatalogRow & row : rows)
        {
            order += row.name + L";";
        }

        Assert::AreEqual (std::wstring (L"lode runner;choplifter;aztec.dsk;zork.dsk;"), order);
    }



    TEST_METHOD (CassoNodeListsImagesNotFolders)
    {
        Host                       host;
        CassoExplorerBrowser       browser { host.fs, host.io };
        std::vector<DxuiTreeNode>  roots;



        browser.GetTreeModel().SetKnownFolders ({ kGames, kWork });
        browser.GetTreeModel().SetDrives ({});
        browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kGames) == 0 || _wcsicmp (path.c_str(), kWork) == 0; });
        browser.SetRecentLoads ([] () { return std::vector<DiskMru::Entry> { { L"C:\\Work\\system.po", 900 } }; });

        browser.GetTreeRoots (roots);
        AssertSucceeded (browser.SelectTreeNode (TreeModel::kCassoRootId));

        Assert::AreEqual ((size_t) 3, browser.GetRows().size(), L"the images, not the two folders");
        Assert::IsNotNull (Find (browser.GetRows(), L"system.po"));
        Assert::AreEqual ((int64_t) 900, Find (browser.GetRows(), L"system.po")->lastUsedUnix);
        Assert::AreEqual ((size_t) 2, browser.GetTreeChildren (TreeModel::kCassoRootId).size(), L"the tree still has the folders");
    }
};
