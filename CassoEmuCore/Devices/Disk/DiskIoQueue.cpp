#include "Pch.h"

#include "DiskIoQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InlineDiskIoQueue::Post
//
////////////////////////////////////////////////////////////////////////////////

void InlineDiskIoQueue::Post (Work work, Completion completion)
{
    if (work)
    {
        work();
    }

    if (completion)
    {
        completion();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InlineDiskIoQueue::WaitUntilIdle
//
//  Every job has run by the time Post returns.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InlineDiskIoQueue::WaitUntilIdle (DWORD timeoutMs)
{
    UNREFERENCED_PARAMETER (timeoutMs);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InlineDiskIoQueue::WaitUntil
//
//  Every job has run by the time Post returns, so a job not done by now
//  never will be.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InlineDiskIoQueue::WaitUntil (const std::function<bool()> & isDone, DWORD timeoutMs)
{
    UNREFERENCED_PARAMETER (timeoutMs);

    return isDone() ? S_OK : HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::ThreadedDiskIoQueue
//
////////////////////////////////////////////////////////////////////////////////

ThreadedDiskIoQueue::ThreadedDiskIoQueue() :
    m_thread ([this] () { Run(); })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::~ThreadedDiskIoQueue
//
////////////////////////////////////////////////////////////////////////////////

ThreadedDiskIoQueue::~ThreadedDiskIoQueue()
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_isStopping = true;
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::Post
//
////////////////////////////////////////////////////////////////////////////////

void ThreadedDiskIoQueue::Post (Work work, Completion completion)
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_jobs.push_back (Job { std::move (work), std::move (completion) });
        m_outstanding++;
    }

    m_wake.notify_one();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::DrainCompletions
//
//  Runs the ready completions one at a time, each taken under the lock and
//  run outside it, so a completion that posts again does not deadlock, and a
//  drain inside a completion runs the rest in posting order.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadedDiskIoQueue::DrainCompletions()
{
    bool  isRun = true;



    while (isRun)
    {
        isRun = RunOneCompletion();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::RunOneCompletion
//
//  Runs the oldest ready completion. Returns whether there was one.
//
////////////////////////////////////////////////////////////////////////////////

bool ThreadedDiskIoQueue::RunOneCompletion()
{
    Completion  completion;
    bool        isReady    = false;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        isReady = !m_completions.empty();

        if (isReady)
        {
            completion = std::move (m_completions.front());
            m_completions.pop_front();
        }
    }

    if (isReady)
    {
        if (completion)
        {
            completion();
        }

        std::lock_guard<std::mutex>  lock (m_lock);

        m_outstanding--;
    }

    return isReady;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::IsBusy
//
////////////////////////////////////////////////////////////////////////////////

bool ThreadedDiskIoQueue::IsBusy() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_outstanding > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::WaitUntilIdle
//
//  Drains as completions arrive, including those of jobs that completions
//  post, until none is left to run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThreadedDiskIoQueue::WaitUntilIdle (DWORD timeoutMs)
{
    HRESULT                                hr       = S_OK;
    std::chrono::steady_clock::time_point  deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    bool                                   isIdle   = false;
    bool                                   isReady  = false;



    for (;;)
    {
        DrainCompletions();

        {
            std::unique_lock<std::mutex>  lock (m_lock);

            isIdle = m_outstanding == 0;

            if (!isIdle)
            {
                isReady = m_ready.wait_until (lock, deadline, [this] () { return !m_completions.empty(); });
            }
        }

        BAIL_OUT_IF (isIdle, S_OK);
        CBREx (isReady, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::WaitUntil
//
//  Runs completions one at a time as they arrive, and stops at the first
//  one after which isDone returns true.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThreadedDiskIoQueue::WaitUntil (const std::function<bool()> & isDone, DWORD timeoutMs)
{
    HRESULT                                hr       = S_OK;
    std::chrono::steady_clock::time_point  deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    bool                                   isRun    = true;
    bool                                   isReady  = false;



    for (;;)
    {
        isRun = true;

        while (!isDone() && isRun)
        {
            isRun = RunOneCompletion();
        }

        BAIL_OUT_IF (isDone(), S_OK);

        {
            std::unique_lock<std::mutex>  lock (m_lock);

            isReady = m_ready.wait_until (lock, deadline, [this] () { return !m_completions.empty(); });
        }

        CBREx (isReady, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue::Run
//
//  Takes jobs in order until stopping and the queue is empty, so work posted
//  just before the destructor runs too.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadedDiskIoQueue::Run()
{
    for (;;)
    {
        Job  job;



        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait (lock, [this] () { return m_isStopping || !m_jobs.empty(); });

            if (m_jobs.empty())
            {
                return;
            }

            job = std::move (m_jobs.front());
            m_jobs.pop_front();
        }

        if (job.work)
        {
            job.work();
        }

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            m_completions.push_back (std::move (job.completion));
        }

        m_ready.notify_all();

        if (m_onReady)
        {
            m_onReady();
        }
    }
}
