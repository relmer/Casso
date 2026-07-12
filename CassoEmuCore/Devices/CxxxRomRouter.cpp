#include "Pch.h"

#include "CxxxRomRouter.h"
#include "Apple2eMmu.h"
#include "Ehm.h"





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
static constexpr int   kSlot3              = 3;
static constexpr Byte  kFloatingBusByte    = 0xFF;
static constexpr int   kAddressPageShift   = 8;
static constexpr int   kSlotNibbleMask     = 0x0F;
static constexpr Word  kPageOffsetMask     = 0x00FF;





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

    if (m_internal.size() < kInternalRomSize)
    {
        m_internal.resize (kInternalRomSize, kFloatingBusByte);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSlotRom
//
//  Installs a slot's 256-byte $Cn00 ROM page. A slot index outside 1..7 is
//  a caller bug and asserts.
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::SetSlotRom (int slot, vector<Byte> data)
{
    HRESULT  hr = S_OK;



    CBRAEx (slot >= kMinSlot && slot <= kMaxSlot, E_INVALIDARG);

    m_slotRom[slot] = move (data);

    if (m_slotRom[slot].size() < kSlotRomPageSize)
    {
        m_slotRom[slot].resize (kSlotRomPageSize, kFloatingBusByte);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSlotRom
//
//  True if slot `slot` has a ROM page installed. A slot index outside 1..7
//  is a caller bug and asserts.
//
////////////////////////////////////////////////////////////////////////////////

bool CxxxRomRouter::HasSlotRom (int slot) const
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    CBRAEx (slot >= kMinSlot && slot <= kMaxSlot, E_INVALIDARG);

    result = !m_slotRom[slot].empty();

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSlotIoDevice
//
//  Registers (or clears, when `device` is nullptr) the active I/O device
//  owning a slot's $Cn00 page. A slot index outside 1..7 is a caller bug
//  and asserts.
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::SetSlotIoDevice (int slot, MemoryDevice * device)
{
    HRESULT  hr = S_OK;



    CBRAEx (slot >= kMinSlot && slot <= kMaxSlot, E_INVALIDARG);

    m_slotIoDevice[slot] = device;

Error:
    return;
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
    HRESULT         hr     = S_OK;
    MemoryDevice *  result = nullptr;
    int             slot   = static_cast<int> ((address >> kAddressPageShift) & kSlotNibbleMask);



    BAIL_OUT_IF (m_mmu.GetIntCxRom(), S_OK);
    BAIL_OUT_IF (address >= kExpansionRomStart || slot < kMinSlot || slot > kMaxSlot, S_OK);
    BAIL_OUT_IF (slot == kSlot3 && !m_mmu.GetSlotC3Rom(), S_OK);

    result = m_slotIoDevice[slot];

Error:
    return result;
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
        if (!m_mmu.GetIntCxRom() && !m_mmu.GetSlotC3Rom())
        {
            m_mmu.SetIntC8Rom (true);
        }
    }

    if (address == kIntC8RomClearAddr)
    {
        m_mmu.ResetIntC8Rom();
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Writes are ignored (ROM); a slot I/O page is delegated to its device,
//  and the $CFFF side effect (STA $CFFF to deactivate expansion ROM) is
//  preserved. The two are mutually exclusive by address, so no page ever
//  both delegates and clears INTC8ROM.
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::Write (Word address, Byte value)
{
    MemoryDevice *  io = SlotIoDeviceFor (address);



    if (io != nullptr)
    {
        io->Write (address, value);
    }
    else if (address == kIntC8RomClearAddr)
    {
        m_mmu.ResetIntC8Rom();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void CxxxRomRouter::Reset()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveByte
//
//  Maps an address to the active byte source per audit §8. Out-of-range
//  addresses and unmapped slots read as the floating bus.
//
////////////////////////////////////////////////////////////////////////////////

Byte CxxxRomRouter::ResolveByte (Word address)
{
    HRESULT  hr        = S_OK;
    Byte     result    = kFloatingBusByte;
    bool     intCx     = false;
    bool     slotC3    = false;
    bool     intC8     = false;
    bool     inSlot3   = false;
    bool     inExp     = false;
    Word     romOffset = 0;
    int      slot      = 0;
    Word     pageOff   = 0;



    BAIL_OUT_IF (address < kCxxxRouterStart || address > kCxxxRouterEnd, S_OK);

    intCx     = m_mmu.GetIntCxRom();
    slotC3    = m_mmu.GetSlotC3Rom();
    intC8     = m_mmu.GetIntC8Rom();
    inSlot3   = (address >= kSlot3PageStart    && address <= kSlot3PageEnd);
    inExp     = (address >= kExpansionRomStart && address <= kExpansionRomLast);
    romOffset = static_cast<Word> (address - kCxxxRouterStart);
    slot      = static_cast<int>  ((address >> kAddressPageShift) & kSlotNibbleMask);
    pageOff   = static_cast<Word> (address & kPageOffsetMask);

    if (intCx || (inSlot3 && !slotC3) || (inExp && intC8))
    {
        result = (romOffset < m_internal.size()) ? m_internal[romOffset] : kFloatingBusByte;
    }
    else if (inExp)
    {
        result = kFloatingBusByte;
    }
    else if (slot >= kMinSlot && slot <= kMaxSlot && !m_slotRom[slot].empty())
    {
        result = (pageOff < m_slotRom[slot].size()) ? m_slotRom[slot][pageOff] : kFloatingBusByte;
    }

Error:
    return result;
}
