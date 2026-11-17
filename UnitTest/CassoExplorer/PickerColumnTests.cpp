#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerColumnTests
//
//  The form factor, formatted size and last used columns: filled for the
//  images of an ordinary folder, their cell text, and their sort order.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PickerColumnTests)
{
public:

    using Column = CatalogModel::Column;

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";
    static constexpr uint64_t         k140K  = 143360;
    static constexpr uint64_t         k800K  = 819200;
    static constexpr uint64_t         k2M    = 4096 * 512;



    struct Host
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };

        Host()
        {
            Add (L"C:\\Disks\\a.dsk",   k140K);
            Add (L"C:\\Disks\\b.po",    k800K);
            Add (L"C:\\Disks\\c.hdv",   k2M);
            Add (L"C:\\Disks\\notes.txt", 10);

            browser.GetTreeModel().SetKnownFolders ({ kDisks });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0; });
        }

        void Add (const wchar_t * path, uint64_t bytes)
        {
            std::string  narrow = TextEncoding::WideToUtf8 (path);



            AssertSucceeded (fs.WriteAllText (path, std::string ((size_t) bytes, '\0')));

            io.files[narrow]  = std::vector<Byte> ((size_t) bytes, 0);
            io.stamps[narrow] = FileStamp { bytes, 100 };
        }

        void OpenDisks()
        {
            std::vector<DxuiTreeNode>  roots;



            browser.GetTreeRoots (roots);
            AssertSucceeded (browser.SelectTreeNode (browser.GetTreeChildren (roots[1].id)[0].id));
        }

        const CatalogRow & FindRow (const wchar_t * name)
        {
            for (const CatalogRow & row : browser.GetRows())
            {
                if (row.name == name)
                {
                    return row;
                }
            }

            Assert::Fail (name);

            return browser.GetRows()[0];
        }
    };



    static CatalogRow MakeRow (const wchar_t * name, ImageFormFactor formFactor, uint64_t bytes, bool isDirectory = false)
    {
        CatalogRow  row;



        row.name                      = name;
        row.isDirectory               = isDirectory;
        row.imageClass.status         = (formFactor == ImageFormFactor::None) ? ImageStatus::NotAnImage : ImageStatus::Classified;
        row.imageClass.formFactor     = formFactor;
        row.imageClass.formattedBytes = bytes;

        return row;
    }



    static std::wstring Names (const std::vector<CatalogRow> & rows)
    {
        std::wstring  names;



        for (const CatalogRow & row : rows)
        {
            names += row.name + L" ";
        }

        return names;
    }



    TEST_METHOD (FolderListingClassifiesItsImages)
    {
        Host  host;



        host.OpenDisks();

        Assert::AreEqual ((int) ImageFormFactor::Floppy525, (int) host.FindRow (L"a.dsk").imageClass.formFactor);
        Assert::AreEqual ((int) ImageFormFactor::Floppy35,  (int) host.FindRow (L"b.po").imageClass.formFactor);
        Assert::AreEqual ((int) ImageFormFactor::HardDisk,  (int) host.FindRow (L"c.hdv").imageClass.formFactor);
        Assert::AreEqual ((int) ImageStatus::NotAnImage,    (int) host.FindRow (L"notes.txt").imageClass.status);
    }



    TEST_METHOD (CellsHaveTheColumnText)
    {
        Host                              host;
        std::vector<DxuiListView::Cell>   image;
        std::vector<DxuiListView::Cell>   text;



        host.OpenDisks();

        image = BrowserCells::ToCells (host.FindRow (L"b.po"));
        text  = BrowserCells::ToCells (host.FindRow (L"notes.txt"));

        Assert::AreEqual (BrowserCells::GetColumns().size(), image.size(), L"a cell for every column");
        Assert::AreEqual (std::wstring (L"3.5-inch"),       image[(size_t) Column::FormFactor].text);
        Assert::AreEqual (std::wstring (L"800K"),           image[(size_t) Column::FormattedSize].text);
        Assert::AreEqual (std::wstring (L"Form factor"),    BrowserCells::GetColumns()[(size_t) Column::FormFactor].title);
        Assert::AreEqual (std::wstring (L"Formatted size"), BrowserCells::GetColumns()[(size_t) Column::FormattedSize].title);
        Assert::AreEqual (std::wstring(),                   text[(size_t) Column::FormFactor].text);
        Assert::AreEqual (std::wstring(),                   text[(size_t) Column::FormattedSize].text);
    }



    TEST_METHOD (ReturningToAFolderReadsNothing)
    {
        Host      host;
        uint64_t  afterFirst = 0;



        host.OpenDisks();
        afterFirst = host.io.bytesReadAt;
        Assert::IsTrue (afterFirst > 0, L"the images were read");

        host.OpenDisks();
        Assert::AreEqual (afterFirst, host.io.bytesReadAt, L"no image read twice");
    }



    TEST_METHOD (FormattedSizeSortsByCapacity)
    {
        std::vector<CatalogRow>  rows =
        {
            MakeRow (L"notes.txt", ImageFormFactor::None,      10),
            MakeRow (L"big.hdv",   ImageFormFactor::HardDisk,  32 * 1024 * 1024),
            MakeRow (L"a.dsk",     ImageFormFactor::Floppy525, k140K),
            MakeRow (L"Games",     ImageFormFactor::None,      0, true),
            MakeRow (L"b.po",      ImageFormFactor::Floppy35,  k800K),
            MakeRow (L"c.po",      ImageFormFactor::Floppy35,  409600),
        };



        CatalogModel::Sort (rows, Column::FormattedSize, false);

        Assert::AreEqual (std::wstring (L"Games a.dsk c.po b.po big.hdv notes.txt "), Names (rows));
    }



    TEST_METHOD (FormFactorSortsBySize)
    {
        std::vector<CatalogRow>  rows =
        {
            MakeRow (L"tape.wav",  ImageFormFactor::Tape,      0),
            MakeRow (L"big.hdv",   ImageFormFactor::HardDisk,  32 * 1024 * 1024),
            MakeRow (L"b.po",      ImageFormFactor::Floppy35,  k800K),
            MakeRow (L"a.dsk",     ImageFormFactor::Floppy525, k140K),
            MakeRow (L"notes.txt", ImageFormFactor::None,      10),
        };



        CatalogModel::Sort (rows, Column::FormFactor, false);

        Assert::AreEqual (std::wstring (L"a.dsk b.po big.hdv tape.wav notes.txt "), Names (rows));
    }
};
