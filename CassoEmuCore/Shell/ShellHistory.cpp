#include "Pch.h"

#include "Shell/ShellHistory.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/DebugViewModel.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/IShellDebugContext.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/MachineHost.h"
#include "Config/DebuggerSettings.h"
#include "Core/TextEncoding.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/HistoryBand.h"
#include "resource.h"





//  The console's line for a step back out with no caller in history.
static constexpr const char  * s_kNoCallerText = "No caller to step back out to.";





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ShellHistory
//
////////////////////////////////////////////////////////////////////////////////

ShellHistory::ShellHistory (
    IDebuggerHost        & host,
    MachineHost          & machine,
    CpuManager           & cpuManager,
    DebuggerSettings     & settings,
    DebugEventLogs       & eventLogs,
    IShellDebugContext   & context,
    DebugViewModel       & viewModel) :
    m_host       (host),
    m_machine    (machine),
    m_cpuManager (cpuManager),
    m_settings   (settings),
    m_eventLogs  (eventLogs),
    m_context    (context),
    m_viewModel  (viewModel)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::~ShellHistory
//
////////////////////////////////////////////////////////////////////////////////

ShellHistory::~ShellHistory() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsRecording
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsRecording() const
{
    return m_reverseHost != nullptr && m_reverseHost->IsRecording();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsBehindLive
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsBehindLive() const
{
    return m_reverseHost != nullptr && m_reverseHost->IsBehindLive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SyncInputGate
//
//  A debugger edit can make a paused machine live again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SyncInputGate()
{
    if (m_reverseHost != nullptr)
    {
        m_reverseHost->SyncInputGate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::Diverge
//
//  Discards the history after the machine's position, and makes the machine
//  live there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellHistory::Diverge()
{
    HRESULT  hr = S_OK;



    CBRA (m_reverseHost != nullptr);

    hr = m_reverseHost->Diverge();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::TryRequest
//
//  The session's reverse requester: a command posted to the CPU thread
//  while recording is on.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::TryRequest (ReverseCommand command, uint64_t argument)
{
    bool  isRecording = IsRecording();



    if (isRecording)
    {
        m_isLandingReplyDue.store (command == ReverseCommand::Seek || command == ReverseCommand::SeekCycle ||
                                   command == ReverseCommand::SeekBack || command == ReverseCommand::GoLive, std::memory_order_release);
        PostCommand (command, argument);
    }

    return isRecording;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ReportCuts
//
//  A replay that didn't match its keyframe cuts history there, whatever
//  started it: a seek, a step, or the machine run on from the past. Each
//  cut goes to the console with the state section the replay differed in,
//  so a cut is never silent.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ReportCuts()
{
    const ReverseController  * controller = (m_reverseHost != nullptr) ? &m_reverseHost->GetController() : nullptr;



    if (controller == nullptr || controller->GetSeeker().GetCutCount() == m_reportedCuts)
    {
        return;
    }

    m_reportedCuts = controller->GetSeeker().GetCutCount();

    m_context.AddConsoleLines ({ "History diverged from its recording on a replay, so the history after the last keyframe that matched was dropped.",
                                 "  Differs in: " + (controller->GetSeeker().GetLastDivergence().empty() ? std::string ("unknown") : controller->GetSeeker().GetLastDivergence()) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::StartRecording
//
//  Begins the machine's history where it stands, once it is built and power
//  cycled, from the settings the emulator read at start or Tools > Options
//  set since; with recording off, any history is dropped instead. The first call creates the host,
//  whose live callback hands the game-port input held back while the
//  machine was behind live to the UI thread, which writes it and releases the
//  keys and mouse button let go of in the meantime. Step back out asks the
//  debugger's call record, while one is attached, whether any call in
//  history entered the code now running.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StartRecording()
{
    HRESULT  hr = S_OK;



    if (!m_isReverseOn)
    {
        StopRecording();
        m_eventLogs.Start (nullptr);
        return;
    }

    if (m_reverseHost == nullptr)
    {
        m_reverseHost = std::make_unique<ReverseHost> (m_machine);
        m_reverseHost->GetController().GetSeeker().SetReplayControl (&m_replayControl);

        m_reverseHost->GetController().GetStepper().SetCallerProbe ([this] (uint64_t historyStartCycle)
        {
            return m_context.GetDebugSession() != nullptr && m_context.GetDebugSession()->GetCallRecords().HasNoCallSince (historyStartCycle);
        });

        m_reverseHost->GetController().GetStepper().SetCallerLinksProbe ([this] (std::vector<CallerLink> & outLinks)
        {
            outLinks.clear();

            if (m_context.GetDebugSession() != nullptr)
            {
                m_context.GetDebugSession()->GetCallRecords().GetCallerLinks (outLinks);
            }
        });

        m_reverseHost->SetLiveCallback ([this] ()
        {
            PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellGameportFlush, 0, 0);
        });
    }

    hr = m_reverseHost->StartRecording (ReverseHost::MakeSettings (m_reverseBudgetMb, m_reverseIntervalFrames));

    if (FAILED (hr))
    {
        DEBUGMSG (L"Reverse execution could not start recording: 0x%08X\n", hr);
    }

    //  A history begun again: the timeline's pictures were of the old one,
    //  and the next are drawn on a machine built as this one is, as are the
    //  heat map's rebuilds and its look-ups of last accesses, and the call
    //  record's rebuilds.
    m_historyThumbnails.Clear();
    m_historyThumbnails.Attach (m_reverseHost->IsRecording() ? &m_reverseHost->GetController().GetKeyframes() : nullptr);
    m_historyRenderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_heatReplayer.SetMachine    (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_heatFinder.SetMachine      (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_callReplayer.SetMachine    (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

    //  A profile from history starts each block inside the calls open at its
    //  keyframe, from the copies of the call record taken there.
    m_eventLogs.GetProfileHistory().SetCallsAt ([this] (uint64_t position, std::vector<CallStackFrame> & outFrames)
    {
        CallRecord             record;
        std::span<const Byte>  packed;
        HRESULT                hr     = S_OK;

        if (m_context.GetDebugController() == nullptr)
        {
            return false;
        }

        packed = m_context.GetDebugController()->GetCallHistory().GetCopies().GetPacked (position);
        hr     = packed.empty() ? E_FAIL : CallRecordCopies::Unpack (packed, record);

        if (FAILED (hr))
        {
            return false;
        }

        outFrames = std::move (record.frames);
        return true;
    });

    //  The event logs begin again with history, or without it when it did
    //  not start.
    m_eventLogs.Start (m_reverseHost->IsRecording() ? &m_reverseHost->GetController() : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::StopRecording
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopRecording()
{
    m_eventLogs.Stop();
    m_historyThumbnails.Attach (nullptr);

    if (m_reverseHost != nullptr)
    {
        m_reverseHost->StopRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ApplyOptions
//
//  CPU thread: Tools > Options' settings, taken at once. Turning recording
//  off makes the machine live first, announced as any return to live is,
//  then drops history; turning it on starts recording where the machine
//  stands. A new budget applies to the history already held: a smaller one
//  drops the oldest snapshots until it fits, and a larger one grows the
//  store now.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ApplyOptions (
    bool  isRecording,
    int   budgetMb)
{
    HRESULT  hr          = S_OK;
    bool     wasOn       = m_isReverseOn && m_reverseHost != nullptr && m_reverseHost->IsRecording();
    size_t   budgetBytes = ReverseHost::MakeSettings (budgetMb, m_reverseIntervalFrames).keyframes.budgetBytes;



    m_isReverseOn     = isRecording;
    m_reverseBudgetMb = budgetMb;

    if (!isRecording)
    {
        if (wasOn && m_reverseHost->IsBehindLive())
        {
            RunCommand (ReverseCommand::GoLive, 0);
        }

        StopRecording();
        m_eventLogs.Start (nullptr);

        //  The caption is otherwise updated only while recording is on, so
        //  a replay's note would stay in the title.
        UpdateCaption();
    }
    else if (!wasOn)
    {
        StartRecording();
    }
    else
    {
        hr = m_reverseHost->GetController().ChangeBudget (budgetBytes);
        CHR (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::PostCommand
//
//  UI thread: queues the command for the CPU thread, which owns the machine.
//  A stop asked for before now belongs to an earlier command, and is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PostCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    m_replayControl.isStopRequested.store (false, memory_order_release);

    m_cpuManager.PostCommand (IDM_DEBUG_REVERSE, [command, argument] (IDebugCommandTarget & target) { target.RunReverseCommand (command, argument); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RunCommand
//
//  CPU thread. Moves the machine through its history and announces where it
//  landed the way a step's stop is announced, with the outcome in the stop's
//  history. The replay ran the speaker, whose clicks are not sound to play,
//  and the screen is drawn from the state the machine now holds.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RunCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    HRESULT        hr          = S_OK;
    EmuCpu       * cpu         = m_machine.GetCpu();
    AppleSpeaker * speaker     = m_machine.GetRefs().speaker;
    bool           isPublished = false;
    ReverseResult  result;
    StopEvent      stop;



    BAIL_OUT_IF (m_reverseHost == nullptr || cpu == nullptr, S_OK);
    BAIL_OUT_IF (!m_reverseHost->IsRecording(), S_OK);

    //  The debugger's status bar reads these while the replay runs, since no
    //  snapshot is built until it ends.
    m_replayControl.progress.store (ReplayControl::kNoProgress, memory_order_relaxed);
    m_replayStartedAt.store (GetTickCount64(), memory_order_relaxed);
    m_isReplayingHistory.store (true, memory_order_release);

    //  Where the machine stands before the command, so the call record
    //  starts again only if the command moves it or replays anything on it.
    if (m_context.GetDebugController() != nullptr)
    {
        m_context.GetDebugController()->GetCallHistory().OnMoving();
    }

    hr = m_reverseHost->Execute (command, argument, m_reverseStopTest, result);

    m_isReplayingHistory.store (false, memory_order_release);

    //  Landed or not, the machine may have moved; the heat map follows it,
    //  rebuilding its heat once a drag of the timeline is let go, and so does
    //  the call record.
    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->GetTarget().NoteHistoryMoved (command == ReverseCommand::ScrubCycle);
    }

    if (m_context.GetDebugController() != nullptr)
    {
        m_context.GetDebugController()->GetCallHistory().OnMoved (command == ReverseCommand::ScrubCycle);
    }

    CHR (hr);

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    stop.reason    = StopReason::Step;
    stop.pc        = cpu->GetPC();
    stop.registers = cpu->GetCpu6502()->GetRegisters();
    stop.history   = result.outcome;

    m_lastReverseOutcome  = result.outcome;
    m_lastReversePosition = m_machine.GetRunner().GetPosition();

    if (result.outcome == ReverseOutcome::NoCaller)
    {
        m_context.AddConsoleLines ({ s_kNoCallerText });
    }

    if (m_isLandingReplyDue.exchange (false, memory_order_acq_rel))
    {
        m_context.AddConsoleLines ({ GetLandingText (result.outcome) });
    }

    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->OnStopped (stop);
    }

    m_viewModel.MarkDirty();

    m_host.DrawFrame();

Error:
    //  Landed or not, the command has run, so a script waiting on it goes on.
    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->SetReversePending (false);
    }

    //  Landed or not, the seek is no longer on its way. The timeline hears
    //  where the machine stands before it hears of the landing, so a line
    //  held where it was dropped moves straight to where the seek landed.
    if (command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle)
    {
        m_seekCycleLanded.store (argument, memory_order_release);

        isPublished = TryPublishPlayhead();
        IGNORE_RETURN_VALUE (isPublished, false);

        m_historyThumbnails.GetPlayhead().NoteSeekLanded (argument);
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"Reverse execution command %d failed: 0x%08X\n", static_cast<int> (command), hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetLandingText
//
//  Where a SEEK or LIVE left the machine: the time it was there, the cycle
//  count as it is in the Registers pane, and the instruction count SEEK
//  INSTR takes; then why the command stopped short, if it did.
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellHistory::GetLandingText (ReverseOutcome outcome)
{
    EmuCpu        * cpu      = m_machine.GetCpu();
    uint64_t        position = m_machine.GetRunner().GetPosition();
    uint64_t        wallTime = 0;
    std::wstring    clock;
    std::wstring    text;



    if (cpu == nullptr)
    {
        return std::string();
    }

    if (m_reverseHost != nullptr && m_reverseHost->GetController().GetSpan().IsInHistory())
    {
        wallTime = m_reverseHost->GetController().GetTimeline().GetWallTimeAtPosition (position, cpu->GetHistoryCycles());
        clock    = DebuggerStatusText::FormatWallClock (wallTime, LOCALE_NAME_USER_DEFAULT);
    }

    text = clock.empty() ? std::wstring (L"Live") : L"At " + clock;
    text += std::format (L", cycle {}, instruction {}.", DebuggerStatusText::FormatCount (cpu->GetTotalCycles()), DebuggerStatusText::FormatCount (position));

    if (outcome != ReverseOutcome::Moved)
    {
        text += L" " + HistoryBand::GetOutcomeText (outcome);
    }

    return TextEncoding::WideToNarrow (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::UpdateCaption
//
//  CPU thread. Behind live the caption says where the machine stands, by the
//  host's clock to the second, so it ticks once a second while the replay
//  runs and moves with every step; live it says nothing. The caption is
//  composed again only when the note changes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::UpdateCaption()
{
    EmuCpu        * cpu       = m_machine.GetCpu();
    bool            isBehind  = m_reverseHost != nullptr && cpu != nullptr && m_reverseHost->IsBehindLive();
    uint64_t        cycle     = 0;
    std::wstring    caption;
    bool            isChanged = false;



    if (isBehind)
    {
        cycle   = cpu->GetHistoryCycles();
        caption = DebuggerStatusText::GetReplayCaption (m_reverseHost->GetController().GetTimeline().GetWallTimeAt (cycle), cycle, LOCALE_NAME_USER_DEFAULT);
    }

    {
        std::lock_guard<std::mutex>  held (m_replayCaptionMutex);

        isChanged = caption != m_replayCaption;

        if (isChanged)
        {
            m_replayCaption = std::move (caption);
        }
    }

    if (isChanged)
    {
        m_host.UpdateWindowTitle();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetCaption
//
//  Any thread. The note UpdateReplayCaption keeps, for the caption.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellHistory::GetCaption()
{
    std::lock_guard<std::mutex>  held (m_replayCaptionMutex);



    return m_replayCaption;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RecordFrame
//
//  CPU thread, ahead of each frame. Recording pauses at a Maximum speed
//  the user chose, and running the machine from the past makes it live again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RecordFrame (bool isUserMaximumSpeed)
{
    HRESULT  hr = S_OK;



    if (m_reverseHost != nullptr)
    {
        hr = m_reverseHost->OnFrame (isUserMaximumSpeed);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::LoadSettings
//
//  CPU thread, when it starts. The settings hold for every machine built
//  after; Tools > Options changes them through ApplyReverseOptions.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::LoadSettings()
{
    m_isReverseOn           = m_settings.reverseRecording;
    m_reverseBudgetMb       = m_settings.reverseBudgetMb;
    m_reverseIntervalFrames = m_settings.reverseIntervalFrames;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SeekCycle
//
//  UI thread: the timeline was clicked, or its playhead dragged, at a cycle,
//  for a machine it has seen stopped; isInterim while the drag goes on. The
//  CPU thread drops it when no history is kept. The cycle and when it was
//  asked for are kept, so the timeline can tell whether the seek has landed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SeekCycle (
    uint64_t  cycle,
    bool      isInterim)
{
    m_seekCyclePosted.store   (cycle,            memory_order_release);
    m_seekCyclePostedAt.store (GetTickCount64(), memory_order_release);

    PostCommand (isInterim ? ReverseCommand::ScrubCycle : ReverseCommand::SeekCycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsSeekBusy
//
//  UI thread: whether the CPU thread has yet to run the last seek to a cycle
//  the timeline asked for. A seek the queue never ran, as one dropped with
//  the machine it was for, stops counting after kPatienceMs, so a drag of
//  the timeline is never held up for good.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsSeekBusy() const
{
    constexpr uint64_t  kPatienceMs = 500;



    uint64_t  posted   = m_seekCyclePosted.load   (memory_order_acquire);
    uint64_t  postedAt = m_seekCyclePostedAt.load (memory_order_acquire);
    uint64_t  landed   = m_seekCycleLanded.load   (memory_order_acquire);



    return posted != landed && GetTickCount64() - postedAt < kPatienceMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ServiceThumbnails
//
//  CPU thread, between frames and while paused: the timeline's points are
//  laid out over the keyframes and the next wanted picture is handed to its
//  worker. Never during a reverse command, which runs from the command
//  queue. The timeline hears where the machine stands every turn, so its
//  live or replay state, its line and its labels follow the machine as it
//  runs, as does the caption's replay note.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ServiceThumbnails()
{
    HRESULT  hr = S_OK;



    if (!TryPublishPlayhead())
    {
        return;
    }

    UpdateCaption();

    hr = m_historyThumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::TryPublishPlayhead
//
//  CPU thread: tells the timeline where the machine stands, whether that is
//  behind live, and where history begins. False while no history is kept.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::TryPublishPlayhead()
{
    EmuCpu                * cpu       = m_machine.GetCpu();
    uint64_t                cycle     = 0;
    bool                    isBehind  = false;
    const KeyframeStore   * keyframes = nullptr;



    if (m_reverseHost == nullptr || !m_reverseHost->IsRecording() || cpu == nullptr)
    {
        return false;
    }

    cycle     = cpu->GetHistoryCycles();
    isBehind  = m_reverseHost->IsBehindLive();
    keyframes = &m_reverseHost->GetController().GetKeyframes();

    m_historyThumbnails.GetPlayhead().SetPosition (m_machine.GetRunner().GetPosition(), isBehind);
    m_historyThumbnails.GetPlayhead().SetTime (cycle, m_reverseHost->GetController().GetTimeline().GetWallTimeAt (cycle), isBehind ? m_reverseHost->GetController().GetSeeker().GetLiveEndCycle() : cycle);

    if (keyframes->GetArena().GetCount() > 0)
    {
        m_historyThumbnails.GetPlayhead().SetBegin (keyframes->GetArena().GetInfo (0).cycle, keyframes->GetArena().GetInfo (0).wallTime);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetReplayProgress
//
//  UI thread: how long the running reverse command has run and how much of
//  history it has searched, read from what the CPU thread sets as it goes.
//
////////////////////////////////////////////////////////////////////////////////

ReplayProgress ShellHistory::GetReplayProgress()
{
    ReplayProgress  progress;



    progress.isReplaying = m_isReplayingHistory.load (memory_order_acquire);

    if (progress.isReplaying)
    {
        progress.elapsedMs = GetTickCount64() - m_replayStartedAt.load (memory_order_relaxed);
        progress.fraction  = m_replayControl.progress.load (memory_order_relaxed);
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetStatus
//
//  The last reverse command's outcome stands until the machine moves from
//  where it landed, by running or by a step, and is dropped then.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus ShellHistory::GetStatus()
{
    HistoryStatus  status;
    bool           hasMoved = !m_cpuManager.IsPaused() || m_machine.GetRunner().GetPosition() != m_lastReversePosition;



    if (m_reverseHost != nullptr)
    {
        status = m_reverseHost->GetStatus();
    }

    if (hasMoved)
    {
        m_lastReverseOutcome.reset();
    }

    status.outcome = m_lastReverseOutcome;
    return status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SyncHeat
//
//  CPU thread. Attached, the debugger's heat map keeps its counts in the
//  history and is told what the history does; detached, before the debugger
//  goes, the two are unlinked.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SyncHeat (bool isAttached)
{
    IDebugTarget       * target     = (m_context.GetDebugSession() != nullptr) ? &m_context.GetDebugSession()->GetTarget() : nullptr;
    ReverseController  * controller = (m_reverseHost != nullptr) ? &m_reverseHost->GetController() : nullptr;
    bool                 isLinked   = isAttached && target != nullptr && controller != nullptr;



    if (controller != nullptr)
    {
        controller->GetCapturer().SetObserver (isLinked ? target->GetHistoryObserver() : nullptr);
    }

    if (target != nullptr)
    {
        target->AttachHistory       (isLinked ? &controller->GetKeyframes() : nullptr, isLinked ? &m_heatReplayer : nullptr);
        target->SetHeatAccessFinder (isLinked ? &m_heatFinder : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ServiceCalls
//
//  CPU thread, once a pass while the debugger exists: the debugger's call
//  record is rebuilt from history whenever it starts mid-run. With no
//  history the two are unlinked, and a rebuild under way is dropped. The
//  debugger going drops one too: its call history unlinks itself as it is
//  destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ServiceCalls()
{
    ReverseController  * controller = (m_reverseHost != nullptr) ? &m_reverseHost->GetController() : nullptr;
    bool                 isLinked   = controller != nullptr;



    if (m_context.GetDebugController() == nullptr)
    {
        return;
    }

    m_context.GetDebugController()->GetCallHistory().Attach (controller, isLinked ? &m_callReplayer : nullptr);
    m_context.GetDebugController()->GetCallHistory().Service();
}





