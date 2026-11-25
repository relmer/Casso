#include "Pch.h"

#include "CassoExplorer/Model/FileSystemCheckScheduler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckSchedulerTests
//
//  The background file-system check of the selected image, on a fake clock
//  and an executor that queues jobs for the test to run.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileSystemCheckSchedulerTests)
{
public:

    struct Rig
    {
        int64_t                                   nowMs    = 1000;
        vector<FileSystemCheckScheduler::Job>     jobs;
        vector<FileSystemCheckKey>                checked;
        int                                       notified = 0;
        FileSystemCheckOutcome                    outcome  = { FileSystemVerdict::Bad, L"the catalog's file count differs from the VTOC's" };
        FileSystemCheckScheduler                  scheduler;

        Rig() :
            scheduler ([this] () { return nowMs; },
                       [this] (FileSystemCheckScheduler::Job job) { jobs.push_back (std::move (job)); },
                       [this] (const FileSystemCheckKey & key, const std::atomic<bool> &, FileSystemCheckOutcome & out)
                       {
                           checked.push_back (key);
                           out = outcome;
                           return true;
                       },
                       [this] () { notified++; })
        {
        }

        void RunJobs()
        {
            vector<FileSystemCheckScheduler::Job>  pending;



            pending.swap (jobs);

            for (FileSystemCheckScheduler::Job & job : pending)
            {
                job();
            }
        }
    };

    static FileSystemCheckKey MakeKey (const wchar_t * path, int64_t modifiedUnix = 100)
    {
        return FileSystemCheckKey { path, 143360, modifiedUnix };
    }



    TEST_METHOD (Tick_BeforeTheSelectionIsStill_StartsNoCheck)
    {
        Rig  rig;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        Assert::AreEqual ((int64_t) 250, rig.scheduler.Tick());

        rig.nowMs += 249;
        Assert::AreEqual ((int64_t) 1, rig.scheduler.Tick());
        Assert::AreEqual ((size_t) 0, rig.jobs.size());

        rig.nowMs += 1;
        Assert::AreEqual ((int64_t) -1, rig.scheduler.Tick());
        Assert::AreEqual ((size_t) 1, rig.jobs.size());
        Assert::IsTrue   (rig.scheduler.IsChecking());

        Assert::AreEqual ((int64_t) -1, rig.scheduler.Tick());
        Assert::AreEqual ((size_t) 1, rig.jobs.size(), L"one check at a time");
    }



    TEST_METHOD (Select_OfAnotherImage_StartsTheWaitOver)
    {
        Rig  rig;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        rig.nowMs += 200;
        rig.scheduler.Select (MakeKey (L"b.dsk"));
        rig.nowMs += 200;

        Assert::AreEqual ((int64_t) 50, rig.scheduler.Tick());
        Assert::AreEqual ((size_t) 0, rig.jobs.size());
    }



    TEST_METHOD (Select_DuringACheck_CancelsItAndDropsItsResult)
    {
        Rig                     rig;
        FileSystemCheckOutcome  outcome;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        rig.nowMs += 250;
        (void) rig.scheduler.Tick();

        rig.scheduler.Select (MakeKey (L"b.dsk"));
        Assert::IsFalse (rig.scheduler.IsChecking());

        rig.RunJobs();
        Assert::AreEqual ((size_t) 0, rig.checked.size(), L"the canceled check doesn't run");
        Assert::AreEqual (0, rig.notified);
        Assert::IsFalse  (rig.scheduler.TakeFinished());
        Assert::IsFalse  (rig.scheduler.TryGetOutcome (outcome));
    }



    TEST_METHOD (Select_WhileTheCheckerRuns_SetsItsCancelFlag)
    {
        Rig                     rig;
        bool                    wasCanceled = false;
        FileSystemCheckOutcome  outcome;

        FileSystemCheckScheduler  scheduler ([&rig] () { return rig.nowMs; },
                                             [&rig] (FileSystemCheckScheduler::Job job) { rig.jobs.push_back (std::move (job)); },
                                             [&] (const FileSystemCheckKey &, const std::atomic<bool> & isCanceled, FileSystemCheckOutcome &)
                                             {
                                                 scheduler.Select (MakeKey (L"b.dsk"));
                                                 wasCanceled = isCanceled;
                                                 return true;
                                             },
                                             [&rig] () { rig.notified++; });



        scheduler.Select (MakeKey (L"a.dsk"));
        rig.nowMs += 250;
        (void) scheduler.Tick();
        rig.RunJobs();

        Assert::IsTrue  (wasCanceled);
        Assert::AreEqual (0, rig.notified);
        Assert::IsFalse (scheduler.TakeFinished());
        Assert::IsFalse (scheduler.TryGetOutcome (outcome));
    }



    TEST_METHOD (TakeFinished_OfTheSelection_HasItsOutcome)
    {
        Rig                     rig;
        FileSystemCheckOutcome  outcome;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        Assert::IsFalse (rig.scheduler.TryGetOutcome (outcome));

        rig.nowMs += 250;
        (void) rig.scheduler.Tick();
        rig.RunJobs();

        Assert::AreEqual (1, rig.notified);
        Assert::IsTrue   (rig.scheduler.TakeFinished());
        Assert::IsFalse  (rig.scheduler.IsChecking());
        Assert::IsTrue   (rig.scheduler.TryGetOutcome (outcome));
        Assert::IsTrue   (outcome.verdict == FileSystemVerdict::Bad);
        Assert::AreEqual (rig.outcome.finding, outcome.finding);
    }



    TEST_METHOD (Select_OfACheckedImage_HasItsOutcomeAtOnce)
    {
        Rig                     rig;
        FileSystemCheckOutcome  outcome;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        rig.nowMs += 250;
        (void) rig.scheduler.Tick();
        rig.RunJobs();
        (void) rig.scheduler.TakeFinished();

        rig.scheduler.Select (MakeKey (L"b.dsk"));
        rig.scheduler.Select (MakeKey (L"a.dsk"));

        Assert::IsTrue   (rig.scheduler.TryGetOutcome (outcome));
        Assert::AreEqual ((int64_t) -1, rig.scheduler.Tick());
        Assert::AreEqual ((size_t) 0, rig.jobs.size());
        Assert::AreEqual ((size_t) 1, rig.checked.size());
    }



    TEST_METHOD (Select_OfAChangedImage_ChecksItAgain)
    {
        Rig                     rig;
        FileSystemCheckOutcome  outcome;



        rig.scheduler.Select (MakeKey (L"a.dsk"));
        rig.nowMs += 250;
        (void) rig.scheduler.Tick();
        rig.RunJobs();
        (void) rig.scheduler.TakeFinished();

        rig.scheduler.Select (MakeKey (L"a.dsk", 200));

        Assert::IsFalse  (rig.scheduler.TryGetOutcome (outcome), L"a new modified time misses the cache");
        rig.nowMs += 250;
        (void) rig.scheduler.Tick();
        Assert::AreEqual ((size_t) 1, rig.jobs.size());
    }



    TEST_METHOD (FormatInfobar_ForEachVerdict)
    {
        Assert::AreEqual (std::wstring (L"The catalog's file count differs from the VTOC's.\nClick Inspect for more information."),
                          FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::Bad, L"the catalog's file count differs from the VTOC's" }));
        Assert::AreEqual (std::wstring (L"There are 7 blocks in use that are marked free.\nClick Inspect for more information."),
                          FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::Suspect, L"There are 7 blocks in use that are marked free" }));
        Assert::AreEqual (std::wstring (L"Bad.\nClick Inspect for more information."),
                          FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::Bad, L"" }));
        Assert::AreEqual (std::wstring (L"No standard file system was found."),
                          FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::None, L"" }));
        Assert::AreEqual (std::wstring(),
                          FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::Ok, L"" }));
    }
};
