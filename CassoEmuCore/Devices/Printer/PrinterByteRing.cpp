#include "Pch.h"

#include "Devices/Printer/PrinterByteRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TryPush
//
//  CPU thread only. Wait-free. The producer owns the tail counter; it reads
//  its own tail relaxed and the consumer's head with acquire so freed slots
//  are visible. If the ring is full, returns false. The store of the byte
//  into m_slots[] happens-before the release-store of the new tail, so any
//  consumer that sees the new tail also sees the written slot.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterByteRing::TryPush (Byte value) noexcept
{
    uint32_t  tail     = m_tail.load (std::memory_order_relaxed);
    uint32_t  head     = m_head.load (std::memory_order_acquire);
    uint32_t  inFlight = tail - head;

    if (inFlight >= kByteRingCapacity)
    {
        return false;
    }

    m_slots[tail & kRingMask] = value;
    m_tail.store (tail + 1, std::memory_order_release);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  TryPop
//
//  UI thread only. Mirror of TryPush. The consumer owns head and reads the
//  producer's tail with acquire to pick up the latest published slot. The
//  byte is copied out before the release-store of the new head so the
//  producer can safely overwrite the slot on its next push.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterByteRing::TryPop (Byte & out) noexcept
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
//  UI thread only. Pops up to `maxCount` bytes in FIFO order. Folds the
//  per-byte head update into a single release-store at the end so the drain
//  is one acquire + N relaxed copies + one release.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PrinterByteRing::Drain (Byte * out, uint32_t maxCount) noexcept
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
//  Advisory diagnostic. The result may be stale by the time the caller uses
//  it; both indices are read relaxed because no synchronization decision
//  rides on the result.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PrinterByteRing::ApproxSize() const noexcept
{
    uint32_t  tail = m_tail.load (std::memory_order_relaxed);
    uint32_t  head = m_head.load (std::memory_order_relaxed);

    return tail - head;
}




////////////////////////////////////////////////////////////////////////////////
//
//  FreeSpace
//
//  Producer-side (CPU thread) free-slot count feeding the card's ready bit.
//  The consumer head is read with acquire so slots freed by a recent drain
//  are counted, letting ready re-assert promptly once the UI thread catches
//  up.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PrinterByteRing::FreeSpace() const noexcept
{
    uint32_t  tail     = m_tail.load (std::memory_order_relaxed);
    uint32_t  head     = m_head.load (std::memory_order_acquire);
    uint32_t  inFlight = tail - head;

    return kByteRingCapacity - inFlight;
}
