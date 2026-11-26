#include "Pch.h"

#include "BlockJobQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueue::QueueJob
//
//  The queued callback is called after the job is queued, so the window
//  draws the next frame, and the job's completion runs soon after its work
//  ends.
//
////////////////////////////////////////////////////////////////////////////////

void BlockJobQueue::QueueJob (BlockInspectorWorker::Job job)
{
    BlockJobProgress  progress;



    if (m_worker == nullptr)
    {
        job.work (progress);
        job.finish();
        return;
    }

    m_worker->Queue (std::move (job));

    if (m_onQueued)
    {
        m_onQueued();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueue::Poll
//
////////////////////////////////////////////////////////////////////////////////

void BlockJobQueue::Poll()
{
    if (m_worker != nullptr)
    {
        m_isLongJob = m_worker->TryGetLongJob (m_label, m_canCancel, m_done, m_total);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueue::GetShare
//
////////////////////////////////////////////////////////////////////////////////

double BlockJobQueue::GetShare() const
{
    return m_total > 0 ? std::min (1.0, (double) m_done / (double) m_total) : 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueue::BuildProgressLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockJobQueue::BuildProgressLabel() const
{
    constexpr int  kPercent = 100;



    return m_total > 0 ? std::format (L"{} ({}%)", m_label, (int) (GetShare() * kPercent)) : m_label;
}
