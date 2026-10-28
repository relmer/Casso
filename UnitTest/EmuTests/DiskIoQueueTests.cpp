#include "Pch.h"
#include "Devices/Disk/DiskIoQueue.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIoQueueTests
//
//  The two disk I/O queues: the inline queue, which runs a job inside Post,
//  and the threaded queue, which runs work on its thread in posting order
//  and completions only when the store's thread drains it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIoQueueTests)
{
public:

    static constexpr DWORD  s_kWaitMs = 5000;



    TEST_METHOD (TheInlineQueueRunsTheWorkThenTheCompletionInsidePost)
    {
        InlineDiskIoQueue          queue;
        std::vector<std::string>   order;



        queue.Post ([&order] { order.push_back ("work"); }, [&order] { order.push_back ("completion"); });
        order.push_back ("after");

        Assert::AreEqual ((size_t) 3, order.size(), L"three steps ran");
        Assert::AreEqual (std::string ("work"),       order[0], L"the work first");
        Assert::AreEqual (std::string ("completion"), order[1], L"then the completion");
        Assert::AreEqual (std::string ("after"),      order[2], L"both before Post returned");
        Assert::IsFalse  (queue.IsBusy(), L"and nothing is left");
    }


    //  Work runs on the queue's thread in posting order; each completion runs
    //  on the thread that drains, and only then.
    TEST_METHOD (TheThreadedQueueRunsWorkInOrderOffTheCallerAndCompletionsOnTheDrain)
    {
        ThreadedDiskIoQueue            queue;
        std::vector<int>               worked;
        std::vector<int>               completed;
        std::vector<std::thread::id>   workThreads;
        std::thread::id                caller   = std::this_thread::get_id();
        HRESULT                        hr       = S_OK;
        int                            i        = 0;



        for (i = 0; i < 3; i++)
        {
            queue.Post ([&worked, &workThreads, i] { worked.push_back (i); workThreads.push_back (std::this_thread::get_id()); },
                        [&completed, i] { completed.push_back (i); });
        }

        Assert::IsTrue (queue.IsBusy(), L"the queue is busy until its completions run");

        hr = queue.WaitUntilIdle (s_kWaitMs);
        AssertSucceeded (hr, L"WaitUntilIdle");

        Assert::AreEqual ((size_t) 3, worked.size(), L"every job's work ran");
        Assert::IsTrue   (worked == std::vector<int> ({ 0, 1, 2 }), L"in posting order");
        Assert::IsTrue   (completed == std::vector<int> ({ 0, 1, 2 }), L"and so did the completions");
        Assert::IsTrue   (std::ranges::all_of (workThreads, [&queue] (std::thread::id id) { return id == queue.GetThreadId(); }), L"the work ran on the queue's thread");
        Assert::IsTrue   (queue.GetThreadId() != caller, L"which is not the caller's");
        Assert::IsFalse  (queue.IsBusy(), L"and nothing is left");
    }


    TEST_METHOD (ACompletionRunsOnlyWhenTheQueueIsDrained)
    {
        ThreadedDiskIoQueue  queue;
        std::atomic<bool>    isWorked    { false };
        std::atomic<bool>    isReady     { false };
        bool                 isCompleted = false;
        std::thread::id      completedOn;
        auto                 deadline    = std::chrono::steady_clock::now() + std::chrono::milliseconds (s_kWaitMs);



        queue.SetCompletionReady ([&isReady] { isReady = true; });
        queue.Post ([&isWorked] { isWorked = true; }, [&isCompleted, &completedOn] { isCompleted = true; completedOn = std::this_thread::get_id(); });

        while (!isReady && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }

        Assert::IsTrue  (isReady.load(),  L"the queue reported the completion ready");
        Assert::IsTrue  (isWorked.load(), L"after the work ran");
        Assert::IsFalse (isCompleted,     L"and the completion waited for a drain");

        queue.DrainCompletions();

        Assert::IsTrue  (isCompleted, L"the drain ran it");
        Assert::IsTrue  (completedOn == std::this_thread::get_id(), L"on the draining thread");
    }


    TEST_METHOD (WaitingUntilIdleRunsTheJobsThatACompletionPosts)
    {
        ThreadedDiskIoQueue  queue;
        int                  completions = 0;
        HRESULT              hr          = S_OK;



        queue.Post ([] {}, [&queue, &completions]
        {
            completions++;
            queue.Post ([] {}, [&completions] { completions++; });
        });

        hr = queue.WaitUntilIdle (s_kWaitMs);
        AssertSucceeded (hr, L"WaitUntilIdle");

        Assert::AreEqual (2, completions, L"the second job's completion ran too");
    }


    TEST_METHOD (WaitingUntilIdleTimesOutForWorkThatDoesNotFinish)
    {
        ThreadedDiskIoQueue  queue;
        std::atomic<bool>    isReleased { false };
        auto                 deadline   = std::chrono::steady_clock::now() + std::chrono::milliseconds (s_kWaitMs);
        HRESULT              hr         = S_OK;



        queue.Post ([&isReleased, deadline]
        {
            while (!isReleased && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::yield();
            }
        }, nullptr);

        hr = queue.WaitUntilIdle (50);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr, L"the wait gives up at its deadline");

        isReleased = true;

        hr = queue.WaitUntilIdle (s_kWaitMs);
        AssertSucceeded (hr, L"and succeeds once the work finishes");
    }


    TEST_METHOD (DestroyingTheQueueRunsThePostedWork)
    {
        std::atomic<int>  worked { 0 };
        int               i      = 0;



        {
            ThreadedDiskIoQueue  queue;

            for (i = 0; i < 5; i++)
            {
                queue.Post ([&worked] { worked++; }, nullptr);
            }
        }

        Assert::AreEqual (5, worked.load(), L"every job's work ran before the queue was gone");
    }
};
