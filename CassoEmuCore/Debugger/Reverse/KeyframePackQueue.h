#pragma once

#include "Pch.h"

#include "Debugger/Reverse/KeyframeCodec.h"
#include "Debugger/Reverse/KeyframeLayout.h"

class IWorkQueue;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframePackQueue
//
//  The packing of a store's keyframes: a few jobs, each with buffers reserved
//  at full size, and the codec. Given a work queue, a job's checksum, XOR and
//  packing run there; without one, Submit runs it at once. Jobs run one at
//  a time and finish in the order they were submitted.
//
//  The codec's buffers and a job's are touched by the worker only while the
//  job is in flight, and by the caller only while none is. TryTakeDone is
//  the one place results cross to the caller.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframePackQueue
{
public:
    struct Job
    {
        KeyframePackQueue  * queue    = nullptr;
        std::vector<Byte>    state;
        std::vector<Byte>    packed;
        std::vector<Byte>    side;
        uint64_t             checksum = 0;
        bool                 isWhole  = false;
        HRESULT              hr       = S_OK;
        std::atomic<bool>    isDone   = false;
    };

    KeyframePackQueue() = default;
    ~KeyframePackQueue();

    KeyframePackQueue (const KeyframePackQueue &)             = delete;
    KeyframePackQueue & operator= (const KeyframePackQueue &) = delete;

    //  Where the jobs run from here on; null runs them inside Submit. The
    //  caller waits for the jobs in flight first.
    void  SetWorkQueue (IWorkQueue * queue) { m_queue = queue; }

    void    Reserve        (size_t stateBytes);
    void    Release        ();
    size_t  GetBufferBytes () const;

    bool    IsFull          () const { return m_pendingCount == m_jobs.size(); }
    size_t  GetPendingCount () const { return m_pendingCount; }

    //  The job the next keyframe's state is written into, with no job of
    //  that buffer in flight (IsFull is false).
    Job  & GetNext();

    //  Submits the next job to run.
    void  Submit (bool isWhole);

    //  The oldest job in flight, when it is done; PopDone retires it.
    bool  TryTakeDone (Job *& outJob);
    void  PopDone     ();

    //  Waits for every job in flight; DropAll then retires them, after a job
    //  failed.
    void  WaitAll();
    void  DropAll();

    //  The pendingIndex-th job in flight, oldest first.
    const Job  & GetPending (size_t pendingIndex) const;

    KeyframeCodec  & GetCodec() { return m_codec; }

private:
    static void  RunJob (void * context);

    IWorkQueue                                      * m_queue        = nullptr;
    std::array<Job, KeyframeLayout::kJobCount>        m_jobs;
    size_t                                            m_nextJob      = 0;     // the job the next keyframe takes
    size_t                                            m_pendingCount = 0;     // jobs submitted and not yet retired
    KeyframeCodec                                     m_codec;
};
