#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SpscEventRing
//
//  A fixed-capacity, lock-free, single-producer single-consumer ring of
//  events, for a device on the emulation thread to report to the UI thread.
//  The capacity is a power of two, so a counter maps to a slot by a mask.
//
//  Exactly one thread pushes and exactly one thread pops. A push to a full
//  ring is dropped and counted, and the consumer reads the count to report
//  the loss.
//
////////////////////////////////////////////////////////////////////////////////

template <typename TEvent, uint32_t TCapacity = 1024>
class SpscEventRing
{
public:
    static constexpr uint32_t  kCapacity = TCapacity;
    static constexpr uint32_t  kMask     = TCapacity - 1;

    static_assert ((TCapacity & kMask) == 0, "the capacity must be a power of two");

    //  Producer only. False when the ring is full; the drop is counted.
    bool TryPush (const TEvent & event) noexcept
    {
        uint32_t  tail    = m_tail.load (std::memory_order_relaxed);
        uint32_t  head    = m_head.load (std::memory_order_acquire);
        bool      hasRoom = (tail - head) < kCapacity;



        if (hasRoom)
        {
            m_slots[tail & kMask] = event;
            m_tail.store (tail + 1, std::memory_order_release);
        }
        else
        {
            m_dropped.fetch_add (1, std::memory_order_relaxed);
        }

        return hasRoom;
    }

    //  Consumer only. False when the ring is empty.
    bool TryPop (TEvent & outEvent) noexcept
    {
        uint32_t  head    = m_head.load (std::memory_order_relaxed);
        uint32_t  tail    = m_tail.load (std::memory_order_acquire);
        bool      hasItem = head != tail;



        if (hasItem)
        {
            outEvent = m_slots[head & kMask];
            m_head.store (head + 1, std::memory_order_release);
        }

        return hasItem;
    }

    //  Consumer only. The drops since the last call, which resets the count.
    uint64_t TakeDroppedCount () noexcept
    {
        return m_dropped.exchange (0, std::memory_order_relaxed);
    }

    uint32_t GetApproxSize () const noexcept
    {
        return m_tail.load (std::memory_order_relaxed) - m_head.load (std::memory_order_relaxed);
    }

private:
    alignas (64) std::atomic<uint32_t>  m_head    { 0 };
    alignas (64) std::atomic<uint32_t>  m_tail    { 0 };
    std::atomic<uint64_t>               m_dropped { 0 };
    std::array<TEvent, TCapacity>       m_slots   {};
};
