#include "Pch.h"

#include "Debugger/DebugMemoryView.h"

#include "Devices/RomDevice.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/CxxxRomRouter.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::DebugMemoryView
//
////////////////////////////////////////////////////////////////////////////////

DebugMemoryView::DebugMemoryView (MachineHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::TryPeek
//
//  $0000-$BFFF and $D000-$FFFF come from the bus's shadow read pages, which
//  hold what the MMU and language card mapped even while a page is watched.
//  $C100-$CFFF comes from the Cxxx ROM router's own peek on machines that have
//  one. Where nothing is mapped, a ROM device's image answers. $C000-$C0FF is
//  never read.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugMemoryView::TryPeek (Word address, Byte & value) const
{
    Byte        * page = nullptr;
    Apple2eMmu  * mmu  = m_host.GetMmu();



    value = 0;

    if (address >= kIoFirst && address <= kIoLast)
    {
        return false;
    }

    if (address >= kSlotRomFirst && address <= kSlotRomLast && mmu != nullptr)
    {
        return mmu->GetCxxxRouter()->TryPeek (address, value);
    }

    page = m_host.GetMemoryBus().GetShadowReadPage (address);

    if (page != nullptr)
    {
        value = page[address & kPageMask];
        return true;
    }

    return TryPeekRomDevice (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::TryPoke
//
//  Below $C000 a poke lands where a CPU write would. In $D000-$FFFF it lands
//  in language-card RAM while that RAM is the read source, so the new byte is
//  the one the CPU and the debugger then see. ROM and I/O are not writable.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugMemoryView::TryPoke (Word address, Byte value)
{
    MemoryBus     & bus    = m_host.GetMemoryBus();
    MemoryRegion    region = GetRegion (address);
    Byte          * page   = nullptr;



    if (address < kIoFirst)
    {
        page = bus.GetShadowWritePage (address);
    }
    else if (region == MemoryRegion::LcBank1 || region == MemoryRegion::LcBank2)
    {
        page = bus.GetShadowReadPage (address);
    }

    if (page == nullptr)
    {
        return false;
    }

    page[address & kPageMask] = value;
    bus.MarkVideoDirty();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::GetRegion
//
////////////////////////////////////////////////////////////////////////////////

MemoryRegion DebugMemoryView::GetRegion (Word address) const
{
    Apple2eMmu    * mmu = m_host.GetMmu();
    LanguageCard  * lc  = m_host.GetRefs().languageCard;



    if (address >= kIoFirst && address <= kIoLast)
    {
        return MemoryRegion::Io;
    }

    if (address >= kSlotRomFirst && address <= kSlotRomLast)
    {
        return (mmu != nullptr && mmu->GetCxxxRouter()->IsInternalRomSelected (address)) ? MemoryRegion::Rom
                                                                                          : MemoryRegion::SlotRom;
    }

    if (address < kIoFirst)
    {
        return IsAuxPage (m_host.GetMemoryBus().GetShadowReadPage (address)) ? MemoryRegion::AuxRam
                                                                             : MemoryRegion::MainRam;
    }

    if (lc != nullptr && lc->IsReadRam())
    {
        return lc->IsBank2() ? MemoryRegion::LcBank2 : MemoryRegion::LcBank1;
    }

    return MemoryRegion::Rom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::TryPeekRomDevice
//
//  The first bus entry covering address answers only if it is a ROM device,
//  whose image can be read directly. Any other device is left alone.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugMemoryView::TryPeekRomDevice (Word address, Byte & value) const
{
    const RomDevice  * rom = nullptr;



    for (const BusEntry & entry : m_host.GetMemoryBus().GetEntries())
    {
        if (address < entry.start || address > entry.end)
        {
            continue;
        }

        rom = dynamic_cast<const RomDevice *> (entry.device);

        if (rom == nullptr)
        {
            return false;
        }

        value = rom->GetData()[address - entry.start];
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView::IsAuxPage
//
//  True when page points into the //e MMU's auxiliary RAM.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugMemoryView::IsAuxPage (const Byte * page) const
{
    const Apple2eMmu  * mmu   = m_host.GetMmu();
    uintptr_t           first = 0;
    uintptr_t           where = (uintptr_t) page;



    if (mmu == nullptr || page == nullptr)
    {
        return false;
    }

    first = (uintptr_t) mmu->GetAuxBuffer();
    return where >= first && where < first + kAuxRamSize;
}
