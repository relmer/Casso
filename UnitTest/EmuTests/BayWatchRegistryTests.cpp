#include "Pch.h"

#include "Devices/Disk/Store/BayWatchRegistry.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "FakeImageWatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BayWatchRegistryTests
//
//  Watch registration per bay, with a fake watcher and a fake that collects
//  each change passed to NoteExternalChange.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BayWatchRegistryTests)
{
public:

    struct FakeNotes : public IImageChangeNotes
    {
        vector<string>  paths;

        void NoteExternalChange (const string & path, ExternalChangeIntent, bool) override
        {
            paths.push_back (path);
        }
    };


    struct Rig
    {
        DiskStoreHooks    hooks;
        DiskBayTable      bays;
        FakeImageWatcher  watcher;
        FakeNotes         notes;
        BayWatchRegistry  watches { bays, hooks, notes };

        Rig()
        {
            hooks.watcher = &watcher;
        }

        void Mount (int slot, int drive, const string & path)
        {
            DiskBay  & bay = bays.GetBay (slot, drive);



            bay.mounted = true;
            bay.path    = path;
        }
    };


    TEST_METHOD (BeginWatching_WatchesTheImageFolder)
    {
        Rig  rig;



        rig.Mount (6, 0, "C:\\disks\\Work.dsk");
        rig.watches.BeginWatching (6, 0);

        Assert::AreEqual ((size_t) 1, rig.watcher.watched.size());
        Assert::AreEqual (string ("C:\\disks"), rig.watcher.watched[0]);
        Assert::IsTrue (rig.bays.GetBay (6, 0).sharedState.IsWatching());
    }


    TEST_METHOD (BeginWatching_ReportsChangesAsUnstated)
    {
        Rig  rig;



        rig.Mount (6, 0, "C:\\disks\\Work.dsk");
        rig.watches.BeginWatching (6, 0);
        rig.watcher.Fire ("C:\\disks", "C:\\disks\\Work.dsk");

        Assert::AreEqual ((size_t) 1, rig.notes.paths.size());
        Assert::AreEqual (string ("C:\\disks\\Work.dsk"), rig.notes.paths[0]);
    }


    TEST_METHOD (BeginWatching_RefusedWatch_LeavesTheBayUnwatched)
    {
        Rig  rig;



        rig.watcher.failWatch = true;
        rig.Mount (6, 0, "\\\\share\\disks\\Work.dsk");
        rig.watches.BeginWatching (6, 0);

        Assert::IsFalse (rig.bays.GetBay (6, 0).sharedState.IsWatching());
    }


    TEST_METHOD (EndWatching_KeepsAFolderAnotherBayUses)
    {
        Rig  rig;



        rig.Mount (6, 0, "C:\\disks\\Boot.dsk");
        rig.Mount (6, 1, "C:\\disks\\Work.dsk");
        rig.watches.BeginWatching (6, 0);
        rig.watches.BeginWatching (6, 1);

        rig.watches.EndWatching (6, 0);

        Assert::AreEqual ((size_t) 0, rig.watcher.unwatched.size(),
            L"ejecting one disk must not stop the watch the other disk needs");
        Assert::IsFalse (rig.bays.GetBay (6, 0).sharedState.IsWatching());
    }


    TEST_METHOD (EndWatching_DropsTheLastUseOfAFolder)
    {
        Rig  rig;



        rig.Mount (6, 0, "C:\\disks\\Boot.dsk");
        rig.watches.BeginWatching (6, 0);

        rig.watches.EndWatching (6, 0);

        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
        Assert::AreEqual (string ("C:\\disks"), rig.watcher.unwatched[0]);
    }


    TEST_METHOD (WithoutAWatcher_NothingIsWatched)
    {
        Rig  rig;



        rig.hooks.watcher = nullptr;
        rig.Mount (6, 0, "C:\\disks\\Work.dsk");
        rig.watches.BeginWatching (6, 0);

        Assert::IsFalse (rig.bays.GetBay (6, 0).sharedState.IsWatching());
    }
};
