#include "Pch.h"

#include "Shell/MachineHost.h"

#include "Core/Prng.h"
#include "Debugger/DebugHook.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::MachineHost
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::MachineHost() :
    m_memoryBus (std::make_unique<MemoryBus>()),
    m_charRom   (std::make_unique<CharacterRomData>()),
    m_diskStore (std::make_unique<DiskImageStore>()),
    m_config    (std::make_unique<MachineConfig>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::~MachineHost
//
//  Out of line so the header can forward-declare the types it holds by
//  unique_ptr rather than including all of them.
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::~MachineHost()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetCpu
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetCpu (std::unique_ptr<EmuCpu> cpu)
{
    m_cpu = std::move (cpu);

    if (m_cpu != nullptr)
    {
        m_cpu->GetCpu6502()->SetOpcodeWatch (m_watchOpcodes, m_watcher);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetPrng
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetPrng (std::unique_ptr<Prng> prng)
{
    m_prng = std::move (prng);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMmu
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMmu (std::unique_ptr<Apple2eMmu> mmu)
{
    m_mmu = std::move (mmu);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetApple2cRomBank
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetApple2cRomBank (std::unique_ptr<Apple2cRomBank> romBank)
{
    m_apple2cRomBank = std::move (romBank);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMouse
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMouse (std::unique_ptr<AppleMouse> mouse)
{
    m_mouse = std::move (mouse);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetJoyport
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetJoyport (std::unique_ptr<SiriusJoyport> joyport)
{
    m_joyport = std::move (joyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetVideoTiming
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetVideoTiming (std::unique_ptr<VideoTiming> videoTiming)
{
    m_videoTiming = std::move (videoTiming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetPendingPrintDir
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path MachineHost::GetPendingPrintDir() const
{
    std::filesystem::path  dir = std::filesystem::path (m_assetBaseDir) / L"Machines" /
                                 std::filesystem::path (m_currentMachineName) / L"PendingPrint";



    return (dir);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOne
//
//  One instruction, and the devices that measure time in instructions.
//
//  The Disk ][ nibble engine is pumped per instruction rather than per slice
//  because the boot ROM sits in a tight LDA $C0EC / BPL loop reading the data
//  latch: advance the engine only at slice boundaries and the CPU sees one
//  valid nibble per ~1000 cycles instead of ~32, and never accumulates enough
//  sync bytes to find a sector header.
//
//  AddCycles is what moves the machine's clock, and through it the //e video
//  timing and the //c mouse. It belongs here, with the instruction that spent
//  the cycles, rather than at each call site -- a single-step that ticked the
//  disk controller but left the clock standing was the shape of the defect
//  this consolidates away.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::StepOne()
{
    if (m_cpu == nullptr)
    {
        return (0);
    }

    if (m_debugHook != nullptr)
    {
        return StepOneWithHook();
    }

    // StepOne polls the interrupt lines itself and dispatches a pending
    // NMI/IRQ vector in place of the opcode fetch, reporting the cost through
    // GetLastInstructionCycles either way -- so a bare StepOne is the whole
    // step, and a separate interrupt poll would be a second, redundant one.
    m_cpu->StepOne();

    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOneWithHook
//
//  StepOne while a debug hook is installed. The hook is asked before the
//  instructions on the pages its filter marks (StepOneAsked); the rest run
//  without it. This runs once per instruction, so the test is one load.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::StepOneWithHook()
{
    const DebugHookFilter  & filter = m_debugHook->GetFilter();
    Word                     pc     = m_cpu->GetPC();



    if (filter.pages[pc >> 8])
    {
        return StepOneAsked (filter, pc);
    }

    m_cpu->StepOne();
    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOneAsked
//
//  An instruction on a page the filter marks, which the hook may stop before.
//  While only an opcode can stop the machine, only an instruction with that
//  opcode, or with one that cannot be read without side effects, is asked
//  about. The opcode is read from the shadow read page, which is what the CPU
//  is about to fetch; I/O and a device's ROM have none there.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) Byte MachineHost::StepOneAsked (const DebugHookFilter & filter, Word pc)
{
    static constexpr Word  kPageMask = 0xFF;
    const Byte           * page      = m_memoryBus->GetShadowReadPage (pc);
    bool                   isAsked   = !filter.opcodesStop || filter.everyInstruction || page == nullptr || filter.opcodes[page[pc & kPageMask]];



    if (isAsked && m_debugHook->ShouldStopBefore (pc))
    {
        return (0);
    }

    m_cpu->StepOne();
    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetOpcodeWatch
//
//  Kept here as well as in the CPU, so a CPU the machine is rebuilt with
//  watches the same opcodes.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    m_watchOpcodes = opcodes;
    m_watcher      = watcher;

    if (m_cpu != nullptr)
    {
        m_cpu->GetCpu6502()->SetOpcodeWatch (opcodes, watcher);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::FinishStep
//
//  What the instruction or interrupt just executed cost, added to the clock
//  and to the devices that count cycles.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::FinishStep()
{
    Byte  cycles = m_cpu->GetLastInstructionCycles();



    m_cpu->AddCycles (cycles);

    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->Tick (cycles);
    }

    if (m_refs.mockingboard != nullptr)
    {
        m_refs.mockingboard->Tick (cycles);
    }

    return (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::RunCycles
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::RunCycles (uint64_t cycleBudget)
{
    uint64_t  spent  = 0;
    Byte      cycles = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    while (spent < cycleBudget)
    {
        cycles  = StepOne();
        spent  += cycles;

        // A hook stop returns a short slice: StepOne declined to execute, or
        // the instruction it just ran raised a stop for the next boundary.
        if (m_debugHook != nullptr && (cycles == 0 || (m_debugHook->GetFilter().everyInstruction && m_debugHook->HasPendingStop())))
        {
            break;
        }
    }

    return (spent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SoftReset
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SoftReset()
{
    m_memoryBus->SoftResetAll();

    if (m_mmu != nullptr)
    {
        m_mmu->OnSoftReset();
    }

    m_interruptController.SoftReset();

    // //c IOU mouse: /RESET clears the interrupt latches + enables and
    // shuts the IOU access gate (matches power-on state).
    if (m_mouse != nullptr)
    {
        m_mouse->Reset();
    }

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->SoftReset();
    }

    if (m_cpu != nullptr)
    {
        m_cpu->SoftReset();
    }

    // Last, so the window is stamped from the CPU's post-reset counter.
    if (m_joyport != nullptr)
    {
        m_joyport->OnMachineReset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::PowerCycle
//
//  Reseeds every DRAM-owning device from the shared Prng, then runs the
//  reset sequence.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::PowerCycle()
{
    HRESULT  hrFlush = S_OK;



    if (m_prng == nullptr)
    {
        return;
    }

    // Auto-flush dirty disks before reseeding device state so writes don't
    // get lost across a power cycle. Mounts persist (matching
    // DiskImageStore::SoftReset semantics -- see the comment block on
    // DiskImageStore::PowerCycle, which is the unmount-everything variant
    // tests can opt into directly).
    hrFlush = m_diskStore->FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_memoryBus->PowerCycleAll (*m_prng);

    if (m_mmu != nullptr)
    {
        m_mmu->OnPowerCycle (*m_prng);
    }

    m_interruptController.PowerCycle();

    // //c IOU mouse: power-on state (latches clear, interrupts masked, IOU
    // access gate shut).
    if (m_mouse != nullptr)
    {
        m_mouse->Reset();
    }

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->PowerCycle (*m_prng);
    }

    if (m_cpu != nullptr)
    {
        m_cpu->PowerCycle (*m_prng);
    }

    // Last: the CPU's power cycle zeroes the cycle counter, and the window
    // is measured from that zero.
    if (m_joyport != nullptr)
    {
        m_joyport->OnMachineReset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachObservers
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachObservers (const MachineObservers & observers)
{
    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->SetEventSink (observers.disk);
    }

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetInputEventSink (observers.input);
    }

    // The //e soft-switch bank reports the switches the keyboard does not
    // own (80COL, 80STORE, ALTCHARSET); null on a ][ or ][+.
    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SetInputEventSink (observers.input);
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SetInputEventSink (observers.input);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetDiagnosticsProviders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<const IDiagnosticsProvider *> MachineHost::GetDiagnosticsProviders() const
{
    std::vector<const IDiagnosticsProvider *>  providers = { this };
    const IDiagnosticsProvider               * devices[] =
    {
        m_refs.keyboard,
        m_refs.softSwitches,
        m_mmu.get(),
        m_refs.diskController,
        m_refs.mockingboard,
        m_refs.printerCard,
    };



    for (const IDiagnosticsProvider * device : devices)
    {
        if (device != nullptr)
        {
            providers.push_back (device);
        }
    }

    return providers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    static constexpr const char * kSpeeds[] = { "authentic", "double", "maximum" };
    DiagnosticsGroup              cpu       { "CPU", {} };
    DiagnosticsGroup              video     { "Video", {} };



    cpu.rows.push_back (MakeTextRow ("Cycles", std::format ("{}", (m_cpu != nullptr) ? m_cpu->GetTotalCycles() : 0)));
    cpu.rows.push_back (MakeTextRow ("Clock",  std::format ("{} Hz", m_config->clockSpeed)));
    cpu.rows.push_back (MakeTextRow ("Speed",  kSpeeds[(size_t) m_speedMode]));
    snapshot.groups.push_back (std::move (cpu));

    if (m_videoTiming != nullptr)
    {
        video.rows.push_back (MakeTextRow ("Scanline",       std::format ("{}", m_videoTiming->GetCurrentScanline())));
        video.rows.push_back (MakeTextRow ("Cycle in line",  std::format ("{}", m_videoTiming->GetHorizontalPos())));
        video.rows.push_back (MakeTextRow ("Cycle in frame", std::format ("{}", m_videoTiming->GetCycleInFrame())));
        snapshot.groups.push_back (std::move (video));
    }
}
