#include "Pch.h"

#include "Shell/BackgroundWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundWorkQueue
//
////////////////////////////////////////////////////////////////////////////////

BackgroundWorkQueue::BackgroundWorkQueue() :
    m_thread ([this] () { Run(); })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~BackgroundWorkQueue
//
////////////////////////////////////////////////////////////////////////////////

BackgroundWorkQueue::~BackgroundWorkQueue()
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
//  Post
//
////////////////////////////////////////////////////////////////////////////////

void BackgroundWorkQueue::Post (Job job)
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_jobs.push_back (std::move (job));
    }

    m_wake.notify_one();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  Takes jobs in order until stopping and the queue is empty, so a job posted
//  just before shutdown still runs.
//
////////////////////////////////////////////////////////////////////////////////

void BackgroundWorkQueue::Run()
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

        job();
    }
}
