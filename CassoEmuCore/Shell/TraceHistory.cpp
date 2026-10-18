#include "Pch.h"

#include "Shell/TraceHistory.h"

#include "Debugger/Reverse/ReverseController.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeProbe
//
//  The runner's probe, and a pointer to it for sizing its ring.
//
////////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<IDebugEventProbe<Cpu::TraceEntry>> MakeProbe (TraceEventProbe *& outProbe)
{
    auto  probe = std::make_unique<TraceEventProbe>();



    outProbe = probe.get();
    return probe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::TraceHistory
//
////////////////////////////////////////////////////////////////////////////////

TraceHistory::TraceHistory (MachineHost & machine, DebugEventHistory & history) :
    m_machine (machine),
    m_history (history),
    m_runner  (MakeProbe (m_probe))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::~TraceHistory
//
////////////////////////////////////////////////////////////////////////////////

TraceHistory::~TraceHistory()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Start
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::Start (ReverseController * controller)
{
    Stop();

    m_controller = controller;
    m_runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Stop
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::Stop()
{
    m_runner.Cancel();
    m_rebuilt.clear();
    m_pending.clear();
    m_controller = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::OnHistoryChanged
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::OnHistoryChanged()
{
    m_runner.Cancel();
    m_rebuilt.clear();
    m_pending.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Pump
//
////////////////////////////////////////////////////////////////////////////////

bool TraceHistory::Pump()
{
    DebugEventStretchResult<Cpu::TraceEntry>  result;
    bool                                      isAny  = false;



    while (m_runner.TryTakeResult (result))
    {
        auto  bounds = std::ranges::find_if (m_pending, [&result] (const DebugEventBounds & each) { return each.startPosition == result.startPosition && each.endPosition == result.endPosition; });



        if (result.generation != m_generation || bounds == m_pending.end())
        {
            continue;
        }

        Store (*bounds, std::move (result));
        isAny = true;
    }

    return isAny;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::GetSpans
//
//  The bounds, joined where one runs on into the next with no gap.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::GetSpans (std::vector<TraceSpan> & outSpans)
{
    std::vector<DebugEventBounds>  bounds;
    bool                           isJoined = false;



    outSpans.clear();
    GetBounds (bounds);

    for (const DebugEventBounds & each : bounds)
    {
        if (isJoined && outSpans.back().to == each.startPosition)
        {
            outSpans.back().to = each.endPosition;
        }
        else
        {
            outSpans.push_back ({ each.startPosition, each.endPosition });
        }

        isJoined = !each.hasGapAfter;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::TryGetEntries
//
//  The stretches [from, to) overlaps, rebuilt at once while the machine is
//  stopped; while it runs, the ones not rebuilt yet are queued for a
//  background rebuild and this call returns nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool TraceHistory::TryGetEntries (uint64_t from, uint64_t to, std::vector<TraceHistoryEntry> & outEntries)
{
    HRESULT                        hr      = S_OK;
    std::vector<DebugEventBounds>  bounds;
    std::vector<DebugEventBounds>  missing;
    bool                           isReady = false;



    outEntries.clear();
    GetBounds (bounds);

    //  Newest first, as a job takes them.
    for (auto each = bounds.rbegin(); each != bounds.rend(); ++each)
    {
        if (each->startPosition < to && each->endPosition > from && Find (*each) == nullptr)
        {
            missing.push_back (*each);
        }
    }

    if (!missing.empty() && !IsStopped())
    {
        hr = RequestAhead (missing);
        IGNORE_RETURN_VALUE (hr, S_OK);
        BAIL_OUT_IF (true, S_OK);
    }

    if (!missing.empty())
    {
        hr = Rebuild (missing);
        CHR (hr);
    }

    for (const DebugEventBounds & each : bounds)
    {
        Rebuilt  * rebuilt = (each.startPosition < to && each.endPosition > from) ? Find (each) : nullptr;
        uint64_t   first   = std::max (from, each.startPosition);
        uint64_t   last    = std::min (to, each.endPosition);



        if (rebuilt == nullptr)
        {
            continue;
        }

        outEntries.insert (outEntries.end(),
                           rebuilt->entries.begin() + (ptrdiff_t) (first - each.startPosition),
                           rebuilt->entries.begin() + (ptrdiff_t) (last  - each.startPosition));
    }

    isReady = outEntries.size() == to - from;

Error:
    if (!isReady)
    {
        outEntries.clear();
    }

    return isReady;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::FindCycle
//
//  The stretch whose keyframe is the last at or before cycle, by the
//  keyframes' cycles, and then the instruction within it; one stretch
//  replayed, not the whole of history.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t TraceHistory::FindCycle (uint64_t cycle)
{
    std::vector<DebugEventBounds>    bounds;
    std::vector<TraceHistoryEntry>   entries;
    const KeyframeStore            * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    size_t                           index     = 0;
    size_t                           i         = 0;
    uint64_t                         found     = UINT64_MAX;



    GetBounds (bounds);

    for (i = bounds.size(); i > 0 && found == UINT64_MAX; i--)
    {
        bool  isFound = keyframes != nullptr && keyframes->TryFindByPosition (bounds[i - 1].startPosition, index);



        if (!isFound || keyframes->GetInfo (index).cycle > cycle)
        {
            continue;
        }

        if (TryGetEntries (bounds[i - 1].startPosition, bounds[i - 1].endPosition, entries))
        {
            auto  at = std::ranges::find_if (entries, [cycle] (const TraceHistoryEntry & each) { return each.isAvailable && each.entry.cycles >= cycle; });



            found = (at != entries.end()) ? bounds[i - 1].startPosition + (uint64_t) (at - entries.begin()) : found;
        }

        //  A cycle after the stretch's last instruction is at the next one's
        //  start.
        if (found == UINT64_MAX && i < bounds.size())
        {
            found = bounds[i].startPosition;
        }

        break;
    }

    if (found == UINT64_MAX && !bounds.empty() && i == 0)
    {
        found = bounds.front().startPosition;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::GetBounds
//
//  The history's stretches up to the machine's position, the one it is in
//  cut there; after the newest keyframe, the live stretch to the machine's
//  position, unless the machine runs live, so it grows, or recording is
//  paused at Maximum speed, so it ends somewhere history has no record of.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::GetBounds (std::vector<DebugEventBounds> & outBounds) const
{
    uint64_t                         position  = m_machine.GetPosition();
    const KeyframeStore            * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    size_t                           count     = (keyframes != nullptr && m_controller->IsRecording()) ? keyframes->GetCount() : 0;
    uint64_t                         newest    = 0;
    bool                             isGrowing = false;
    std::vector<DebugEventBounds>    stretches;



    outBounds.clear();

    if (count == 0)
    {
        return;
    }

    m_history.GetStretches (stretches);

    for (DebugEventBounds bounds : stretches)
    {
        if (bounds.startPosition >= position)
        {
            break;
        }

        if (bounds.endPosition > position)
        {
            bounds.endPosition = position;
            bounds.hasGapAfter = false;
        }

        outBounds.push_back (bounds);
    }

    newest    = keyframes->GetInfo (count - 1).position;
    isGrowing = m_controller->IsRecordingLive() && !IsStopped();

    if (newest < position && !isGrowing && !m_controller->IsPaused())
    {
        outBounds.push_back ({ newest, position, false });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Find
//
//  A stretch rebuilt with these bounds, made the most recently used.
//
////////////////////////////////////////////////////////////////////////////////

TraceHistory::Rebuilt * TraceHistory::Find (const DebugEventBounds & bounds)
{
    auto  at = std::ranges::find_if (m_rebuilt, [&bounds] (const Rebuilt & each) { return each.bounds.startPosition == bounds.startPosition && each.bounds.endPosition == bounds.endPosition; });



    if (at == m_rebuilt.end())
    {
        return nullptr;
    }

    m_rebuilt.splice (m_rebuilt.begin(), m_rebuilt, at);

    return &m_rebuilt.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Rebuild
//
//  On this thread: any background job is dropped first, since the runner
//  replays one job at a time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TraceHistory::Rebuild (const std::vector<DebugEventBounds> & stretches)
{
    HRESULT                                               hr = S_OK;
    DebugEventJob                                         job;
    std::vector<DebugEventStretchResult<Cpu::TraceEntry>> results;
    size_t                                                i  = 0;



    m_runner.Cancel();
    m_runner.WaitForWork();
    m_pending.clear();

    for (const DebugEventBounds & each : stretches)
    {
        m_probe->Reserve ((size_t) (each.endPosition - each.startPosition));
    }

    hr = m_history.BuildJob (stretches, job);
    CHR (hr);

    hr = m_runner.Rebuild (job, results);
    CHR (hr);

    for (i = 0; i < results.size() && i < stretches.size(); i++)
    {
        Store (stretches[i], std::move (results[i]));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::RequestAhead
//
//  A background job for the stretches, unless the one running already has
//  them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TraceHistory::RequestAhead (const std::vector<DebugEventBounds> & stretches)
{
    HRESULT        hr  = S_OK;
    DebugEventJob  job;



    BAIL_OUT_IF (stretches == m_pending, S_OK);

    for (const DebugEventBounds & each : stretches)
    {
        m_probe->Reserve ((size_t) (each.endPosition - each.startPosition));
    }

    hr = m_history.BuildJob (stretches, job);
    CHR (hr);

    m_pending    = stretches;
    m_generation = job.generation;

    hr = m_runner.Submit (std::make_shared<const DebugEventJob> (std::move (job)));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::Store
//
//  One entry for each position of the bounds: the probe's, at their
//  positions, and an unavailable one for any position it has none for.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHistory::Store (const DebugEventBounds & bounds, DebugEventStretchResult<Cpu::TraceEntry> && result)
{
    Rebuilt   rebuilt;
    uint64_t  length  = bounds.endPosition - bounds.startPosition;



    rebuilt.bounds        = bounds;
    rebuilt.isUnavailable = result.isUnavailable || FAILED (result.hr);
    rebuilt.entries.assign ((size_t) length, TraceHistoryEntry { Cpu::TraceEntry {}, false });

    for (const DebugEventEntry<Cpu::TraceEntry> & each : result.entries)
    {
        if (!rebuilt.isUnavailable && each.position >= bounds.startPosition && each.position < bounds.endPosition)
        {
            rebuilt.entries[(size_t) (each.position - bounds.startPosition)] = { each.event, true };
        }
    }

    m_rebuilt.push_front (std::move (rebuilt));

    while (m_rebuilt.size() > kRebuiltStretches)
    {
        m_rebuilt.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistory::IsStopped
//
////////////////////////////////////////////////////////////////////////////////

bool TraceHistory::IsStopped() const
{
    return !m_isStopped || m_isStopped();
}
