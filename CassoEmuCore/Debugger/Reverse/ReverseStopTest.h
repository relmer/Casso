#pragma once

#include "Core/IWatchSink.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/DiskBreakpointMonitor.h"
#include "Debugger/Reverse/IReverseStopTest.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest
//
//  A debug session's breakpoints, memory conditions and watchpoints as the
//  stop test reverse continue replays history with. It reads the session's
//  tables and changes nothing in them: no hit is counted, no stop is
//  recorded, nothing is logged, and the session's own watch sink never sees
//  a replayed access.
//
//  As in a forward run, a read watchpoint passes over the CPU's fetch of the
//  instruction's own bytes and an indexed store's read of its target.
//
//  Disk breakpoints are matched by a listener of the stop test's own, which
//  primes from the record of the snapshot a replay starts from, as the
//  debugger's does after a move through history. An RWTS or driver
//  breakpoint stops at the routine its vector had at that point in the
//  replay: the routines are taken from the vectors when a replay starts,
//  which the cycle count jumping marks, and again after a write to a
//  vector's bytes.
//
//  Applesoft's breakpoints are matched by a monitor of its own as well, its
//  variable entries' values taken from the snapshot a replay starts from. A
//  change made before that snapshot by a statement that ends after it is
//  not seen.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseStopTest : public IReverseStopTest,
                        public IWatchSink
{
public:
    explicit ReverseStopTest (DebugSession & session);

    // IReverseStopTest
    bool              ShouldStopBefore (MachineHost & machine, Word pc) override;
    bool              TakePendingStop  () override;
    IWatchSink      * GetWatchSink     () override { return this; }
    IDisk2EventSink * GetDiskSink      () override;

    // IWatchSink
    void          OnWatchedAccess  (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override;

private:
    bool          TryConsumeCpuOwnRead (Word address);
    void          ResolveRoutines  ();

    DebugSession         & m_session;
    bool                   m_isPending   = false;
    Word                   m_fetchPc     = 0;
    Byte                   m_fetchesLeft = 0;       // bit n: the byte at m_fetchPc + n
    std::optional<Word>    m_storeTarget;

    DiskBreakpointMonitor    m_disk;
    BasicStopMonitor         m_basic;
    std::map<int, Word>      m_routines;
    std::vector<Word>        m_vectorBytes;
    std::optional<uint64_t>  m_lastCycle;
};
