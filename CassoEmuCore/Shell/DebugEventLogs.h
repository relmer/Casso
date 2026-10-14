#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/DebugEventReplayer.h"
#include "Debugger/Events/DebugLogDelta.h"
#include "Debugger/Events/MachineEventFeed.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Shell/ScratchEventReplayer.h"

class MachineHost;
class ReverseController;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventLogs
//
//  The Disk II event log and the input log (FR-210 to FR-215): each an event
//  cache the running machine's History slot feeds from the start of the
//  session (FR-211), a rebuild of older stretches on a scratch machine
//  (FR-212), and what was sent to its pane.
//
//  The caches take events only while the machine is live and recording, so
//  nothing the machine does replaying its own history, or in a gap, goes in.
//  At each keyframe taken live the input read edges are reset, as a rebuild
//  resets them at the keyframe it starts from, and a keyframe after a gap
//  marks the gap in both caches. Each time the machine becomes live both
//  caches are cut back to its position, which drops the future a change
//  made in the past discarded; when keyframes go with it, the rebuilt
//  stretches after the cut go too.
//
//  A pane open on a log is sent deltas: everything in the cache when it
//  opens, then each event as it is cached, and older rows rebuilt from
//  history after a request for the rows before its oldest. The input log's
//  host input, and both logs' resets and power cycles, come from the input
//  journal's entries, in position order with the events.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class DebugEventLogs
{
public:
    static constexpr size_t  kCacheEvents = 65536;

    explicit DebugEventLogs (MachineHost & machine);
    ~DebugEventLogs();

    DebugEventLogs             (const DebugEventLogs &) = delete;
    DebugEventLogs & operator= (const DebugEventLogs &) = delete;

    //  History begins again at the machine's position: after a machine is
    //  built, a state is loaded, or recording is turned on or off. The
    //  controller is null while recording is off; the caches then take
    //  every event.
    void  Start (ReverseController * controller);
    void  Stop  ();

    //  Takes the stretches rebuilt since the last call.
    void  Pump  ();

    void  SetOpen        (DebugLogKind kind, bool isOpen);
    void  RequestEarlier (DebugLogKind kind);
    bool  IsOpen         (DebugLogKind kind) const;

    //  What changed for an open pane since the last call; an empty delta
    //  while closed. True when the delta has anything for the pane: a row
    //  added or dropped, or a change to where its rows start or to whether
    //  more are being rebuilt.
    bool  TakeDelta (DiskLogDelta & outDelta);
    bool  TakeDelta (InputLogDelta & outDelta);

    const DebugEventCache<Disk2Event>  & GetDiskCache  () const { return m_disk.cache; }
    const DebugEventCache<InputEvent>  & GetInputCache () const { return m_input.cache; }

    //  Test seam: rebuilds on the given queue in place of a pool thread.
    void  SetWorkQueue (IWorkQueue * queue);

private:
    template <typename T>
    struct Log
    {
        Log (IDebugEventHistory & history, std::unique_ptr<IDebugEventProbe<T>> probe)
            : runner   (std::move (probe)),
              replayer (cache, history, runner)
        {
        }

        DebugEventCache<T>       cache        { kCacheEvents };
        ScratchEventReplayer<T>  runner;
        DebugEventReplayer<T>    replayer;

        bool                     isOpen       = false;
        bool                     isResetDue   = false;   // the pane gets everything again
        uint64_t                 cutFrom      = DebugLogDelta<T>::kNoCut;
        uint64_t                 sentSerial   = 0;       // the cache's entries before this serial were sent
        size_t                   sentJournal  = 0;       // and the journal's entries before this index
        uint64_t                 heldFrom     = 0;       // the pane's rows start here
        uint64_t                 wantedBefore = 0;       // the pane's request for the rows before this; 0 for none

        uint64_t                 sentHeldFrom   = UINT64_MAX;   // the last delta sent to the pane
        bool                     sentAtStart    = false;
        bool                     sentRebuilding = false;
    };

    void      OnKeyframe   (uint64_t position);
    void      OnLive       (uint64_t position);
    void      EndAt        (uint64_t position);
    void      OnDrop       (KeyframeDrop drop);
    bool      IsRecording  () const;
    uint64_t  GetWallTime  (uint64_t cycle) const;
    uint64_t  GetOldest    () const;

    template <typename T>
    void      CutBack      (Log<T> & log, uint64_t position);

    template <typename T>
    bool      BuildDelta   (Log<T> & log, DebugLogDelta<T> & outDelta);

    template <typename T>
    void      SendNewer    (Log<T> & log, DebugLogDelta<T> & outDelta);

    template <typename T>
    void      SendOlder    (Log<T> & log, DebugLogDelta<T> & outDelta);

    template <typename T>
    void      MergeJournal (std::vector<DebugLogRow<T>> & rows, uint64_t from, uint64_t to, size_t firstIndex, size_t endIndex);

    template <typename T>
    void      AppendEntries (const std::vector<DebugEventEntry<T>> & entries, std::vector<DebugLogRow<T>> & rows) const;

    static bool  TryMakeRow (const InputRecord & record, DiskLogRow & outRow);
    static bool  TryMakeRow (const InputRecord & record, InputLogRow & outRow);

    size_t    FindJournalIndex (uint64_t position) const;

    MachineHost          & m_machine;
    ReverseController    * m_controller = nullptr;
    bool                   m_isStarted  = false;
    DebugEventHistory      m_history;
    MachineEventFeed       m_feed;
    Log<Disk2Event>        m_disk;
    Log<InputEvent>        m_input;
};
