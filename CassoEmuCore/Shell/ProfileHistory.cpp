#include "Pch.h"

#include "Shell/ProfileHistory.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeProbe
//
//  The probe for the runner to own, and the same probe for the history to
//  set the period on and read the progress from.
//
////////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<IDebugEventProbe<ProfileCountsPtr>> MakeProbe (ProfileEventProbe *& outProbe)
{
    auto  probe = std::make_unique<ProfileEventProbe>();



    outProbe = probe.get();
    return probe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::ProfileHistory
//
////////////////////////////////////////////////////////////////////////////////

ProfileHistory::ProfileHistory (MachineHost & machine, DebugEventHistory & history) :
    m_machine (machine),
    m_history (history),
    m_runner  (MakeProbe (m_probe))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::~ProfileHistory
//
////////////////////////////////////////////////////////////////////////////////

ProfileHistory::~ProfileHistory()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Start
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::Start (ReverseController * controller)
{
    Stop();

    m_controller = controller;
    m_runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::Stop()
{
    m_runner.Cancel();
    m_remaining  = 0;
    m_latest.reset();
    m_controller = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::IsStopped
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileHistory::IsStopped() const
{
    return !m_isStopped || m_isStopped();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetBounds
//
//  History's stretches from the keyframe at or before fromCycle to the
//  machine's position, the one it is in cut there, joined wherever no gap
//  divides them; then the stretch since the newest keyframe, unless the
//  machine runs live, so it grows, or recording is paused, so it ends where
//  history has no record.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::GetBounds (uint64_t fromCycle, std::vector<DebugEventBounds> & outBounds) const
{
    uint64_t                         position  = m_machine.GetPosition();
    const KeyframeStore            * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    size_t                           count     = (keyframes != nullptr && m_controller->IsRecording()) ? keyframes->GetCount() : 0;
    size_t                           first     = 0;
    uint64_t                         start     = 0;
    uint64_t                         newest    = 0;
    std::vector<DebugEventBounds>    stretches;



    outBounds.clear();

    if (count == 0)
    {
        return;
    }

    start = keyframes->TryFindAtOrBefore (fromCycle, first) ? keyframes->GetInfo (first).position : keyframes->GetInfo (0).position;

    m_history.GetStretches (stretches);

    for (DebugEventBounds bounds : stretches)
    {
        if (bounds.endPosition <= start)
        {
            continue;
        }

        if (bounds.startPosition >= position)
        {
            break;
        }

        if (bounds.endPosition > position)
        {
            bounds.endPosition = position;
            bounds.hasGapAfter = false;
        }

        if (!outBounds.empty() && !outBounds.back().hasGapAfter && outBounds.back().endPosition == bounds.startPosition)
        {
            outBounds.back().endPosition = bounds.endPosition;
            outBounds.back().hasGapAfter = bounds.hasGapAfter;
        }
        else
        {
            outBounds.push_back (bounds);
        }
    }

    newest = keyframes->GetInfo (count - 1).position;

    if (newest < position && newest >= start && !(m_controller->IsRecordingLive() && !IsStopped()) && !m_controller->IsPaused())
    {
        if (!outBounds.empty() && !outBounds.back().hasGapAfter && outBounds.back().endPosition == newest)
        {
            outBounds.back().endPosition = position;
        }
        else
        {
            outBounds.push_back ({ newest, position, false });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Build
//
//  The runner replays one job at a time, so the one under way is dropped
//  before the probe takes the new period.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProfileHistory::Build (uint64_t fromCycle, std::string & error)
{
    HRESULT                         hr      = S_OK;
    std::vector<DebugEventBounds>   bounds;
    DebugEventJob                   job;
    uint64_t                        toCycle = (m_machine.GetCpu() != nullptr) ? m_machine.GetCpu()->GetTotalCycles() : 0;
    bool                            isAny   = false;



    GetBounds (fromCycle, bounds);
    isAny = !bounds.empty();
    CBRF (isAny, error = "There is no history to profile.");

    m_runner.Cancel();
    m_runner.WaitForWork();

    //  The job takes the stretches newest first.
    std::ranges::reverse (bounds);

    hr = m_history.BuildJob (bounds, job);
    CHRF (hr, error = "History could not be read for the profile.");

    m_probe->Begin (fromCycle, toCycle);
    m_generation = job.generation;
    m_remaining  = bounds.size();
    m_latest.reset();

    hr = m_runner.Submit (std::make_shared<const DebugEventJob> (std::move (job)));
    CHRF (hr, error = "The profile could not be started.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Pump
//
//  Each stretch's entry has the counts so far, so the latest one replaces
//  the one before it until the job's last stretch is in.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileHistory::Pump (ProfileCountsPtr & outCounts)
{
    DebugEventStretchResult<ProfileCountsPtr>  result;



    while (m_remaining > 0 && m_runner.TryTakeResult (result))
    {
        if (result.generation != m_generation)
        {
            continue;
        }

        if (!result.entries.empty())
        {
            m_latest = result.entries.back().event;
        }

        m_remaining--;
    }

    if (m_remaining > 0 || m_latest == nullptr)
    {
        return false;
    }

    outCounts = std::move (m_latest);
    m_latest.reset();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetProgress
//
////////////////////////////////////////////////////////////////////////////////

std::optional<double> ProfileHistory::GetProgress() const
{
    return (m_remaining > 0) ? std::optional<double> (m_probe->GetProgress()) : std::nullopt;
}
