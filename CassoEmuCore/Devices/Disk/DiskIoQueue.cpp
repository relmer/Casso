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
//  Takes the ready completions under the lock and runs them outside it, so
//  a completion that posts again does not deadlock.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadedDiskIoQueue::DrainCompletions()
{
    std::deque<Completion>  ready;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        ready.swap (m_completions);
    }

    for (Completion & completion : ready)
    {
        if (completion)
        {
            completion();
        }

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            m_outstanding--;
        }
    }
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
