#include "Pch.h"

#include "Debugger/TraceController.h"

#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::~TraceController
//
//  The bus holds this object as its trace sink while the trace is on, so the
//  trace goes off with it.
//
////////////////////////////////////////////////////////////////////////////////

TraceController::~TraceController()
{
    Off();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::On
//
//  Starts a new trace: the ring is emptied and sized, and the bus reports
//  every access here. The bus is switched first, so the first entry already
//  gets its access.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::On()
{
    MemoryBus  & bus = m_host.GetMemoryBus();
    Cpu6502    * cpu = m_host.GetCpu()->GetCpu6502();



    bus.SetTraceSink     (this);
    bus.SetTraceAllPages (true);
    cpu->EnableTrace     (kCapacity);

    m_isOn       = true;
    m_hasEntries = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::Off
//
//  Stops recording and gives the bus back to the watch mask. What the ring
//  holds stays, for HISTORY and HISTORY SAVE.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::Off()
{
    MemoryBus  & bus = m_host.GetMemoryBus();
    EmuCpu     * cpu = m_host.GetCpu();



    if (!m_isOn)
    {
        return;
    }

    if (cpu != nullptr)
    {
        cpu->GetCpu6502()->StopTrace();
    }

    bus.SetTraceAllPages (false);
    bus.SetTraceSink     (nullptr);

    m_isOn = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::Clear
//
//  After a machine switch the addresses in the ring belong to another
//  machine, so the entries go with the trace.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::Clear()
{
    MemoryBus  & bus = m_host.GetMemoryBus();



    bus.SetTraceAllPages (false);
    bus.SetTraceSink     (nullptr);

    if (m_hasEntries)
    {
        m_host.GetCpu()->GetCpu6502()->EnableTrace (0);
    }

    m_isOn       = false;
    m_hasEntries = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::GetSize
//
//  Nothing until the trace has been on: a Debug build keeps a short ring of
//  its own for the illegal-opcode dump, which is not the debugger's trace.
//
////////////////////////////////////////////////////////////////////////////////

size_t TraceController::GetSize() const
{
    return m_hasEntries ? m_host.GetCpu()->GetCpu6502()->GetTraceSize() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::GetWindow
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::GetWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const
{
    const Cpu6502    * cpu   = m_host.GetCpu()->GetCpu6502();
    size_t             size  = GetSize();
    size_t             last  = (first < size) ? first + std::min (count, size - first) : first;
    Cpu::TraceEntry    entry = {};
    TraceRecord        record;



    entries.clear();

    for (size_t index = first; index < last; index++)
    {
        if (!cpu->TryGetTraceEntry (index, entry))
        {
            break;
        }

        record               = TraceRecord();
        record.index         = index;
        record.cycles        = entry.cycles;
        record.pc            = entry.pc;
        record.opcode        = entry.opcode;
        record.op1           = entry.op1;
        record.op2           = entry.op2;
        record.a             = entry.a;
        record.x             = entry.x;
        record.y             = entry.y;
        record.sp            = entry.sp;
        record.p             = entry.p;
        record.isInterrupt   = entry.intr != Cpu::kTraceIntrNone;
        record.hasAccess     = entry.hasAccess;
        record.accessIsWrite = entry.accessIsWrite;
        record.accessData    = entry.accessData;
        record.accessAddress = entry.accessAddress;

        entries.push_back (record);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::OnWatchedAccess
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::OnWatchedAccess (
    Word                  address,
    Byte                  value,
    BusAccess             access,
    std::optional<Byte>   previous)
{
    UNREFERENCED_PARAMETER (previous);

    m_host.GetCpu()->GetCpu6502()->RecordTraceAccess (address, value, access == BusAccess::Write);
}
