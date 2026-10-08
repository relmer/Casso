#include "Pch.h"

#include "Core/ParallelWorkPool.h"
#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::Batch
//
//  One RunAll's jobs. Each thread that joins takes the next job not yet
//  taken, so a job runs once whichever thread reaches it. The caller waits
//  for every job and for every callback it queued, since a callback the pool
//  starts after the last job still reads the batch.
//
////////////////////////////////////////////////////////////////////////////////

struct ParallelWorkPool::Batch
{
    std::span<const Job>     jobs;
    const wchar_t          * name      = nullptr;
    std::atomic<size_t>      next      = 0;
    std::atomic<size_t>      remaining = 0;
    std::atomic<size_t>      callbacks = 0;
    std::mutex               lock;
    std::condition_variable  done;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ~ParallelWorkPool
//
//  Waits for any callback still running, then closes the pool.
//
////////////////////////////////////////////////////////////////////////////////

ParallelWorkPool::~ParallelWorkPool()
{
    if (m_cleanup != nullptr)
    {
        CloseThreadpoolCleanupGroupMembers (m_cleanup, FALSE, nullptr);
        CloseThreadpoolCleanupGroup (m_cleanup);
    }

    if (m_pool != nullptr)
    {
        CloseThreadpool (m_pool);
        DestroyThreadpoolEnvironment (&m_environ);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::Create
//
//  A pool of at most maxThreads threads and at least one, kept whether or not
//  there is work, so a RunAll never waits for a thread to start. Called once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ParallelWorkPool::Create (DWORD maxThreads, const wchar_t * name)
{
    HRESULT  hr          = S_OK;
    BOOL     isMinSet    = FALSE;
    bool     isUnused    = m_pool == nullptr;
    bool     hasThreads  = maxThreads > 0;



    CBRAEx (isUnused,   E_UNEXPECTED);
    CBRAEx (hasThreads, E_INVALIDARG);

    m_name = name;
    m_pool = CreateThreadpool (nullptr);
    CWRA (m_pool);

    SetThreadpoolThreadMaximum (m_pool, maxThreads);

    isMinSet = SetThreadpoolThreadMinimum (m_pool, 1);
    CWRA (isMinSet);

    m_cleanup = CreateThreadpoolCleanupGroup();
    CWRA (m_cleanup);

    InitializeThreadpoolEnvironment (&m_environ);
    SetThreadpoolCallbackPool (&m_environ, m_pool);
    SetThreadpoolCallbackCleanupGroup (&m_environ, m_cleanup, nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::RunAll
//
//  Asks the pool for a thread per job but the first, then takes jobs on this
//  thread as well, so the caller works rather than only waits. A pool that
//  cannot take a callback leaves its jobs to the threads that did start.
//
////////////////////////////////////////////////////////////////////////////////

void ParallelWorkPool::RunAll (std::span<const Job> jobs)
{
    HRESULT                       hr        = S_OK;
    Batch                         batch;
    BOOL                          isQueued  = FALSE;
    std::unique_lock<std::mutex>  held (batch.lock, std::defer_lock);



    if (m_pool == nullptr || jobs.size() <= 1)
    {
        InlineParallelRunner().RunAll (jobs);
        return;
    }

    batch.jobs      = jobs;
    batch.name      = m_name;
    batch.remaining = jobs.size();

    for (size_t i = 1; i < jobs.size(); i++)
    {
        batch.callbacks++;

        isQueued = TrySubmitThreadpoolCallback (OnJob, &batch, &m_environ);

        if (!isQueued)
        {
            batch.callbacks--;
        }

        CWRA (isQueued);
    }

Error:
    RunJobs (batch);

    held.lock();
    batch.done.wait (held, [&batch] { return batch.remaining.load() == 0 && batch.callbacks.load() == 0; });
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::OnJob
//
//  One queued callback: takes jobs until none are left, then counts itself
//  out under the lock and wakes the caller. The count drops under the lock
//  so the caller cannot see it reach zero, return and free the batch while
//  this callback still holds a reference to it.
//
////////////////////////////////////////////////////////////////////////////////

void CALLBACK ParallelWorkPool::OnJob (
    PTP_CALLBACK_INSTANCE    instance,
    void                   * context)
{
    Batch                         & batch = *static_cast<Batch *> (context);
    std::unique_lock<std::mutex>    held (batch.lock, std::defer_lock);



    UNREFERENCED_PARAMETER (instance);

    //  The pool's threads run only this pool's work, so the name stays.
    if (batch.name != nullptr)
    {
        ThreadName::Set (batch.name);
    }

    RunJobs (batch);

    held.lock();
    batch.callbacks--;
    batch.done.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::RunJobs
//
//  Takes jobs until none are left. The thread that finishes the last one
//  wakes the caller.
//
////////////////////////////////////////////////////////////////////////////////

void ParallelWorkPool::RunJobs (Batch & batch)
{
    size_t  index = 0;



    for (index = batch.next++; index < batch.jobs.size(); index = batch.next++)
    {
        batch.jobs[index]();

        if (batch.remaining.fetch_sub (1) == 1)
        {
            Wake (batch);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool::Wake
//
//  Under the lock, so the wake cannot fall between the caller's test and its
//  wait.
//
////////////////////////////////////////////////////////////////////////////////

void ParallelWorkPool::Wake (Batch & batch)
{
    std::lock_guard<std::mutex>  held (batch.lock);



    batch.done.notify_all();
}
