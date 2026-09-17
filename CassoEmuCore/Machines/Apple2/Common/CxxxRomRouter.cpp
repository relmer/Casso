#include "Pch.h"

#include "Machines/Apple2/Common/CxxxRomRouter.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"





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
//  GetFastMapReadPtr
//
//  See the header. Passive internal-ROM pages on the //c return a pointer into
//  m_internal; reactive pages ($C3, $CF) and all //e pages return null so the
//  Read handler runs.
//
////////////////////////////////////////////////////////////////////////////////

Byte * CxxxRomRouter::GetFastMapReadPtr (int page)
{
    static constexpr int    kPageSize = 0x100;
    size_t                  offset    = 0;
    Byte                  * ptr       = nullptr;



    // Three reasons to decline the fast path, all meaning "run the Read
    // handler instead":
    //   - slots exist (//e), or no internal image is loaded;
    //   - $C3xx latches INTC8ROM and $CFxx clears it, so those pages keep
    //     their side effects (inert on the //c, but modeled faithfully);
    //   - the page falls past the end of a short internal image.
    bool     passive = m_noExternalSlots
                       && !m_internal.empty()
                       && page != 0xC3
                       && page != 0xCF;

    offset = static_cast<size_t> ((page - 0xC1) * kPageSize);

    if (passive && offset + kPageSize <= m_internal.size())
    {
        ptr = m_internal.data() + offset;
    }

    return ptr;
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

    m_hasSlotIoDevice = false;
    for (MemoryDevice * io : m_slotIoDevice)
    {
        if (io != nullptr)
        {
            m_hasSlotIoDevice = true;
            break;
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSlotIoDevice
//
//  Returns the active I/O device owning `address`'s slot page, or nullptr
//  if the address should resolve to ROM. Slot cards are only visible when
//  INTCXROM=0; slot 3 additionally yields to the internal 80-column
//  firmware unless SLOTC3ROM=1, and the $C800+ expansion window is never a
//  slot I/O page.
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * CxxxRomRouter::GetSlotIoDevice (Word address) const
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
//  ResolveByte
//
//  Maps an address to the active byte source per audit §8. Out-of-range
//  addresses and unmapped slots read as the floating bus.
//
//  FORCED INLINE, AND DEFINED AHEAD OF READ. Read is the //e's hottest device
//  read: every fetch from the internal ROM at $C100-$CFFF comes through it, and
//  the idle Applesoft prompt's keyboard loop runs there. With TryPeek as a
//  second caller the compiler stopped inlining this into Read, which cost about
//  5% of emulation speed (Release x64, 50M //e cycles pinned to one core:
//  287.7 ms called, 273.9 ms inlined, 273.1 ms before TryPeek existed). The
//  internal-ROM test is written out for the same reason rather than calling
//  IsInternalRomSelected.
//
////////////////////////////////////////////////////////////////////////////////

__forceinline Byte CxxxRomRouter::ResolveByte (Word address) const
{
    HRESULT  hr         = S_OK;
    Byte     result     = kFloatingBusByte;
    bool     inSlot3    = false;
    bool     inExp      = false;
    bool     isInternal = false;
    Word     romOffset  = 0;
    int      slot       = 0;
    Word     pageOff    = 0;



    BAIL_OUT_IF (address < kCxxxRouterStart || address > kCxxxRouterEnd, S_OK);

    //  IsInternalRomSelected's test, written out (see above).
    inSlot3    = (address >= kSlot3PageStart    && address <= kSlot3PageEnd);
    inExp      = (address >= kExpansionRomStart && address <= kExpansionRomLast);
    isInternal = m_noExternalSlots || m_mmu.GetIntCxRom() || (inSlot3 && !m_mmu.GetSlotC3Rom()) || (inExp && m_mmu.GetIntC8Rom());
    romOffset  = static_cast<Word> (address - kCxxxRouterStart);
    slot       = static_cast<int>  ((address >> kAddressPageShift) & kSlotNibbleMask);
    pageOff    = static_cast<Word> (address & kPageOffsetMask);

    if (isInternal)
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
    Byte  value;



    if (m_noExternalSlots && !m_hasSlotIoDevice)
    {
        // Apple //c fast path: with no external slots the whole $C100-$CFFF
        // window is internal firmware regardless of INTCXROM/SLOTC3ROM/INTC8ROM
        // (see SetNoExternalSlots), and there is no slot device to delegate to.
        // Resolve the internal byte directly -- this is the //c hot path, e.g.
        // the mouse firmware executing from $C700 -- skipping GetSlotIoDevice
        // and ResolveByte's four MMU state pulls. The $C3xx/$CFFF side effects
        // below still run for fidelity.
        Word  off = static_cast<Word> (address - kCxxxRouterStart);
        value = (off < m_internal.size()) ? m_internal[off] : kFloatingBusByte;
    }
    else
    {
        MemoryDevice *  io = GetSlotIoDevice (address);
        value = (io != nullptr) ? io->Read (address) : ResolveByte (address);
    }

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
    MemoryDevice *  io = GetSlotIoDevice (address);



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
//  IsInternalRomSelected
//
//  Apple //c (m_noExternalSlots): with no external card slots the whole
//  $C100-$CFFF window (incl. the $C800 expansion space) is always internal
//  firmware regardless of INTCXROM/SLOTC3ROM/INTC8ROM -- the //c ROM enters
//  $C800 with them clear. On the //e this stays false and normal arbitration
//  applies.
//
////////////////////////////////////////////////////////////////////////////////

bool CxxxRomRouter::IsInternalRomSelected (Word address) const
{
    bool  inSlot3 = (address >= kSlot3PageStart    && address <= kSlot3PageEnd);
    bool  inExp   = (address >= kExpansionRomStart && address <= kExpansionRomLast);



    return m_noExternalSlots          ||
           m_mmu.GetIntCxRom()        ||
           (inSlot3 && !m_mmu.GetSlotC3Rom()) ||
           (inExp   &&  m_mmu.GetIntC8Rom());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeek
//
//  ResolveByte without Read's INTC8ROM side effects. A slot page delegated to
//  an I/O device has no side-effect-free answer.
//
////////////////////////////////////////////////////////////////////////////////

bool CxxxRomRouter::TryPeek (Word address, Byte & value) const
{
    bool  isDevicePage = GetSlotIoDevice (address) != nullptr;



    value = kFloatingBusByte;

    if (!isDevicePage)
    {
        value = ResolveByte (address);
    }

    return !isDevicePage;
}
