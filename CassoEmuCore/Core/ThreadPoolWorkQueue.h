#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadPoolWorkQueue
//
//  A thin wrapper around one Windows thread pool work object
//  (CreateThreadpoolWork, SubmitThreadpoolWork) on the process's default
//  pool, or on a one-thread pool of its own when it is given a name. Each
//  Submit queues one callback; the callbacks take the items from a fixed ring
//  in order, one at a time, so items run in the order they were submitted
//  and never overlap, whichever pool threads run them. A named queue's thread
//  runs only its items, so it carries the name for good and a profiler can
//  tell its time from everyone else's.
//
//  The ring holds at most the capacity given to Create; submitting past it
//  is a caller bug. Destroying the queue waits for every item submitted.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadPoolWorkQueue : public IWorkQueue
{
public:
                          ThreadPoolWorkQueue  () = default;
                          ~ThreadPoolWorkQueue () override;

                          ThreadPoolWorkQueue  (const ThreadPoolWorkQueue &) = delete;
    ThreadPoolWorkQueue & operator=            (const ThreadPoolWorkQueue &) = delete;

    HRESULT               Create               (size_t capacity, const wchar_t * name = nullptr);
    bool                  IsCreated            () const { return m_work != nullptr; }

    HRESULT               Submit               (WorkFunction function, void * context) override;
    void                  WaitAll              () override;

private:
    struct Item
    {
        WorkFunction  function = nullptr;
        void        * context  = nullptr;
    };

    static void CALLBACK  OnWork               (PTP_CALLBACK_INSTANCE instance, void * context, PTP_WORK work);

    void                  RunNext              ();

    PTP_WORK             m_work    = nullptr;
    PTP_POOL             m_pool    = nullptr;
    TP_CALLBACK_ENVIRON  m_environ = {};
    const wchar_t      * m_name    = nullptr;
    SRWLOCK              m_runLock = SRWLOCK_INIT;     // held while an item runs, so items never overlap
    SRWLOCK              m_lock    = SRWLOCK_INIT;     // guards the ring
    std::vector<Item>    m_items;
    size_t               m_head    = 0;
    size_t               m_count   = 0;
};
