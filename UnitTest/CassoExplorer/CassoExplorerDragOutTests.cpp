#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerDragOut.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOutTests
//
//  The bytes a drag out of the list offers: the descriptor block and file
//  drop layouts, the conversion each descriptor asks for, and the formats a
//  selection in the scratch image produces, with contents read on request.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerDragOutTests)
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

        bytes = CassoExplorerDragOut::MakeFileGroupDescriptor (descriptors);
        group = (const FILEGROUPDESCRIPTORW *) bytes.data();

        Assert::AreEqual ((UINT) 2, group->cItems);
        Assert::AreEqual (std::wstring (L"HELLO.Applesoft BASIC.txt"), std::wstring (group->fgd[0].cFileName));
        Assert::AreEqual ((DWORD) FILE_ATTRIBUTE_DIRECTORY, group->fgd[1].dwFileAttributes);
    }


    TEST_METHOD (HDrop_ListsThePathsDoubleTerminated)
    {
        std::vector<uint8_t>  bytes  = CassoExplorerDragOut::MakeHDrop ({ L"C:\\a.dsk", L"C:\\b.po" });
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
        Assert::IsTrue (CassoExplorerDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Basic);

        descriptor.relativePath = L"NOTES.Text.txt";
        Assert::IsTrue (CassoExplorerDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Text);

        descriptor.relativePath = L"ODD.Binary.$0803.bin";
        descriptor.converted    = false;
        Assert::IsTrue (CassoExplorerDragOut::GetEncoding (descriptor) == DiskOperations::Encoding::Verbatim);
    }


    //  A file's first data sector on a DOS 3.3 disk pointed at track 40, past
    //  the last track: the catalog reads, and the file does not. The image is
    //  in DOS order, 16 sectors of 256 bytes a track.
    static void PointPastTheDisk (std::vector<Byte> & image, const char * name)
    {
        constexpr size_t  kSector      = 256;
        constexpr size_t  kVtoc        = 17 * 16 * kSector;
        constexpr size_t  kFirstEntry  = 0x0B;
        constexpr size_t  kEntrySize   = 0x23;
        constexpr size_t  kFirstPair   = 0x0C;
        size_t            catalog      = ((size_t) image[kVtoc + 1] * 16 + image[kVtoc + 2]) * kSector;

        for (size_t entry = 0; entry < 7; entry++)
        {
            size_t  at    = catalog + kFirstEntry + entry * kEntrySize;
            bool    match = true;

            for (size_t c = 0; name[c] != '\0'; c++)
            {
                match = match && (image[at + 3 + c] & 0x7F) == (Byte) name[c];
            }

            if (match)
            {
                image[((size_t) image[at] * 16 + image[at + 1]) * kSector + kFirstPair] = 40;
                return;
            }
        }

        Assert::Fail (L"the file is not in the first catalog sector");
    }


    //  A file that cannot be read fails as a damaged file, which File
    //  Explorer reports in words, not as "Unspecified error", and the log
    //  has which file and why, for the message in Casso Explorer.
    TEST_METHOD (AnUnreadableFile_FailsAsADamagedFile_AndIsNamedInTheLog)
    {
        InMemoryFileSystem                       fs;
        FakeDiskFileIo                           io;
        CassoExplorerBrowser                     browser (fs, io);
        FixtureProvider                          fixtures;
        std::vector<Byte>                        image;
        std::vector<DxuiTreeNode>                roots;
        std::vector<uint8_t>                     contents;
        int                                      hello    = -1;
        std::vector<DxuiDragDropSource::Format>  formats;
        auto                                     log      = std::make_shared<CassoExplorerDragOut::RenderLog>();

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", image));
        PointPastTheDisk (image, "HELLO");
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
        formats = CassoExplorerDragOut::BuildFormats (browser, HostFileNaming::Style::Descriptive, log);

        Assert::IsTrue   (formats[1].render (0, contents) == HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));
        Assert::AreEqual (std::wstring (L"HELLO has a damaged sector chain"), log->GetFailure());
        Assert::IsFalse  (log->IsWhole());
    }


    TEST_METHOD (ImageSelection_OffersDescriptorsContentsAndCatalogEntries)
    {
        InMemoryFileSystem         fs;
        FakeDiskFileIo             io;
        CassoExplorerBrowser             browser (fs, io);
        FixtureProvider                          fixtures;
        std::vector<Byte>                        image;
        std::vector<DxuiTreeNode>                roots;
        std::vector<uint8_t>                     contents;
        int                                      hello    = -1;
        std::string                              text;
        std::vector<DxuiDragDropSource::Format>  formats;
        auto                                     log      = std::make_shared<CassoExplorerDragOut::RenderLog>();

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", image));
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

        formats = CassoExplorerDragOut::BuildFormats (browser, HostFileNaming::Style::Descriptive, log);

        Assert::AreEqual ((size_t) 3, formats.size());
        Assert::AreEqual (1, formats[1].count);
        Assert::IsFalse  (log->IsWhole(), L"a file the target has not read is not whole, so a move deletes nothing");

        AssertSucceeded (formats[1].render (0, contents));
        Assert::IsTrue (log->IsWhole(), L"and once read from a sound disk it is");

        text.assign (contents.begin(), contents.end());

        Assert::AreNotEqual (std::string::npos, text.find ("PRINT"));
    }


    //  A directory has nothing to read, and a file is whole only after its
    //  last read was.
    TEST_METHOD (RenderLog_IsWholeOnlyWhenEveryFileReadWhole)
    {
        CassoExplorerDragOut::RenderLog       log;
        std::vector<DragPayload::Descriptor>  descriptors (2);

        descriptors[0].isDirectory = true;

        log.Reset (descriptors);
        Assert::IsFalse (log.IsWhole());

        log.SetWhole (1, true);
        Assert::IsTrue  (log.IsWhole());

        log.SetWhole (1, false);
        Assert::IsFalse (log.IsWhole(), L"a later damaged read of the same file");
    }
};
