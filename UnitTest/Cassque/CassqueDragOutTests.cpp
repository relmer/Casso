#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/CassqueDragOut.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueDragOutTests
//
//  The bytes a drag out of the list offers: the descriptor block and file
//  drop layouts, the conversion each descriptor asks for, and the formats a
//  selection in the scratch image produces, with contents read on request.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueDragOutTests)
{
public:

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";



    TEST_METHOD (FileGroupDescriptor_HoldsEachName)
    {
        std::vector<DragPayload::Descriptor>  descriptors (2);
        std::vector<uint8_t>                  bytes;
        const FILEGROUPDESCRIPTORW *          group = nullptr;

        descriptors[0].relativePath = L"HELLO.Applesoft BASIC.txt";
        descriptors[1].relativePath = L"SUB";
        descriptors[1].isDirectory  = true;

        bytes = CassqueDragOut::MakeFileGroupDescriptor (descriptors);
        group = (const FILEGROUPDESCRIPTORW *) bytes.data();

        Assert::AreEqual ((UINT) 2, group->cItems);
        Assert::AreEqual (std::wstring (L"HELLO.Applesoft BASIC.txt"), std::wstring (group->fgd[0].cFileName));
        Assert::AreEqual ((DWORD) FILE_ATTRIBUTE_DIRECTORY, group->fgd[1].dwFileAttributes);
    }


    TEST_METHOD (HDrop_ListsThePathsDoubleTerminated)
    {
        std::vector<uint8_t>  bytes  = CassqueDragOut::MakeHDrop ({ L"C:\\a.dsk", L"C:\\b.po" });
        const DROPFILES *     header = (const DROPFILES *) bytes.data();
        const wchar_t *       first  = (const wchar_t *) (bytes.data() + header->pFiles);
        const wchar_t *       second = first + wcslen (first) + 1;

        Assert::IsTrue   (header->fWide != FALSE);
        Assert::AreEqual (std::wstring (L"C:\\a.dsk"), std::wstring (first));
        Assert::AreEqual (std::wstring (L"C:\\b.po"),  std::wstring (second));
        Assert::AreEqual (L'\0', second[wcslen (second) + 1]);
    }


    TEST_METHOD (Encoding_FollowsTheDescriptiveName)
    {
        DragPayload::Descriptor  descriptor;

        descriptor.relativePath = L"HELLO.Applesoft BASIC.txt";
        descriptor.converted    = true;
        Assert::IsTrue (CassqueDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Basic);

        descriptor.relativePath = L"NOTES.Text.txt";
        Assert::IsTrue (CassqueDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Text);

        descriptor.relativePath = L"ODD.Binary.$0803.bin";
        descriptor.converted    = false;
        Assert::IsTrue (CassqueDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Verbatim);
    }


    TEST_METHOD (ImageSelection_OffersDescriptorsContentsAndCatalogEntries)
    {
        InMemoryFileSystem         fs;
        FakeDiskFileIo             io;
        CassqueBrowser             browser (fs, io);
        FixtureProvider                          fixtures;
        std::vector<Byte>                        image;
        std::vector<DxuiTreeNode>                roots;
        std::vector<uint8_t>                     contents;
        int                                      hello    = -1;
        std::string                              text;
        std::vector<DxuiDragDropSource::Format>  formats;

        AssertSucceeded (fixtures.OpenFixture ("Cassque/dos33.dsk", image));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\dos33.dsk", std::string (image.begin(), image.end())));
        io.files["C:\\Disks\\dos33.dsk"]  = image;
        io.stamps["C:\\Disks\\dos33.dsk"] = FileStamp { image.size(), 100 };

        browser.GetTreeModel().SetKnownFolders ({ kDisks });
        browser.GetTreeModel().SetDrives ({});
        browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring &) { return true; });
        browser.GetTreeRoots (roots);
        AssertSucceeded (browser.SelectTreeNode (browser.GetTreeChildren (roots[0].id)[0].id));
        Assert::IsTrue (browser.OpenRow (0));

        for (size_t i = 0; i < browser.GetRows().size(); i++)
        {
            if (browser.GetRows()[i].name == L"HELLO")
            {
                hello = (int) i;
            }
        }

        browser.SetSelectedRows ({ hello });

        formats = CassqueDragOut::BuildFormats (browser, HostFileNaming::Style::Descriptive);

        Assert::AreEqual ((size_t) 3, formats.size());
        Assert::AreEqual (1, formats[1].count);

        AssertSucceeded (formats[1].render (0, contents));

        text.assign (contents.begin(), contents.end());

        Assert::AreNotEqual (std::string::npos, text.find ("PRINT"));
    }
};
