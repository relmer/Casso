#include "Pch.h"

#include "Debugger/Reverse/ReverseController.h"
#include "Debugger/Reverse/IHistoryObserver.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::ReverseController
//
//  Every replay that moves the machine to a point runs muted: the sound and
//  printout it reproduces happened when the machine first ran them. Running
//  on from the past has its sound (ReverseSeeker::ReplayHere). The history
//  observer gets every keyframe a replay loads. Each capture schedules the
//  next, and each change between live and history goes through
//  OnLiveChanged.
//
////////////////////////////////////////////////////////////////////////////////

ReverseController::ReverseController (MachineHost & machine) :
    m_machine  (machine),
    m_replayer (machine, m_keyframes)
{
    m_replayer.SetOutputGate ([&machine] (bool isMuted) { machine.GetOutputMute().SetMuted (isMuted); });

    m_replayer.SetStateLoadedCallback ([this] ()
    {
        IHistoryObserver  * observer = m_capturer.GetObserver();



        if (observer != nullptr)
        {
            observer->OnKeyframeLoaded (m_machine.GetRunner().GetPosition());
        }
    });

    m_capturer.SetCapturedCallback ([this] () { ScheduleCaptures(); });
    m_seeker.SetLiveChangedCallback ([this] (bool isLive) { OnLiveChanged (isLive); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::~ReverseController
//
////////////////////////////////////////////////////////////////////////////////

ReverseController::~ReverseController()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Turns the input journal on and takes the first keyframe at once, so
//  history begins at the moment the journal attaches; the keyframe store
//  takes its whole budget there. The disks are machine state from here: a
//  disk that leaves its bay is stored in memory while history may put it
//  back, and the automatic flushes wait whenever the machine is behind live.
//  A pause chosen before the start applies from the first keyframe on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::Start (const ReverseSettings & settings)
{
    constexpr uint64_t  kMinInstructionCycles = 2;
    constexpr size_t    kStepSlack            = 64;
    HRESULT             hr                    = S_OK;
    EmuCpu            * cpu                   = m_machine.GetCpu();
    size_t              stepRoom              = static_cast<size_t> (settings.keyframes.intervalCycles / kMinInstructionCycles) + kStepSlack;



    CBRA (cpu);

    m_keyframes.Configure (settings.keyframes);

    hr = m_capturer.UseWorkQueue();
    CHR (hr);

    m_stepTable.Reserve (stepRoom);
    m_stepper.Reserve   (stepRoom);

    m_machine.GetHostInput().GetJournal().Clear();
    m_machine.GetHostInput().SetJournalOn (true);
    m_machine.GetRunner().SetHistoryRecorder (this);

    m_machine.GetDiskStore().SetFlushHold      (false);
    m_machine.GetDiskStore().SetMediaRetention (true);

    m_span.Start();

    hr = m_capturer.CaptureNow();
    CHR (hr);

    if (m_span.IsPaused())
    {
        m_seeker.BecomeLive();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLiveListener
//
//  The owner's listener replaces any it set before; a null one removes it.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::SetLiveListener (
    const void    * owner,
    LiveListener    listener)
{
    std::erase_if (m_liveListeners, [owner] (const std::pair<const void *, LiveListener> & each) { return each.first == owner; });

    if (listener)
    {
        m_liveListeners.emplace_back (owner, std::move (listener));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRecordingLive
//
//  Whether what the machine does now becomes new history: recording, live,
//  not paused at Maximum speed, and not replaying history.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseController::IsRecordingLive() const
{
    return m_span.IsRecording() && m_span.IsLive() && !m_span.IsPaused() && !m_replayer.IsReplaying();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Detaches from the machine, drops all history and frees the memory it
//  used. The disks keep any writes they have, which the next flush writes as
//  they are. A machine run on from the past leaves the disk store marked as
//  replaying, and the printer muted, until it is live again, so the mark and
//  the mute go here with the flush pause: quitting, a machine switch and a
//  state load stop recording wherever the machine is, and flush, print or
//  start recording again right after.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::Stop()
{
    if (m_machine.GetRunner().GetHistoryRecorder() == this)
    {
        m_machine.GetRunner().SetHistoryRecorder (nullptr);
    }

    if (m_span.IsRecording())
    {
        m_machine.GetHostInput().SetJournalOn (false);
        m_machine.GetHostInput().GetJournal().Clear();

        m_machine.GetDiskStore().SetFlushHold      (false);
        m_machine.GetDiskStore().SetReplaying      (false);
        m_machine.GetDiskStore().SetMediaRetention (false);

        m_machine.GetOutputMute().SetMuted (false);
    }

    m_keyframes.Release();
    m_stepTable.Release();
    m_stepper.Release();
    m_span.Stop();

    m_nextDueCycle = 0;
    m_isArrivalDue = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetUserMaximumSpeed
//
//  The user chose Maximum speed, or left it. Recording pauses at Maximum:
//  where the machine is, is the end of history, until a keyframe taken
//  where recording resumes starts the next part, marked as following a gap.
//  A speed raised automatically must not come here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::SetUserMaximumSpeed (bool isMaximum)
{
    HRESULT   hr        = S_OK;
    uint64_t  gapStart  = m_span.GetPauseStart();
    bool      isChanged = false;
    bool      hasGap    = false;



    isChanged = m_span.SetPaused (isMaximum);
    BAIL_OUT_IF (!isChanged, S_OK);

    BAIL_OUT_IF (!m_span.IsRecording() || !m_span.IsLive(), S_OK);

    if (isMaximum)
    {
        m_seeker.BecomeLive();
        BAIL_OUT_IF (true, S_OK);
    }

    hasGap = m_machine.GetRunner().GetPosition() > gapStart;

    m_machine.GetHostInput().SetJournalOn (true);

    if (hasGap)
    {
        hr = m_capturer.CaptureBoundary (true, gapStart);
        CHR (hr);
    }

    m_seeker.BecomeLive();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeBudget
//
//  Takes a new byte budget for the keyframes. In the past, the group of the
//  keyframe at or before the machine's position is not dropped, since every
//  step from there replays from that keyframe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::ChangeBudget (size_t budgetBytes)
{
    return m_keyframes.ChangeBudget (budgetBytes, m_machine.GetRunner().GetPosition());
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMediaChanged
//
//  A disk went in or out, or its file's write protection changed. The journal
//  has the command, but a replay cannot redo it from the file, which may
//  have changed since; a boundary keyframe stores the disks as they are
//  instead. It is taken as a debugger edit's is: in the past at once, and
//  live before the next instruction or reverse command. Taken at once live,
//  it could store a machine no replay arrives at: the reset command reads the
//  disks back before the reset itself, so the keyframe had the machine from
//  before the reset with the reset's record behind it, and a replay loading
//  it ran on as though the reset had never happened.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnMediaChanged (MachineHost & machine)
{
    OnMachineEdited (machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineEdited
//
//  The debugger changed memory, registers or I/O state. In the past that
//  drops the history after it at once. Live, the change becomes one boundary
//  keyframe before the next instruction or reverse command, so a fill or a
//  run of pokes costs one capture rather than one each.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnMachineEdited (MachineHost & machine)
{
    HRESULT  hr = S_OK;



    if (!m_span.IsRecording() || m_replayer.IsReplaying() || &machine != &m_machine)
    {
        return;
    }

    if (!m_span.IsLive())
    {
        hr = m_seeker.OnMachineChanged();
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    m_span.MarkEdit();
    m_nextDueCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCaptureDue
//
//  A keyframe fell due, or the machine is in history, where every
//  instruction start comes here. During a replay the keyframes exist
//  already. A machine run or stepped from the past by anything but a replay
//  replays the recorded future as it goes, until it reaches the end of it.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnCaptureDue (uint64_t cycle)
{
    HRESULT  hr = S_OK;



    UNREFERENCED_PARAMETER (cycle);

    if (m_replayer.IsReplaying())
    {
        return;
    }

    if (!m_span.IsLive())
    {
        hr = m_seeker.ReplayHere (true);
    }
    else if (m_span.TakeEdit())
    {
        hr = m_seeker.OnMachineChanged();
    }
    else
    {
        hr = m_capturer.CaptureNow();
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnArrived
//
//  Behind live, after an instruction the machine ran itself: the inputs
//  recorded where it arrived are applied, so it is where a replay stopping
//  there would leave it.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnArrived()
{
    HRESULT  hr = S_OK;



    if (m_replayer.IsReplaying() || m_span.IsLive())
    {
        return;
    }

    hr = m_seeker.ReplayHere (false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLiveChanged
//
//  Behind live, every instruction start and end comes to the recorder.
//  Live again, the step back out's caller links go, the keyframes fall due
//  on their schedule, and the live listeners are called last.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnLiveChanged (bool isLive)
{
    if (!isLive)
    {
        m_nextDueCycle = 0;
        m_isArrivalDue = true;
        return;
    }

    m_stepper.DiscardCallerLinks();

    ScheduleCaptures();

    for (const auto & [owner, listener] : m_liveListeners)
    {
        listener (m_machine.GetRunner().GetPosition());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleCaptures
//
//  The next cycle at which a keyframe falls due: never while paused, and
//  zero in history, so every instruction start is checked there.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::ScheduleCaptures()
{
    m_isArrivalDue = m_span.IsInHistory();

    if (!m_span.IsLive())
    {
        m_nextDueCycle = 0;
    }
    else if (m_span.IsPaused())
    {
        m_nextDueCycle = UINT64_MAX;
    }
    else
    {
        m_nextDueCycle = m_keyframes.GetNextDueCycle();
    }
}
