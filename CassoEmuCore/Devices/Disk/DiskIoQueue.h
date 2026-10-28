#pragma once

#include "Pch.h"

#include "IDiskIoQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InlineDiskIoQueue
//
//  Runs a job's work and then its completion inside Post, on the posting
//  thread. The disk store's queue when no other is set: the command-line
//  tools, scratch machines and tests.
//
////////////////////////////////////////////////////////////////////////////////

class InlineDiskIoQueue : public IDiskIoQueue
{
public:
    void     Post             (Work work, Completion completion) override;
    void     DrainCompletions () override {}
    bool     IsBusy           () const override { return false; }
    HRESULT  WaitUntilIdle    (DWORD timeoutMs) override;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadedDiskIoQueue
//
//  Runs posted work on one thread, in posting order, so two commits to one
//  file never run out of order, and collects the completions for the
//  store's thread to drain. The destructor runs the work already posted,
//  then joins; completions not drained by then are not run.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadedDiskIoQueue : public IDiskIoQueue
{
public:
    using CompletionReady = std::function<void ()>;

    ThreadedDiskIoQueue  ();
    ~ThreadedDiskIoQueue () override;

    ThreadedDiskIoQueue (const ThreadedDiskIoQueue &)             = delete;
    ThreadedDiskIoQueue & operator= (const ThreadedDiskIoQueue &) = delete;

    //  Called on the I/O thread each time a completion is ready, so the
    //  store's thread can be scheduled to drain. Set before the first Post.
    void     SetCompletionReady (CompletionReady ready) { m_onReady = std::move (ready); }

    void     Post             (Work work, Completion completion) override;
    void     DrainCompletions () override;
    bool     IsBusy           () const override;
    HRESULT  WaitUntilIdle    (DWORD timeoutMs) override;

    //  The I/O thread's id, for a test that checks where work ran.
    std::thread::id  GetThreadId () const { return m_thread.get_id(); }

private:
    struct Job
    {
        Work        work;
        Completion  completion;
    };

    void  Run ();

    mutable std::mutex       m_lock;
    std::condition_variable  m_wake;
    std::condition_variable  m_ready;
    std::deque<Job>          m_jobs;
    std::deque<Completion>   m_completions;
    size_t                   m_outstanding = 0;
    bool                     m_isStopping  = false;
    CompletionReady          m_onReady;
    std::thread              m_thread;
};
