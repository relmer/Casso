#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/ImageVolumeFacts.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFactsTests
//
//  The file system and volume name of an image, read on the worker's thread
//  and kept by path, length and time. Each wait for the worker has a
//  deadline, so a worker that never reports fails the test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageVolumeFactsTests)
{
public:

    static constexpr auto  kDeadline = std::chrono::seconds (10);



    //  Counts the worker's reports, for a test to wait on.
    struct Reports
    {
        std::mutex               lock;
        std::condition_variable  arrived;
        int                      count = 0;

        void Add()
        {
            {
                std::lock_guard<std::mutex>  guard (lock);

                count++;
            }

            arrived.notify_all();
        }

        bool WaitFor (int wanted)
        {
            std::unique_lock<std::mutex>  guard (lock);



            return arrived.wait_for (guard, kDeadline, [this, wanted] () { return count >= wanted; });
        }
    };



    static void Seed (FakeDiskFileIo & io, InMemoryFileSystem * fs, const char * fixture, const std::string & path)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;



        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };

        if (fs != nullptr)
        {
            AssertSucceeded (fs->WriteAllText (TextEncoding::Utf8ToWide (path), std::string (bytes.begin(), bytes.end())));
        }
    }



    TEST_METHOD (ReadsTheFileSystemAndVolumeName)
    {
        FakeDiskFileIo  io;
        VolumeFacts     dos;
        VolumeFacts     proDos;



        Seed (io, nullptr, "CassoExplorer/dos33.dsk", "C:\\Disks\\dos33.dsk");
        Seed (io, nullptr, "CassoExplorer/prodos.po", "C:\\Disks\\prodos.po");

        dos    = ImageVolumeFacts::Read (io, "C:\\Disks\\dos33.dsk");
        proDos = ImageVolumeFacts::Read (io, "C:\\Disks\\prodos.po");

        Assert::IsFalse (dos.fileSystem.empty(),    L"a DOS 3.3 disk has a file system");
        Assert::IsFalse (proDos.fileSystem.empty(), L"a ProDOS disk has a file system");
        Assert::IsFalse (proDos.volumeName.empty(), L"a ProDOS disk has a volume name");
        Assert::AreNotEqual (dos.fileSystem, proDos.fileSystem);
    }



    TEST_METHOD (UnreadableImageHasNoFacts)
    {
        FakeDiskFileIo  io;



        Assert::IsTrue (ImageVolumeFacts::Read (io, "C:\\Disks\\missing.dsk") == VolumeFacts());
    }



    TEST_METHOD (WorkerReadsEachQueuedImageOnce)
    {
        FakeDiskFileIo     io;
        Reports            reports;
        std::atomic<int>   reads { 0 };
        ImageVolumeFacts   facts (io, [&reports] () { reports.Add(); },
                                  [&reads] (const std::string & path) { reads++; return VolumeFacts { L"ProDOS", TextEncoding::Utf8ToWide (path) }; });
        VolumeFacts        found;



        facts.Queue ({ { L"C:\\a.po", 10, 1 }, { L"C:\\b.po", 20, 1 } });
        Assert::IsTrue (reports.WaitFor (2), L"both images reported");

        Assert::IsTrue   (facts.TryGet (L"C:\\a.po", 10, 1, found));
        Assert::AreEqual (std::wstring (L"C:\\a.po"), found.volumeName);
        Assert::IsFalse  (facts.TryGet (L"C:\\a.po", 11, 1, found), L"a changed length is read again");

        facts.Queue ({ { L"C:\\a.po", 10, 1 }, { L"C:\\c.po", 30, 1 } });
        Assert::IsTrue (reports.WaitFor (3), L"the new image reported");
        Assert::AreEqual (3, reads.load(), L"a.po was not read a second time");
    }



    TEST_METHOD (FolderListingFillsTheColumnsInTheBackground)
    {
        InMemoryFileSystem         fs;
        FakeDiskFileIo             io;
        Reports                    reports;
        std::vector<DxuiTreeNode>  roots;
        bool                       filled = false;
        int                        tries  = 0;



        Seed (io, &fs, "CassoExplorer/prodos.po", "C:\\Disks\\prodos.po");

        {
            CassoExplorerBrowser  browser { fs, io };



            browser.SetVolumeFactsNotify ([&reports] () { reports.Add(); });
            browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks" });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), L"C:\\Disks") == 0; });
            browser.GetTreeRoots (roots);

            AssertSucceeded (browser.SelectTreeNode (browser.GetTreeChildren (roots[1].id)[0].id));
            Assert::IsTrue (browser.GetRows()[0].collection.fileSystem.empty(), L"not filled while listing");

            Assert::IsTrue (reports.WaitFor (1), L"the image was read");

            for (tries = 0; tries < 3 && !filled; tries++)
            {
                filled = browser.ApplyVolumeFacts();
            }

            Assert::IsTrue  (filled, L"a row changed");
            Assert::IsFalse (browser.GetRows()[0].collection.fileSystem.empty());
            Assert::IsFalse (browser.GetRows()[0].collection.volumeName.empty());
        }
    }
};
