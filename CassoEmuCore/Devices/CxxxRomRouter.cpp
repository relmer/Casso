#include "Pch.h"

#include "CxxxRomRouter.h"
#include "AppleIIeMmu.h"





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

CxxxRomRouter::CxxxRomRouter (AppleIIeMmu & mmu)
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
//  Read
//
//  Resolves the byte then handles the $CFFF post-read side effect
//  (clears INTC8ROM, deactivating expansion ROM).
//
////////////////////////////////////////////////////////////////////////////////

Byte CxxxRomRouter::Read (Word address)
{
    Byte  value = ResolveByte (address);



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
    UNREFERENCED_PARAMETER (value);

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
