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
#include "Config/HistorySettings.h"
#include "Core/TextEncoding.h"
#include "Ui/Chrome/CassoTheme.h"
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
//  Both timelines' fork rows are labeled alike, a click on one switches to
//  its branch, and the row under the pointer is drawn over the screen.
//
////////////////////////////////////////////////////////////////////////////////

ShellHistory::ShellHistory (
    IDebuggerHost        & host,
    MachineHost          & machine,
    CpuManager           & cpuManager,
    HistorySettings      & settings,
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
    for (HistoryThumbnails * timeline : { &m_thumbnails, &m_barThumbnails })
    {
        timeline->SetRowLabeler     (&LabelBranch);
        timeline->SetOnBranchSwitch ([this] (uint32_t id) { SwitchBranch (id); });
        timeline->SetOnBranchHover  ([this] (uint32_t id) { m_hoveredBranch.store (id, std::memory_order_release); });
    }
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
//  CPU thread: the fork queued ahead of a change made behind live. The
//  history after the machine's position becomes a branch left behind, and
//  the machine is live there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellHistory::Diverge()
{
    HRESULT  hr = S_OK;



    CBRA (m_reverseHost != nullptr);

    hr = m_reverseHost->Fork();
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
//  ShellHistory::LoadSettings
//
//  CPU thread, when it starts. The settings apply to every machine built
//  after; the options change them through ApplyOptions.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::LoadSettings()
{
    m_isOn     = m_settings.isRecording;
    m_budgetMb = m_settings.budgetMb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::StartRecording
//
//  Begins the machine's history where it stands, once it is built and power
//  cycled, from the settings read at start or changed since; with recording
//  off, any history is dropped instead. The first call creates the host,
//  whose live callback posts the game-port input deferred while the machine
//  was behind live to the UI thread, which writes it and releases the keys
//  and mouse button let go of in the meantime. Step back out asks the
//  debugger's call record, while one is attached, whether any call in
//  history entered the code now running. A loaded state's history section,
//  when there is one, is started from instead, unless it cannot be loaded.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StartRecording()
{
    HRESULT              hr          = S_OK;
    ReverseController  * controller  = nullptr;
    bool                 isFromSaved = false;



    if (!m_isOn)
    {
        m_pendingHistory.clear();

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

    if (!m_pendingHistory.empty())
    {
        hr          = StartFromSaved();
        isFromSaved = SUCCEEDED (hr);
    }

    if (!isFromSaved)
    {
        hr = m_reverseHost->StartRecording (ReverseHost::MakeSettings (m_budgetMb, static_cast<int> (KeyframeSettings::kDefaultFrames)));
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"History could not start recording: 0x%08X\n", hr);
    }

    controller = m_reverseHost->IsRecording() ? &m_reverseHost->GetController() : nullptr;

    //  A history begun again: the timelines' pictures were of the old one,
    //  and the next are drawn on a machine built as this one is, as are the
    //  heat map's rebuilds and its look-ups of last accesses, and the call
    //  record's rebuilds.
    m_thumbnails.Clear();
    m_thumbnails.Attach (controller != nullptr ? &controller->GetKeyframes() : nullptr);
    m_renderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

    m_barThumbnails.Clear();
    m_barThumbnails.Attach (controller != nullptr ? &controller->GetKeyframes() : nullptr);
    m_barRenderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

    m_marks.Clear();

    m_heatReplayer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_heatFinder.SetMachine   (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_callReplayer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

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
    m_eventLogs.Start (controller);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::StopRecording
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopRecording()
{
    m_eventLogs.Stop();

    m_thumbnails.Attach (nullptr);
    m_barThumbnails.Attach (nullptr);

    if (m_reverseHost != nullptr)
    {
        m_reverseHost->StopRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ApplyOptions
//
//  CPU thread: the options' settings, taken at once. Turning recording off
//  makes the machine live first, announced as any return to live is, then
//  drops history; turning it on starts recording where the machine is. A
//  new budget applies to the history in memory: a smaller one drops the
//  oldest snapshots until it fits, and a larger one grows the store now.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ApplyOptions (
    bool  isRecording,
    int   budgetMb)
{
    HRESULT  hr          = S_OK;
    bool     wasOn       = m_isOn && IsRecording();
    size_t   budgetBytes = ReverseHost::MakeSettings (budgetMb, static_cast<int> (KeyframeSettings::kDefaultFrames)).keyframes.budgetBytes;



    m_isOn     = isRecording;
    m_budgetMb = budgetMb;

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
    else if (m_reverseHost != nullptr)
    {
        hr = m_reverseHost->GetController().ChangeBudget (budgetBytes);
        CHR (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SaveOptions
//
//  UI thread: saved, and posted to the CPU thread, which owns the history
//  and applies them at once.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::SaveOptions (
    bool  isRecording,
    int   budgetMb)
{
    bool  isCanceled = m_settings.isRecording && !isRecording && !ConfirmTurnOff();
    bool  isOn       = false;



    m_settings.isRecording = isRecording || isCanceled;
    m_settings.budgetMb    = budgetMb;
    m_host.SaveSettings();

    isOn = m_settings.isRecording;

    m_cpuManager.PostCommand (IDM_DEBUG_REVERSE_OPTIONS, [isOn, budgetMb] (IDebugCommandTarget & target) { target.ApplyReverseOptions (isOn, budgetMb); });

    return !isCanceled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ConfirmTurnOff
//
//  UI thread: whether the user agrees to delete the machine's history, with
//  Cancel the default.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::ConfirmTurnOff()
{
    std::vector<DxuiMessageBoxButton>  buttons = { { kpszTurnOffButton, IDOK }, { L"Cancel", IDCANCEL } };
    int                                choice  = IDCANCEL;



    if (m_confirmTurnOff)
    {
        return m_confirmTurnOff();
    }

    choice = DxuiMessageBox (m_host.GetMainWindow(), &m_host.GetChromeTheme(), kpszTurnOffText, kpszTurnOffTitle, MB_ICONWARNING, buttons, IDCANCEL);

    return choice == IDOK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RunMenuCommand
//
//  UI thread. A command not enabled now, as an accelerator can send, does
//  nothing. Rewind and Forward from the menu move ten seconds, and they and
//  Step back pause a running machine first.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RunMenuCommand (WORD id)
{
    bool  isSaved = false;



    if (!IsMenuCommandEnabled (id))
    {
        return;
    }

    switch (id)
    {
        case IDM_HISTORY_REWIND:
            PauseMachine();
            PostCommand (ReverseCommand::SkipBack);
            break;

        case IDM_HISTORY_FORWARD:
            PauseMachine();
            PostCommand (ReverseCommand::SkipForward);
            break;

        case IDM_HISTORY_STEP_BACK:
            PauseMachine();
            PostCommand (ReverseCommand::StepBack);
            break;

        case IDM_HISTORY_GO_LIVE:
            PostCommand (ReverseCommand::GoLive);
            break;

        case IDM_HISTORY_TOGGLE:
            isSaved = SaveOptions (!m_settings.isRecording, m_settings.budgetMb);
            IGNORE_RETURN_VALUE (isSaved, false);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsMenuCommandEnabled
//
//  UI thread, from what the CPU thread last published.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsMenuCommandEnabled (WORD id) const
{
    return IsMenuItemEnabled (id, m_settings.isRecording, m_barThumbnails.IsBehindLive(), m_isAtOldestForUi.load (std::memory_order_acquire));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsMenuItemEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsMenuItemEnabled (
    WORD  id,
    bool  isOn,
    bool  isBehindLive,
    bool  isAtOldest)
{
    bool  isEnabled = true;



    switch (id)
    {
        case IDM_HISTORY_REWIND:
        case IDM_HISTORY_STEP_BACK:
            isEnabled = isOn && !isAtOldest;
            break;

        case IDM_HISTORY_FORWARD:
        case IDM_HISTORY_GO_LIVE:
            isEnabled = isOn && isBehindLive;
            break;

        default:
            break;
    }

    return isEnabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RecordFrame
//
//  CPU thread, ahead of each frame: the machine run from the past made live
//  again. Recording continues at every speed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RecordFrame()
{
    HRESULT  hr = S_OK;



    if (m_reverseHost != nullptr)
    {
        hr = m_reverseHost->OnFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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
    m_replayControl.isStopRequested.store (false, std::memory_order_release);

    m_cpuManager.PostCommand (IDM_DEBUG_REVERSE, [command, argument] (IDebugCommandTarget & target) { target.RunReverseCommand (command, argument); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RunCommand
//
//  CPU thread. Moves the machine through its history and announces where it
//  landed the way a step's stop is announced, with the outcome in the stop's
//  history. The replay ran the speaker, whose clicks are not sound to play,
//  and the screen is drawn from the machine's state now.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RunCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    HRESULT         hr          = S_OK;
    EmuCpu        * cpu         = m_machine.GetCpu();
    AppleSpeaker  * speaker     = m_machine.GetRefs().speaker;
    bool            isPublished = false;
    ReverseResult   result;



    BAIL_OUT_IF (m_reverseHost == nullptr || cpu == nullptr, S_OK);
    BAIL_OUT_IF (!m_reverseHost->IsRecording(), S_OK);

    //  The debugger's status bar reads these while the replay runs, since no
    //  snapshot is built until it ends.
    m_replayControl.progress.store (ReplayControl::kNoProgress, std::memory_order_relaxed);
    m_replayStartedAt.store (GetTickCount64(), std::memory_order_relaxed);
    m_isReplaying.store (true, std::memory_order_release);

    NoteMoving();

    hr = m_reverseHost->Execute (command, argument, m_reverseStopTest, result);

    m_isReplaying.store (false, std::memory_order_release);

    //  Landed or not, the machine may have moved, and the debugger follows.
    NoteMoved (command);

    CHR (hr);

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    m_lastOutcome  = result.outcome;
    m_lastPosition = m_machine.GetRunner().GetPosition();

    if (m_isLandingReplyDue.exchange (false, std::memory_order_acq_rel))
    {
        m_context.AddConsoleLines ({ GetLandingText (result.outcome) });
    }

    NoteLanded (result);

    m_host.DrawFrame();

Error:
    //  Landed or not, the command has run, so a script waiting on it goes on.
    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->SetReversePending (false);
    }

    //  Landed or not, the seek is no longer on its way. The timeline is given
    //  the machine's position before the landing, so a line left where it
    //  was dropped moves straight to where the seek landed.
    if (command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle)
    {
        m_seekCycleLanded.store (argument, std::memory_order_release);

        isPublished = TryPublishPlayhead();
        IGNORE_RETURN_VALUE (isPublished, false);

        m_thumbnails.GetPlayhead().NoteSeekLanded    (argument);
        m_barThumbnails.GetPlayhead().NoteSeekLanded (argument);
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"History command %d failed: 0x%08X\n", static_cast<int> (command), hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::NoteMoving
//
//  The machine's position before a reverse command, so the call record
//  starts again only if the command moves it or replays anything on it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::NoteMoving()
{
    if (m_context.GetDebugController() != nullptr)
    {
        m_context.GetDebugController()->GetCallHistory().OnMoving();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::NoteMoved
//
//  Landed or not, the machine may have moved; the heat map follows it,
//  rebuilding its heat once a drag of the timeline is let go or a rewind
//  ends, and so does the call record.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::NoteMoved (ReverseCommand command)
{
    bool  isScrub = command == ReverseCommand::ScrubCycle || command == ReverseCommand::RewindFrame;



    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->GetTarget().NoteHistoryMoved (isScrub);
    }

    if (m_context.GetDebugController() != nullptr)
    {
        m_context.GetDebugController()->GetCallHistory().OnMoved (isScrub);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::NoteLanded
//
//  Reported the way a step's stop is, with the outcome in the stop's
//  history, and the views built again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::NoteLanded (const ReverseResult & result)
{
    EmuCpu     * cpu = m_machine.GetCpu();
    StopEvent    stop;



    if (cpu == nullptr)
    {
        return;
    }

    stop.reason    = StopReason::Step;
    stop.pc        = cpu->GetPC();
    stop.registers = cpu->GetCpu6502()->GetRegisters();
    stop.history   = result.outcome;

    if (result.outcome == ReverseOutcome::NoCaller)
    {
        m_context.AddConsoleLines ({ s_kNoCallerText });
    }

    if (m_context.GetDebugSession() != nullptr)
    {
        m_context.GetDebugSession()->OnStopped (stop);
    }

    m_viewModel.MarkDirty();
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
//  ShellHistory::StartRewind
//
//  UI thread, on the rewind key's press. A paused machine is run, so the
//  CPU thread's frames go on and step it back, and the debugger is sent the
//  resume as the Pause command sends it. A key held down repeats, and only
//  the first press counts.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StartRewind()
{
    if (!m_settings.isRecording || m_isRewindKeyDown.exchange (true, std::memory_order_acq_rel))
    {
        return;
    }

    if (m_cpuManager.IsPaused())
    {
        m_cpuManager.SetPaused (false);
        m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, [] (IDebugCommandTarget & target) { target.NotifyDebugPauseChanged (false); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SwitchBranch
//
//  UI thread, a click on a fork mark's row: the machine pauses, and lands
//  at the branch's live end, paused there.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SwitchBranch (uint32_t branchId)
{
    m_hoveredBranch.store (0, std::memory_order_release);

    PauseMachine();
    PostCommand (ReverseCommand::SwitchBranch, branchId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetHoveredBranchPicture
//
//  UI thread: the live end of the branch whose row is under the pointer,
//  from whichever timeline drew it, or null with none.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image ShellHistory::GetHoveredBranchPicture()
{
    uint32_t                  id      = m_hoveredBranch.load (std::memory_order_acquire);
    HistoryThumbnails::Image  picture;



    if (id == 0)
    {
        return nullptr;
    }

    picture = m_barThumbnails.GetBranchPicture (id);

    return (picture != nullptr) ? picture : m_thumbnails.GetBranchPicture (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::LabelBranch
//
//  A fork row's lines: how long the branch ran past the fork, and when the
//  user left it, or for the active branch, that the machine is on it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::LabelBranch (
    const KeyframeBranch  & branch,
    std::wstring          & outRanFor,
    std::wstring          & outLeftAt)
{
    uint64_t  ran = (branch.liveEndCycle > branch.forkCycle) ? branch.liveEndCycle - branch.forkCycle : 0;



    outRanFor = L"Ran " + DebuggerStatusText::FormatTime (ran) + L" past the fork";
    outLeftAt = (branch.id == HistoryThumbnails::kActiveBranchId)
                    ? std::wstring (L"You are on this branch")
                    : L"Left at " + DebuggerStatusText::FormatWallClock (branch.leftAtWallTime, LOCALE_NAME_USER_DEFAULT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::PauseMachine
//
//  The wait is bounded: the CPU thread may first be busy with a long
//  command, and the UI waits a moment for it rather than hanging.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PauseMachine()
{
    constexpr std::chrono::milliseconds  kParkWait { 100 };
    bool                                 isParked  = false;



    if (m_cpuManager.IsPaused())
    {
        return;
    }

    m_cpuManager.SetPaused (true);

    isParked = m_cpuManager.TryWaitUntilParked (kParkWait);
    IGNORE_RETURN_VALUE (isParked, false);

    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, [] (IDebugCommandTarget & target) { target.NotifyDebugPauseChanged (true); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::StopRewind
//
//  UI thread, on the rewind key's release. The CPU thread's next frame
//  pauses the machine where the last one left it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopRewind()
{
    m_isRewindKeyDown.store (false, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::RunRewindFrame
//
//  CPU thread, at the start of each frame. While the rewind key is down, the
//  machine steps back one frame's cycles and the screen is drawn from where
//  it landed; the frame runs nothing forward, so no sound is made, and the
//  replay's speaker clicks are dropped. The frames are paced at the video
//  rate at Maximum speed too. The first frame after the release, or the
//  frame that lands on the oldest point, pauses the machine there. False when
//  no rewind is under way, so the frame runs as usual.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::RunRewindFrame()
{
    HRESULT          hr      = S_OK;
    bool             isDown  = m_isRewindKeyDown.load (std::memory_order_acquire);
    AppleSpeaker   * speaker = m_machine.GetRefs().speaker;
    ReverseResult    result;



    if (!m_isRewinding)
    {
        if (!isDown)
        {
            return false;
        }

        if (!IsRecording() || m_machine.GetCpu() == nullptr)
        {
            m_isRewindKeyDown.store (false, std::memory_order_release);
            return false;
        }

        m_isRewinding = true;
        m_cpuManager.SetPaceForced (true);
    }

    if (!isDown)
    {
        FinishRewind();
        return true;
    }

    NoteMoving();

    hr = m_reverseHost->Execute (ReverseCommand::RewindFrame, 0, nullptr, result);

    NoteMoved (ReverseCommand::RewindFrame);

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    m_lastOutcome  = result.outcome;
    m_lastPosition = m_machine.GetRunner().GetPosition();

    m_host.DrawFrame();

    if (FAILED (hr) || result.outcome == ReverseOutcome::AtHistoryStart)
    {
        m_isRewindKeyDown.store (false, std::memory_order_release);
        FinishRewind();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::FinishRewind
//
//  CPU thread: the machine pauses on the frame on screen. The debugger is
//  sent the pause, and the landing as a step's stop.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::FinishRewind()
{
    EmuCpu         * cpu = m_machine.GetCpu();
    ReverseResult    result;



    m_isRewinding = false;

    m_cpuManager.SetPaceForced (false);
    m_cpuManager.SetPaused     (true);
    m_context.NotifyDebugPauseChanged (true);

    result.outcome  = m_lastOutcome.value_or (ReverseOutcome::Moved);
    result.position = m_machine.GetRunner().GetPosition();
    result.cycle    = (cpu != nullptr) ? cpu->GetHistoryCycles() : 0;

    //  Each frame moved as a drag of the timeline does, and the end of the
    //  rewind is the drag's release, after which the heat map is rebuilt.
    NoteMoving();
    NoteMoved  (ReverseCommand::Seek);
    NoteLanded (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::UpdateCaption
//
//  CPU thread. Behind live the caption note is the machine's position by
//  the host's clock to the second, so it ticks once a second while the
//  replay runs and moves with every step; live it is empty. The caption is
//  composed again only when the note changes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::UpdateCaption()
{
    EmuCpu        * cpu       = m_machine.GetCpu();
    bool            isBehind  = cpu != nullptr && IsBehindLive();
    uint64_t        cycle     = 0;
    std::wstring    caption;
    std::wstring    indicator;
    std::wstring    time;
    uint64_t        wallTime  = 0;
    bool            isChanged = false;



    if (isBehind)
    {
        cycle    = cpu->GetHistoryCycles();
        wallTime = m_reverseHost->GetController().GetTimeline().GetWallTimeAt (cycle);
        caption  = DebuggerStatusText::GetReplayCaption (wallTime, cycle, LOCALE_NAME_USER_DEFAULT);
        time     = DebuggerStatusText::FormatWallClock (wallTime, LOCALE_NAME_USER_DEFAULT);

        if (time.empty())
        {
            time = DebuggerStatusText::GetPowerText (cycle, LOCALE_NAME_USER_DEFAULT);
        }

        indicator = L"Replaying  " + time;
    }

    {
        std::lock_guard<std::mutex>  held (m_replayCaptionMutex);

        isChanged = caption != m_replayCaption;

        if (isChanged)
        {
            m_replayCaption = std::move (caption);
        }

        m_replayIndicatorText = std::move (indicator);
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
//  Any thread. The note UpdateCaption sets, for the caption.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellHistory::GetCaption()
{
    std::lock_guard<std::mutex>  held (m_replayCaptionMutex);



    return m_replayCaption;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetReplayIndicatorText
//
//  Any thread. "Replaying" and the host's time of the machine's position,
//  or the time since power-on when the host's time is unknown; empty live.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellHistory::GetReplayIndicatorText()
{
    std::lock_guard<std::mutex>  held (m_replayCaptionMutex);



    return m_replayIndicatorText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::SeekCycle
//
//  UI thread: the timeline was clicked, or its playhead dragged, at a cycle,
//  for a machine it has seen stopped; isInterim while the drag goes on. The
//  CPU thread drops it when no history is recorded. The cycle and when it
//  was requested are saved, so the timeline can tell whether the seek has
//  landed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SeekCycle (
    uint64_t  cycle,
    bool      isInterim)
{
    m_seekCyclePosted.store   (cycle,            std::memory_order_release);
    m_seekCyclePostedAt.store (GetTickCount64(), std::memory_order_release);

    PostCommand (isInterim ? ReverseCommand::ScrubCycle : ReverseCommand::SeekCycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::IsSeekBusy
//
//  UI thread: whether the CPU thread has yet to run the last seek to a cycle
//  the timeline requested. A seek the queue never ran, as one dropped with
//  the machine it was for, stops counting after kPatienceMs, so a drag of
//  the timeline never stalls for good.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsSeekBusy() const
{
    constexpr uint64_t  kPatienceMs = 500;



    uint64_t  posted   = m_seekCyclePosted.load   (std::memory_order_acquire);
    uint64_t  postedAt = m_seekCyclePostedAt.load (std::memory_order_acquire);
    uint64_t  landed   = m_seekCycleLanded.load   (std::memory_order_acquire);



    return posted != landed && GetTickCount64() - postedAt < kPatienceMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::ServiceThumbnails
//
//  CPU thread, between frames and while paused: the timelines' points are
//  laid out over the keyframes and the next wanted picture is posted to its
//  worker. Never during a reverse command, which runs from the command
//  queue. The timelines are given the machine's position every turn, so
//  their live or replay state, their line and their labels follow the
//  machine as it runs, as does the caption's replay note.
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
    PublishMarks();

    hr = m_thumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = m_barThumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::TryPublishPlayhead
//
//  CPU thread: passes the timelines the machine's position, whether that is
//  behind live, and where history begins. False while no history is
//  recorded.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::TryPublishPlayhead()
{
    EmuCpu                * cpu       = m_machine.GetCpu();
    uint64_t                cycle     = 0;
    uint64_t                position  = m_machine.GetRunner().GetPosition();
    bool                    isBehind  = false;
    const KeyframeArena   * keyframes = nullptr;
    ReverseController     * control   = nullptr;



    if (!IsRecording() || cpu == nullptr)
    {
        return false;
    }

    control   = &m_reverseHost->GetController();
    cycle     = cpu->GetHistoryCycles();
    isBehind  = m_reverseHost->IsBehindLive();
    keyframes = &control->GetKeyframes().GetArena();

    m_isAtOldestForUi.store (keyframes->GetCount() == 0 || position <= keyframes->GetInfo (0).position, std::memory_order_release);

    for (HistoryThumbnails * timeline : { &m_thumbnails, &m_barThumbnails })
    {
        timeline->GetPlayhead().SetPosition (position, isBehind);
        timeline->GetPlayhead().SetTime     (cycle, control->GetTimeline().GetWallTimeAt (cycle), isBehind ? control->GetSeeker().GetLiveEndCycle() : cycle);

        if (keyframes->GetCount() > 0)
        {
            timeline->GetPlayhead().SetBegin (keyframes->GetInfo (0).cycle, keyframes->GetInfo (0).wallTime);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::PublishMarks
//
//  CPU thread, after TryPublishPlayhead: the marks on the active path, read
//  from the journal records made since the last turn, go to both timelines
//  when any has come or gone or a keyframe has moved them, with the branches
//  left behind for the fork marks' rows. History ends at the live end behind
//  live and at the machine's position live.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PublishMarks()
{
    ReverseController         & controller  = m_reverseHost->GetController();
    EmuCpu                    * cpu         = m_machine.GetCpu();
    bool                        isBehind    = m_reverseHost->IsBehindLive();
    uint64_t                    endPosition = isBehind ? controller.GetSeeker().GetLiveEndPosition() : m_machine.GetRunner().GetPosition();
    uint64_t                    endCycle    = isBehind ? controller.GetSeeker().GetLiveEndCycle()    : cpu->GetHistoryCycles();
    std::vector<HistoryMark>    marks;
    std::vector<KeyframeBranch> branches;



    if (!m_marks.Update (controller.GetKeyframes(), m_machine.GetHostInput().GetJournal(), endPosition, endCycle, marks))
    {
        return;
    }

    controller.GetKeyframes().GetArena().GetBranches (branches);

    m_thumbnails.SetMarks       (marks);
    m_barThumbnails.SetMarks    (std::move (marks));
    m_thumbnails.SetBranches    (branches);
    m_barThumbnails.SetBranches (std::move (branches));
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



    progress.isReplaying = m_isReplaying.load (std::memory_order_acquire);

    if (progress.isReplaying)
    {
        progress.elapsedMs = GetTickCount64() - m_replayStartedAt.load (std::memory_order_relaxed);
        progress.fraction  = m_replayControl.progress.load (std::memory_order_relaxed);
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory::GetStatus
//
//  The last reverse command's outcome stays until the machine moves from
//  where it landed, by running or by a step, and is dropped then.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus ShellHistory::GetStatus()
{
    HistoryStatus  status;
    bool           hasMoved = !m_cpuManager.IsPaused() || m_machine.GetRunner().GetPosition() != m_lastPosition;



    if (m_reverseHost != nullptr)
    {
        status = m_reverseHost->GetStatus();
    }

    if (hasMoved)
    {
        m_lastOutcome.reset();
    }

    status.outcome = m_lastOutcome;
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
