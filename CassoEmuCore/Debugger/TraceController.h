#pragma once

#include "Core/IWatchSink.h"
#include "Debugger/Reply.h"

class MachineHost;





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

    //  Up to count entries from first, oldest first, with the raw fields
    //  only: no disassembly and no symbols.
    void    GetWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const;

    // IWatchSink
    void    OnWatchedAccess (Word                  address,
                             Byte                  value,
                             BusAccess             access,
                             std::optional<Byte>   previous) override;

private:
    MachineHost  & m_host;
    bool           m_isOn       = false;
    bool           m_hasEntries = false;
};
