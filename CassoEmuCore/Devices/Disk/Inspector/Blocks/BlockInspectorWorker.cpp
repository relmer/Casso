#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::BlockInspectorWorker
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorWorker::BlockInspectorWorker() :
    m_thread ([this] () { Run(); })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::~BlockInspectorWorker
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorWorker::~BlockInspectorWorker()
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_isStopping = true;
        m_progress.isCanceled = true;
        m_queue.clear();
        m_finished.clear();
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::Queue
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWorker::Queue (Job job)
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_queue.push_back (std::move (job));
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::RunFinished
//
//  Each completion runs outside the lock, so it can queue another job.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWorker::RunFinished()
{
    Finish  finish;



    for (;;)
    {
        {
            std::lock_guard<std::mutex>  lock (m_lock);

            if (m_finished.empty())
            {
                break;
            }

            finish = std::move (m_finished.front());
            m_finished.pop_front();
        }

        if (finish)
        {
            finish();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWorker::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    if (m_isRunning && m_running.canCancel)
    {
        m_progress.isCanceled = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::IsBusy
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWorker::IsBusy() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_isRunning || !m_queue.empty() || !m_finished.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::TryGetLongJob
//
//  The running job when it is long, otherwise the first long job queued,
//  which hasn't started.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWorker::TryGetLongJob (std::wstring & outLabel, bool & outCanCancel, uint64_t & outDone, uint64_t & outTotal) const
{
    std::lock_guard<std::mutex>  lock (m_lock);
    const Job                  * job = (m_isRunning && !m_running.label.empty()) ? &m_running : nullptr;



    outDone  = 0;
    outTotal = 0;

    for (auto queued = m_queue.begin(); job == nullptr && queued != m_queue.end(); ++queued)
    {
        job = queued->label.empty() ? nullptr : &*queued;
    }

    if (job == &m_running)
    {
        outDone  = m_progress.done;
        outTotal = m_progress.total;
    }

    outLabel     = job != nullptr ? job->label : std::wstring();
    outCanCancel = job != nullptr && job->canCancel;

    return job != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::WaitForWork
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWorker::WaitForWork()
{
    std::unique_lock<std::mutex>  lock (m_lock);



    m_idle.wait (lock, [this] () { return !m_isRunning && m_queue.empty(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker::Run
//
//  On the worker's thread: each job's work, then its completion queued for
//  the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWorker::Run()
{
    Work  work;



    for (;;)
    {
        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait (lock, [this] () { return m_isStopping || !m_queue.empty(); });

            if (m_isStopping)
            {
                break;
            }

            m_running = std::move (m_queue.front());
            m_queue.pop_front();

            m_isRunning           = true;
            m_progress.done       = 0;
            m_progress.total      = 0;
            m_progress.isCanceled = false;
            work                  = std::move (m_running.work);
        }

        if (work)
        {
            work (m_progress);
        }

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            if (!m_isStopping)
            {
                m_finished.push_back (std::move (m_running.finish));
            }

            m_running   = Job();
            m_isRunning = false;
        }

        m_idle.notify_all();
    }

    m_idle.notify_all();
}
