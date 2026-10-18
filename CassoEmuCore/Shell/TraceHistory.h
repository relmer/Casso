#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/TraceEventProbe.h"
#include "Debugger/ITraceHistory.h"
#include "Shell/ScratchEventReplayer.h"

class MachineHost;
class ReverseController;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory
//
//  The instruction trace's depth, from history (FR-198, FR-210 to FR-212):
//  each stretch between keyframes is replayed on a scratch machine with
//  TraceEventProbe, and the most recently rebuilt stretches are cached, so
//  scrolling back and forth replays nothing twice.
//
//  The spans end at the machine's position. While the machine runs live,
//  the stretch since the newest keyframe grows with every instruction, so
//  it is left out until the machine stops; the live trace ring covers it
//  while HISTORY is on. A rebuild runs at once while the machine is
//  stopped, and in the background while it runs, so the machine's thread
//  never waits on a replay then.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class TraceHistory : public ITraceHistory
{
public:
    static constexpr size_t  kRebuiltStretches = 8;

    using IsStoppedFn = std::function<bool()>;

    TraceHistory (MachineHost & machine, DebugEventHistory & history);
    ~TraceHistory() override;

    TraceHistory             (const TraceHistory &) = delete;
    TraceHistory & operator= (const TraceHistory &) = delete;

    //  History begins again; null while recording is off.
    void  Start (ReverseController * controller);
    void  Stop  ();

    //  The stretches rebuilt so far no longer match history: the future
    //  after the machine's position was discarded, or keyframes went.
    void  OnHistoryChanged ();

    //  Takes the stretches a background rebuild finished. True when any
    //  came in.
    bool  Pump ();

    //  Whether the machine is stopped, so a rebuild may run at once.
    void  SetIsStopped  (IsStoppedFn isStopped) { m_isStopped = std::move (isStopped); }

    //  Test seam: rebuilds on the given queue in place of a pool thread.
    void  SetWorkQueue  (IWorkQueue * queue)    { m_runner.SetWorkQueue (queue); }

    // ITraceHistory
    void      GetSpans      (std::vector<TraceSpan> & outSpans) override;
    bool      TryGetEntries (uint64_t from, uint64_t to, std::vector<TraceHistoryEntry> & outEntries) override;
    uint64_t  FindCycle     (uint64_t cycle) override;

private:
    struct Rebuilt
    {
        DebugEventBounds                 bounds;
        bool                             isUnavailable = false;
        std::vector<TraceHistoryEntry>   entries;      // one for each position of the bounds
    };

    void        GetBounds    (std::vector<DebugEventBounds> & outBounds) const;
    Rebuilt   * Find         (const DebugEventBounds & bounds);
    HRESULT     Rebuild      (const std::vector<DebugEventBounds> & stretches);
    HRESULT     RequestAhead (const std::vector<DebugEventBounds> & stretches);
    void        Store        (const DebugEventBounds & bounds, DebugEventStretchResult<Cpu::TraceEntry> && result);
    bool        IsStopped    () const;

    MachineHost                                  & m_machine;
    DebugEventHistory                            & m_history;
    ReverseController                            * m_controller = nullptr;
    TraceEventProbe                              * m_probe      = nullptr;   // owned by the runner
    ScratchEventReplayer<Cpu::TraceEntry>          m_runner;
    std::list<Rebuilt>                             m_rebuilt;                // most recently used first
    std::vector<DebugEventBounds>                  m_pending;                // the background job's stretches
    uint64_t                                       m_generation = 0;
    IsStoppedFn                                    m_isStopped;
};
