#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskIoQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredDiskIoQueue
//
//  A disk I/O queue for tests: posted work waits until the test runs it, on
//  the test's thread, and its completion then waits for a drain. A test can
//  stop between the post, the work and the completion and look at the store
//  at each point.
//
////////////////////////////////////////////////////////////////////////////////

class DeferredDiskIoQueue : public IDiskIoQueue
{
public:
    void     Post             (Work work, Completion completion) override
    {
        m_jobs.push_back (Job { std::move (work), std::move (completion) });
    }

    void     DrainCompletions () override
    {
        while (!m_ready.empty())
        {
            Completion  completion = std::move (m_ready.front());

            m_ready.pop_front();

            if (completion)
            {
                completion();
            }
        }
    }

    bool     IsBusy           () const override { return !m_jobs.empty() || !m_ready.empty(); }

    HRESULT  WaitUntilIdle    (DWORD timeoutMs) override
    {
        UNREFERENCED_PARAMETER (timeoutMs);

        RunAll();
        return S_OK;
    }

    //  Runs the oldest posted work, and readies its completion. Returns
    //  whether there was work to run.
    bool     RunNextWork ()
    {
        if (m_jobs.empty())
        {
            return false;
        }

        Job  job = std::move (m_jobs.front());

        m_jobs.pop_front();

        if (job.work)
        {
            job.work();
        }

        m_ready.push_back (std::move (job.completion));
        return true;
    }

    //  Runs work and drains until nothing is posted, including what the
    //  completions post.
    void     RunAll ()
    {
        bool  isWorking = true;



        while (IsBusy())
        {
            for (isWorking = RunNextWork(); isWorking; isWorking = RunNextWork());

            DrainCompletions();
        }
    }

    size_t   GetPostedCount () const { return m_jobs.size(); }
    size_t   GetReadyCount  () const { return m_ready.size(); }

private:
    struct Job
    {
        Work        work;
        Completion  completion;
    };

    std::deque<Job>         m_jobs;
    std::deque<Completion>  m_ready;
};
