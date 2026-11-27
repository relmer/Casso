#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "FakeRecycleBin.h"
#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"
#include "CassoExplorer/Browser/Preview/ShellItemPreviewer.h"
#include "CassoExplorer/Model/ArchiveProbe.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewerTests
//
//  The preview of a file or archive the shell lists: a disk image in a zip, a
//  file on the disk, a file read through the shell, and an archive's files.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellItemPreviewerTests)
{
public:

    //  The disk image previews called, as a list of calls.
    class FakeImagePreviews : public IImagePreviews
    {
    public:
        void  PreviewImageCatalog (const std::wstring & imagePath) override
        {
            catalogs.push_back (imagePath);
        }

        void  PreviewEntry (const std::wstring &, const std::string &, const FileEntry &, VolumeKind, std::span<const Byte> = {}, std::span<const std::vector<Byte>> = {}) override
        {
            entries++;
        }

        std::vector<std::wstring>  catalogs;
        int                        entries = 0;
    };


    //  The shell, with a listing and a read that can fail.
    class FakeShell : public FakeRecycleBin
    {
    public:
        HRESULT  ListShellFolder (const std::wstring &, std::vector<ShellFolderItem> & outItems) override
        {
            outItems = shellItems;

            return listResult;
        }

        HRESULT  ReadItemBytes (const std::wstring &, size_t maxBytes, std::vector<Byte> & outBytes) override
        {
            outBytes.assign (itemBytes.begin(), itemBytes.begin() + (std::min) (maxBytes, itemBytes.size()));

            return readResult;
        }

        HRESULT  listResult = S_OK;
        HRESULT  readResult = S_OK;
    };


    class FakeSelectionSource : public IBrowserSelectionSource
    {
    public:
        Location               GetLocation                  () const override  { return Location(); }
        bool                   IsHomeLocation               () const override  { return false; }
        bool                   IsImageLocation              () const override  { return false; }
        bool                   IsVolumeLocation             () const override  { return false; }
        const VolumeListing  & GetListing                   () const override  { return m_listing; }
        bool                   TryGetSelectedCassoLinkPaths (std::vector<std::wstring> &) const override  { return false; }
        void                   UpdatePreviewAndStatus       () override        {}

    private:
        VolumeListing  m_listing;
    };


    struct Rig
    {
        BrowserPartsFixture  fixture;
        FakeShell            shell;
        FakeImagePreviews    images;
        FakeSelectionSource  source;
        BrowserSelection     selection { fixture.model, fixture.list.rows, fixture.list.selectedRows, source };
        BrowserArchives      archives  { fixture.parts, selection };
        ShellItemPreviewer   previewer { fixture.parts, fixture.previewParts, archives, images };

        Rig()
        {
            fixture.sources.shellVerbs = &shell;
        }
    };


    static IShellItemVerbs::ShellFolderItem MakeItem (const wchar_t * name, bool isFolder)
    {
        IShellItemVerbs::ShellFolderItem  item;

        item.id       = name;
        item.name     = name;
        item.isFolder = isFolder;

        return item;
    }


    TEST_METHOD (Folder_PreviewsNothing)
    {
        Rig         rig;
        CatalogRow  row;

        row.isDirectory = true;
        row.hostPath    = L"C:\\Disks\\games";

        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent().kind);
        Assert::IsTrue (rig.images.catalogs.empty());
    }


    TEST_METHOD (DiskImageOnTheDisk_PreviewsAsItsCatalogByItsPath)
    {
        Rig         rig;
        CatalogRow  row;

        row.isDiskImage = true;
        row.hostPath    = L"C:\\Disks\\a.dsk";

        rig.previewer.PreviewShellItem (row);

        Assert::AreEqual ((size_t) 1, rig.images.catalogs.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), rig.images.catalogs[0]);
    }


    TEST_METHOD (DiskImageInAZip_PreviewsAsItsCatalogByItsPathThroughTheZip)
    {
        Rig         rig;
        CatalogRow  row;

        row.isDiskImage   = true;
        row.containerPath = L"C:\\Disks\\games.zip|a.dsk";

        rig.previewer.PreviewShellItem (row);

        Assert::AreEqual ((size_t) 1, rig.images.catalogs.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\games.zip|a.dsk"), rig.images.catalogs[0]);
    }


    TEST_METHOD (FileOnTheDisk_PreviewsAsHexFromTheFile)
    {
        Rig         rig;
        CatalogRow  row;

        row.name     = L"data.bin";
        row.hostPath = L"C:\\Disks\\data.bin";

        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue   (rig.fixture.preview.kind == PreviewContent::Kind::Hex);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\data.bin"), rig.fixture.preview.hostPath);
        Assert::IsTrue   (rig.images.catalogs.empty());
    }


    TEST_METHOD (FileWithNoShellId_PreviewsNothing)
    {
        Rig         rig;
        CatalogRow  row;

        row.name = L"inside.bin";

        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent().kind);
        Assert::IsTrue (rig.fixture.preview.bytes.empty());
    }


    TEST_METHOD (FileInsideAZip_IsReadThroughTheShellAndPreviewsAsHex)
    {
        Rig         rig;
        CatalogRow  row;

        row.name    = L"inside.bin";
        row.shellId = L"zip-item-1";

        rig.shell.itemBytes = { 0x01, 0x02, 0x03, 0x04 };

        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Hex);
        Assert::IsTrue (rig.fixture.preview.bytes == std::vector<Byte> ({ 0x01, 0x02, 0x03, 0x04 }));
        Assert::IsTrue (rig.fixture.preview.hostCharset == HostCharset::None, L"Control codes are no text");
        Assert::IsFalse (rig.fixture.preview.textFile);
    }


    TEST_METHOD (TextInsideAZip_IsHexWithItsCharset_UnlessItIsUtf8)
    {
        Rig         rig;
        CatalogRow  row;

        row.name    = L"notes.txt";
        row.shellId = L"zip-item-2";

        rig.shell.itemBytes = { 'h', 'i', '\r', '\n' };
        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Hex, L"Plain ASCII opens in the hex view");
        Assert::IsTrue (rig.fixture.preview.hostCharset == HostCharset::Ascii);
        Assert::IsTrue (rig.fixture.preview.textFile);

        rig.fixture.preview = PreviewContent();
        rig.shell.itemBytes = { 'c', 'a', 'f', 0xC3, 0xA9 };
        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Text, L"UTF-8 text is lines of text");
        Assert::IsTrue (rig.fixture.preview.lines == std::vector<std::wstring> ({ L"caf\u00E9" }));
    }


    TEST_METHOD (FileInsideAZipThatCannotBeRead_PreviewsAsAnError)
    {
        Rig         rig;
        CatalogRow  row;

        row.name    = L"inside.bin";
        row.shellId = L"zip-item-3";

        rig.shell.readResult = E_FAIL;
        rig.previewer.PreviewShellItem (row);

        Assert::IsTrue   (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::AreEqual (std::wstring (L"The file could not be read"), rig.fixture.preview.message);
    }


    TEST_METHOD (Archive_ListsFoldersFirstAndEachGroupInNaturalOrder)
    {
        Rig  rig;

        rig.fixture.SeedHostFile (L"C:\\Disks\\notes.zip", "zip");
        rig.shell.shellItems = { MakeItem (L"disk10.dsk", false), MakeItem (L"Zebra", true), MakeItem (L"disk2.dsk", false), MakeItem (L"alpha", true) };

        rig.previewer.PreviewArchive (L"C:\\Disks\\notes.zip");

        const PreviewContent &  preview = rig.fixture.preview;

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Catalog);
        Assert::IsTrue   (preview.archive);
        Assert::AreEqual ((size_t) 4, preview.rows.size());
        Assert::AreEqual (std::wstring (L"alpha"),      preview.rows[0].name);
        Assert::AreEqual (std::wstring (L"Zebra"),      preview.rows[1].name);
        Assert::AreEqual (std::wstring (L"disk2.dsk"),  preview.rows[2].name);
        Assert::AreEqual (std::wstring (L"disk10.dsk"), preview.rows[3].name);
        Assert::IsTrue   (preview.rows[0].isDirectory);
        Assert::AreEqual (std::wstring (L"alpha"), preview.rows[0].shellId);
    }


    TEST_METHOD (Archive_ThatTheShellCannotList_PreviewsTheReasonForIt)
    {
        Rig  rig;

        rig.fixture.SeedHostFile (L"C:\\Disks\\notes.zip", "not a zip at all");
        rig.shell.listResult = E_FAIL;

        rig.previewer.PreviewArchive (L"C:\\Disks\\notes.zip");

        Assert::IsTrue   (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::AreEqual (ArchiveProbe::GetReason (L"notes.zip", ArchiveProbe::Verdict::NotZip), rig.fixture.preview.message);
    }


    TEST_METHOD (Archive_ThatTheShellCannotListAndHasNoReason_PreviewsThatItCouldNotBeRead)
    {
        Rig          rig;
        std::string  emptyZip ("PK\x05\x06", 4);

        emptyZip.append (18, '\0');
        rig.fixture.SeedHostFile (L"C:\\Disks\\empty.zip", emptyZip);
        rig.shell.listResult = E_FAIL;

        rig.previewer.PreviewArchive (L"C:\\Disks\\empty.zip");

        Assert::IsTrue   (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::AreEqual (std::wstring (L"The archive could not be read"), rig.fixture.preview.message);
    }


    TEST_METHOD (Archive_ListedWithNothingInIt_PreviewsAsTheReasonItIsEmpty)
    {
        Rig  rig;

        rig.fixture.SeedHostFile (L"C:\\Disks\\notes.zip", "not a zip at all");

        rig.previewer.PreviewArchive (L"C:\\Disks\\notes.zip");

        Assert::IsTrue   (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::AreEqual (ArchiveProbe::GetReason (L"notes.zip", ArchiveProbe::Verdict::NotZip), rig.fixture.preview.message);
    }


    TEST_METHOD (Archive_WithNoShell_PreviewsNothing)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = nullptr;
        rig.previewer.PreviewArchive (L"C:\\Disks\\notes.zip");

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent().kind);
        Assert::IsTrue (rig.fixture.preview.rows.empty());
    }


    TEST_METHOD (ShowShellPreviewBytes_ChangesOnlyAShellPreviewToHex)
    {
        Rig  rig;

        rig.fixture.preview.kind = PreviewContent::Kind::Shell;
        rig.previewer.ShowShellPreviewBytes();
        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Hex);

        rig.fixture.preview.kind = PreviewContent::Kind::Picture;
        rig.previewer.ShowShellPreviewBytes();
        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Picture);
    }


    TEST_METHOD (ArchiveColumns_AreTheFirstColumnsOfTheListFittedToTheirContent)
    {
        std::vector<DxuiListView::Column>  columns = ShellItemPreviewer::GetArchivePreviewColumns();

        Assert::AreEqual (ShellItemPreviewer::kArchivePreviewColumns, columns.size());

        for (const DxuiListView::Column & column : columns)
        {
            Assert::AreEqual (0, column.widthDip);
            Assert::IsFalse  (column.stretch);
        }
    }


    TEST_METHOD (ArchiveCells_AreTheFirstCellsOfTheRow)
    {
        Rig         rig;
        CatalogRow  row;

        row.name    = L"inside.bin";
        row.shellId = L"zip-item-4";

        Assert::AreEqual (ShellItemPreviewer::kArchivePreviewColumns, rig.previewer.GetArchivePreviewCells (row).size());
    }
};
