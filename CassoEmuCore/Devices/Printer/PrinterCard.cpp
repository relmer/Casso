#include "Pch.h"

#include "Devices/Printer/PrinterCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterCard
//
//  Computes the 16-byte I/O window from the slot number. The base and stride
//  place slot 1 at $C090-$C09F, slot 2 at $C0A0, and so on.
//
////////////////////////////////////////////////////////////////////////////////

PrinterCard::PrinterCard (int slot)
{
    m_slot    = slot;
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + kSlotIoSize - 1);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Every offset in the window reads as status (tolerant decode -- some
//  drivers read the latch address for status, others read +$1 or beyond).
//  No guest-observable state machine exists; the value is purely a function
//  of ring headroom.
//
////////////////////////////////////////////////////////////////////////////////

Byte PrinterCard::Read (Word address)
{
    (void) address;

    return ReadStatus();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write to +$0 latches the byte into the ring and arms the first-touch
//  flag; writes to any other offset are ignored. A failed push means the
//  guest wrote past the high-water guard it was told to honor -- a
//  programming error, caught in debug.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterCard::Write (Word address, Byte value)
{
    Word   offset = (Word) (address - m_ioStart);
    bool   pushed = false;

    if (offset != kDataOffset)
    {
        return;
    }

    m_everTouched = true;
    pushed        = m_ring.TryPush (value);
    ASSERT (pushed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Re-arms the first-touch reveal for a fresh engagement. In-flight ring
//  bytes and the downstream paper strip are unaffected -- the strip persists
//  across resets (FR-026) and the ring is drained by the presenter.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterCard::Reset ()
{
    m_everTouched = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadStatus
//
//  Ready while the ring has comfortable headroom; busy within the high-water
//  margin so a handshake-honoring guest waits instead of overflowing.
//
////////////////////////////////////////////////////////////////////////////////

Byte PrinterCard::ReadStatus () const
{
    if (m_ring.FreeSpace() > kReadyHighWater)
    {
        return kStatusReady;
    }

    return kStatusBusy;
}
