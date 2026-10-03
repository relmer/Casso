#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InlineWorkQueue
//
//  A work queue for tests: items wait until the test runs them, or until
//  something waits for them, and then run on the test's own thread in the
//  order they were submitted. It counts the waits, so a test can tell that
//  code it called waited for the work in flight.
//
////////////////////////////////////////////////////////////////////////////////

class InlineWorkQueue : public IWorkQueue
{
public:
    HRESULT  Submit          (WorkFunction function, void * context) override;
    void     WaitAll         () override;

    bool     TryRunNext      ();
    size_t   GetPendingCount () const { return m_items.size(); }
    size_t   GetWaitCount    () const { return m_waits; }
    size_t   GetRunCount     () const { return m_runs; }

private:
    struct Item
    {
        WorkFunction  function = nullptr;
        void        * context  = nullptr;
    };

    std::deque<Item>  m_items;
    size_t            m_waits = 0;
    size_t            m_runs  = 0;
};
