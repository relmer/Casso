#pragma once

#include "Core/IParallelRunner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParallelWorkPool
//
//  A thread pool of its own, apart from the process's default pool, so the
//  jobs it runs neither wait behind nor hold up work on that pool. RunAll
//  hands all but one job to the pool and runs jobs on the calling thread too
//  until none are left, then waits for the rest to finish. One caller at a
//  time. Before Create, or for a single job, it runs the jobs inline.
//
////////////////////////////////////////////////////////////////////////////////

class ParallelWorkPool : public IParallelRunner
{
public:
                     ParallelWorkPool  () = default;
                     ~ParallelWorkPool () override;

    ParallelWorkPool             (const ParallelWorkPool &) = delete;
    ParallelWorkPool & operator= (const ParallelWorkPool &) = delete;

    HRESULT          Create            (DWORD maxThreads);
    bool             IsCreated         () const { return m_pool != nullptr; }

    // IParallelRunner
    void             RunAll            (std::span<const Job> jobs) override;

private:
    struct Batch;

    static void CALLBACK  OnJob        (PTP_CALLBACK_INSTANCE instance, void * context);
    static void           RunJobs      (Batch & batch);
    static void           Wake         (Batch & batch);

    PTP_POOL             m_pool    = nullptr;
    PTP_CLEANUP_GROUP    m_cleanup = nullptr;
    TP_CALLBACK_ENVIRON  m_environ = {};
};
