#include "Pch.h"

#include "EmuTests/ThreadAllocationCounter.h"





std::atomic<uint64_t>  ThreadAllocationCounter::s_count    = 0;
std::atomic<DWORD>     ThreadAllocationCounter::s_threadId = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  ~ThreadAllocationCounter
//
////////////////////////////////////////////////////////////////////////////////

ThreadAllocationCounter::~ThreadAllocationCounter()
{
    if (m_isRunning)
    {
        Stop();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Counts from zero the allocations the calling thread makes from here on.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadAllocationCounter::Start()
{
    s_count    = 0;
    s_threadId = GetCurrentThreadId();

    m_previous  = _CrtSetAllocHook (OnAllocation);
    m_isRunning = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Puts the previous hook back and gives the count.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ThreadAllocationCounter::Stop()
{
    if (m_isRunning)
    {
        _CrtSetAllocHook (m_previous);

        s_threadId  = 0;
        m_isRunning = false;
    }

    return s_count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ThreadAllocationCounter::GetCount() const
{
    return s_count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAllocation
//
//  Counts a new block or a reallocation on the counted thread, and lets
//  every request through.
//
////////////////////////////////////////////////////////////////////////////////

int __cdecl ThreadAllocationCounter::OnAllocation (
    int                    allocType,
    void                 * userData,
    size_t                 size,
    int                    blockType,
    long                   request,
    const unsigned char  * file,
    int                    line)
{
    UNREFERENCED_PARAMETER (userData);
    UNREFERENCED_PARAMETER (size);
    UNREFERENCED_PARAMETER (blockType);
    UNREFERENCED_PARAMETER (request);
    UNREFERENCED_PARAMETER (file);
    UNREFERENCED_PARAMETER (line);

    if ((allocType == _HOOK_ALLOC || allocType == _HOOK_REALLOC) && GetCurrentThreadId() == s_threadId)
    {
        s_count++;
    }

    return TRUE;
}





