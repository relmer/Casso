#pragma once

#include "Pch.h"

#include "Core/ThreadOwnership.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OwnerThreadStandIn
//
//  A thread that stands in for the CPU thread in a test: it holds an object's
//  ThreadOwnership token, so a call the test thread makes into that object is
//  a call from off its owner, and work the owner must do runs here through
//  RunOnOwner.
//
//  Take releases the token on the test thread and has the stand-in claim it.
//  GiveBack has the stand-in release it and the test thread claim it again,
//  and the destructor does the same if the test did not. A C++ exception
//  thrown by work on the stand-in comes back from RunOnOwner to the thread
//  that called it, and so does an EHM assertion raised there, as a
//  std::runtime_error with the assertion's text. A failed Assert does not: it
//  raises SEH exception 0xE3530001, which catch (...) does not catch under
//  /EHsc, and off the test thread it ends the test host. Work that checks
//  something there writes the result to a variable, and the test thread
//  asserts on it. Every wait ends after five seconds with ERROR_TIMEOUT.
//
////////////////////////////////////////////////////////////////////////////////

class OwnerThreadStandIn
{
public:
             OwnerThreadStandIn  () = default;
             ~OwnerThreadStandIn ();

             OwnerThreadStandIn  (const OwnerThreadStandIn &) = delete;
    OwnerThreadStandIn & operator= (const OwnerThreadStandIn &) = delete;

    HRESULT  Take                (ThreadOwnership & ownership);
    HRESULT  RunOnOwner          (const std::function<void ()> & work);
    HRESULT  GiveBack            ();

private:
    void     Run                 ();
    HRESULT  Stop                ();

    static constexpr std::chrono::seconds  s_kWaitLimit { 5 };

    std::mutex                       m_lock;
    std::condition_variable          m_wake;
    ThreadOwnership                * m_ownership  = nullptr;
    const std::function<void ()>   * m_work       = nullptr;
    std::exception_ptr               m_failure;
    bool                             m_isClaimed  = false;
    bool                             m_isWorkDone = false;
    bool                             m_isStopping = false;
    bool                             m_isReleased = false;
    std::thread                      m_thread;
};
