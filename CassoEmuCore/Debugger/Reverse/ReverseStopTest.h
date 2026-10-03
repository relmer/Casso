#pragma once

#include "Core/IWatchSink.h"
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
////////////////////////////////////////////////////////////////////////////////

class ReverseStopTest : public IReverseStopTest,
                        public IWatchSink
{
public:
    explicit ReverseStopTest (DebugSession & session) : m_session (session) {}

    // IReverseStopTest
    bool          ShouldStopBefore (MachineHost & machine, Word pc) override;
    bool          TakePendingStop  () override;
    IWatchSink  * GetWatchSink     () override { return this; }

    // IWatchSink
    void          OnWatchedAccess  (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override;

private:
    bool          TryConsumeCpuOwnRead (Word address);

    DebugSession         & m_session;
    bool                   m_isPending   = false;
    Word                   m_fetchPc     = 0;
    Byte                   m_fetchesLeft = 0;       // bit n: the byte at m_fetchPc + n
    std::optional<Word>    m_storeTarget;
};
