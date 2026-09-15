#pragma once

#include "Core/IWatchSink.h"
#include "Debugger/IDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Watchpoint
//
////////////////////////////////////////////////////////////////////////////////

struct Watchpoint
{
    int          id      = 0;
    WatchAccess  access  = WatchAccess::ReadWrite;
    Word         first   = 0;
    Word         last    = 0;
    bool         enabled = true;
    uint32_t     hits    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable
//
//  Memory watchpoints (BPM, BPMR, BPMW). The table is the bus's watch sink:
//  the target publishes the pages of enabled watchpoints to the bus, the bus
//  reports every access to those pages, and a report inside an enabled range
//  with a matching access records the hit and raises a pending stop, which
//  takes effect at the next instruction boundary.
//
////////////////////////////////////////////////////////////////////////////////

class WatchpointTable : public IWatchSink
{
public:
    explicit WatchpointTable (int & nextId);

    void   SetTarget        (IDebugTarget * target);
    void   SetAccessPc      (Word pc) { m_accessPc = pc; }

    int    Add              (WatchAccess access, Word first, Word last);
    bool   TryClear         (int id);
    void   ClearAll         ();
    bool   TrySetEnabled    (int id, bool enabled);

    const std::vector<Watchpoint> &  GetAll () const { return m_entries; }

    bool   HasEnabled       () const;
    bool   HasPendingStop   () const { return m_pendingHit.has_value(); }
    const std::optional<WatchHit> &  GetPendingHit () const { return m_pendingHit; }
    void   ClearPending     () { m_pendingHit.reset(); }

    WatchedPages  GetWatchedPages () const;

    void   OnWatchedAccess  (Word address, Byte value, BusAccess access) override;

private:
    static constexpr int  kPageShift = 8;

    static bool  IsAccessMatch (WatchAccess watched, BusAccess actual);
    void         Publish       ();

    int                      & m_nextId;
    std::vector<Watchpoint>    m_entries;
    IDebugTarget             * m_target   = nullptr;
    Word                       m_accessPc = 0;
    std::optional<WatchHit>    m_pendingHit;
};
