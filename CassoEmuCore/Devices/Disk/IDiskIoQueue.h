#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIoQueue
//
//  Where the disk store's file I/O runs. The thread with the store's token
//  posts a job of two parts: the work, which reads and writes files and runs
//  on the queue's thread in posting order, and the completion, which applies
//  the result to the bays and runs on the store's thread when the store
//  drains the queue.
//
//  InlineDiskIoQueue runs both parts inside Post, so a store with no I/O
//  thread runs in the order a synchronous save does. ThreadedDiskIoQueue
//  runs the work on a separate thread.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskIoQueue
{
public:
    using Work       = std::function<void ()>;
    using Completion = std::function<void ()>;

    virtual           ~IDiskIoQueue () = default;

    virtual void      Post             (Work work, Completion completion) = 0;

    //  Runs the completions of finished work, in posting order, on the
    //  calling thread. A completion may post again.
    virtual void      DrainCompletions () = 0;

    //  Whether a posted job's completion has not run yet.
    virtual bool      IsBusy           () const = 0;

    //  Drains until no posted job's completion is left to run, and returns
    //  ERROR_TIMEOUT when timeoutMs passes first.
    virtual HRESULT   WaitUntilIdle    (DWORD timeoutMs) = 0;

    //  Drains, in posting order, until isDone returns true, and returns
    //  ERROR_TIMEOUT when timeoutMs passes first. For a wait for one job,
    //  which can start inside another job's completion: the completion that
    //  is running counts as outstanding until it returns, so the queue is
    //  never idle inside it.
    virtual HRESULT   WaitUntil        (const std::function<bool ()> & isDone, DWORD timeoutMs) = 0;
};
