#include "Pch.h"

#include "DiskIIEventRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TryPush
//
//  CPU thread only. Wait-free. The producer owns the tail counter; it
//  reads its own tail relaxed (no other thread writes to it) and reads
//  the consumer's head with acquire so any consumer-side update to the
//  free-slot count is visible. If the ring is full, returns false and
//  the caller bumps its drop counter (FR-010).
//
//  The store of the event into m_slots[] happens-before the
//  release-store of the new tail: any consumer that subsequently sees
//  the new tail will also see the fully-written slot.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIEventRing::TryPush (const DiskIIEvent & e) noexcept
{
    uint32_t  tail     = m_tail.load (std::memory_order_relaxed);
    uint32_t  head     = m_head.load (std::memory_order_acquire);
    uint32_t  inFlight = tail - head;

    if (inFlight >= kEventRingCapacity)
    {
        return false;
    }

    m_slots[tail & kRingMask] = e;
    m_tail.store (tail + 1, std::memory_order_release);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPop
//
//  UI thread only. Mirror of TryPush. The consumer owns head and reads
//  the producer's tail with acquire to pick up the latest published
//  slot. The slot is copied out before the release-store of the new
//  head so the producer can safely overwrite it on its next push.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIEventRing::TryPop (DiskIIEvent & out) noexcept
{
    uint32_t  head = m_head.load (std::memory_order_relaxed);
    uint32_t  tail = m_tail.load (std::memory_order_acquire);

    if (head == tail)
    {
        return false;
    }

    out = m_slots[head & kRingMask];
    m_head.store (head + 1, std::memory_order_release);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Drain
//
//  UI thread only. Pops up to `maxCount` entries in FIFO order. Folds
//  the per-entry head update into a single release-store at the end so
//  the drain is one acquire + N relaxed copies + one release instead of
//  N acquire/release pairs.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskIIEventRing::Drain (DiskIIEvent * out, uint32_t maxCount) noexcept
{
    uint32_t  head     = m_head.load (std::memory_order_relaxed);
    uint32_t  tail     = m_tail.load (std::memory_order_acquire);
    uint32_t  inFlight = tail - head;
    uint32_t  toCopy   = (inFlight < maxCount) ? inFlight : maxCount;
    uint32_t  i        = 0;

    for (i = 0; i < toCopy; i++)
    {
        out[i] = m_slots[(head + i) & kRingMask];
    }

    if (toCopy > 0)
    {
        m_head.store (head + toCopy, std::memory_order_release);
    }

    return toCopy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApproxSize
//
//  Advisory diagnostic. The result may be stale by the time the caller
//  uses it; both indices are read relaxed because no synchronization
//  decision rides on the result.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskIIEventRing::ApproxSize () const noexcept
{
    uint32_t  tail = m_tail.load (std::memory_order_relaxed);
    uint32_t  head = m_head.load (std::memory_order_relaxed);

    return tail - head;
}
