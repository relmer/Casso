#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadAllocationCounter
//
//  Counts the heap allocations one thread makes between Start and Stop,
//  through the debug CRT's allocation hook, so a test can hold a hot path to
//  allocating nothing. Debug builds only: the release CRT has no hook, so
//  IsAvailable is false there and a test must not claim a count it did not
//  take. The AddressSanitizer build allocates through the sanitizer's own
//  heap, which bypasses the hook, so it is unavailable there too. One counter
//  at a time.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadAllocationCounter
{
public:
    static constexpr bool  IsAvailable()
    {
#if defined(_DEBUG) && !defined(__SANITIZE_ADDRESS__)
        return true;
#else
        return false;
#endif
    }

             ThreadAllocationCounter  () = default;
             ~ThreadAllocationCounter ();

    void     Start    ();
    uint64_t Stop     ();
    uint64_t GetCount () const;

private:
    static int __cdecl  OnAllocation (int allocType, void * userData, size_t size, int blockType, long request, const unsigned char * file, int line);

    static std::atomic<uint64_t>  s_count;
    static std::atomic<DWORD>     s_threadId;

    _CRT_ALLOC_HOOK  m_previous  = nullptr;
    bool             m_isRunning = false;
};
