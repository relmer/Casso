#include "Pch.h"

#include "CassoExplorer/Model/HostFolderReader.h"
#include "FakeRecycleBin.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReaderTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostFolderReaderTests)
{
public:

    static FileSystemEntry MakeEntry (const wchar_t * name, bool isFolder)
    {
        FileSystemEntry  entry;

        entry.name     = name;
        entry.isFolder = isFolder;

        return entry;
    }



    TEST_METHOD (IsSupportedImage_TakesTheDiskToolsExtensions)
    {
        Assert::IsTrue  (HostFolderReader::IsSupportedImage (L"GAME.WOZ"));
        Assert::IsTrue  (HostFolderReader::IsSupportedImage (L"disk.po"));
        Assert::IsFalse (HostFolderReader::IsSupportedImage (L"notes.txt"));
        Assert::IsFalse (HostFolderReader::IsSupportedImage (L"noext"));
    }



    TEST_METHOD (Classify_SortsEntriesIntoFoldersImagesAndFiles)
    {
        HostFolderReader             reader;
        std::vector<HostFolderItem>  items;

        reader.Classify (L"C:\\Disks", { MakeEntry (L"Sub", true), MakeEntry (L"game.dsk", false), MakeEntry (L"readme.txt", false) }, items);

        Assert::AreEqual ((size_t) 3, items.size());
        Assert::IsTrue   (items[0].kind == HostFolderItem::Kind::Folder);
        Assert::IsTrue   (items[1].kind == HostFolderItem::Kind::Image);
        Assert::IsTrue   (items[2].kind == HostFolderItem::Kind::File);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.dsk"), items[1].path);
        Assert::AreEqual (std::wstring (L"game.dsk"), items[1].label);
    }



    TEST_METHOD (Classify_LeavesOutHiddenEntriesUnlessTheyAreShown)
    {
        HostFolderReader             reader;
        std::vector<HostFolderItem>  items;
        FileSystemEntry              hidden = MakeEntry (L"secret.dsk", false);
        FolderOptions                options;

        hidden.isHidden = true;

        reader.Classify (L"C:\\Disks", { hidden }, items);
        Assert::IsTrue (items.empty());

        options.showHidden = true;
        reader.SetFolderOptions (options);
        reader.Classify (L"C:\\Disks", { hidden }, items);
        Assert::AreEqual ((size_t) 1, items.size());
    }



    TEST_METHOD (Classify_LabelsASystemFolderByItsLocalizedName)
    {
        HostFolderReader             reader;
        std::vector<HostFolderItem>  items;
        FileSystemEntry              system = MakeEntry (L"Docs", true);
        FileSystemEntry              plain  = MakeEntry (L"Plain", true);

        system.isReadOnly = true;
        reader.SetLocalizedNameLookup ([] (const std::wstring & path) { return path == L"C:\\Docs" ? std::wstring (L"Documents") : std::wstring (L"Wrong"); });

        reader.Classify (L"C:\\", { system, plain }, items);

        Assert::AreEqual (std::wstring (L"Documents"), items[0].label);
        Assert::AreEqual (std::wstring (L"Plain"), items[1].label, L"only a read-only or system folder is looked up");
    }



    TEST_METHOD (Classify_FindsArchivesThroughTheShell)
    {
        HostFolderReader             reader;
        FakeRecycleBin               shell;
        std::vector<HostFolderItem>  items;

        shell.archiveExtension = L".zip";
        reader.SetShellVerbs (&shell);

        reader.Classify (L"C:\\Disks", { MakeEntry (L"games.zip", false), MakeEntry (L"game.dsk", false) }, items);

        Assert::IsTrue (items[0].kind == HostFolderItem::Kind::Archive);
        Assert::IsTrue (items[1].kind == HostFolderItem::Kind::Image, L"an image is never an archive");
    }
};
