#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Picker/PickerScope.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScopeTests
//
//  The rows a picker lists: folders always, files by the active filter
//  entry's masks, and, for a drive, by the drive rule as well. Then the
//  same scope set on a browser over an in-memory host, and the list of
//  commands a picker has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PickerScopeTests)
{
public:

    static constexpr uint64_t  k140K = 143360;
    static constexpr uint64_t  k800K = 819200;



    static CatalogRow MakeFile (const wchar_t * name, ImageFormFactor formFactor, ImageLayout layout, uint64_t bytes)
    {
        CatalogRow  row;



        row.name                      = name;
        row.imageClass.status         = (formFactor == ImageFormFactor::None) ? ImageStatus::Unclassified : ImageStatus::Classified;
        row.imageClass.formFactor     = formFactor;
        row.imageClass.layout         = layout;
        row.imageClass.formattedBytes = bytes;
        row.imageClass.blockCount     = (uint32_t) (bytes / 512);

        return row;
    }



    static CatalogRow MakeFolder (const wchar_t * name)
    {
        CatalogRow  row;



        row.name        = name;
        row.isDirectory = true;

        return row;
    }



    static PickerRequest MakeRequest (std::initializer_list<FileDialogFilter> filters, std::optional<ReceivingDrive> drive = std::nullopt)
    {
        PickerRequest  request;



        request.filters        = filters;
        request.receivingDrive = drive;

        return request;
    }



    TEST_METHOD (FoldersAlwaysPass)
    {
        PickerScope  scope (MakeRequest ({ { L"ProDOS images", L"*.po" } }, ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }));



        Assert::IsTrue (scope.Passes (MakeFolder (L"Games")));
        Assert::IsTrue (scope.Passes (MakeFolder (L"Not an image.txt")));
    }



    //  A stock disk is in the list for the drive already; the filter applies
    //  to it by its file's name, whatever its label.
    TEST_METHOD (StockDisk_FollowsTheActiveFilterByItsFileName)
    {
        PickerScope  scope (MakeRequest ({ { L"ProDOS images", L"*.po" }, { L"All files", L"*.*" } }, ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }));
        CatalogRow   po;
        CatalogRow   dsk;



        po.name         = L"ProDOS 2.4.3";
        po.hostPath     = L"C:\\Disks\\ProDOS 2.4.3.po";
        po.isStockDisk  = true;
        dsk.name        = L"DOS 3.3 System Master";
        dsk.hostPath    = L"C:\\Disks\\DOS 3.3 System Master.dsk";
        dsk.isStockDisk = true;

        Assert::IsTrue  (scope.Passes (po));
        Assert::IsFalse (scope.Passes (dsk));

        scope.SetActiveFilter (1);
        Assert::IsTrue  (scope.Passes (dsk), L"not downloaded, so it has no class for the drive rule");
    }



    TEST_METHOD (OnlyTheActiveFilterApplies)
    {
        PickerScope  scope (MakeRequest ({ { L"ProDOS images", L"*.po;*.2mg" }, { L"All images", L"*.po;*.2mg;*.dsk;*.woz" } }));
        CatalogRow   po  = MakeFile (L"a.po",  ImageFormFactor::Floppy525, ImageLayout::Sector, k140K);
        CatalogRow   dsk = MakeFile (L"b.dsk", ImageFormFactor::Floppy525, ImageLayout::Sector, k140K);



        Assert::AreEqual ((size_t) 2, scope.GetFilterCount());
        Assert::AreEqual ((size_t) 0, scope.GetActiveFilter(), L"the first entry is active");
        Assert::IsTrue  (scope.Passes (po));
        Assert::IsFalse (scope.Passes (dsk));

        scope.SetActiveFilter (1);
        Assert::IsTrue (scope.Passes (po));
        Assert::IsTrue (scope.Passes (dsk));

        scope.SetActiveFilter (7);
        Assert::AreEqual ((size_t) 1, scope.GetActiveFilter(), L"an index past the entries changes nothing");
    }



    TEST_METHOD (DriveRuleAppliesWithTheMasks)
    {
        PickerScope  scope (MakeRequest ({ { L"ProDOS images", L"*.po" } }, ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }));



        Assert::IsTrue  (scope.Passes (MakeFile (L"small.po", ImageFormFactor::Floppy525, ImageLayout::Sector, k140K)));
        Assert::IsFalse (scope.Passes (MakeFile (L"big.po",   ImageFormFactor::Floppy35,  ImageLayout::Raw,    k800K)), L"an 800K image in a Disk II");
        Assert::IsFalse (scope.Passes (MakeFile (L"small.dsk", ImageFormFactor::Floppy525, ImageLayout::Sector, k140K)), L"the mask leaves it out");
    }



    TEST_METHOD (AllFilesWithADriveListsOnlyItsImages)
    {
        PickerScope  scope (MakeRequest ({ { L"All files", L"*.*" } }, ReceivingDrive { ReceivingDriveKind::Apple35, 0 }));



        Assert::IsTrue  (scope.Passes (MakeFile (L"big.po",    ImageFormFactor::Floppy35,  ImageLayout::Raw,    k800K)));
        Assert::IsFalse (scope.Passes (MakeFile (L"small.dsk", ImageFormFactor::Floppy525, ImageLayout::Sector, k140K)));
        Assert::IsFalse (scope.Passes (MakeFile (L"notes.txt", ImageFormFactor::None,      ImageLayout::None,   0)));
    }



    TEST_METHOD (UnclassifiedImagePassesOnlyWithoutADrive)
    {
        CatalogRow   damaged = MakeFile (L"damaged.dsk", ImageFormFactor::None, ImageLayout::None, 0);
        PickerScope  byName  (MakeRequest ({ { L"Disk images", L"*.dsk" } }));
        PickerScope  byDrive (MakeRequest ({ { L"Disk images", L"*.dsk" } }, ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }));



        Assert::IsTrue  (byName.Passes (damaged));
        Assert::IsFalse (byDrive.Passes (damaged));
    }



    TEST_METHOD (NoFiltersListsEveryFile)
    {
        PickerScope  scope (MakeRequest ({}));



        Assert::IsTrue (scope.Passes (MakeFile (L"notes.txt", ImageFormFactor::None, ImageLayout::None, 0)));
    }



    TEST_METHOD (BrowserListsOnlyRowsInScope)
    {
        InMemoryFileSystem         fs;
        FakeDiskFileIo             io;
        CassoExplorerBrowser       browser { fs, io };
        PickerScope                scope (MakeRequest ({ { L"All files", L"*.*" } }, ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }));
        std::vector<DxuiTreeNode>  roots;
        std::wstring               names;



        for (auto [path, bytes] : { std::pair<const wchar_t *, uint64_t> { L"C:\\Disks\\a.dsk", k140K }, { L"C:\\Disks\\b.po", k800K }, { L"C:\\Disks\\notes.txt", 10 }, { L"C:\\Disks\\Sub\\c.dsk", k140K } })
        {
            std::string  narrow = TextEncoding::WideToUtf8 (path);

            AssertSucceeded (fs.WriteAllText (path, std::string ((size_t) bytes, '\0')));
            io.files[narrow]  = std::vector<Byte> ((size_t) bytes, 0);
            io.stamps[narrow] = FileStamp { bytes, 100 };
        }

        browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks" });
        browser.GetTreeModel().SetDrives ({});
        browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), L"C:\\Disks") == 0; });
        browser.SetRowScope ([&scope] (const CatalogRow & row) { return scope.Passes (row); });
        browser.GetTreeRoots (roots);

        AssertSucceeded (browser.SelectTreeNode (browser.GetTreeChildren (roots[1].id)[0].id));

        for (const CatalogRow & row : browser.GetRows())
        {
            names += row.name + L";";
        }

        Assert::IsTrue (names.find (L"a.dsk;") != std::wstring::npos, names.c_str());
        Assert::IsTrue (names.find (L"Sub;")   != std::wstring::npos, names.c_str());
        Assert::IsTrue (names.find (L"b.po")   == std::wstring::npos, names.c_str());
        Assert::IsTrue (names.find (L"notes")  == std::wstring::npos, names.c_str());
        Assert::AreEqual ((size_t) 2, browser.GetScopedOutCount(), L"the 800K image and the text file");

        //  The Casso node's list, by the same scope.
        AssertSucceeded (browser.SelectTreeNode (TreeModel::kCassoRootId));
        Assert::AreEqual ((size_t) 1, browser.GetRows().size());
        Assert::AreEqual (std::wstring (L"a.dsk"), browser.GetRows()[0].name);
    }



    TEST_METHOD (PickerHasOnlyItsListedCommands)
    {
        using C = CassoExplorerCommands;

        std::vector<int>  ids     = C::GetAllIds();
        size_t            allowed = 0;



        Assert::IsTrue (ids.size() > 100, L"the sweep has the table's commands");

        for (int id : { (int) C::kCutItems, (int) C::kCopyItems, (int) C::kPasteItems, (int) C::kRenameItem, (int) C::kDeleteItems, (int) C::kUndo,
                        (int) C::kNewDisk, (int) C::kNewTab, (int) C::kOptions, (int) C::kInspectDiskImage, (int) C::kCompareDiskImages,
                        (int) C::kReadDiskToImage, (int) C::kWriteImageToDisk, (int) C::kAddLink, (int) C::kExit, (int) C::kCheckForUpdates,
                        (int) C::kShareItems, (int) C::kExtractAll, (int) C::kEmptyRecycleBin, (int) C::kRestoreItems, (int) C::kFullSendStatus,
                        (int) C::kInitializeCard, (int) C::kCopy, (int) C::kThemeDark })
        {
            Assert::IsFalse (C::IsAllowedInPicker (id, false), std::format (L"command {}", id).c_str());
            Assert::IsFalse (C::IsAllowedInPicker (id, true),  std::format (L"command {} in a save form", id).c_str());
        }

        for (int id : { (int) C::kBack, (int) C::kForward, (int) C::kUp, (int) C::kRefresh, (int) C::kSort, (int) C::kView, (int) C::kTogglePreview,
                        (int) C::kSortByColumn + 3, (int) C::kViewFirst + 0, (int) C::kGroupByField + 2 })
        {
            Assert::IsTrue (C::IsAllowedInPicker (id, false), std::format (L"command {}", id).c_str());
        }

        Assert::IsFalse (C::IsAllowedInPicker (C::kNewFolder, false), L"New folder, opening a file");
        Assert::IsTrue  (C::IsAllowedInPicker (C::kNewFolder, true),  L"New folder, saving one");

        for (int id : ids)
        {
            allowed += C::IsAllowedInPicker (id, false) ? 1 : 0;
        }

        Assert::IsTrue (allowed > 0 && allowed * 3 < ids.size(), L"most of the table is left out of a picker");
    }
};
