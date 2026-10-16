#include "Pch.h"

#include "Debugger/Events/TraceEventProbe.h"

#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceEventProbe::Reserve
//
////////////////////////////////////////////////////////////////////////////////

void TraceEventProbe::Reserve (size_t instructions)
{
    size_t  current = m_capacity.load (std::memory_order_acquire);



    while (instructions > current && !m_capacity.compare_exchange_weak (current, instructions, std::memory_order_acq_rel))
    {
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceEventProbe::Attach
//
//  The ring started again, allocated again only when Reserve raised its
//  size, and the bus reporting every access, before the stretch's first
//  instruction runs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TraceEventProbe::Attach (
    MachineHost                                    & machine,
    std::vector<DebugEventEntry<Cpu::TraceEntry>>  & entries)
{
    HRESULT    hr       = S_OK;
    EmuCpu   * cpu      = machine.GetCpu();
    size_t     capacity = m_capacity.load (std::memory_order_acquire);



    CBRA (cpu);

    m_machine = &machine;
    m_entries = &entries;
    m_start   = machine.GetPosition();

    if (cpu->GetCpu6502()->GetTraceCapacity() != capacity)
    {
        cpu->GetCpu6502()->EnableTrace (capacity);
    }
    else
    {
        cpu->GetCpu6502()->RestartTrace();
    }

    machine.GetMemoryBus().SetTraceSink     (this);
    machine.GetMemoryBus().SetTraceAllPages (true);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceEventProbe::Detach
//
//  The ring's entries, oldest first. When the stretch ran longer than the
//  ring's capacity, the first instructions are gone, and the first entry's
//  position is later than the stretch's start.
//
////////////////////////////////////////////////////////////////////////////////

void TraceEventProbe::Detach (MachineHost & machine)
{
    Cpu              * cpu     = (machine.GetCpu() != nullptr) ? machine.GetCpu()->GetCpu6502() : nullptr;
    size_t             size    = 0;
    uint64_t           first   = 0;
    size_t             i       = 0;
    Cpu::TraceEntry    entry   = {};



    machine.GetMemoryBus().SetTraceAllPages (false);
    machine.GetMemoryBus().SetTraceSink     (nullptr);

    if (cpu == nullptr || m_entries == nullptr)
    {
        return;
    }

    cpu->StopTrace();

    size  = cpu->GetTraceSize();
    first = m_start + (cpu->GetTraceCount() - size);

    m_entries->reserve (m_entries->size() + size);

    for (i = 0; i < size && cpu->TryGetTraceEntry (i, entry); i++)
    {
        m_entries->push_back ({ DebugEventEntryKind::Event, first + i, entry.cycles, entry, nullptr });
    }

    m_entries = nullptr;
    m_machine = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceEventProbe::OnWatchedAccess
//
////////////////////////////////////////////////////////////////////////////////

void TraceEventProbe::OnWatchedAccess (
    Word                  address,
    Byte                  value,
    BusAccess             access,
    std::optional<Byte>   previous)
{
    UNREFERENCED_PARAMETER (previous);

    if (m_machine != nullptr && m_machine->GetCpu() != nullptr)
    {
        m_machine->GetCpu()->GetCpu6502()->RecordTraceAccess (address, value, access == BusAccess::Write);
    }
}
