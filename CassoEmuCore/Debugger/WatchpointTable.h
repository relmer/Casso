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
    WatchMode    mode    = WatchMode::After;
    bool         enabled = true;
    uint32_t     hits    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable
//
//  Memory watchpoints (BPM, BPMR, BPMW). The table is the bus's watch sink:
//  the target publishes the pages of enabled after-mode watchpoints to the
//  bus, the bus reports every access to those pages, and a report inside an
//  enabled range with a matching access records the hit and raises a pending
//  stop, which takes effect at the next instruction boundary.
//
//  One instruction can touch the same byte more than once: the CPU's indexed
//  fetches read a store's target before writing it. Within one instruction a
//  write replaces a pending read of the same address, and a later write
//  replaces an earlier one, so the stop reports the write and its value.
//
//  Before-mode watchpoints put no page in the mask; the session predicts them
//  from the instruction about to execute.
//
////////////////////////////////////////////////////////////////////////////////

class WatchpointTable : public IWatchSink
{
public:
    explicit WatchpointTable (int & nextId);

    void   SetTarget        (IDebugTarget * target);
    void   SetAccessPc      (Word pc) { m_accessPc = pc; }

    int    Add              (WatchAccess access, Word first, Word last, WatchMode mode = WatchMode::After);
    bool   TryClear         (int id);
    void   ClearAll         ();
    bool   TrySetEnabled    (int id, bool enabled);

    const std::vector<Watchpoint> &  GetAll () const { return m_entries; }

    bool   HasEnabled       () const;
    bool   HasPendingStop   () const { return m_pendingHit.has_value(); }
    const std::optional<WatchHit> &  GetPendingHit () const { return m_pendingHit; }
    void   ClearPending     () { m_pendingHit.reset(); }

    WatchedPages  GetWatchedPages () const;

    void   OnWatchedAccess  (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override;

private:
    static constexpr int  kPageShift = 8;

    static bool  IsAccessMatch  (WatchAccess watched, BusAccess actual);
    bool         ShouldReplace  (Word address, BusAccess access) const;
    void         Publish        ();

    int                      & m_nextId;
    std::vector<Watchpoint>    m_entries;
    IDebugTarget             * m_target   = nullptr;
    Word                       m_accessPc = 0;
    std::optional<WatchHit>    m_pendingHit;
};
