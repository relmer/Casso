#include "Pch.h"

#include "Debugger/Reverse/ReverseHost.h"

#include "Debugger/Reply.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseHost::ReverseHost
//
////////////////////////////////////////////////////////////////////////////////

ReverseHost::ReverseHost (MachineHost & machine) :
    m_machine    (machine),
    m_controller (machine)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartRecording
//
//  Begins history where the machine stands, which is right after a machine
//  is built and power cycled; a recording already running is dropped first,
//  since a machine switch replaces every device it was taken from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseHost::StartRecording (const ReverseSettings & settings)
{
    HRESULT  hr = S_OK;



    StopRecording();

    hr = m_controller.Start (settings);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_controller.Stop();
    }

    SyncInputGate();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopRecording
//
//  Drops history and lets the host's input through again. Call it before a
//  machine is torn down, and at exit, so the disks' held writes are flushed
//  by the paths that flush them.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseHost::StopRecording()
{
    m_controller.Stop();

    SyncInputGate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFrame
//
//  Once per CPU-thread frame, before the machine runs: recording pauses
//  while the user has chosen Maximum speed and resumes when they leave it.
//  A frame that runs the machine from the past makes it live again, which
//  the gate follows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseHost::OnFrame (bool isUserMaximumSpeed)
{
    HRESULT  hr = S_OK;



    if (m_controller.IsRecording())
    {
        hr = m_controller.SetUserMaximumSpeed (isUserMaximumSpeed);
        CHR (hr);
    }

Error:
    SyncInputGate();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncInputGate
//
//  Holds the host input gate while the machine is behind live and releases
//  it once it is not, calling the live callback on the release.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseHost::SyncInputGate()
{
    HostInputGate  & gate     = m_machine.GetHostInputGate();
    bool             isBehind = m_controller.IsInHistory();
    bool             isHeld   = gate.IsHeld();



    if (isBehind && !isHeld)
    {
        gate.Hold();
    }
    else if (!isBehind && isHeld)
    {
        gate.Release();

        if (m_onLive)
        {
            m_onLive();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Execute
//
//  Runs one reverse command. The host input gate is held before the machine
//  moves, so no host write lands in the replay, and it stays held for as
//  long as the command leaves the machine behind live. ReverseContinue
//  needs a stop test; Seek takes its position as argument. Behind live, the
//  instructions that led to where the machine landed are made ready for the
//  trace pane.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseHost::Execute (
    ReverseCommand       command,
    uint64_t             argument,
    IReverseStopTest   * stopTest,
    ReverseResult      & result)
{
    HRESULT  hr          = S_OK;
    bool     isRecording = m_controller.IsRecording();



    CBREx (isRecording, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE));

    m_machine.GetHostInputGate().Hold();

    hr = Move (command, argument, stopTest, result);
    CHR (hr);

    hr = m_controller.PrepareRecentSteps (result);
    CHR (hr);

Error:
    SyncInputGate();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Diverge
//
//  The user agreed to discard the recorded history after where the machine
//  stands, so a change can be made there: the future is dropped and the
//  machine is live where it is. Live, there is nothing to drop.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseHost::Diverge()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_controller.IsInHistory(), S_OK);

    hr = m_controller.OnMachineChanged();
    CHR (hr);

Error:
    SyncInputGate();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Move
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseHost::Move (
    ReverseCommand       command,
    uint64_t             argument,
    IReverseStopTest   * stopTest,
    ReverseResult      & result)
{
    HRESULT  hr = S_OK;



    switch (command)
    {
        case ReverseCommand::StepBack:
            hr = m_controller.StepBack (result);
            break;

        case ReverseCommand::StepBackOver:
            hr = m_controller.StepBackOver (result);
            break;

        case ReverseCommand::StepBackOut:
            hr = m_controller.StepBackOut (result);
            break;

        case ReverseCommand::ReverseContinue:
            CBREx (stopTest != nullptr, E_INVALIDARG);
            hr = m_controller.ReverseContinue (*stopTest, result);
            break;

        case ReverseCommand::StepForward:
            hr = m_controller.StepForward (result);
            break;

        case ReverseCommand::Seek:
            hr = m_controller.SeekToPosition (argument, result);
            break;

        case ReverseCommand::SeekCycle:
        case ReverseCommand::ScrubCycle:
            hr = m_controller.SeekToCycle (argument, result);
            break;

        case ReverseCommand::GoLive:
            hr = m_controller.SeekToPosition (m_controller.GetLiveEndPosition(), result);
            break;

        default:
            CBRAEx (false, E_INVALIDARG);
            break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeSettings
//
//  The controller's settings from the user's: the keyframe budget in
//  megabytes, and the keyframe interval in video frames.
//
////////////////////////////////////////////////////////////////////////////////

ReverseSettings ReverseHost::MakeSettings (
    int  budgetMb,
    int  intervalFrames)
{
    constexpr size_t  kBytesPerMb = 1024 * 1024;
    ReverseSettings   settings;



    settings.keyframes.budgetBytes    = static_cast<size_t> (std::max (budgetMb, 1)) * kBytesPerMb;
    settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles * static_cast<uint64_t> (std::max (intervalFrames, 1));

    return settings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStatus
//
//  Live, or not recording, the machine is no distance behind. Behind live,
//  the disks holding writes their files do not are counted too: the
//  automatic flushes are held there, so those writes wait for a commit.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus ReverseHost::GetStatus() const
{
    HistoryStatus    status;
    EmuCpu         * cpu      = m_machine.GetCpu();
    uint64_t         position = m_machine.GetPosition();
    uint64_t         cycle    = (cpu != nullptr) ? cpu->GetTotalCycles() : 0;
    uint64_t         endPos   = m_controller.GetLiveEndPosition();
    uint64_t         endCycle = m_controller.GetLiveEndCycle();



    status.isRecording  = m_controller.IsRecording();
    status.isBehindLive = m_controller.IsInHistory();
    status.cycle        = cycle;
    status.wallTime     = m_controller.GetWallTimeAt (cycle);

    if (status.isRecording)
    {
        FillBudget (m_controller.GetKeyframes(), status);
    }

    if (status.isBehindLive)
    {
        status.instructionsBehind = (endPos   > position) ? endPos   - position : 0;
        status.cyclesBehind       = (endCycle > cycle)    ? endCycle - cycle    : 0;
        status.unsavedDisks       = m_machine.GetDiskStore().CountUnsavedDisks();
    }

    return status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillBudget
//
//  The bytes the store holds against its budget, never more than the budget
//  and all of it once the oldest are being dropped, whether they are, and the
//  cycle and host time of its oldest keyframe, which move forward as the
//  oldest groups are dropped to stay within it.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseHost::FillBudget (
    const KeyframeStore  & store,
    HistoryStatus        & status)
{
    size_t  budget = store.GetSettings().budgetBytes;
    size_t  count  = store.GetCount();



    status.budgetBytes   = budget;
    status.usedBytes     = store.GetUsedBytes();
    status.hasHistory    = count > 0;
    status.isFull        = store.IsFull();
    status.beginCycle    = (count > 0) ? store.GetInfo (0).cycle    : 0;
    status.beginWallTime = (count > 0) ? store.GetInfo (0).wallTime : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecentTrace
//
//  Behind live, up to count instructions that led to the current position,
//  oldest first, as trace entries; their text and symbols are left for the
//  trace command's describer. Nothing while live.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseHost::GetRecentTrace (
    size_t                      count,
    std::vector<TraceRecord>  & outEntries) const
{
    std::vector<ReplayStep>  steps;
    TraceRecord              record;



    outEntries.clear();

    if (!m_controller.IsInHistory())
    {
        return;
    }

    m_controller.GetRecentSteps (count, steps);

    for (const ReplayStep & step : steps)
    {
        record        = TraceRecord();
        record.index  = outEntries.size();
        record.cycles = step.cycles;
        record.pc     = step.pc;
        record.opcode = step.opcode;
        record.op1    = step.op1;
        record.op2    = step.op2;
        record.a      = step.a;
        record.x      = step.x;
        record.y      = step.y;
        record.sp     = step.sp;
        record.p      = step.p;

        outEntries.push_back (record);
    }
}





