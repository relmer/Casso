#include "Pch.h"

#include "Machines/Apple2/Common/PrinterCard.h"
#include "Core/MachineConfig.h"





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
    char    path[MAX_PATH] = {};
    size_t  length         = 0;



    (void) getenv_s (&length, path, kTextPathVariable);
    m_textPath = (length > 0) ? path : "";
    m_slot     = slot;
    m_ioStart  = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd    = (Word) (m_ioStart + kSlotIoSize - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The parallel printer defaults to slot 1 (Apple
//  II convention) when the config does not pin a slot.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> PrinterCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    int   slot = config.hasSlot ? config.slot : 1;



    (void) bus;

    return make_unique<PrinterCard> (slot);
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
    WriteTextCopy (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteTextCopy
//
//  A copy of what the guest prints, as text, for whoever set
//  CASSO_PRINTER_TEXT to a path. The printer itself renders dots on paper,
//  which is right for a printout and useless for reading the words back, so
//  this tee exists to capture a program's printed output: the high bit the
//  Apple II sets is dropped, and its carriage returns end lines.
//
//  Nothing is written when the variable is unset, which is every ordinary
//  run.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterCard::WriteTextCopy (Byte value)
{
    static constexpr Byte  kCarriageReturn = 0x0D;
    static constexpr Byte  kLineFeed       = 0x0A;
    char                   ch              = (char) (value & 0x7F);



    if (m_textPath.empty())
    {
        return;
    }

    if (!m_text.is_open())
    {
        m_text.open (m_textPath, std::ios::binary | std::ios::app);

        if (!m_text.is_open())
        {
            m_textPath.clear();
            return;
        }
    }

    //  The Apple II ends a line with a carriage return; a driver that adds
    //  a line feed of its own would otherwise double-space the copy.
    if (ch == kLineFeed)
    {
        return;
    }

    if (ch == kCarriageReturn)
    {
        m_text << '\n';
    }
    else
    {
        m_text << ch;
    }

    m_text.flush();
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

void PrinterCard::Reset()
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

Byte PrinterCard::ReadStatus() const
{
    return (m_ring.GetFreeBytes() > kReadyHighWater) ? kStatusReady : kStatusBusy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterCard::GetDiagnostics
//
//  The status byte as the guest reads it: bit 7 ready, and the low three bits
//  the Centronics lines a Grappler-style driver tests.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterCard::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    DiagnosticsGroup  group { "Card", {} };



    group.rows.push_back (MakeTextRow ("Slot",          std::format ("{}", m_slot)));
    group.rows.push_back (MakeByteRow ("Status",        ReadStatus(), { "READY", "", "", "", "", "PAPER OUT", "FAULT#", "SELECT" }));
    group.rows.push_back (MakeFlagRow ("Used",          m_everTouched));
    group.rows.push_back (MakeTextRow ("Bytes waiting", std::format ("{}", m_ring.GetApproxSize())));
    group.rows.push_back (MakeTextRow ("Space left",    std::format ("{}", m_ring.GetFreeBytes())));

    snapshot.groups.push_back (std::move (group));
}
