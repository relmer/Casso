#include "Pch.h"

#include "Shell/Components/ShellHistory.h"
#include "Config/HistorySettings.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/HeldHostInputs.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/IHistoryClient.h"
#include "Shell/MachineHost.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory
//
//  Both timelines' fork rows are labeled alike, a click on one switches to
//  its branch, and the row under the pointer is drawn over the screen.
//
////////////////////////////////////////////////////////////////////////////////

ShellHistory::ShellHistory (
    IDebuggerHost    & host,
    MachineHost      & machine,
    CpuManager       & cpuManager,
    HistorySettings  & settings) :
    m_host       (host),
    m_machine    (machine),
    m_cpuManager (cpuManager),
    m_settings   (settings)
{
    for (HistoryThumbnails * timeline : { &m_thumbnails, &m_barThumbnails })
    {
        timeline->SetRowLabeler     (&LabelBranch);
        timeline->SetOnBranchSwitch ([this] (uint32_t id) { SwitchBranch (id); });
        timeline->SetOnBranchHover  ([this] (uint32_t id) { m_hoveredBranch.store (id, memory_order_release); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellHistory
//
////////////////////////////////////////////////////////////////////////////////

ShellHistory::~ShellHistory() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSettings
//
//  CPU thread, when it starts. The settings apply to every machine built
//  after; the options change them through ApplyOptions.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::LoadSettings()
{
    m_isOn           = m_settings.isRecording;
    m_budgetMb       = m_settings.budgetMb;
    m_intervalFrames = m_settings.intervalFrames;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartRecording
//
//  Begins the machine's history where it stands, once it is built and power
//  cycled, from the settings read at start or changed since; with recording
//  off, any history is dropped instead. The first call creates the host,
//  whose live callback posts the game-port input deferred while the machine
//  was behind live to the UI thread, which writes it and releases the keys
//  and mouse button let go of in the meantime. Step back out consults the
//  client's call record, while one is attached, on whether any call in
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

        if (m_client != nullptr)
        {
            m_client->OnHistoryRestarted (nullptr);
        }

        return;
    }

    if (m_reverseHost == nullptr)
    {
        m_reverseHost = std::make_unique<ReverseHost> (m_machine);
        m_reverseHost->GetController().SetReplayControl (&m_replayControl);

        m_reverseHost->GetController().SetCallerProbe ([this] (uint64_t historyStartCycle)
        {
            return m_client != nullptr && m_client->HasNoCallSince (historyStartCycle);
        });

        m_reverseHost->GetController().SetCallerLinksProbe ([this] (std::vector<CallerLink> & outLinks)
        {
            outLinks.clear();

            if (m_client != nullptr)
            {
                m_client->GetCallerLinks (outLinks);
            }
        });

        m_reverseHost->SetLiveCallback ([this] ()
        {
            PostMessageW (m_host.GetMainWindow(), WM_APP_GAMEPORT_FLUSH, 0, 0);
        });
    }

    if (!m_pendingHistory.empty())
    {
        hr          = StartFromSaved();
        isFromSaved = SUCCEEDED (hr);
    }

    if (!isFromSaved)
    {
        hr = m_reverseHost->StartRecording (ReverseHost::MakeSettings (m_budgetMb, m_intervalFrames));
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"History could not start recording: 0x%08X\n", hr);
    }

    controller = m_reverseHost->IsRecording() ? &m_reverseHost->GetController() : nullptr;

    //  A history begun again: the timeline's pictures were of the old one,
    //  and the next are drawn on a machine built as this one is.
    m_thumbnails.Clear();
    m_thumbnails.Attach (controller != nullptr ? &controller->GetKeyframes() : nullptr);
    m_renderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

    m_barThumbnails.Clear();
    m_barThumbnails.Attach (controller != nullptr ? &controller->GetKeyframes() : nullptr);
    m_barRenderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());

    m_marks.Clear();

    if (m_client != nullptr)
    {
        m_client->OnHistoryRestarted (controller);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopRecording
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopRecording()
{
    if (m_client != nullptr)
    {
        m_client->OnHistoryStopping();
    }

    m_thumbnails.Attach (nullptr);
    m_barThumbnails.Attach (nullptr);

    if (m_reverseHost != nullptr)
    {
        m_reverseHost->StopRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyOptions
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
    size_t   budgetBytes = ReverseHost::MakeSettings (budgetMb, m_intervalFrames).keyframes.budgetBytes;



    m_isOn     = isRecording;
    m_budgetMb = budgetMb;

    if (!isRecording)
    {
        if (wasOn && m_reverseHost->IsBehindLive())
        {
            RunCommand (ReverseCommand::GoLive, 0);
        }

        StopRecording();

        if (m_client != nullptr)
        {
            m_client->OnHistoryRestarted (nullptr);
        }

        //  The caption is otherwise updated only while recording is on, so
        //  a replay's note would stay in the title.
        UpdateReplayCaption();
    }
    else if (!wasOn && !m_isNetworkFenced)
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
//  SaveOptions
//
//  UI thread: saved, and posted to the CPU thread, which owns the history
//  and applies the recording and budget at once. The interval is read when
//  history next starts, since a keyframe interval changed under recorded
//  history would mix two schedules.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::SaveOptions (
    bool  isRecording,
    int   budgetMb,
    int   intervalFrames)
{
    bool  isCanceled = m_settings.isRecording && !isRecording && !ConfirmTurnOff();



    m_settings.isRecording    = isRecording || isCanceled;
    m_settings.budgetMb       = budgetMb;
    m_settings.intervalFrames = std::clamp (intervalFrames, 1, HistorySettings::kMaxIntervalFrames);
    m_host.SaveSettings();

    m_host.PostCommand (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (m_settings.isRecording, budgetMb));

    return !isCanceled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfirmTurnOff
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
//  RunMenuCommand
//
//  UI thread. A command not enabled now, as an accelerator can send, does
//  nothing. Rewind from the menu steps back one frame, the stretch F2 steps
//  each frame it is down; it and Step back pause a running machine first.
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
            PostCommand (ReverseCommand::RewindFrame);
            break;

        case IDM_HISTORY_STEP_BACK:
            PauseMachine();
            PostCommand (ReverseCommand::StepBack);
            break;

        case IDM_HISTORY_GO_LIVE:
            PostCommand (ReverseCommand::GoLive);
            break;

        case IDM_HISTORY_TOGGLE:
            isSaved = SaveOptions (!m_settings.isRecording, m_settings.budgetMb, m_settings.intervalFrames);
            IGNORE_RETURN_VALUE (isSaved, false);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMenuCommandEnabled
//
//  UI thread, from what the CPU thread last published.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsMenuCommandEnabled (WORD id) const
{
    return IsMenuItemEnabled (id, m_settings.isRecording, m_barThumbnails.IsBehindLive(), m_isAtOldestForUi.load (memory_order_acquire));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMenuItemEnabled
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
//  RecordFrame
//
//  CPU thread, ahead of each frame: the network fence, and the machine run
//  from the past made live again. Recording continues at every speed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RecordFrame()
{
    HRESULT  hr = S_OK;



    FenceForNetwork();

    if (m_reverseHost != nullptr)
    {
        hr = m_reverseHost->OnFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FenceForNetwork
//
//  CPU thread, ahead of each frame and each reverse command. The Uthernet
//  II's open sockets and flows are host state that no keyframe has and no
//  replay can make again, so history is off while any is open: the history
//  so far is dropped, and recording starts again once the card is idle.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::FenceForNetwork()
{
    UthernetCard  * card     = m_machine.GetRefs().uthernet;
    bool            isActive = card != nullptr && card->IsActive();
    bool            isOn     = IsRecording();



    if (isActive && isOn)
    {
        StopRecording();

        if (m_client != nullptr)
        {
            m_client->OnHistoryRestarted (nullptr);
        }

        UpdateReplayCaption();
    }
    else if (!isActive && m_isNetworkFenced && m_isOn && !isOn)
    {
        StartRecording();
    }

    m_isNetworkFenced = isActive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
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

    m_host.PostCommand (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (command, argument));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCommand
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
    HRESULT             hr          = S_OK;
    EmuCpu            * cpu         = m_machine.GetCpu();
    AppleSpeaker      * speaker     = m_machine.GetRefs().speaker;
    IReverseStopTest  * stopTest    = nullptr;
    bool                isPublished = false;
    ReverseResult       result;



    BAIL_OUT_IF (m_reverseHost == nullptr || cpu == nullptr, S_OK);

    //  A socket opened since the last frame: the replay would run the
    //  instructions that opened it.
    FenceForNetwork();
    BAIL_OUT_IF (!m_reverseHost->IsRecording(), S_OK);

    //  The debugger's status bar reads these while the replay runs, since no
    //  snapshot is built until it ends.
    m_replayControl.progress.store (ReplayControl::kNoProgress, memory_order_relaxed);
    m_replayStartedAt.store (GetTickCount64(), memory_order_relaxed);
    m_isReplaying.store (true, memory_order_release);

    if (m_client != nullptr)
    {
        m_client->OnHistoryMoving();
        stopTest = m_client->GetReverseStopTest();
    }

    hr = m_reverseHost->Execute (command, argument, stopTest, result);

    m_isReplaying.store (false, memory_order_release);

    //  Landed or not, the machine may have moved, and the debugger follows.
    if (m_client != nullptr)
    {
        m_client->OnHistoryMoved (command);
    }

    CHR (hr);

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    m_lastOutcome  = result.outcome;
    m_lastPosition = m_machine.GetPosition();

    if (m_client != nullptr)
    {
        m_client->OnHistoryLanded (result);
    }

    m_host.DrawFrame();

Error:
    //  Landed or not, the seek is no longer on its way. The timeline is given
    //  the machine's position before the landing, so a line left where it
    //  was dropped moves straight to where the seek landed.
    if (command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle)
    {
        m_seekCycleLanded.store (argument, memory_order_release);

        isPublished = TryPublishPlayhead();
        IGNORE_RETURN_VALUE (isPublished, false);

        m_thumbnails.NoteSeekLanded    (argument);
        m_barThumbnails.NoteSeekLanded (argument);
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"History command %d failed: 0x%08X\n", static_cast<int> (command), hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartRewind
//
//  UI thread, on the rewind key's press. A paused machine is run, so the
//  CPU thread's frames go on and step it back, and the debugger is sent the
//  resume as the Pause command sends it. A key held down repeats, and only
//  the first press counts.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StartRewind()
{
    if (!m_settings.isRecording || m_isRewindKeyDown.exchange (true, memory_order_acq_rel))
    {
        return;
    }

    if (m_cpuManager.IsPaused())
    {
        m_cpuManager.SetPaused (false);
        m_host.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "0");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchBranch
//
//  UI thread, a click on a fork mark's row: the machine pauses, and lands
//  at the branch's live end, paused there.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::SwitchBranch (uint32_t branchId)
{
    m_hoveredBranch.store (0, memory_order_release);

    PauseMachine();
    PostCommand (ReverseCommand::SwitchBranch, branchId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHoveredBranchPicture
//
//  UI thread: the live end of the branch whose row is under the pointer,
//  from whichever timeline drew it, or null with none.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image ShellHistory::GetHoveredBranchPicture()
{
    uint32_t                  id      = m_hoveredBranch.load (memory_order_acquire);
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
//  LabelBranch
//
//  A fork row's lines: how long the branch ran past the fork, and when the
//  user left it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::LabelBranch (
    const KeyframeBranch  & branch,
    std::wstring          & outRanFor,
    std::wstring          & outLeftAt)
{
    uint64_t  ran = (branch.liveEndCycle > branch.forkCycle) ? branch.liveEndCycle - branch.forkCycle : 0;



    outRanFor = L"Ran " + DebuggerStatusText::FormatTime (ran) + L" past the fork";
    outLeftAt = L"Left at " + DebuggerStatusText::FormatWallClock (branch.leftAtWallTime, LOCALE_NAME_USER_DEFAULT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseMachine
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

    m_host.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "1");
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopRewind
//
//  UI thread, on the rewind key's release. The CPU thread's next frame
//  pauses the machine where the last one left it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopRewind()
{
    m_isRewindKeyDown.store (false, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunRewindFrame
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
    bool             isDown  = m_isRewindKeyDown.load (memory_order_acquire);
    AppleSpeaker   * speaker = m_machine.GetRefs().speaker;
    ReverseResult    result;



    if (!m_isRewinding)
    {
        if (!isDown)
        {
            return false;
        }

        FenceForNetwork();

        if (!IsRecording() || m_machine.GetCpu() == nullptr)
        {
            m_isRewindKeyDown.store (false, memory_order_release);
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

    if (m_client != nullptr)
    {
        m_client->OnHistoryMoving();
    }

    hr = m_reverseHost->Execute (ReverseCommand::RewindFrame, 0, nullptr, result);

    if (m_client != nullptr)
    {
        m_client->OnHistoryMoved (ReverseCommand::RewindFrame);
    }

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    m_lastOutcome  = result.outcome;
    m_lastPosition = m_machine.GetPosition();

    m_host.DrawFrame();

    if (FAILED (hr) || result.outcome == ReverseOutcome::AtHistoryStart)
    {
        m_isRewindKeyDown.store (false, memory_order_release);
        FinishRewind();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishRewind
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
    m_host.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "1");

    result.outcome  = m_lastOutcome.value_or (ReverseOutcome::Moved);
    result.position = m_machine.GetPosition();
    result.cycle    = (cpu != nullptr) ? cpu->GetHistoryCycles() : 0;

    //  Each frame moved as a drag of the timeline does, and the end of the
    //  rewind is the drag's release, after which the heat map is rebuilt.
    if (m_client != nullptr)
    {
        m_client->OnHistoryMoving();
        m_client->OnHistoryMoved  (ReverseCommand::Seek);
        m_client->OnHistoryLanded (result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AllowCommand
//
//  The command queue's gate, on the posting thread. A command that changes
//  the machine, posted from the UI thread while the machine is behind live,
//  has a fork queued ahead of it, with no question. Every command goes ahead.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::AllowCommand (
    WORD                 id,
    const std::string  & payload)
{
    bool               isUiThread = m_host.GetMainWindow() != nullptr && GetWindowThreadProcessId (m_host.GetMainWindow(), nullptr) == GetCurrentThreadId();
    DivergenceVerdict  verdict    = DivergenceVerdict::Proceed;



    UNREFERENCED_PARAMETER (payload);

    if (!isUiThread || id == IDM_DEBUG_DIVERGE)
    {
        return true;
    }

    verdict = DivergenceGate::Judge (IsBehindLiveForUi(), DivergenceGate::IsStateChangingCommand (id));

    // Queued ahead of the command, so history forks before it runs.
    if (verdict == DivergenceVerdict::Fork)
    {
        m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Diverge
//
//  CPU thread: the fork queued ahead of a change made behind live.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::Diverge()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_reverseHost == nullptr, S_OK);

    hr = m_reverseHost->Fork();
    CHR (hr);

    UpdateReplayCaption();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardEdit
//
//  CPU thread, the debugger session's history guard. Behind live, history
//  forks first, with no question, and the edit is then made live.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::GuardEdit (const DebugCommand & command)
{
    UNREFERENCED_PARAMETER (command);

    if (IsBehindLive())
    {
        Diverge();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBehindLiveForUi
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsBehindLiveForUi()
{
    return m_machine.GetHostInputGate().IsHeld();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HoldInputBehindLive
//
//  UI thread. The guest mouse's button or the //c's 80/40 switch, refused by
//  the host input gate. The button is deferred until the guest reads it;
//  the switch prompts at once.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::HoldInputBehindLive (HeldInput input)
{
    DivergenceVerdict  verdict = m_divergenceGate.JudgeInput (true, input == HeldInput::MousePress);



    if (verdict == DivergenceVerdict::Fork || verdict == DivergenceVerdict::Hold)
    {
        m_divergenceGate.HoldInput (input);
    }

    PublishHeldInput();

    if (verdict == DivergenceVerdict::Hold)
    {
        RunForHeldInput();
    }

    if (verdict == DivergenceVerdict::Fork)
    {
        PostMessageW (m_host.GetMainWindow(), WM_APP_FORK_FOR_INPUT, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunForHeldInput
//
//  UI thread, for input deferred while the machine is paused behind live:
//  the replay runs, so the replayed guest reads the input where it would,
//  and history goes on from there. Not while the rewind key is down.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::RunForHeldInput()
{
    if (!m_cpuManager.IsPaused() || m_isRewindKeyDown.load (memory_order_acquire))
    {
        return;
    }

    m_cpuManager.SetPaused (false);
    m_host.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "0");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishHeldInput
//
//  UI thread, after any change to the deferred input: the lines the CPU
//  thread watches the guest's reads of, empty unless input is deferred with
//  no question asked.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PublishHeldInput()
{
    m_heldInputWatch.Publish (m_divergenceGate.GetHeldLines());
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopForHeldInputRead
//
//  CPU thread, after a slice that ended on a read the deferred input would
//  change. The machine stops, a debugger run ending as a pause does, and is
//  put back where the reading instruction began, so a yes makes the input
//  live before that instruction runs. Whether it was running decides
//  whether it runs on after the answer; a debugger step stays stopped.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::StopForHeldInputRead()
{
    uint64_t  position  = m_heldInputWatch.GetHitPosition();
    bool      isStep    = m_client != nullptr && m_client->IsDebugStepRunning();
    bool      isRunning = !m_cpuManager.IsPaused() && !isStep;



    m_heldInputWatch.ClearHit();

    m_cpuManager.SetPaused (true);

    if (m_client != nullptr)
    {
        m_client->OnPauseChanged (true);
    }

    RunCommand (ReverseCommand::Seek, position);

    m_isResumeOwedAfterHeldRead.store (isRunning, memory_order_release);

    m_host.UpdateWindowTitle();
    PostMessageW (m_host.GetMainWindow(), WM_APP_HELD_INPUT_READ, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeldInputRead
//
//  UI thread: the replay stopped where the guest reads deferred input. With
//  input still deferred, that prompts; with nothing deferred any more --
//  let go of while the message was on its way -- the replay runs on.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::OnHeldInputRead()
{
    bool  isAsking = false;



    // A question already open answers for this read too, and its answer runs
    // the replay on.
    if (m_divergenceGate.IsForking())
    {
        return;
    }

    isAsking = m_divergenceGate.OnHeldInputRead();

    PublishHeldInput();

    if (isAsking)
    {
        ForkForHeldInput();
    }
    else
    {
        ResumeAfterHeldInput();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResumeAfterHeldInput
//
//  UI thread: a replay stopped at a deferred read runs on, as the pause
//  button would run it, once the question is answered and, after a yes, the
//  input has gone in.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ResumeAfterHeldInput()
{
    bool  isOwed = m_isResumeOwedAfterHeldRead.exchange (false, memory_order_acq_rel);



    if (!isOwed)
    {
        return;
    }

    m_cpuManager.SetPaused (false);
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "0");

    m_host.UpdateWindowTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForkForHeldInput
//
//  UI thread: history forks for the input deferred, with no question. The
//  fork is queued; the machine going live then writes the game port as the
//  host has it now and posts back what was deferred, and a stopped replay
//  runs on from there.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ForkForHeldInput()
{
    if (!m_divergenceGate.IsForking())
    {
        return;
    }

    m_divergenceGate.Answer (true);

    PublishHeldInput();

    m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);

    // Live already, the replay having caught up while the fork was on its
    // way: no return to live posts the input back, so post it now.
    if (!IsBehindLiveForUi())
    {
        PostMessageW (m_host.GetMainWindow(), WM_APP_GAMEPORT_FLUSH, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyHeldInputs
//
//  UI thread, the machine live: the inputs a yes accepted, in order.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ApplyHeldInputs (const std::vector<HeldInput> & inputs)
{
    for (HeldInput input : inputs)
    {
        switch (input)
        {
            case HeldInput::MousePress:
                PressGuestMouseHeldBehindLive();
                break;

            case HeldInput::EightyColumnToggle:
                ToggleHeldEightyColumnSwitch();
                break;

            default:
                break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyHeldMouseTarget
//
//  UI thread, the machine live: the pointer's place over the picture,
//  deferred behind live, becomes the guest mouse's target, as a move there
//  would.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ApplyHeldMouseTarget (uint32_t target)
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;



    BAIL_OUT_IF (!lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    BAIL_OUT_IF (!isGateOpen, S_OK);

    m_machine.GetMouse()->SetHostTargetFraction (static_cast<uint16_t> (target >> 16), static_cast<uint16_t> (target & 0xFFFF));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PressGuestMouseHeldBehindLive
//
//  The press goes down as a real one would. The question may have taken the
//  release, which went to the message box, so a button no longer down is
//  let go of after kClickHoldMs, long enough for the guest to see a click.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PressGuestMouseHeldBehindLive()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    BAIL_OUT_IF (!lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    BAIL_OUT_IF (!isGateOpen, S_OK);

    m_machine.GetMouse()->SetButton (true);
    m_host.GetHeldHostInputs().OnPress (HeldHostInputs::kMouseButton, true);

    if (!isHostDown)
    {
        hr = m_host.SetUiTimer (kClickReleaseTimerId, kClickHoldMs);
        CHRA (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGuestMouseAfterClick
//
//  The end of the click a deferred press made, unless the user has the
//  button down again by now, when its own release lets go.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ReleaseGuestMouseAfterClick()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    hr = m_host.KillUiTimer (kClickReleaseTimerId);
    IGNORE_RETURN_VALUE (hr, S_OK);

    BAIL_OUT_IF (isHostDown || !lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    m_host.GetHeldHostInputs().OnRelease (HeldHostInputs::kMouseButton, isGateOpen);

    if (isGateOpen)
    {
        m_machine.GetMouse()->SetButton (false);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleHeldEightyColumnSwitch
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ToggleHeldEightyColumnSwitch()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);



    if (lifetime.owns_lock())
    {
        ToggleEightyColumnSwitch (m_machine.GetRefs().iieKeyboard);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleEightyColumnSwitch
//
//  The //c's 80/40 switch, flipped and saved with the preferences. Behind
//  live it prompts first, and on a yes flips once the machine is live. The
//  caller has the lifetime lock.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::ToggleEightyColumnSwitch (Apple2eKeyboard * iieKbd)
{
    HRESULT                              hr         = S_OK;
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 newIn      = false;



    BAIL_OUT_IF (iieKbd == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);

    if (!isGateOpen)
    {
        HoldInputBehindLive (HeldInput::EightyColumnToggle);
        BAIL_OUT_IF (true, S_OK);
    }

    newIn = !iieKbd->IsEightyColumnSwitchIn();
    iieKbd->SetEightyColumnSwitchIn (newIn);
    m_host.PersistSwitchState ("eightyColumnSwitch", newIn);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeJoyportSyncOwed
//
//  UI thread, once live: whether a players' change made behind live is
//  owed to the Joyport, cleared as it is read.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::TakeJoyportSyncOwed()
{
    bool  isOwed = m_isJoyportSyncOwed;



    m_isJoyportSyncOwed = false;

    return isOwed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateReplayCaption
//
//  CPU thread. Behind live the caption note is the machine's position by
//  the host's clock to the second, so it ticks once a second while the
//  replay runs and moves with every step; live it is empty. The caption is
//  composed again only when the note changes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::UpdateReplayCaption()
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
        wallTime = m_reverseHost->GetController().GetWallTimeAt (cycle);
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
//  GetReplayCaption
//
//  Any thread. The note UpdateReplayCaption maintains, for the caption.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellHistory::GetReplayCaption()
{
    std::lock_guard<std::mutex>  held (m_replayCaptionMutex);



    return m_replayCaption;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReplayIndicatorText
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
//  SeekCycle
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
    m_seekCyclePosted.store   (cycle,            memory_order_release);
    m_seekCyclePostedAt.store (GetTickCount64(), memory_order_release);

    PostCommand (isInterim ? ReverseCommand::ScrubCycle : ReverseCommand::SeekCycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSeekBusy
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



    uint64_t  posted   = m_seekCyclePosted.load   (memory_order_acquire);
    uint64_t  postedAt = m_seekCyclePostedAt.load (memory_order_acquire);
    uint64_t  landed   = m_seekCycleLanded.load   (memory_order_acquire);



    return posted != landed && GetTickCount64() - postedAt < kPatienceMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceThumbnails
//
//  CPU thread, between frames and while paused: the timeline's points are
//  laid out over the keyframes and the next wanted picture is posted to its
//  worker. Never during a reverse command, which runs from the command
//  queue. The timeline is given the machine's position every turn, so its
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

    UpdateReplayCaption();
    PublishMarks();

    hr = m_thumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = m_barThumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPublishPlayhead
//
//  CPU thread: passes the timeline the machine's position, whether that is
//  behind live, and where history begins. False while no history is
//  recorded.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::TryPublishPlayhead()
{
    EmuCpu                * cpu       = m_machine.GetCpu();
    uint64_t                cycle     = 0;
    bool                    isBehind  = false;
    const KeyframeStore   * keyframes = nullptr;



    if (!IsRecording() || cpu == nullptr)
    {
        return false;
    }

    cycle     = cpu->GetHistoryCycles();
    isBehind  = m_reverseHost->IsBehindLive();
    keyframes = &m_reverseHost->GetController().GetKeyframes();

    m_isAtOldestForUi.store (keyframes->GetCount() == 0 || m_machine.GetPosition() <= keyframes->GetInfo (0).position, memory_order_release);

    for (HistoryThumbnails * timeline : { &m_thumbnails, &m_barThumbnails })
    {
        timeline->SetPlayhead     (m_machine.GetPosition(), isBehind);
        timeline->SetPlayheadTime (cycle, m_reverseHost->GetController().GetWallTimeAt (cycle), isBehind ? m_reverseHost->GetController().GetLiveEndCycle() : cycle);

        if (keyframes->GetCount() > 0)
        {
            timeline->SetBegin (keyframes->GetInfo (0).cycle, keyframes->GetInfo (0).wallTime);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishMarks
//
//  CPU thread, after TryPublishPlayhead: the marks on the active path, read
//  from the journal records made since the last turn, go to both timelines
//  when any has come or gone or a keyframe has moved them, with the branches
//  left behind for the fork marks' rows. History ends at
//  the live end behind live and at the machine's position live.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistory::PublishMarks()
{
    ReverseController         & controller  = m_reverseHost->GetController();
    EmuCpu                    * cpu         = m_machine.GetCpu();
    bool                        isBehind    = m_reverseHost->IsBehindLive();
    uint64_t                    endPosition = isBehind ? controller.GetLiveEndPosition() : m_machine.GetPosition();
    uint64_t                    endCycle    = isBehind ? controller.GetLiveEndCycle()    : cpu->GetHistoryCycles();
    std::vector<HistoryMark>    marks;
    std::vector<KeyframeBranch> branches;



    if (!m_marks.Update (controller.GetKeyframes(), m_machine.GetInputJournal(), endPosition, endCycle, marks))
    {
        return;
    }

    controller.GetKeyframes().GetBranches (branches);

    m_thumbnails.SetMarks       (marks);
    m_barThumbnails.SetMarks    (std::move (marks));
    m_thumbnails.SetBranches    (branches);
    m_barThumbnails.SetBranches (std::move (branches));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReplayProgress
//
//  UI thread: how long the running reverse command has run and how much of
//  history it has searched, read from what the CPU thread sets as it goes.
//
////////////////////////////////////////////////////////////////////////////////

ReplayProgress ShellHistory::GetReplayProgress()
{
    ReplayProgress  progress;



    progress.isReplaying = m_isReplaying.load (memory_order_acquire);

    if (progress.isReplaying)
    {
        progress.elapsedMs = GetTickCount64() - m_replayStartedAt.load (memory_order_relaxed);
        progress.fraction  = m_replayControl.progress.load (memory_order_relaxed);
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStatus
//
//  The last reverse command's outcome stays until the machine moves from
//  where it landed, by running or by a step, and is dropped then.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus ShellHistory::GetStatus()
{
    HistoryStatus  status;
    bool           hasMoved = !m_cpuManager.IsPaused() || m_machine.GetPosition() != m_lastPosition;



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
//  IsRecording
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsRecording() const
{
    return m_reverseHost != nullptr && m_reverseHost->IsRecording();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBehindLive
//
////////////////////////////////////////////////////////////////////////////////

bool ShellHistory::IsBehindLive() const
{
    return m_reverseHost != nullptr && m_reverseHost->IsBehindLive();
}





