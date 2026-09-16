#include "Pch.h"

#include "Debugger/MachineDebugTarget.h"

#include "Core/Cpu65C02.h"
#include "Debugger/IRunDriver.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::MachineDebugTarget
//
////////////////////////////////////////////////////////////////////////////////

MachineDebugTarget::MachineDebugTarget (MachineHost & host) :
    m_host    (host),
    m_view    (host),
    m_runHook (host, m_view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetRunDriver
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetRunDriver (IRunDriver * driver)
{
    m_driver = driver;

    if (m_driver != nullptr)
    {
        m_driver->SetRunObserver (m_observer);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetRegisters
//
////////////////////////////////////////////////////////////////////////////////

Cpu6502Registers MachineDebugTarget::GetRegisters() const
{
    return m_host.GetCpu()->GetCpu6502()->GetRegisters();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetRegisters
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetRegisters (const Cpu6502Registers & registers)
{
    m_host.GetCpu()->GetCpu6502()->SetRegisters (registers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryPeek
//
//  Peek, poke and region go through the side-effect-free view.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryPeek (Word address, Byte & value) const
{
    return m_view.TryPeek (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryPoke
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryPoke (Word address, Byte value)
{
    return m_view.TryPoke (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetRegion
//
////////////////////////////////////////////////////////////////////////////////

MemoryRegion MachineDebugTarget::GetRegion (Word address) const
{
    return m_view.GetRegion (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::ReadIo
//
//  Real bus access, with its side effects, for IN.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineDebugTarget::ReadIo (Word address)
{
    return m_host.GetMemoryBus().ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::WriteIo
//
//  Real bus access, for OUT.
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::WriteIo (Word address, Byte value)
{
    m_host.GetMemoryBus().WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetSoftSwitches
//
//  The video switches every machine has, then the //e's, the MMU's banking
//  switches, and the language card's state, each only where the machine has
//  the part.
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::GetSoftSwitches (std::vector<SoftSwitch> & switches) const
{
    const MachineRefs            & refs = m_host.GetRefs();
    const Apple2eMmu             * mmu  = m_host.GetMmu();
    const AppleSoftSwitchBank    * bank = refs.softSwitches;
    const Apple2eSoftSwitchBank  * iie  = refs.iieSoftSwitches;
    const LanguageCard           * lc   = refs.languageCard;



    switches.clear();

    if (bank != nullptr)
    {
        switches.push_back ({ "TEXT",  !bank->IsGraphicsMode() });
        switches.push_back ({ "MIXED", bank->IsMixedMode() });
        switches.push_back ({ "PAGE2", bank->IsPage2() });
        switches.push_back ({ "HIRES", bank->IsHiresMode() });
    }

    if (iie != nullptr)
    {
        switches.push_back ({ "80COL",      iie->Is80ColMode() });
        switches.push_back ({ "DHIRES",     iie->IsDoubleHiRes() });
        switches.push_back ({ "ALTCHARSET", iie->IsAltCharSet() });
    }

    if (mmu != nullptr)
    {
        switches.push_back ({ "RAMRD",     mmu->GetRamRd() });
        switches.push_back ({ "RAMWRT",    mmu->GetRamWrt() });
        switches.push_back ({ "ALTZP",     mmu->GetAltZp() });
        switches.push_back ({ "80STORE",   mmu->Get80Store() });
        switches.push_back ({ "INTCXROM",  mmu->GetIntCxRom() });
        switches.push_back ({ "SLOTC3ROM", mmu->GetSlotC3Rom() });
        switches.push_back ({ "INTC8ROM",  mmu->GetIntC8Rom() });
    }

    if (lc != nullptr)
    {
        switches.push_back ({ "LCREAD",  lc->IsReadRam() });
        switches.push_back ({ "LCWRITE", lc->IsWriteRam() });
        switches.push_back ({ "LCBANK2", lc->IsBank2() });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetRunObserver
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetRunObserver (IRunObserver * observer)
{
    m_observer = observer;

    if (m_driver != nullptr)
    {
        m_driver->SetRunObserver (observer);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::StartRun
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineDebugTarget::StartRun (const RunRequest & request)
{
    HRESULT  hr = S_OK;



    CBRAEx (m_driver, E_UNEXPECTED);

    hr = m_driver->Start (request);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::RequestPause
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::RequestPause()
{
    if (m_driver != nullptr)
    {
        m_driver->Pause();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetHookInstalled
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetHookInstalled (bool installed)
{
    m_host.SetDebugHook (installed ? &m_runHook : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetStopConditions
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetStopConditions (DebugHook * conditions)
{
    m_runHook.SetConditions (conditions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetWatchedPages
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetWatchedPages (const WatchedPages & pages)
{
    for (size_t page = 0; page < pages.size(); ++page)
    {
        m_host.GetMemoryBus().SetWatchedPage ((int) page, pages[page]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetVideoPosition
//
////////////////////////////////////////////////////////////////////////////////

VideoPosition MachineDebugTarget::GetVideoPosition() const
{
    const VideoTiming  * timing   = m_host.GetVideoTiming();
    VideoPosition        position;



    if (timing != nullptr)
    {
        position.scanline    = timing->GetCurrentScanline();
        position.cycleInLine = timing->GetHorizontalPos();
    }

    return position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetCycleCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineDebugTarget::GetCycleCount() const
{
    return m_host.GetCpu()->GetTotalCycles();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetCpuKind
//
////////////////////////////////////////////////////////////////////////////////

DebugCpuKind MachineDebugTarget::GetCpuKind() const
{
    const Cpu6502  * cpu = m_host.GetCpu()->GetCpu6502();



    return dynamic_cast<const Cpu65C02 *> (cpu) != nullptr ? DebugCpuKind::M65C02 : DebugCpuKind::M6502;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetInstructionSet
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * MachineDebugTarget::GetInstructionSet() const
{
    return m_host.GetCpu()->GetCpu6502()->GetInstructionSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetMachineInfo
//
//  The machine's name and slot 6's two drives, with an empty entry for an
//  empty drive.
//
////////////////////////////////////////////////////////////////////////////////

DebugMachineInfo MachineDebugTarget::GetMachineInfo() const
{
    static constexpr int    kDiskSlot = 6;
    const std::wstring    & name      = m_host.GetCurrentMachineName();
    const DiskImageStore  & disks     = m_host.GetDiskStore();
    DebugMachineInfo        info;



    info.name.assign (name.begin(), name.end());

    for (int drive = 0; drive < DiskImageStore::kDriveCount; ++drive)
    {
        info.disks.push_back (disks.IsMounted (kDiskSlot, drive) ? disks.GetSourcePath (kDiskSlot, drive) : std::string());
    }

    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::InjectKey
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::InjectKey (Byte key)
{
    if (m_host.GetRefs().keyboard != nullptr)
    {
        m_host.GetRefs().keyboard->PressKey (key);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::IsKeyPending
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::IsKeyPending() const
{
    const AppleKeyboard  * keyboard = m_host.GetRefs().keyboard;



    return keyboard != nullptr && !keyboard->IsStrobeClear();
}
