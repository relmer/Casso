#include "Pch.h"

#include "AppleIIeMmu.h"
#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/LanguageCard.h"
#include "Devices/AppleSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Memory map constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int  kAuxRamSize         = 0x10000;
static constexpr int  kPageSize           = 0x100;

static constexpr int  kZeroPageFirst      = 0x00;
static constexpr int  kZeroPageLast       = 0x01;

static constexpr int  kText04_07First     = 0x04;
static constexpr int  kText04_07Last      = 0x07;

static constexpr int  kHires20_3FFirst    = 0x20;
static constexpr int  kHires20_3FLast     = 0x3F;

static constexpr int  kMain02_BFFirst     = 0x02;
static constexpr int  kMain02_BFLast      = 0xBF;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeMmu
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeMmu::AppleIIeMmu ()
    : m_auxRam (kAuxRamSize, 0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Wires the MMU to the bus and to the main RAM buffer. Aux RAM is owned
//  internally (m_auxRam). Caches the bus pointer for later RebindPageTable
//  calls. The auxRam / internalRom / lc / ssBank parameters are accepted
//  for contract symmetry; current Phase 2 routing only needs the main RAM
//  page-table pointers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleIIeMmu::Initialize (
    MemoryBus            *   bus,
    RamDevice            *   mainRam,
    RamDevice            *   auxRam,
    RomDevice            *   internalRom,
    LanguageCard         *   lc,
    AppleSoftSwitchBank  *   ssBank)
{
    HRESULT  hr = S_OK;

    UNREFERENCED_PARAMETER (auxRam);
    UNREFERENCED_PARAMETER (internalRom);
    UNREFERENCED_PARAMETER (lc);

    CBR (bus     != nullptr);
    CBR (mainRam != nullptr);

    m_bus        = bus;
    m_mainRamPtr = mainRam->GetData ();
    m_ssBank     = ssBank;

    RebindPageTable ();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRamRd / SetRamWrt / SetAltZp / Set80Store / SetIntCxRom / SetSlotC3Rom
//
//  Each setter mutates the flag and remaps the affected page-table region.
//  No-op if state is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::SetRamRd (bool v)
{
    if (m_ramRd == v)
    {
        return;
    }

    m_ramRd = v;
    ResolveMain02_BF ();
}



void AppleIIeMmu::SetRamWrt (bool v)
{
    if (m_ramWrt == v)
    {
        return;
    }

    m_ramWrt = v;
    ResolveMain02_BF ();
}



void AppleIIeMmu::SetAltZp (bool v)
{
    if (m_altZp == v)
    {
        return;
    }

    m_altZp = v;
    ResolveZeroPage ();
}



void AppleIIeMmu::Set80Store (bool v)
{
    if (m_store80 == v)
    {
        return;
    }

    m_store80 = v;

    ResolveText04_07  ();
    ResolveHires20_3F ();
    ResolveMain02_BF  ();
}



void AppleIIeMmu::SetIntCxRom (bool v)
{
    m_intCxRom = v;
}



void AppleIIeMmu::SetSlotC3Rom (bool v)
{
    m_slotC3Rom = v;
}



void AppleIIeMmu::ResetIntC8Rom ()
{
    m_intC8Rom = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSoftSwitchChanged
//
//  Invoked by the soft-switch bank when display flags (PAGE2, HIRES, TEXT,
//  MIXED) change. The 80STORE-routed regions ($0400-$07FF text page and
//  $2000-$3FFF hires page) re-resolve here so per-access banking stays a
//  page-table lookup with zero branching.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::OnSoftSwitchChanged ()
{
    ResolveText04_07  ();
    ResolveHires20_3F ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSoftReset
//
//  Per audit §10 / Sather UTAIIe: soft reset on //e clears the MMU paging
//  flags but preserves RAM contents.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::OnSoftReset ()
{
    m_ramRd     = false;
    m_ramWrt    = false;
    m_altZp     = false;
    m_store80   = false;
    m_intCxRom  = false;
    m_slotC3Rom = false;
    m_intC8Rom  = false;

    RebindPageTable ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPowerCycle
//
//  Power-on defaults match soft reset for the flags. RAM seeding is
//  performed by the RamDevice power-cycle path (Phase 4).
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::OnPowerCycle ()
{
    OnSoftReset ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebindPageTable
//
//  Re-resolves every region that the MMU controls. Called once on
//  Initialize and on full resets.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::RebindPageTable ()
{
    ResolveZeroPage   ();
    ResolveMain02_BF  ();
    ResolveText04_07  ();
    ResolveHires20_3F ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveZeroPage
//
//  $0000-$01FF (zero page + stack). ALTZP routes the entire region to
//  aux RAM for both reads and writes.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::ResolveZeroPage ()
{
    Byte *  base = m_altZp ? m_auxRam.data () : m_mainRamPtr;

    if (m_bus == nullptr || base == nullptr)
    {
        return;
    }

    for (int page = kZeroPageFirst; page <= kZeroPageLast; page++)
    {
        Byte *  p = base + (page * kPageSize);
        m_bus->SetReadPage  (page, p);
        m_bus->SetWritePage (page, p);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMain02_BF
//
//  $0200-$BFFF main routing under RAMRD/RAMWRT. The text and hires
//  carve-outs are re-applied by their own resolvers afterwards (when
//  80STORE is on). Pages $02-$03 / $08-$1F / $40-$BF are always pure
//  RAMRD/RAMWRT regions; pages $04-$07 and $20-$3F we touch only when
//  80STORE is OFF (otherwise the carve-out resolvers own them).
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::ResolveMain02_BF ()
{
    if (m_bus == nullptr || m_mainRamPtr == nullptr)
    {
        return;
    }

    for (int page = kMain02_BFFirst; page <= kMain02_BFLast; page++)
    {
        bool  inText  = (page >= kText04_07First  && page <= kText04_07Last);
        bool  inHires = (page >= kHires20_3FFirst && page <= kHires20_3FLast);

        if (m_store80 && (inText || inHires))
        {
            continue;
        }

        Byte *  readPtr  = SelectMainRead  (page);
        Byte *  writePtr = SelectMainWrite (page);

        m_bus->SetReadPage  (page, readPtr);
        m_bus->SetWritePage (page, writePtr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectMainRead / SelectMainWrite
//
//  Helpers returning the per-page main vs. aux pointer based on
//  RAMRD / RAMWRT.
//
////////////////////////////////////////////////////////////////////////////////

Byte * AppleIIeMmu::SelectMainRead (int page)
{
    Byte *  base = m_ramRd ? m_auxRam.data () : m_mainRamPtr;
    return base + (page * kPageSize);
}



Byte * AppleIIeMmu::SelectMainWrite (int page)
{
    Byte *  base = m_ramWrt ? m_auxRam.data () : m_mainRamPtr;
    return base + (page * kPageSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveText04_07
//
//  $0400-$07FF text page 1. With 80STORE on, PAGE2 chooses aux/main for
//  both reads and writes (independent of RAMRD/RAMWRT). With 80STORE off,
//  routing reverts to RAMRD/RAMWRT. PAGE2 / HIRES are read from the
//  soft-switch bank pointer captured at Initialize.
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::ResolveText04_07 ()
{
    if (m_bus == nullptr || m_mainRamPtr == nullptr)
    {
        return;
    }

    if (!m_store80)
    {
        for (int page = kText04_07First; page <= kText04_07Last; page++)
        {
            Byte *  readPtr  = SelectMainRead  (page);
            Byte *  writePtr = SelectMainWrite (page);
            m_bus->SetReadPage  (page, readPtr);
            m_bus->SetWritePage (page, writePtr);
        }
        return;
    }

    bool   page2 = false;
    if (m_ssBank != nullptr)
    {
        page2 = m_ssBank->IsPage2 ();
    }

    Byte *  base = page2 ? m_auxRam.data () : m_mainRamPtr;

    for (int page = kText04_07First; page <= kText04_07Last; page++)
    {
        Byte *  p = base + (page * kPageSize);
        m_bus->SetReadPage  (page, p);
        m_bus->SetWritePage (page, p);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveHires20_3F
//
//  $2000-$3FFF hires page 1. Banked only when 80STORE AND HIRES are both
//  on; PAGE2 then chooses aux/main. Otherwise routing follows
//  RAMRD/RAMWRT (or main if both clear).
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeMmu::ResolveHires20_3F ()
{
    if (m_bus == nullptr || m_mainRamPtr == nullptr)
    {
        return;
    }

    bool  page2 = false;
    bool  hires = false;

    if (m_ssBank != nullptr)
    {
        page2 = m_ssBank->IsPage2 ();
        hires = m_ssBank->IsHiresMode ();
    }

    bool  banked = m_store80 && hires;

    if (!banked)
    {
        for (int page = kHires20_3FFirst; page <= kHires20_3FLast; page++)
        {
            Byte *  readPtr  = SelectMainRead  (page);
            Byte *  writePtr = SelectMainWrite (page);
            m_bus->SetReadPage  (page, readPtr);
            m_bus->SetWritePage (page, writePtr);
        }
        return;
    }

    Byte *  base = page2 ? m_auxRam.data () : m_mainRamPtr;

    for (int page = kHires20_3FFirst; page <= kHires20_3FLast; page++)
    {
        Byte *  p = base + (page * kPageSize);
        m_bus->SetReadPage  (page, p);
        m_bus->SetWritePage (page, p);
    }
}
