#include "Pch.h"

#include "Core/ThreadOwnership.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadOwnership::Claim
//
//  An unowned token passes to the calling thread. The holder claiming it again
//  changes nothing. Any other holder keeps it, and the claim asserts.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadOwnership::Claim()
{
    HRESULT          hr       = S_OK;
    std::thread::id  self     = std::this_thread::get_id();
    std::thread::id  holder;
    bool             isTaken  = false;
    bool             isMine   = false;



    isTaken = m_holder.compare_exchange_strong (holder, self, std::memory_order_acq_rel);
    isMine  = isTaken || holder == self;

    CBRA (isMine);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadOwnership::Release
//
//  The holder gives the token up, leaving it unowned. Releasing an unowned
//  token changes nothing. A release by a thread other than the holder leaves
//  the holder as it was, and asserts.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadOwnership::Release()
{
    HRESULT          hr          = S_OK;
    std::thread::id  self        = std::this_thread::get_id();
    std::thread::id  holder      = self;
    bool             isReleased  = false;
    bool             isUnowned   = false;
    bool             isAllowed   = false;



    isReleased = m_holder.compare_exchange_strong (holder, std::thread::id(), std::memory_order_acq_rel);
    isUnowned  = holder == std::thread::id();
    isAllowed  = isReleased || isUnowned;

    CBRA (isAllowed);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadOwnership::IsHeldByCurrentThread
//
////////////////////////////////////////////////////////////////////////////////

bool ThreadOwnership::IsHeldByCurrentThread() const
{
    return m_holder.load (std::memory_order_acquire) == std::this_thread::get_id();
}
