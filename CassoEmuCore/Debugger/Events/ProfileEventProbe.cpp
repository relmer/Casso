#include "Pch.h"

#include "Debugger/Events/ProfileEventProbe.h"

#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::ProfileEventProbe
//
//  Every opcode is watched, so every fetch is billed.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEventProbe::ProfileEventProbe()
{
    m_opcodes.fill (true);
    m_counts.table.SetOn (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::Begin
//
////////////////////////////////////////////////////////////////////////////////

void ProfileEventProbe::Begin (uint64_t fromCycle, uint64_t toCycle)
{
    m_fromCycle = fromCycle;
    m_toCycle   = std::max (toCycle, fromCycle + 1);
    m_counts.table.Reset();
    m_counts.routines.Reset();
    m_pending.reset();
    m_progress.store (0.0, std::memory_order_relaxed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::Attach
//
//  A stretch starts at a keyframe, with nothing of the stretch before it
//  pending.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProfileEventProbe::Attach (MachineHost & machine, std::vector<DebugEventEntry<ProfileCountsPtr>> & entries)
{
    HRESULT    hr  = S_OK;
    EmuCpu   * cpu = machine.GetCpu();



    CBRA (cpu);

    m_machine = &machine;
    m_entries = &entries;
    m_pending.reset();
    machine.SetOpcodeWatch (m_opcodes.data(), this);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::Detach
//
//  The stretch's last instruction is billed at the cycle the stretch ends,
//  and the counts so far are its entry.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileEventProbe::Detach (MachineHost & machine)
{
    EmuCpu  * cpu = machine.GetCpu();



    machine.SetOpcodeWatch (nullptr, nullptr);

    if (cpu != nullptr)
    {
        Bill (cpu->GetTotalCycles(), cpu->GetSP(), cpu->GetCpu6502()->GetLastPenalties());
    }

    if (m_entries != nullptr)
    {
        m_entries->push_back ({ DebugEventEntryKind::Event, machine.GetPosition(), (cpu != nullptr) ? cpu->GetTotalCycles() : 0,
                                std::make_shared<const ProfileCounts> (m_counts), nullptr });
    }

    m_pending.reset();
    m_machine = nullptr;
    m_entries = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::OnWatchedFetch
//
//  An interrupt reads as opcode $00, as BRK, which the routine profile takes
//  as a call either way.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileEventProbe::OnWatchedFetch (Word pc, Byte sp, Byte opcode)
{
    EmuCpu    * cpu   = (m_machine != nullptr) ? m_machine->GetCpu() : nullptr;
    uint64_t    cycle = 0;



    if (cpu == nullptr)
    {
        return;
    }

    cycle = cpu->GetTotalCycles();
    Bill (cycle, sp, cpu->GetCpu6502()->GetLastPenalties());
    m_pending = Pending { pc, opcode, cycle };

    if (m_untilNote-- == 0)
    {
        m_untilNote = kProgressEvery;
        m_progress.store (std::clamp ((double) (std::max (cycle, m_fromCycle) - m_fromCycle) / (double) (m_toCycle - m_fromCycle), 0.0, 1.0),
                          std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe::Bill
//
//  A count that went backward, as a power cycle's does, bills nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileEventProbe::Bill (uint64_t cycle, Byte spAfter, Byte penalties)
{
    uint64_t  cycles = 0;



    if (!m_pending.has_value() || cycle < m_pending->startCycle ||
        m_pending->startCycle < m_fromCycle || m_pending->startCycle >= m_toCycle)
    {
        m_pending.reset();
        return;
    }

    cycles = std::min<uint64_t> (cycle - m_pending->startCycle, 0xFF);

    m_counts.table.Record    (m_pending->pc, m_pending->opcode, (Byte) cycles, penalties);
    m_counts.routines.Record (m_pending->pc, m_pending->opcode, cycles, spAfter, m_pending->startCycle, RoutineProfile::kCyclesPerFrame);
    m_pending.reset();
}
