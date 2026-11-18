#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobProgress
//
//  A running job's progress, read on the UI thread while the job updates it
//  on the worker's thread, and whether the user canceled it.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockJobProgress
{
    std::atomic<uint64_t>  done       = 0;
    std::atomic<uint64_t>  total      = 0;
    std::atomic<bool>      isCanceled = false;

    bool  IsCanceled () const { return isCanceled.load(); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWorker
//
//  Block mode's reads and writes of an image, on a separate thread, so no
//  read or write of a file or a card runs on the window's thread. Jobs
//  run one at a time, in the order queued. A job's work runs on the
//  worker's thread; its completion runs on the UI thread when RunFinished
//  is called.
//
//  While IsBusy is true, only the worker's thread touches the model. A long
//  job has a label and progress for the window to draw. The work of a job
//  that can be canceled reads BlockJobProgress::isCanceled between steps.
//
//  The destructor cancels the running job, joins the thread, and drops the
//  jobs after it without running their completions.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorWorker
{
public:
    using Work   = std::function<void (BlockJobProgress & progress)>;
    using Finish = std::function<void ()>;

    struct Job
    {
        std::wstring  label;            // empty for a short job, such as reading a block
        bool          canCancel = false;
        Work          work;
        Finish        finish;
    };

    BlockInspectorWorker  ();
    ~BlockInspectorWorker ();

    BlockInspectorWorker (const BlockInspectorWorker &)             = delete;
    BlockInspectorWorker & operator= (const BlockInspectorWorker &) = delete;

    void  Queue       (Job job);
    void  RunFinished ();
    void  Cancel      ();

    //  A job queued, running, or finished with its completion not yet run.
    bool  IsBusy      () const;

    //  The running or next long job's label, whether it can be canceled, and
    //  its progress. False when no long job is queued or running.
    bool  TryGetLongJob (std::wstring & outLabel, bool & outCanCancel, uint64_t & outDone, uint64_t & outTotal) const;

    //  Blocks until every job queued so far has run its work. For tests.
    void  WaitForWork ();

private:
    void  Run ();

    mutable std::mutex       m_lock;
    std::condition_variable  m_wake;
    std::condition_variable  m_idle;
    std::deque<Job>          m_queue;
    std::deque<Finish>       m_finished;
    Job                      m_running;
    bool                     m_isRunning  = false;
    bool                     m_isStopping = false;
    BlockJobProgress         m_progress;
    std::thread              m_thread;
};
