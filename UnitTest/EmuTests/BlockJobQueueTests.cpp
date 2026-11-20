#include "Pch.h"
#include "Ui/DiskInspector/Blocks/BlockJobQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueueTests
//
//  Block mode's jobs with no worker, and a long job's progress on a worker.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockJobQueueTests)
{
public:

    TEST_METHOD (NoWorker_RunsTheJobAtOnce)
    {
        BlockJobQueue  jobs;
        bool           isWorked   = false;
        bool           isFinished = false;
        bool           isQueued   = false;



        jobs.SetOnQueued ([&] () { isQueued = true; });
        jobs.QueueJob ({ L"", false, [&] (BlockJobProgress &) { isWorked = true; }, [&] () { isFinished = true; } });

        Assert::IsTrue  (isWorked);
        Assert::IsTrue  (isFinished);
        Assert::IsFalse (isQueued);
        Assert::IsFalse (jobs.IsBusy());
    }


    TEST_METHOD (LongJob_ReportsItsProgress)
    {
        constexpr auto  kDeadline = std::chrono::seconds (10);

        BlockInspectorWorker      worker;
        BlockJobQueue             jobs;
        std::promise<void>        started;
        std::promise<void>        released;
        std::shared_future<void>  release  = released.get_future().share();
        bool                      isQueued = false;



        jobs.SetWorker   (&worker);
        jobs.SetOnQueued ([&] () { isQueued = true; });
        jobs.QueueJob ({ L"Copying", true, [&] (BlockJobProgress & progress)
        {
            progress.total = 4;
            progress.done  = 1;
            started.set_value();
            (void) release.wait_for (kDeadline);
        }, [] () {} });

        Assert::IsTrue (isQueued);
        Assert::IsTrue (jobs.IsBusy());
        Assert::IsTrue (started.get_future().wait_for (kDeadline) == std::future_status::ready);

        jobs.Poll();

        Assert::IsTrue   (jobs.IsLongJob());
        Assert::IsTrue   (jobs.CanCancel());
        Assert::AreEqual (0.25, jobs.GetShare());
        Assert::AreEqual (std::wstring (L"Copying (25%)"), jobs.BuildProgressLabel());

        released.set_value();
        worker.WaitForWork();
    }
};
