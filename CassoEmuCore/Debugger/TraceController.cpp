#include "Pch.h"

#include "Debugger/TraceController.h"

#include "Shell/MachineHost.h"
#include "Debugger/Reply.h"





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
//  gets its access. A trace already on goes on as it is, entries and all.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::On()
{
    MemoryBus  & bus = m_host.GetMemoryBus();
    Cpu6502    * cpu = m_host.GetCpu()->GetCpu6502();



    if (m_isOn)
    {
        return;
    }

    bus.SetTraceSink     (this);
    bus.SetTraceAllPages (true);
    cpu->EnableTrace     (kCapacity);

    m_ringFrom   = m_host.GetPosition();
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
//  After a machine switch the entries belonged to the old CPU, which went
//  with them. The new CPU's ring is its own -- the --trace capture or a Debug
//  build's look-back -- and is left recording.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::Clear()
{
    MemoryBus  & bus = m_host.GetMemoryBus();



    bus.SetTraceAllPages (false);
    bus.SetTraceSink     (nullptr);

    m_isOn       = false;
    m_hasEntries = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::GetSize
//
//  Without history, nothing until the trace has been on: a Debug build has
//  a short ring of its own for the illegal-opcode dump, which is not the
//  debugger's trace.
//
////////////////////////////////////////////////////////////////////////////////

size_t TraceController::GetSize() const
{
    std::vector<TraceSpan>  spans;
    uint64_t                total = 0;



    if (m_history == nullptr)
    {
        return m_hasEntries ? m_host.GetCpu()->GetCpu6502()->GetTraceSize() : 0;
    }

    GetSpans (spans);

    for (const TraceSpan & span : spans)
    {
        total += span.to - span.from;
    }

    return (size_t) total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::TryGetRingSpan
//
////////////////////////////////////////////////////////////////////////////////

bool TraceController::TryGetRingSpan (TraceSpan & outSpan) const
{
    const Cpu  * cpu      = (m_host.GetCpu() != nullptr) ? m_host.GetCpu()->GetCpu6502() : nullptr;
    uint64_t     position = m_host.GetPosition();
    bool         isWhole  = false;



    isWhole = m_hasEntries && cpu != nullptr && position >= m_ringFrom && cpu->GetTraceCount() == position - m_ringFrom && cpu->GetTraceSize() > 0;

    if (isWhole)
    {
        outSpan = { position - cpu->GetTraceSize(), position };
    }

    return isWhole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::GetSpans
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::GetSpans (std::vector<TraceSpan> & outSpans) const
{
    TraceSpan               ring;
    std::vector<TraceSpan>  joined;



    m_history->GetSpans (outSpans);

    if (!TryGetRingSpan (ring))
    {
        return;
    }

    outSpans.push_back (ring);
    std::ranges::sort (outSpans, [] (const TraceSpan & a, const TraceSpan & b) { return a.from < b.from; });

    for (const TraceSpan & span : outSpans)
    {
        if (!joined.empty() && span.from <= joined.back().to)
        {
            joined.back().to = std::max (joined.back().to, span.to);
        }
        else
        {
            joined.push_back (span);
        }
    }

    outSpans = std::move (joined);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::GetWindow
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::GetWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const
{
    const Cpu6502            * cpu    = m_host.GetCpu()->GetCpu6502();
    Cpu::TraceEntry            entry  = {};
    std::vector<TraceSpan>     spans;
    uint64_t                   skip   = first;
    uint64_t                   wanted = count;



    entries.clear();

    if (m_history == nullptr)
    {
        size_t  size = GetSize();
        size_t  last = (first < size) ? first + std::min (count, size - first) : first;



        for (size_t index = first; index < last && cpu->TryGetTraceEntry (index, entry); index++)
        {
            entries.push_back (MakeRecord (entry));
        }

        return;
    }

    //  The index counts the instructions the spans cover before it.
    GetSpans (spans);

    for (const TraceSpan & span : spans)
    {
        uint64_t  length = span.to - span.from;
        uint64_t  from   = 0;
        uint64_t  to     = 0;



        if (wanted == 0)
        {
            break;
        }

        if (skip >= length)
        {
            skip -= length;
            continue;
        }

        from   = span.from + skip;
        to     = from + std::min (wanted, length - skip);
        skip   = 0;
        wanted -= to - from;

        AddEntries (from, to, entries);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::AddEntries
//
//  The ring's entries for the positions it has; history's for those before,
//  or entries marked as rebuilding while history's rebuild runs.
//
////////////////////////////////////////////////////////////////////////////////

void TraceController::AddEntries (uint64_t from, uint64_t to, std::vector<TraceRecord> & entries) const
{
    const Cpu6502                   * cpu     = m_host.GetCpu()->GetCpu6502();
    TraceSpan                         ring;
    uint64_t                          split   = to;
    std::vector<TraceHistoryEntry>    rebuilt;
    Cpu::TraceEntry                   entry   = {};
    uint64_t                          pos     = 0;



    if (TryGetRingSpan (ring))
    {
        split = std::clamp (ring.from, from, to);
    }

    if (split > from)
    {
        if (m_history->TryGetEntries (from, split, rebuilt))
        {
            for (const TraceHistoryEntry & each : rebuilt)
            {
                TraceRecord  record = MakeRecord (each.entry);



                record.isAvailable = each.isAvailable;
                entries.push_back (record);
            }
        }
        else
        {
            for (pos = from; pos < split; pos++)
            {
                TraceRecord  record;



                record.isAvailable  = false;
                record.isRebuilding = true;
                entries.push_back (record);
            }
        }
    }

    for (pos = split; pos < to && cpu->TryGetTraceEntry ((size_t) (pos - ring.from), entry); pos++)
    {
        entries.push_back (MakeRecord (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::TryFindCycle
//
//  History finds the instruction by its keyframes' cycles, so only the
//  stretch it is in is rebuilt; the ring, newer than history's spans, by a
//  search through its own entries.
//
////////////////////////////////////////////////////////////////////////////////

bool TraceController::TryFindCycle (uint64_t cycle, size_t & outIndex) const
{
    std::vector<TraceSpan>  spans;
    uint64_t                position = UINT64_MAX;
    uint64_t                index    = 0;
    TraceSpan               ring;



    if (m_history == nullptr)
    {
        return false;
    }

    position = m_history->FindCycle (cycle);

    if (position == UINT64_MAX && TryGetRingSpan (ring))
    {
        const Cpu6502    * cpu   = m_host.GetCpu()->GetCpu6502();
        size_t             low   = 0;
        size_t             high  = (size_t) (ring.to - ring.from);
        Cpu::TraceEntry    entry = {};



        while (low < high)
        {
            size_t  mid = low + (high - low) / 2;



            if (cpu->TryGetTraceEntry (mid, entry) && entry.cycles < cycle)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }

        position = (low < (size_t) (ring.to - ring.from)) ? ring.from + low : UINT64_MAX;
    }

    GetSpans (spans);

    for (const TraceSpan & span : spans)
    {
        if (position != UINT64_MAX && position < span.to)
        {
            index += (position > span.from) ? position - span.from : 0;
            outIndex = (size_t) index;
            return true;
        }

        index += span.to - span.from;
    }

    outIndex = (size_t) index;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController::MakeRecord
//
////////////////////////////////////////////////////////////////////////////////

TraceRecord TraceController::MakeRecord (const Cpu::TraceEntry & entry)
{
    TraceRecord  record;



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

    return record;
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

    if (m_isHostAccess)
    {
        return;
    }

    m_host.GetCpu()->GetCpu6502()->RecordTraceAccess (address, value, access == BusAccess::Write);
}
