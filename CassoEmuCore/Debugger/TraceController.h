#pragma once

#include "Core/IWatchSink.h"
#include "Debugger/ITraceHistory.h"

class MachineHost;
struct TraceRecord;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceController
//
//  The debugger's instruction trace over a machine. On sizes the CPU's trace
//  ring to kCapacity entries and publishes every page of the bus to the
//  watched path, with this object as the trace sink, so each instruction's
//  last data access is recorded onto its entry. Off stops recording and puts
//  the bus back on the watch mask; the retained entries stay readable until
//  the trace is turned on again or cleared.
//
//  WITH HISTORY, THE TRACE IS HISTORY'S (FR-198, FR-210): its entries are
//  every instruction history covers, in position order, and an entry's
//  index counts the instructions history covers before it. The ring is only
//  a cache of the newest of them, used while it has an entry for every
//  instruction since it started, so for none once the machine has moved
//  through history; the rest are rebuilt from history. With no history, as
//  in a batch run or with recording off, the trace is the ring's alone.
//
//  While off, neither the CPU nor the bus does any work for the trace: the
//  CPU's gate is false and the bus's watched path holds the watchpoints'
//  pages alone.
//
//  The machine is reached through the host on each call, because a machine
//  switch replaces the CPU and the bus.
//
////////////////////////////////////////////////////////////////////////////////

class TraceController : public IWatchSink
{
public:
    static constexpr size_t  kCapacity = 100000;

    explicit TraceController (MachineHost & host) : m_host (host) {}
    ~TraceController() override;

    void    On        ();
    void    Off       ();

    //  Off, and the retained entries discarded, for a machine switch.
    void    Clear     ();

    bool    IsOn      () const { return m_isOn; }
    size_t  GetSize   () const;

    //  Where older instructions come from; null for none.
    void    SetHistory (ITraceHistory * history) { m_history = history; }

    //  The index of the first entry at or after cycle; false when the trace
    //  cannot find it any faster than a search through its entries would.
    bool    TryFindCycle (uint64_t cycle, size_t & outIndex) const;

    //  While set, bus accesses are the debugger's own (IN and OUT between
    //  instructions) and are not recorded onto the newest entry.
    void    SetHostAccess (bool isHostAccess) { m_isHostAccess = isHostAccess; }

    //  Up to count entries from first, oldest first, with the raw fields
    //  only: no disassembly and no symbols.
    void    GetWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const;

    // IWatchSink
    void    OnWatchedAccess (Word                  address,
                             Byte                  value,
                             BusAccess             access,
                             std::optional<Byte>   previous) override;

private:
    //  The positions the ring has, [first, the machine's position), while it
    //  has an entry for every instruction since it started; false otherwise.
    bool    TryGetRingSpan (TraceSpan & outSpan) const;

    //  History's spans and the ring's, joined, oldest first.
    void    GetSpans       (std::vector<TraceSpan> & outSpans) const;

    //  The entries of [from, to): the ring's where it has them, history's
    //  before.
    void    AddEntries     (uint64_t from, uint64_t to, std::vector<TraceRecord> & entries) const;

    static TraceRecord  MakeRecord (const Cpu::TraceEntry & entry);

    MachineHost    & m_host;
    ITraceHistory  * m_history      = nullptr;
    uint64_t         m_ringFrom     = 0;        // the machine's position when the ring started
    bool             m_isOn         = false;
    bool             m_hasEntries   = false;
    bool             m_isHostAccess = false;
};
