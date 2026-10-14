#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/ReplayDiskCopier.h"

class IReverseStopTest;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventStretch
//
//  One stretch of history, keyframe to keyframe: the packed keyframe it
//  begins at, with its position, cycle and journal index, and the position
//  and cycle it ends at, where the next keyframe begins.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugEventStretch
{
    PackedKeyframe  start;
    uint64_t        startPosition = 0;
    uint64_t        startCycle    = 0;
    size_t          journalIndex  = 0;
    uint64_t        endPosition   = 0;
    uint64_t        endCycle      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventJob
//
//  The input of a rebuild of events, copied out of history on the thread
//  that runs the machine so the replay can run on another: the stretches,
//  newest first; the journal's inputs from the oldest stretch's start, with
//  the index of the first; and the disks in the bays. A stretch is replayed
//  only when its keyframe was taken with those disks in the bays.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugEventJob
{
    uint64_t                        generation = 0;
    std::vector<DebugEventStretch>  stretches;
    size_t                          inputsFrom = 0;
    std::vector<InputRecord>        inputs;
    std::vector<ReplayDisk>         disks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventStretchResult
//
//  The events one stretch's replay produced, oldest first; which stretch;
//  and whether it worked. A stretch whose keyframe was taken with other
//  disks than the job's is not replayed, and isUnavailable is set: its
//  events cannot be rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct DebugEventStretchResult
{
    uint64_t                         generation    = 0;
    uint64_t                         startPosition = 0;
    uint64_t                         endPosition   = 0;
    std::vector<DebugEventEntry<T>>  entries;
    bool                             isUnavailable = false;
    HRESULT                          hr            = S_OK;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugEventProbe
//
//  A view's probe on a machine replaying history: Attach connects it to the
//  machine's event source, after which it appends each event to the entries
//  given, stamped with the machine's position and cycle; Detach disconnects
//  it. A probe that watches memory returns a stop test from GetStopTest, and
//  the replay puts that stop test's watch sink on the bus. Called on the
//  replaying thread.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class IDebugEventProbe
{
public:
    virtual                      ~IDebugEventProbe() = default;

    virtual HRESULT              Attach      (MachineHost & machine, std::vector<DebugEventEntry<T>> & entries) = 0;
    virtual void                 Detach      (MachineHost & machine) = 0;
    virtual IReverseStopTest   * GetStopTest ()                       { return nullptr; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugEventRunner
//
//  Replays the stretches of a job away from the thread that runs the
//  machine, one job at a time, with each stretch's events available from
//  TryTakeResult once it is done. A job submitted while another waits
//  replaces it, and one being replayed is abandoned, so only the newest
//  finishes. Results are taken on the machine's thread, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class IDebugEventRunner
{
public:
    virtual          ~IDebugEventRunner() = default;

    virtual HRESULT  Submit        (std::shared_ptr<const DebugEventJob> job) = 0;
    virtual bool     TryTakeResult (DebugEventStretchResult<T> & outResult) = 0;
    virtual void     Cancel        () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventBounds
//
//  Where one stretch of history begins and ends, by position. A stretch
//  before a gap in recording ends where recording paused, with hasGapAfter
//  set.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugEventBounds
{
    uint64_t  startPosition = 0;
    uint64_t  endPosition   = 0;
    bool      hasGapAfter   = false;

    bool  operator== (const DebugEventBounds & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugEventHistory
//
//  History as a rebuild of events uses it, on the thread that runs the
//  machine: its stretches, oldest first, the newest ending where the
//  machine's newest keyframe begins the live stretch; and a job for some of
//  them, given newest first.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugEventHistory
{
public:
    virtual          ~IDebugEventHistory() = default;

    virtual void     GetStretches (std::vector<DebugEventBounds> & outStretches) const = 0;
    virtual HRESULT  BuildJob     (const std::vector<DebugEventBounds> & stretches, DebugEventJob & outJob) = 0;
};
