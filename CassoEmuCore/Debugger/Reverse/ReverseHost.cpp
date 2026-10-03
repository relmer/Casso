#include "Pch.h"

#include "Debugger/Reverse/ReverseHost.h"

#include "Debugger/Reverse/IReverseStopTest.h"
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
//  needs a stop test; Seek takes its position as argument.
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





