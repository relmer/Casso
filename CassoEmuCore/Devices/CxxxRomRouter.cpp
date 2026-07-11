#include "Pch.h"

#include "CxxxRomRouter.h"
#include "Apple2eMmu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Memory map constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Word  kCxxxRouterStart    = 0xC100;
static constexpr Word  kCxxxRouterEnd      = 0xCFFF;
static constexpr Word  kSlot3PageStart     = 0xC300;
static constexpr Word  kSlot3PageEnd       = 0xC3FF;
static constexpr Word  kExpansionRomStart  = 0xC800;
static constexpr Word  kExpansionRomLast   = 0xCFFF;
static constexpr Word  kIntC8RomClearAddr  = 0xCFFF;
static constexpr Word  kSlotRomPageSize    = 0x0100;
static constexpr Word  kInternalRomSize    = 0x0F00;
static constexpr int   kMinSlot            = 1;
static constexpr int   kMaxSlot            = 7;
static constexpr Byte  kFloatingBusByte    = 0xFF;





////////////////////////////////////////////////////////////////////////////////
//
//  CxxxRomRouter
//
////////////////////////////////////////////////////////////////////////////////

CxxxRomRouter::CxxxRomRouter (Apple2eMmu & mmu)
    : m_mmu (mmu)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInternalRom
//
//  Internal //e $C100-$CFFF ROM image (3840 bytes). Smaller arrays are
//  zero-padded out to size for safety.
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::SetInternalRom (vector<Byte> data)
{
    m_internal = move (data);

    if (m_internal.size () < kInternalRomSize)
    {
        m_internal.resize (kInternalRomSize, kFloatingBusByte);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSlotRom
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::SetSlotRom (int slot, vector<Byte> data)
{
    if (slot < kMinSlot || slot > kMaxSlot)
    {
        return;
    }

    m_slotRom[slot] = move (data);

    if (m_slotRom[slot].size () < kSlotRomPageSize)
    {
        m_slotRom[slot].resize (kSlotRomPageSize, kFloatingBusByte);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSlotRom
//
////////////////////////////////////////////////////////////////////////////////

bool CxxxRomRouter::HasSlotRom (int slot) const
{
    if (slot < kMinSlot || slot > kMaxSlot)
    {
        return false;
    }

    return !m_slotRom[slot].empty ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSlotIoDevice
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::SetSlotIoDevice (int slot, MemoryDevice * device)
{
    if (slot < kMinSlot || slot > kMaxSlot)
    {
        return;
    }

    m_slotIoDevice[slot] = device;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SlotIoDeviceFor
//
//  Returns the active I/O device owning `address`'s slot page, or nullptr
//  if the address should resolve to ROM. Slot cards are only visible when
//  INTCXROM=0; slot 3 additionally yields to the internal 80-column
//  firmware unless SLOTC3ROM=1, and the $C800+ expansion window is never a
//  slot I/O page.
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * CxxxRomRouter::SlotIoDeviceFor (Word address) const
{
    int   slot = static_cast<int> ((address >> 8) & 0x0F);

    if (m_mmu.GetIntCxRom ())
    {
        return nullptr;
    }

    if (address >= kExpansionRomStart || slot < kMinSlot || slot > kMaxSlot)
    {
        return nullptr;
    }

    if (slot == 3 && !m_mmu.GetSlotC3Rom ())
    {
        return nullptr;
    }

    return m_slotIoDevice[slot];
}




////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Resolves the byte then handles the $CFFF post-read side effect
//  (clears INTC8ROM, deactivating expansion ROM).
//
////////////////////////////////////////////////////////////////////////////////

Byte CxxxRomRouter::Read (Word address)
{
    MemoryDevice *  io    = SlotIoDeviceFor (address);
    Byte            value = (io != nullptr) ? io->Read (address) : ResolveByte (address);



    if (address >= kSlot3PageStart && address <= kSlot3PageEnd)
    {
        if (!m_mmu.GetIntCxRom () && !m_mmu.GetSlotC3Rom ())
        {
            m_mmu.SetIntC8Rom (true);
        }
    }

    if (address == kIntC8RomClearAddr)
    {
        m_mmu.ResetIntC8Rom ();
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Writes are ignored (ROM); only the $CFFF side effect is preserved so
//  software using STA $CFFF to deactivate expansion ROM still works.
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::Write (Word address, Byte value)
{
    MemoryDevice *  io = SlotIoDeviceFor (address);

    if (io != nullptr)
    {
        io->Write (address, value);
        return;
    }

    if (address == kIntC8RomClearAddr)
    {
        m_mmu.ResetIntC8Rom ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::Reset ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveByte
//
//  Maps an address to the active byte source per audit §8.
//
////////////////////////////////////////////////////////////////////////////////

Byte CxxxRomRouter::ResolveByte (Word address)
{
    if (address < kCxxxRouterStart || address > kCxxxRouterEnd)
    {
        return kFloatingBusByte;
    }

    bool  intCx     = m_mmu.GetIntCxRom ();
    bool  slotC3    = m_mmu.GetSlotC3Rom ();
    bool  intC8     = m_mmu.GetIntC8Rom ();
    bool  inSlot3   = (address >= kSlot3PageStart    && address <= kSlot3PageEnd);
    bool  inExp     = (address >= kExpansionRomStart && address <= kExpansionRomLast);
    Word  romOffset = static_cast<Word> (address - kCxxxRouterStart);



    if (intCx)
    {
        return romOffset < m_internal.size () ? m_internal[romOffset] : kFloatingBusByte;
    }

    if (inSlot3 && !slotC3)
    {
        return romOffset < m_internal.size () ? m_internal[romOffset] : kFloatingBusByte;
    }

    if (inExp)
    {
        if (intC8)
        {
            return romOffset < m_internal.size () ? m_internal[romOffset] : kFloatingBusByte;
        }

        return kFloatingBusByte;
    }

    int   slot    = static_cast<int> ((address >> 8) & 0x0F);
    Word  pageOff = static_cast<Word> (address & 0xFF);

    if (slot < kMinSlot || slot > kMaxSlot || m_slotRom[slot].empty ())
    {
        return kFloatingBusByte;
    }

    return pageOff < m_slotRom[slot].size () ? m_slotRom[slot][pageOff] : kFloatingBusByte;
}
