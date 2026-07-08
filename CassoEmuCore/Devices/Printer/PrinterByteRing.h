#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterByteRing
//
//  Fixed-capacity single-producer / single-consumer lock-free ring buffer
//  carrying raw printer data bytes from the CPU emulation thread (producer --
//  the slot card's Write) to the presenter tick on the UI thread (consumer).
//  Power-of-two capacity so the index-to-slot mapping is a cheap mask.
//
//  Capacity is deliberately large (64 KiB) -- orders of magnitude beyond the
//  fastest sustained 6502 store-loop burst across many drain intervals. The
//  card exposes a ready bit driven by FreeSpace: it de-asserts within a
//  high-water margin of capacity, so a guest honoring the handshake stalls
//  rather than overflows if the drain is delayed (e.g. a modal print dialog
//  holds the UI thread). Overflow past that guard is a programming error and
//  surfaces as TryPush returning false.
//
//  Concurrency contract mirrors InputEventRing / Disk2EventRing (standard
//  SPSC formulation, Vyukov 2010):
//    * Exactly one thread calls TryPush / FreeSpace (the CPU thread).
//    * Exactly one thread calls TryPop / Drain (the UI/presenter thread).
//    * Head/tail are 32-bit unsigned counters; the mask isolates the slot.
//      Counter overflow is harmless because subtraction modulo 2^32 still
//      yields the in-flight count.
//
//  alignas(64) on the two indices keeps the producer and consumer cache
//  lines from false-sharing during a steady-state burst.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterByteRing
{
public:
    static constexpr uint32_t   kByteRingCapacity = 65536;
    static constexpr uint32_t   kRingMask         = kByteRingCapacity - 1;

    static_assert ((kByteRingCapacity & kRingMask) == 0,
                   "kByteRingCapacity must be a power of two");

    PrinterByteRing() = default;

    // CPU thread only. Returns false when the ring is full.
    bool       TryPush    (Byte value) noexcept;

    // UI thread only. Returns false when the ring is empty.
    bool       TryPop     (Byte & out) noexcept;

    // UI thread only. Pops up to `maxCount` bytes into `out` and returns the
    // number actually written. `out` must hold at least `maxCount` bytes.
    uint32_t   Drain      (Byte * out, uint32_t maxCount) noexcept;

    // Advisory; either thread may call. May be stale when observed.
    uint32_t   ApproxSize () const noexcept;

    // Producer-side free-slot count (CPU thread). Drives the card's ready
    // bit; reads the consumer head with acquire so freed slots are seen.
    uint32_t   FreeSpace  () const noexcept;

private:
    alignas(64) std::atomic<uint32_t>   m_head { 0 };   // consumer-owned
    alignas(64) std::atomic<uint32_t>   m_tail { 0 };   // producer-owned
    Byte                                m_slots[kByteRingCapacity] {};
};
