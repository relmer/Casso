#pragma once

#include "Pch.h"

#include "InputEvent.h"




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventRing
//
//  Fixed-capacity single-producer / single-consumer lock-free ring
//  buffer carrying InputEvent values from the CPU emulation thread
//  (producer) to the UI thread (consumer). Power-of-two capacity so the
//  index-to-slot mapping is a cheap mask.
//
//  Contract:
//    * Exactly one thread calls TryPush at a time (the CPU thread). Both
//      the guest soft-switch reads and the //e auto-repeat re-latch fire
//      on that thread; real Windows key-down/up events run on the UI
//      thread and are staged by the panel WITHOUT touching this ring.
//    * Exactly one thread calls TryPop / Drain at a time (the UI thread).
//    * Head/tail are 32-bit unsigned counters; the mask isolates the
//      slot index. Counter overflow is harmless because subtraction
//      modulo 2^32 still yields the in-flight count (Vyukov 2010).
//    * No locks, no condition variables, no kernel calls on the push /
//      pop fast paths.
//
//  alignas(64) on the two indices keeps the producer and consumer cache
//  lines from false-sharing during a steady-state burst.
//
////////////////////////////////////////////////////////////////////////////////

class InputEventRing
{
public:
    static constexpr uint32_t   kEventRingCapacity = 4096;
    static constexpr uint32_t   kRingMask          = kEventRingCapacity - 1;

    static_assert ((kEventRingCapacity & kRingMask) == 0,
                   "kEventRingCapacity must be a power of two");

    InputEventRing() = default;

    // CPU thread only. Returns false when the ring is full (caller
    // should bump its own drop counter and continue).
    bool       TryPush    (const InputEvent & e) noexcept;

    // UI thread only. Returns false when the ring is empty.
    bool       TryPop     (InputEvent & out) noexcept;

    // UI thread only. Pops up to `maxCount` entries into `out` and
    // returns the number actually written. `out` must point at storage
    // for at least `maxCount` events.
    uint32_t   Drain      (InputEvent * out, uint32_t maxCount) noexcept;

    // Advisory; either thread may call. Result may be stale by the time
    // the caller observes it. Intended only for diagnostics.
    uint32_t   ApproxSize() const noexcept;

private:
    alignas(64) std::atomic<uint32_t>   m_head { 0 };   // consumer-owned
    alignas(64) std::atomic<uint32_t>   m_tail { 0 };   // producer-owned
    InputEvent                          m_slots[kEventRingCapacity] {};
};
