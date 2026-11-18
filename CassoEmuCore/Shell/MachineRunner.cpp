#include "Pch.h"

#include "Shell/MachineRunner.h"
#include "Debugger/DebugHook.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Devices/Acia6551.h"
#include "Devices/Floppy35/DeviceCpu.h"
#include "Devices/Floppy35/ICatchUpDevice.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Machines/Apple3/Apple3System.h"
#include "Shell/MachineConnectors.h"
#include "Shell/MachineParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::MachineRunner
//
////////////////////////////////////////////////////////////////////////////////

MachineRunner::MachineRunner (MachineParts & parts, MachineConnectors & connectors) :
    m_parts      (parts),
    m_connectors (connectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::StepOne
//
//  One instruction, and the devices that measure time in instructions.
//
//  The Disk II nibble engine is pumped per instruction rather than per slice
//  because the boot ROM sits in a tight LDA $C0EC / BPL loop reading the data
//  latch: advance the engine only at slice boundaries and the CPU reads one
//  valid nibble per ~1000 cycles instead of ~32, and never accumulates enough
//  sync bytes to find a sector header.
//
//  The CPU's StepOne polls the interrupt lines and dispatches a pending NMI
//  or IRQ vector in place of the opcode fetch, reporting the cost through
//  GetLastInstructionCycles either way, so it is the whole step.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineRunner::StepOne()
{
    EmuCpu  * cpu = m_parts.cpu.get();



    if (cpu == nullptr)
    {
        return 0;
    }

    if (m_debugHook != nullptr)
    {
        return StepOneWithHook();
    }

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*cpu->GetCpu6502());
    }

    cpu->StepOne();

    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::StepOneWithHook
//
//  StepOne while a debug hook is installed. The hook is called before the
//  instructions on the pages its filter marks (StepOneAsked); the rest run
//  without it. This runs once per instruction, so the test is one load.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineRunner::StepOneWithHook()
{
    EmuCpu                 & cpu    = *m_parts.cpu;
    const DebugHookFilter  & filter = m_debugHook->GetFilter();
    Word                     pc     = cpu.GetPC();



    if (filter.pages[pc >> 8])
    {
        return StepOneAsked (filter, pc);
    }

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*cpu.GetCpu6502());
    }

    cpu.StepOne();

    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::StepOneAsked
//
//  An instruction on a page the filter marks, which the hook may stop before.
//  While only an opcode can stop the machine, only an instruction with that
//  opcode, or with one that cannot be read without side effects, goes to
//  the hook. The opcode is read from the shadow read page, which is what the CPU
//  is about to fetch; I/O and a device's ROM have none there.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) Byte MachineRunner::StepOneAsked (const DebugHookFilter & filter, Word pc)
{
    static constexpr Word  kPageMask = 0xFF;
    EmuCpu               & cpu       = *m_parts.cpu;
    const Byte           * page      = m_parts.memoryBus->GetShadowReadPage (pc);
    bool                   isAsked   = !filter.opcodesStop || filter.everyInstruction || page == nullptr || filter.opcodes[page[pc & kPageMask]];



    if (isAsked && m_debugHook->ShouldStopBefore (pc))
    {
        return 0;
    }

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*cpu.GetCpu6502());
    }

    cpu.StepOne();

    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::FinishStep
//
//  What the instruction or interrupt just executed cost, added to the clock
//  and to the devices that count cycles. The clock moves the //e video
//  timing and the //c mouse with it.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineRunner::FinishStep()
{
    EmuCpu         & cpu           = *m_parts.cpu;
    MachineRefs    & refs          = m_parts.refs;
    Byte             cycles        = cpu.GetLastInstructionCycles();
    uint64_t         historyCycles = 0;



    //  The //c Plus's accelerator runs the CPU faster than the machine's
    //  clock, so the instruction takes fewer of the machine's cycles.
    if (m_parts.accelerator != nullptr)
    {
        cycles = m_parts.accelerator->Scale (cycles);
    }

    //  The Apple /// counts time in ticks of its 2 MHz clock, and a cycle at
    //  1 MHz is two of them.
    if (m_parts.apple3 != nullptr)
    {
        cycles = (Byte) m_parts.apple3->OnInstruction (cycles, *cpu.GetInstructionStartPtr());
    }

    cpu.AddCycles (cycles);

    historyCycles = cpu.GetHistoryCycles();

    if ((historyCycles >> kCatchUpShift) != ((historyCycles - cycles) >> kCatchUpShift))
    {
        cpu.SyncBusCycle();
        CatchUpCards();
    }

    //  The Apple /// runs its floppy controller from master clocks.
    if (refs.diskController != nullptr && m_parts.apple3 == nullptr)
    {
        refs.diskController->Tick (cycles);
    }

    if (refs.mockingboard != nullptr)
    {
        refs.mockingboard->Tick (cycles);
    }

    for (Acia6551 * acia : refs.serialChips)
    {
        acia->Tick (cycles);
    }

    m_connectors.Tick (cycles);

    if (refs.uthernet != nullptr)
    {
        refs.uthernet->Tick (cycles);
    }

    m_position++;

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionEnd();
    }

    return cycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::CatchUpCards
//
//  Cards with separate processors run up to the Apple's time, so their
//  firmware's timers run while the guest doesn't access them. FinishStep
//  calls it each time the history's cycle count crosses a multiple of
//  2^kCatchUpShift, after the bus cycle is synced, so the state at a
//  position is the same however the run is cut into slices.
//
////////////////////////////////////////////////////////////////////////////////

void MachineRunner::CatchUpCards()
{
    for (ICatchUpDevice * device : m_parts.refs.catchUpDevices)
    {
        device->CatchUp (CatchUpKind::BetweenFrames);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::RunCycles
//
//  A pause raised on another thread ends the slice between two
//  instructions. The flag is read relaxed, since the stop only has to be
//  seen: the CPU thread takes the pause mutex before it parks. A hook stop
//  returns a short slice: StepOne declined to execute, or the instruction it
//  just ran raised a stop for the next boundary. An instruction that read a
//  line the input behind live would change ends it too: the caller puts the
//  machine back before it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineRunner::RunCycles (uint64_t cycleBudget)
{
    uint64_t  spent  = 0;
    Byte      cycles = 0;



    if (m_parts.cpu == nullptr)
    {
        return 0;
    }

    while (spent < cycleBudget)
    {
        if (m_stopFlag != nullptr && m_stopFlag->load (std::memory_order_relaxed))
        {
            break;
        }

        cycles  = StepOne();
        spent  += cycles;

        if (m_debugHook != nullptr && (cycles == 0 || (m_debugHook->GetFilter().everyInstruction && m_debugHook->HasPendingStop())))
        {
            break;
        }

        if (m_heldInputWatch != nullptr && m_heldInputWatch->HasHit())
        {
            break;
        }
    }

    return spent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::SetOpcodeWatch
//
////////////////////////////////////////////////////////////////////////////////

void MachineRunner::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    m_watchOpcodes = opcodes;
    m_watcher      = watcher;

    ApplyOpcodeWatch();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner::ApplyOpcodeWatch
//
//  The watch is stored here as well as in the CPU, so a CPU the machine is
//  rebuilt with watches the same opcodes.
//
////////////////////////////////////////////////////////////////////////////////

void MachineRunner::ApplyOpcodeWatch()
{
    if (m_parts.cpu != nullptr)
    {
        m_parts.cpu->GetCpu6502()->GetFetchObservers().SetOpcodeWatch (m_watchOpcodes, m_watcher);
    }
}
