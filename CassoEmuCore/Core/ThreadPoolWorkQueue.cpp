#include "Pch.h"

#include "Core/ThreadPoolWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ~ThreadPoolWorkQueue
//
//  Waits for every item submitted, then closes the work object.
//
////////////////////////////////////////////////////////////////////////////////

ThreadPoolWorkQueue::~ThreadPoolWorkQueue()
{
    if (m_work != nullptr)
    {
        WaitForThreadpoolWorkCallbacks (m_work, FALSE);
        CloseThreadpoolWork (m_work);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Creates the work object and sizes the ring for capacity items. Called
//  once, before any Submit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThreadPoolWorkQueue::Create (size_t capacity)
{
    HRESULT  hr        = S_OK;
    bool     isUnused  = m_work == nullptr;
    bool     hasRoom   = capacity > 0;



    CBRAEx (isUnused, E_UNEXPECTED);
    CBRAEx (hasRoom,  E_INVALIDARG);

    m_work = CreateThreadpoolWork (OnWork, this, nullptr);
    CWRA (m_work);

    m_items.assign (capacity, Item());

    m_head  = 0;
    m_count = 0;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Submit
//
//  Puts the item at the back of the ring and queues one callback for it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThreadPoolWorkQueue::Submit (
    WorkFunction    function,
    void          * context)
{
    HRESULT  hr        = S_OK;
    bool     isCreated = m_work != nullptr;
    bool     hasRoom   = false;



    CBRAEx (isCreated, E_UNEXPECTED);
    CBRAEx (function,  E_INVALIDARG);

    AcquireSRWLockExclusive (&m_lock);

    hasRoom = m_count < m_items.size();

    if (hasRoom)
    {
        m_items[(m_head + m_count) % m_items.size()] = Item { function, context };
        m_count++;
    }

    ReleaseSRWLockExclusive (&m_lock);

    CBRAEx (hasRoom, HRESULT_FROM_WIN32 (ERROR_BUFFER_OVERFLOW));

    SubmitThreadpoolWork (m_work);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WaitAll
//
//  Returns once every callback queued so far has finished, and with it every
//  item submitted.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadPoolWorkQueue::WaitAll()
{
    if (m_work != nullptr)
    {
        WaitForThreadpoolWorkCallbacks (m_work, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnWork
//
//  One callback per submitted item, on a pool thread.
//
////////////////////////////////////////////////////////////////////////////////

void CALLBACK ThreadPoolWorkQueue::OnWork (
    PTP_CALLBACK_INSTANCE    instance,
    void                   * context,
    PTP_WORK                 work)
{
    UNREFERENCED_PARAMETER (instance);
    UNREFERENCED_PARAMETER (work);

    static_cast<ThreadPoolWorkQueue *> (context)->RunNext();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunNext
//
//  Takes the oldest item and runs it, under the run lock: two callbacks
//  running at once take their turns, and the one that runs first takes the
//  oldest item, so the order holds.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadPoolWorkQueue::RunNext()
{
    Item  item;



    AcquireSRWLockExclusive (&m_runLock);
    AcquireSRWLockExclusive (&m_lock);

    if (m_count != 0)
    {
        item   = m_items[m_head];
        m_head = (m_head + 1) % m_items.size();
        m_count--;
    }

    ReleaseSRWLockExclusive (&m_lock);

    if (item.function != nullptr)
    {
        item.function (item.context);
    }

    ReleaseSRWLockExclusive (&m_runLock);
}





