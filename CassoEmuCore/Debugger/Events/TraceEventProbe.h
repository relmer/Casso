#pragma once

#include "Pch.h"

#include "Core/IWatchSink.h"
#include "Debugger/Events/DebugEventJob.h"
#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceEventProbe
//
//  The instruction trace as a rebuild's probe: while a stretch replays, each
//  instruction goes into the replaying CPU's own trace ring and the bus
//  reports every access here, as the debugger's HISTORY does live, so a
//  rebuilt entry has the fields a live one has. Detaching moves the ring's
//  entries out, each stamped with its position: the instructions retired
//  before it since the stretch's start.
//
//  The ring's size is that of the longest stretch reserved so far, and its
//  buffer is reused from one stretch to the next.
//
////////////////////////////////////////////////////////////////////////////////

class TraceEventProbe : public IDebugEventProbe<Cpu::TraceEntry>, public IWatchSink
{
public:
    //  The ring's size before any Reserve raises it.
    static constexpr size_t  kInitialCapacity = 65536;

    //  Room for a stretch of this many instructions. Any thread; the ring
    //  only grows.
    void     Reserve (size_t instructions);

    HRESULT  Attach  (MachineHost & machine, std::vector<DebugEventEntry<Cpu::TraceEntry>> & entries) override;
    void     Detach  (MachineHost & machine) override;

    // IWatchSink
    void     OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override;

private:
    std::atomic<size_t>                                m_capacity { kInitialCapacity };
    MachineHost                                      * m_machine  = nullptr;
    std::vector<DebugEventEntry<Cpu::TraceEntry>>    * m_entries  = nullptr;
    uint64_t                                           m_start    = 0;
};
