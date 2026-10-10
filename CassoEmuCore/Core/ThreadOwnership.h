#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadOwnership
//
//  The one thread allowed to use an object that has no lock. The thread that
//  constructs the token holds it first. It moves by Release on the holder and
//  then Claim on the thread that takes over, and between the two no thread
//  holds it.
//
//  A claim by the holder and a release of an unowned token change nothing,
//  because some callers claim whether or not anything ever took the token
//  away. A claim while another thread holds the token, or a release by a
//  thread other than the holder, is a bug: it asserts and leaves the holder as
//  it was.
//
//  ASSERT_THREAD_OWNERSHIP goes first in each guarded entry point. In Debug it
//  is an EHM assert at the call site, so each entry point reports as its own
//  site; in Release it compiles to nothing. IsHeldByCurrentThread is compiled
//  in both, so a handover can be tested in Release too.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadOwnership
{
public:
                       ThreadOwnership       () = default;
                       ThreadOwnership       (const ThreadOwnership &) = delete;
    ThreadOwnership &  operator=             (const ThreadOwnership &) = delete;

    void               Claim                 ();
    void               Release               ();
    bool               IsHeldByCurrentThread () const;

private:
    std::atomic<std::thread::id>  m_holder { std::this_thread::get_id() };
};


#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
    #define ASSERT_THREAD_OWNERSHIP(__ownership)                                \
        {                                                                       \
            bool  __isOwnershipHeld = (__ownership).IsHeldByCurrentThread();    \
            ASSERT (__isOwnershipHeld);                                         \
        }
#else
    #define ASSERT_THREAD_OWNERSHIP(__ownership)  ((void) 0)
#endif
