#include "Pch.h"

#include "EmuTests/OwnerThreadStandIn.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::~OwnerThreadStandIn
//
//  Gives the token back to the test thread if the test did not. A stand-in
//  that never released it leaves the token alone rather than claim over it,
//  since that claim would assert in a destructor; its thread is still joined,
//  because destroying a running std::thread ends the process.
//
////////////////////////////////////////////////////////////////////////////////

OwnerThreadStandIn::~OwnerThreadStandIn()
{
    HRESULT  hr = S_OK;



    hr = Stop();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::Take
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OwnerThreadStandIn::Take (ThreadOwnership & ownership)
{
    HRESULT                       hr        = S_OK;
    bool                          isClaimed = false;
    bool                          isIdle    = !m_thread.joinable();
    std::unique_lock<std::mutex>  held (m_lock, std::defer_lock);



    CBRAEx (isIdle, E_UNEXPECTED);

    m_ownership  = &ownership;
    m_isClaimed  = false;
    m_isStopping = false;
    m_isReleased = false;

    ownership.Release();

    m_thread = std::thread ([this] { Run(); });

    held.lock();
    isClaimed = m_wake.wait_for (held, s_kWaitLimit, [this] { return m_isClaimed; });
    held.unlock();

    CBREx (isClaimed, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::RunOnOwner
//
//  Runs work on the stand-in and waits for it. An exception the work throws
//  is rethrown here, on the test thread.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OwnerThreadStandIn::RunOnOwner (const std::function<void()> & work)
{
    HRESULT                       hr         = S_OK;
    bool                          isRunning  = false;
    bool                          isDone     = false;
    std::exception_ptr            failure;
    std::unique_lock<std::mutex>  held (m_lock);



    isRunning = m_thread.joinable() && m_isClaimed && !m_isStopping;
    CBRAEx (isRunning, E_UNEXPECTED);

    m_work       = &work;
    m_isWorkDone = false;
    m_failure    = nullptr;
    m_wake.notify_all();

    isDone = m_wake.wait_for (held, s_kWaitLimit, [this] { return m_isWorkDone; });
    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    failure = m_failure;

Error:
    m_work = nullptr;
    held.unlock();

    if (failure)
    {
        std::rethrow_exception (failure);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::GiveBack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OwnerThreadStandIn::GiveBack()
{
    return Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::Stop
//
//  Has the stand-in release the token and end, joins it, and claims the token
//  on the calling thread. A stand-in that does not release in time is left
//  running and holding the token, so nothing asserts here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OwnerThreadStandIn::Stop()
{
    HRESULT                       hr         = S_OK;
    bool                          isRunning  = m_thread.joinable();
    bool                          isReleased = false;
    std::unique_lock<std::mutex>  held (m_lock, std::defer_lock);



    BAIL_OUT_IF (!isRunning, S_OK);

    held.lock();
    m_isStopping = true;
    m_wake.notify_all();
    isReleased = m_wake.wait_for (held, s_kWaitLimit, [this] { return m_isReleased; });
    held.unlock();

    CBREx (isReleased, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    m_thread.join();
    m_ownership->Claim();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn::Run
//
//  The stand-in's thread: claims the token, runs each piece of work it is
//  given, and releases the token when told to stop. It waits without a limit
//  of its own, because every caller that wakes it waits with one.
//
////////////////////////////////////////////////////////////////////////////////

void OwnerThreadStandIn::Run()
{
    std::unique_lock<std::mutex>  held (m_lock);
    std::exception_ptr            failure;



    m_ownership->Claim();
    m_isClaimed = true;
    m_wake.notify_all();

    while (!m_isStopping)
    {
        m_wake.wait (held, [this] { return m_isStopping || (m_work != nullptr && !m_isWorkDone); });

        if (m_work != nullptr && !m_isWorkDone)
        {
            const std::function<void()>  & work = *m_work;

            held.unlock();

            try
            {
                work();
            }
            catch (...)
            {
                failure = std::current_exception();
            }

            held.lock();

            m_failure    = failure;
            m_isWorkDone = true;
            failure      = nullptr;
            m_wake.notify_all();
        }
    }

    m_ownership->Release();
    m_isReleased = true;
    m_wake.notify_all();
}
