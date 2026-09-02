#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Win32ImageWatcher.h"
#include "Devices/Disk/DiskImageStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcherTests
//
//  The one part of change notification a fake cannot stand in for.
//
//  EVERYTHING ABOVE THE SEAM IS COVERED WITHOUT A FILESYSTEM, and that is the
//  point of the seam -- but it leaves the platform half completely unasserted,
//  which is precisely where a directory that is never actually watched looks
//  identical to one that is. These use a real temporary directory and a real
//  write.
//
//  THEY WAIT ON AN EVENT RATHER THAN ON A DURATION. The platform decides when
//  it reports, so a fixed sleep is either flaky or slow; a bounded wait on the
//  callback's own signal is neither.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Win32ImageWatcherTests)
{
public:

    //  Long enough that a machine under load still reports, short enough that a
    //  broken watcher fails the suite rather than hanging it.
    static constexpr int  kWaitMs = 5000;



    //
    //  A temporary directory that removes itself.
    //
    struct ScratchDirectory
    {
        fs::path  path;

        ScratchDirectory()
        {
            std::error_code  ec;

            path = fs::temp_directory_path (ec) / L"CassoWatcherTests";

            fs::remove_all      (path, ec);
            fs::create_directories (path, ec);
        }

        ~ScratchDirectory()
        {
            std::error_code  ec;

            fs::remove_all (path, ec);
        }
    };



    //  Writes bytes to a path the way both writers in this system do: into a
    //  sibling temporary, then renamed over the target.
    static void  CommitAtomically (const fs::path & target, Byte fill)
    {
        std::vector<Byte>  bytes (256, fill);

        AssertSucceeded (DiskImageStore::WriteFileAtomically (target.string(), bytes));
    }



    TEST_METHOD (AWriteUnderAWatchedDirectoryIsReported)
    {
        ScratchDirectory          scratch;
        Win32ImageWatcher         watcher;
        std::mutex                mutex;
        std::condition_variable   signal;
        std::vector<std::string>  seen;
        fs::path                  target = scratch.path / L"work.dsk";
        bool                      began  = false;



        CommitAtomically (target, 0x11);

        began = watcher.Watch (scratch.path.string(),
                               [&] (const std::string & path)
                               {
                                   std::lock_guard<std::mutex>  guard (mutex);

                                   seen.push_back (path);
                                   signal.notify_all();
                               });

        Assert::IsTrue (began, L"a real directory must be watchable");

        //  The case that matters: both writers commit by renaming a temporary
        //  over the image, so the record saying the image changed is a rename
        //  record and not a write one.
        CommitAtomically (target, 0x22);

        {
            std::unique_lock<std::mutex>  guard (mutex);

            signal.wait_for (guard, std::chrono::milliseconds (kWaitMs),
                             [&] { return !seen.empty(); });
        }

        Assert::IsTrue (!seen.empty(), L"the commit must be reported");

        {
            std::lock_guard<std::mutex>  guard (mutex);
            bool                         named = false;

            for (const std::string & path : seen)
            {
                named = named || MountedImageState::SamePath (path, target.string());
            }

            Assert::IsTrue (named, L"and the report must name the file that changed");
        }

        watcher.Unwatch (scratch.path.string());
    }



    TEST_METHOD (ADirectoryThatIsNotThereCannotBeWatched)
    {
        Win32ImageWatcher  watcher;
        bool               began = false;



        //  A state to degrade into rather than an error: the check made before
        //  every write is what carries the guarantee.
        began = watcher.Watch ("Z:\\no\\such\\place", [] (const std::string &) {});

        Assert::IsFalse (began);
    }



    TEST_METHOD (UnwatchingSomethingNeverWatchedIsHarmless)
    {
        Win32ImageWatcher  watcher;



        watcher.Unwatch ("Z:\\no\\such\\place");
    }



    TEST_METHOD (TheSecondWatchOfOneDirectoryIsTheSameWatch)
    {
        ScratchDirectory   scratch;
        Win32ImageWatcher  watcher;
        bool               first  = false;
        bool               second = false;



        first  = watcher.Watch (scratch.path.string(), [] (const std::string &) {});
        second = watcher.Watch (scratch.path.string(), [] (const std::string &) {});

        //  Two disks out of one folder is ordinary, and it is one watch.
        Assert::IsTrue (first);
        Assert::IsTrue (second);

        watcher.Unwatch (scratch.path.string());
    }



    TEST_METHOD (AWatcherStopsCleanlyWithAWatchStillOpen)
    {
        ScratchDirectory  scratch;



        //  The destructor has to stop every worker thread and join it. A leak
        //  here does not fail a test on its own -- it hangs the suite -- so it
        //  is asserted by arriving at the next line at all.
        {
            Win32ImageWatcher  watcher;

            Assert::IsTrue (watcher.Watch (scratch.path.string(),
                                           [] (const std::string &) {}));
        }
    }
};
