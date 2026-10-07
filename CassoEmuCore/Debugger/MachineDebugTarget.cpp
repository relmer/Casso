#include "Pch.h"

#include "Debugger/MachineDebugTarget.h"

#include "Core/Cpu65C02.h"
#include "Core/TextEncoding.h"
#include "Debugger/IRunDriver.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
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
    m_runHook (host, m_view),
    m_trace   (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::~MachineDebugTarget
//
//  The bus and the CPU hold the heat map while it is on, so it goes off with
//  this target.
//
////////////////////////////////////////////////////////////////////////////////

MachineDebugTarget::~MachineDebugTarget()
{
    SetHeatMapOn (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetHeatMapOn
//
//  On, the bus puts every page on its watched path, so every read the CPU
//  makes reaches it, and the CPU reports each fetch, read and write to the
//  map; the bus's own readers, such as the video modes, are not counted. Off
//  gives both back, so the CPU reads its pages inline again and reports to
//  nobody. A CPU not on the bus has nothing to report. The map counts from
//  the machine's position when it comes on, and its history hears of both.
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetHeatMapOn (bool on)
{
    EmuCpu  * cpu         = m_host.GetCpu();
    bool      isConnected = false;



    if (on == m_heat.IsOn() || cpu == nullptr)
    {
        return;
    }

    if (on)
    {
        m_heat.SetPositionSource (m_host.GetPositionPtr());
        m_heat.Start (GetInstructionSet(), GetCycleCount());

        isConnected = TryConnectHeatMap (m_host, &m_heat);

        if (!isConnected)
        {
            m_heat.Stop();
            return;
        }

        m_heatHistory.OnMapStarted();
        return;
    }

    isConnected = TryConnectHeatMap (m_host, nullptr);
    IGNORE_RETURN_VALUE (isConnected, false);

    m_heatHistory.OnMapStopped();
    m_heat.Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryConnectHeatMap
//
//  A map puts every page of the machine's bus on its watched path and takes
//  the CPU's reports of each fetch, read and write; null gives both back.
//  False when the machine's CPU is not on its bus.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryConnectHeatMap (
    MachineHost    & host,
    AccessHeatMap  * map)
{
    EmuCpu        * cpu    = host.GetCpu();
    MemoryBusCpu  * busCpu = (cpu != nullptr) ? dynamic_cast<MemoryBusCpu *> (cpu->GetCpu()) : nullptr;



    if (busCpu == nullptr)
    {
        return false;
    }

    host.GetMemoryBus().SetAllPagesWatched (map != nullptr);
    busCpu->SetAccessSink (map);
    busCpu->SetFetchSink  (map);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::ClearHeatMap
//
//  After a machine switch the CPU that held the map is gone; the new bus is
//  given back its pages and the map is forgotten.
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::ClearHeatMap()
{
    m_host.GetMemoryBus().SetAllPagesWatched (false);
    m_heatHistory.OnMapStopped();
    m_heat.Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::AttachHistory
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::AttachHistory (
    KeyframeStore   * keyframes,
    IHeatRebuilder  * rebuilder)
{
    m_heatHistory.Attach       (keyframes);
    m_heatHistory.SetRebuilder (rebuilder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::FoldHeatMap
//
//  A rebuilt heat that has come in is merged first.
//
////////////////////////////////////////////////////////////////////////////////

const AccessHeatMap * MachineDebugTarget::FoldHeatMap()
{
    if (!m_heat.IsOn())
    {
        return nullptr;
    }

    m_heatHistory.Service();

    m_heat.Fold (GetCycleCount());
    return &m_heat;
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
    m_host.NoteDebuggerEdit();
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
    bool  isPoked = m_view.TryPoke (address, value);



    if (isPoked)
    {
        m_host.NoteDebuggerEdit();
    }

    return isPoked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryPatch
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryPatch (Word address, Byte value)
{
    bool  isPatched = m_view.TryPatch (address, value);



    if (isPatched)
    {
        m_host.NoteDebuggerEdit();
    }

    return isPatched;
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
//  Real bus access, with its side effects, for IN. The access is the
//  debugger's, not the program's: the trace marks it as the host's, so it
//  does not attach to the last traced instruction, and the watch sink does
//  not see it, so it records no watchpoint hit.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineDebugTarget::ReadIo (Word address)
{
    MemoryBus   & bus   = m_host.GetMemoryBus();
    IWatchSink  * sink  = bus.GetWatchSink();
    Byte          value = 0;



    m_trace.SetHostAccess (true);
    bus.SetWatchSink (nullptr);
    value = bus.ReadByte (address);
    bus.SetWatchSink (sink);
    m_trace.SetHostAccess (false);

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::WriteIo
//
//  Real bus access, for OUT, kept out of the trace and the watchpoints as
//  IN is.
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::WriteIo (Word address, Byte value)
{
    MemoryBus   & bus  = m_host.GetMemoryBus();
    IWatchSink  * sink = bus.GetWatchSink();



    m_trace.SetHostAccess (true);
    bus.SetWatchSink (nullptr);
    bus.WriteByte (address, value);
    bus.SetWatchSink (sink);
    m_trace.SetHostAccess (false);

    m_host.NoteDebuggerEdit();
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
//  MachineDebugTarget::IsPausePending
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::IsPausePending() const
{
    return m_driver != nullptr && m_driver->IsPausePending();
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
//  MachineDebugTarget::SetOpcodeWatch
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    m_host.SetOpcodeWatch (opcodes, watcher);
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
//  MachineDebugTarget::SetWatchSink
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetWatchSink (IWatchSink * sink)
{
    m_host.GetMemoryBus().SetWatchSink (sink);
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
//  MachineDebugTarget::GetLastPenalties
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineDebugTarget::GetLastPenalties() const
{
    return m_host.GetCpu()->GetLastPenalties();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryGetLastBranch
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryGetLastBranch (Word & from) const
{
    return m_host.GetCpu()->TryGetLastBranch (from);
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
//  empty drive. A machine with no Disk II controller has no drives.
//
////////////////////////////////////////////////////////////////////////////////

DebugMachineInfo MachineDebugTarget::GetMachineInfo() const
{
    static constexpr int    kDiskSlot = 6;
    const std::wstring    & name      = m_host.GetCurrentMachineName();
    const DiskImageStore  & disks     = m_host.GetDiskStore();
    DebugMachineInfo        info;



    info.name = TextEncoding::WideToNarrow (name);

    if (m_host.GetRefs().diskController == nullptr)
    {
        return info;
    }

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
        m_host.NoteDebuggerEdit();
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





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::SetTraceOn
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::SetTraceOn (bool on)
{
    if (on)
    {
        m_trace.On();
    }
    else
    {
        m_trace.Off();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetTraceWindow
//
////////////////////////////////////////////////////////////////////////////////

void MachineDebugTarget::GetTraceWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const
{
    m_trace.GetWindow (first, count, entries);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::GetDiagnosticsProviders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<const IDiagnosticsProvider *> MachineDebugTarget::GetDiagnosticsProviders() const
{
    return m_host.GetDiagnosticsProviders();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget::TryGetMockingboardBase
//
////////////////////////////////////////////////////////////////////////////////

bool MachineDebugTarget::TryGetMockingboardBase (Word & base) const
{
    const MockingboardCard  * card = m_host.GetRefs().mockingboard;



    if (card == nullptr)
    {
        return false;
    }

    base = card->GetStart();
    return true;
}





