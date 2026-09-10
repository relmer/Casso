#include "Pch.h"

#include "Shell/MachineHost.h"

#include "Core/Prng.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/MockingboardCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::MachineHost
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::MachineHost()
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
    Byte  cycles = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    // StepOne polls the interrupt lines itself and dispatches a pending
    // NMI/IRQ vector in place of the opcode fetch, reporting the cost through
    // GetLastInstructionCycles either way -- so a bare StepOne is the whole
    // step, and a separate interrupt poll would be a second, redundant one.
    m_cpu->StepOne();

    cycles = m_cpu->GetLastInstructionCycles();
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

uint32_t MachineHost::RunCycles (uint32_t cycleBudget)
{
    uint32_t  spent = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    while (spent < cycleBudget)
    {
        spent += StepOne();
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
    m_memoryBus.SoftResetAll();

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
    hrFlush = m_diskStore.FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_memoryBus.PowerCycleAll (*m_prng);

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
