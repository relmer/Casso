#include "Pch.h"

#include "Debugger/Reverse/KeyframePackQueue.h"
#include "Core/IWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::~KeyframePackQueue
//
//  Waits for the work in flight, which writes into this queue's buffers.
//
////////////////////////////////////////////////////////////////////////////////

KeyframePackQueue::~KeyframePackQueue()
{
    WaitAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::Reserve
//
//  The buffers at full size, with room for a state that grows a little, so
//  packing seldom allocates.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::Reserve (size_t stateBytes)
{
    size_t  stateRoom   = KeyframeLayout::GetStateRoom   (stateBytes);
    size_t  packedBound = KeyframeLayout::GetPackedBound (stateBytes);



    m_codec.Reserve (stateRoom);

    for (Job & job : m_jobs)
    {
        job.state.reserve  (stateRoom);
        job.packed.reserve (packedBound);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::Release
//
//  Frees every buffer. With nothing in flight.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::Release()
{
    m_codec.Release();

    for (Job & job : m_jobs)
    {
        job.state  = std::vector<Byte>();
        job.packed = std::vector<Byte>();
        job.side   = std::vector<Byte>();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::GetBufferBytes
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframePackQueue::GetBufferBytes() const
{
    size_t  bytes = m_codec.GetBufferBytes();



    for (const Job & job : m_jobs)
    {
        bytes += job.state.capacity() + job.packed.capacity();
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::GetNext
//
////////////////////////////////////////////////////////////////////////////////

KeyframePackQueue::Job & KeyframePackQueue::GetNext()
{
    Job  & job = m_jobs[m_nextJob];



    job.queue = this;

    return job;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::Submit
//
//  With no work queue, or when Submit on it fails, the job runs here once
//  the jobs before it are done, so no job stays in flight with nothing to
//  run it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::Submit (bool isWhole)
{
    Job      & job      = GetNext();
    HRESULT    hrSubmit = E_FAIL;
    bool       isQueued = false;



    job.isWhole = isWhole;
    job.isDone.store (false, std::memory_order_relaxed);

    m_nextJob = (m_nextJob + 1) % m_jobs.size();
    m_pendingCount++;

    if (m_queue != nullptr)
    {
        hrSubmit = m_queue->Submit (RunJob, &job);
    }

    isQueued = SUCCEEDED (hrSubmit);

    if (!isQueued && m_queue != nullptr)
    {
        m_queue->WaitAll();
    }

    if (!isQueued)
    {
        RunJob (&job);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::TryTakeDone
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframePackQueue::TryTakeDone (Job *& outJob)
{
    Job  * job = nullptr;



    if (m_pendingCount == 0)
    {
        return false;
    }

    job = &m_jobs[(m_nextJob + m_jobs.size() - m_pendingCount) % m_jobs.size()];

    if (!job->isDone.load (std::memory_order_acquire))
    {
        return false;
    }

    outJob = job;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::PopDone
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::PopDone()
{
    if (m_pendingCount > 0)
    {
        m_pendingCount--;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::WaitAll
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::WaitAll()
{
    if (m_queue != nullptr && m_pendingCount != 0)
    {
        m_queue->WaitAll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::DropAll
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::DropAll()
{
    WaitAll();

    m_pendingCount = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::GetPending
//
////////////////////////////////////////////////////////////////////////////////

const KeyframePackQueue::Job & KeyframePackQueue::GetPending (size_t pendingIndex) const
{
    return m_jobs[(m_nextJob + m_jobs.size() - m_pendingCount + pendingIndex) % m_jobs.size()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue::RunJob
//
//  The work function a work queue runs: the codec packs the job's state.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframePackQueue::RunJob (void * context)
{
    Job  * job = static_cast<Job *> (context);



    job->hr = job->queue->m_codec.Pack (job->state, job->isWhole, job->packed, job->checksum);
    job->isDone.store (true, std::memory_order_release);
}
