#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeFolderWatcher.h"
#include "CassoExplorer/Model/FolderWatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatchTests
//
//  Which folders are watched as the browser moves around, and what a burst of
//  changes under one of them comes back as.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FolderWatchTests)
{
public:

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";
    static constexpr const wchar_t *  kGames = L"C:\\Games";
    static constexpr const wchar_t *  kDocs  = L"C:\\Docs";



    TEST_METHOD (WatchesWhatItIsGiven)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);

        watch.SetWatched ({ kDisks, kGames });

        Assert::AreEqual ((size_t) 2, watcher.watched.size());
        Assert::AreEqual ((size_t) 2, watch.GetWatched().size());
    }



    TEST_METHOD (AnUnchangedListCostsNothing)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);

        watch.SetWatched ({ kDisks, kGames });
        watcher.watched.clear();
        watcher.unwatched.clear();

        watch.SetWatched ({ kDisks, kGames });

        Assert::IsTrue (watcher.watched.empty(),   L"Nothing is watched a second time");
        Assert::IsTrue (watcher.unwatched.empty(), L"and nothing is dropped");
    }



    TEST_METHOD (OnlyTheDifferenceIsWatchedAndDropped)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);

        watch.SetWatched ({ kDisks, kGames });
        watcher.watched.clear();
        watcher.unwatched.clear();

        //  The tree closes one folder and opens another.
        watch.SetWatched ({ kDisks, kDocs });

        Assert::AreEqual ((size_t) 1, watcher.watched.size());
        Assert::AreEqual ((size_t) 1, watcher.unwatched.size());
        Assert::AreEqual ((size_t) 2, watch.GetWatched().size());
    }



    TEST_METHOD (TheSameFolderSpeltDifferentlyIsOneWatch)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);

        watch.SetWatched ({ L"C:\\Disks", L"c:\\disks\\", L"C:\\DISKS" });

        Assert::AreEqual ((size_t) 1, watcher.watched.size());
    }



    TEST_METHOD (ABurstUnderOneFolderComesBackOnce)
    {
        FakeFolderWatcher          watcher;
        FolderWatch                watch (watcher);
        std::vector<std::wstring>  changed;
        int                        i = 0;

        watch.SetWatched ({ kDisks });

        for (i = 0; i < 100; i++)
        {
            watcher.Fire (watcher.watched[0]);
        }

        Assert::IsTrue   (watch.TakeChanged (changed));
        Assert::AreEqual ((size_t) 1, changed.size(), L"A hundred changes are one folder to re-read");
    }



    TEST_METHOD (TakingTheChangesClearsThem)
    {
        FakeFolderWatcher          watcher;
        FolderWatch                watch (watcher);
        std::vector<std::wstring>  changed;

        watch.SetWatched ({ kDisks });
        watcher.Fire (watcher.watched[0]);

        Assert::IsTrue  (watch.TakeChanged (changed));
        Assert::IsFalse (watch.TakeChanged (changed), L"and nothing is reported twice");
        Assert::IsTrue  (changed.empty());
    }



    TEST_METHOD (AChangeWakesTheOwner)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);
        int               woken = 0;

        watch.SetOnChanged ([&] () { woken++; });
        watch.SetWatched ({ kDisks });
        watcher.Fire (watcher.watched[0]);

        Assert::AreEqual (1, woken);
    }



    TEST_METHOD (AFolderThatCannotBeWatchedIsTriedAgain)
    {
        FakeFolderWatcher  watcher;
        FolderWatch       watch (watcher);

        watcher.failWatch = true;
        watch.SetWatched ({ kDisks });

        Assert::IsTrue (watch.GetWatched().empty(), L"An unwatchable folder is not recorded");

        //  The share comes back.
        watcher.failWatch = false;
        watch.SetWatched ({ kDisks });

        Assert::AreEqual ((size_t) 1, watch.GetWatched().size());
    }



    TEST_METHOD (EverythingIsDroppedOnTheWayOut)
    {
        FakeFolderWatcher  watcher;

        {
            FolderWatch  watch (watcher);

            watch.SetWatched ({ kDisks, kGames });
        }

        Assert::AreEqual ((size_t) 2, watcher.unwatched.size(), L"Both watches are given up");
        Assert::IsTrue   (watcher.callbacks.empty());
    }
};
