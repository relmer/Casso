#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Debugger/DebugCommandPayload.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "resource.h"





//  The console's line for a step back out with no caller in history.
static constexpr const char  * s_kNoCallerText = "No caller to step back out to.";





////////////////////////////////////////////////////////////////////////////////
//
//  StartReverseRecording
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

void EmulatorShell::StartReverseRecording()
{
    HRESULT  hr = S_OK;



    if (!m_isReverseOn)
    {
        StopReverseRecording();
        return;
    }

    if (m_reverseHost == nullptr)
    {
        m_reverseHost = std::make_unique<ReverseHost> (m_machine);
        m_reverseHost->GetController().SetReplayControl (&m_replayControl);

        m_reverseHost->GetController().SetCallerProbe ([this] (uint64_t historyStartCycle)
        {
            return m_debugSession != nullptr && m_debugSession->HasNoCallSince (historyStartCycle);
        });

        m_reverseHost->GetController().SetCallerLinksProbe ([this] (std::vector<CallerLink> & outLinks)
        {
            outLinks.clear();

            if (m_debugSession != nullptr)
            {
                m_debugSession->GetCallerLinks (outLinks);
            }
        });

        m_reverseHost->SetLiveCallback ([this] ()
        {
            PostMessageW (m_hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
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
    m_historyRenderer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_heatReplayer.SetMachine    (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_heatFinder.SetMachine      (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_callReplayer.SetMachine    (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopReverseRecording
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopReverseRecording()
{
    if (m_reverseHost != nullptr)
    {
        m_reverseHost->StopRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyReverseOptions
//
//  CPU thread: Tools > Options' settings, taken at once. Turning recording
//  off makes the machine live first, announced as any return to live is,
//  then drops history; turning it on starts recording where the machine
//  stands. A new budget applies to the history already held: a smaller one
//  drops the oldest snapshots until it fits, and a larger one grows the
//  store now.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyReverseOptions (
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
            RunReverseCommand (ReverseCommand::GoLive, 0);
        }

        StopReverseRecording();
    }
    else if (!wasOn)
    {
        StartReverseRecording();
    }
    else
    {
        hr = m_reverseHost->GetController().GetKeyframes().ChangeBudget (budgetBytes);
        CHR (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostReverseCommand
//
//  UI thread: queues the command for the CPU thread, which owns the machine.
//  A stop asked for before now belongs to an earlier command, and is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostReverseCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    m_replayControl.isStopRequested.store (false, memory_order_release);

    PostCommand (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (command, argument));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunReverseCommand
//
//  CPU thread. Moves the machine through its history and announces where it
//  landed the way a step's stop is announced, with the outcome in the stop's
//  history. The replay ran the speaker, whose clicks are not sound to play,
//  and the screen is drawn from the state the machine now holds.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunReverseCommand (
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

    //  The debugger's status bar reads these while the replay runs, since no
    //  snapshot is built until it ends.
    m_replayControl.progress.store (ReplayControl::kNoProgress, memory_order_relaxed);
    m_replayStartedAt.store (GetTickCount64(), memory_order_relaxed);
    m_isReplayingHistory.store (true, memory_order_release);

    //  Where the machine stands before the command, so the call record
    //  starts again only if the command moves it or replays anything on it.
    if (m_debugger != nullptr)
    {
        m_debugger->GetCallHistory().OnMoving();
    }

    hr = m_reverseHost->Execute (command, argument, m_reverseStopTest, result);

    m_isReplayingHistory.store (false, memory_order_release);

    //  Landed or not, the machine may have moved; the heat map follows it,
    //  rebuilding its heat once a drag of the timeline is let go, and so does
    //  the call record.
    if (m_debugSession != nullptr)
    {
        m_debugSession->GetTarget().NoteHistoryMoved (command == ReverseCommand::ScrubCycle);
    }

    if (m_debugger != nullptr)
    {
        m_debugger->GetCallHistory().OnMoved (command == ReverseCommand::ScrubCycle);
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
    m_lastReversePosition = m_machine.GetPosition();

    if (result.outcome == ReverseOutcome::NoCaller)
    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugConsolePending.push_back (s_kNoCallerText);
    }

    if (m_debugSession != nullptr)
    {
        m_debugSession->OnStopped (stop);
    }

    m_isDebugViewDirty = true;

    RenderFramebuffer();
    PublishFramebuffer();

Error:
    //  Landed or not, the seek is no longer on its way. The timeline hears
    //  where the machine stands before it hears of the landing, so a line
    //  held where it was dropped moves straight to where the seek landed.
    if (command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle)
    {
        m_seekCycleLanded.store (argument, memory_order_release);

        isPublished = TryPublishHistoryPlayhead();
        IGNORE_RETURN_VALUE (isPublished, false);

        m_historyThumbnails.NoteSeekLanded (argument);
    }

    if (FAILED (hr))
    {
        DEBUGMSG (L"Reverse execution command %d failed: 0x%08X\n", static_cast<int> (command), hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AllowCommand
//
//  The command queue's gate, asked on the posting thread. A command that
//  changes the machine, posted from the UI thread while the machine is behind
//  live, asks first; a yes queues the cut ahead of it, a no drops it. The CPU
//  thread's own commands, and the rest, go ahead.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::AllowCommand (
    WORD                 id,
    const std::string  & payload)
{
    bool               isUiThread = m_hwnd != nullptr && GetWindowThreadProcessId (m_hwnd, nullptr) == GetCurrentThreadId();
    DivergenceVerdict  verdict    = DivergenceVerdict::Proceed;
    bool               isAllowed  = true;



    UNREFERENCED_PARAMETER (payload);

    if (!isUiThread || id == IDM_DEBUG_DIVERGE)
    {
        return true;
    }

    verdict = DivergenceGate::Judge (IsBehindLiveForUi(), DivergenceGate::IsStateChangingCommand (id));

    if (verdict == DivergenceVerdict::Ask)
    {
        isAllowed = AskToDiverge();

        if (isAllowed)
        {
            m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);
        }
    }

    return isAllowed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AskToDiverge
//
//  UI thread. Whether the user agrees to discard the recorded history after
//  where the machine stands.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::AskToDiverge()
{
    int  choice = DxuiMessageBox (m_hwnd, &m_chromeTheme, DivergenceGate::kpszQuestion, DivergenceGate::kpszTitle,
                                  MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);



    return choice == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergeHistory
//
//  CPU thread: the yes, acted on ahead of the change that asked.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DivergeHistory()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_reverseHost == nullptr, S_OK);

    hr = m_reverseHost->Diverge();
    CHR (hr);

    UpdateReplayCaption();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardHistoryEdit
//
//  CPU thread, the debugger session's history guard. Live, the edit is
//  made. Behind live it is held back: the command it came from is handed to
//  the UI thread, which asks, and on a yes queues the cut and the command
//  again, when it runs live.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::GuardHistoryEdit (
    const std::string  & line,
    CommandMode          mode)
{
    bool  isBehind = m_reverseHost != nullptr && m_reverseHost->IsBehindLive();



    if (!isBehind)
    {
        return true;
    }

    {
        std::lock_guard<std::mutex>  held (m_divergeMutex);

        m_pendingDivergeCommand = DebugCommandPayload::Encode (m_debugCommandClient, line, mode);
    }

    PostMessageW (m_hwnd, WM_APP_CONFIRM_DIVERGE, 0, 0);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnConfirmDiverge
//
//  UI thread: asks about the debugger edit held back, and on a yes queues
//  the cut and the edit's command again.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnConfirmDiverge()
{
    std::optional<std::string>  command;



    {
        std::lock_guard<std::mutex>  held (m_divergeMutex);

        command.swap (m_pendingDivergeCommand);
    }

    if (!command.has_value() || !AskToDiverge())
    {
        return;
    }

    m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);
    m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, *command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HoldInputBehindLive
//
//  UI thread. The guest mouse's button or the //c's 80/40 switch, refused by
//  the host input gate. The button is held until the guest reads it; the
//  switch asks at once.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HoldInputBehindLive (HeldInput input)
{
    DivergenceVerdict  verdict = m_divergenceGate.JudgeInput (true, input == HeldInput::MousePress);



    if (verdict == DivergenceVerdict::Ask || verdict == DivergenceVerdict::Hold)
    {
        m_divergenceGate.HoldInput (input);
    }

    PublishHeldInput();

    if (verdict == DivergenceVerdict::Ask)
    {
        PostMessageW (m_hwnd, WM_APP_CONFIRM_INPUT, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishHeldInput
//
//  UI thread, after any change to what is held: the lines the CPU thread
//  watches the guest's reads of, empty unless input is held unasked.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PublishHeldInput()
{
    m_heldInputWatch.Publish (m_divergenceGate.GetHeldLines());
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopForHeldInputRead
//
//  CPU thread, after a slice that ended on a read the held input would
//  change. The machine stops, a debugger run ending as a pause does, and is
//  put back where the reading instruction began, so a yes makes the input
//  live before that instruction runs. Whether it was running decides
//  whether it runs on after the answer; a debugger step stays stopped.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopForHeldInputRead()
{
    uint64_t  position  = m_heldInputWatch.GetHitPosition();
    bool      isStep    = m_debugRunDriver != nullptr && m_debugRunDriver->IsSilent();
    bool      isRunning = !m_cpuManager.IsPaused() && !isStep;



    m_heldInputWatch.ClearHit();

    m_cpuManager.SetPaused (true);
    NotifyDebugPauseChanged (true);

    RunReverseCommand (ReverseCommand::Seek, position);

    m_isResumeOwedAfterHeldRead.store (isRunning, memory_order_release);

    UpdateWindowTitle();
    PostMessageW (m_hwnd, WM_APP_HELD_INPUT_READ, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeldInputRead
//
//  UI thread: the replay stopped where the guest reads held input. With
//  input still held, that asks; with nothing held any more -- let go of
//  while the message was on its way -- the replay runs on.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnHeldInputRead()
{
    bool  isAsking = false;



    // A question already open answers for this read too, and its answer runs
    // the replay on.
    if (m_divergenceGate.IsAsking())
    {
        return;
    }

    isAsking = m_divergenceGate.OnHeldInputRead();

    PublishHeldInput();

    if (isAsking)
    {
        OnConfirmInputDiverge();
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
//  UI thread: a replay stopped at a held read runs on, as the pause button
//  would run it, once the question is answered and, after a yes, the input
//  has gone in.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResumeAfterHeldInput()
{
    bool  isOwed = m_isResumeOwedAfterHeldRead.exchange (false, memory_order_acq_rel);



    if (!isOwed)
    {
        return;
    }

    m_cpuManager.SetPaused (false);
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, "0");

    UpdateWindowTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnConfirmInputDiverge
//
//  UI thread: asks about the input held back. A yes queues the cut; the
//  machine going live then writes the game port as the host has it now and
//  hands back what was held, and a stopped replay runs on from there. A no
//  drops it, a paste included, and the replay goes on.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnConfirmInputDiverge()
{
    bool  isConfirmed  = false;
    bool  hasHeldPaste = m_divergenceGate.HasHeldPaste();



    if (!m_divergenceGate.IsAsking())
    {
        return;
    }

    isConfirmed = AskToDiverge();
    m_divergenceGate.Answer (isConfirmed);

    PublishHeldInput();

    if (!isConfirmed)
    {
        if (hasHeldPaste && m_clipboardManager != nullptr)
        {
            m_clipboardManager->TruncatePaste (m_pasteLengthBeforeHold);
        }

        ResumeAfterHeldInput();
        return;
    }

    m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);

    // Live already, the replay having caught up while the question was
    // open: no return to live will hand the input back, so hand it now.
    if (!IsBehindLiveForUi())
    {
        PostMessageW (m_hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyHeldInputs
//
//  UI thread, the machine live: the inputs a yes kept, in order.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyHeldInputs (const std::vector<HeldInput> & inputs)
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
//  UI thread, the machine live: the pointer's place over the picture, held
//  behind live, becomes the guest mouse's target, as a move there would.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyHeldMouseTarget (uint32_t target)
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    std::shared_lock<std::shared_mutex>  gate;
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

void EmulatorShell::PressGuestMouseHeldBehindLive()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    std::shared_lock<std::shared_mutex>  gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    BAIL_OUT_IF (!lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    BAIL_OUT_IF (!isGateOpen, S_OK);

    m_machine.GetMouse()->SetButton (true);
    m_heldHostInputs.OnPress (HeldHostInputs::kMouseButton, true);

    if (!isHostDown)
    {
        hr = m_host->SetTimer (kClickReleaseTimerId, kClickHoldMs);
        CHRA (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGuestMouseAfterClick
//
//  The end of the click a held press made, unless the user has the button
//  down again by now, when its own release lets go.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReleaseGuestMouseAfterClick()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    std::shared_lock<std::shared_mutex>  gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    hr = m_host->KillTimer (kClickReleaseTimerId);
    IGNORE_RETURN_VALUE (hr, S_OK);

    BAIL_OUT_IF (isHostDown || !lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    m_heldHostInputs.OnRelease (HeldHostInputs::kMouseButton, isGateOpen);

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

void EmulatorShell::ToggleHeldEightyColumnSwitch()
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
//  The //c's 80/40 switch, flipped and kept with the preferences. Behind
//  live it asks first, and on a yes flips once the machine is live. The
//  caller holds the lifetime lock.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ToggleEightyColumnSwitch (Apple2eKeyboard * iieKbd)
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  gate;
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
    PersistSwitchState ("eightyColumnSwitch", newIn);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateReplayCaption
//
//  CPU thread. Behind live the caption says where the machine stands, by the
//  host's clock to the second, so it ticks once a second while the replay
//  runs and moves with every step; live it says nothing. The caption is
//  composed again only when the note changes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateReplayCaption()
{
    EmuCpu        * cpu       = m_machine.GetCpu();
    bool            isBehind  = m_reverseHost != nullptr && cpu != nullptr && m_reverseHost->IsBehindLive();
    uint64_t        cycle     = 0;
    std::wstring    caption;
    bool            isChanged = false;



    if (isBehind)
    {
        cycle   = cpu->GetTotalCycles();
        caption = DebuggerStatusText::GetReplayCaption (m_reverseHost->GetController().GetWallTimeAt (cycle), cycle, LOCALE_NAME_USER_DEFAULT);
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
        UpdateWindowTitle();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeekHistoryCycle
//
//  UI thread: the timeline was clicked, or its playhead dragged, at a cycle,
//  for a machine it has seen stopped; isInterim while the drag goes on. The
//  CPU thread drops it when no history is kept. The cycle and when it was
//  asked for are kept, so the timeline can tell whether the seek has landed.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SeekHistoryCycle (
    uint64_t  cycle,
    bool      isInterim)
{
    m_seekCyclePosted.store   (cycle,            memory_order_release);
    m_seekCyclePostedAt.store (GetTickCount64(), memory_order_release);

    PostReverseCommand (isInterim ? ReverseCommand::ScrubCycle : ReverseCommand::SeekCycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHistorySeekBusy
//
//  UI thread: whether the CPU thread has yet to run the last seek to a cycle
//  the timeline asked for. A seek the queue never ran, as one dropped with
//  the machine it was for, stops counting after kPatienceMs, so a drag of
//  the timeline is never held up for good.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsHistorySeekBusy() const
{
    constexpr uint64_t  kPatienceMs = 500;



    uint64_t  posted   = m_seekCyclePosted.load   (memory_order_acquire);
    uint64_t  postedAt = m_seekCyclePostedAt.load (memory_order_acquire);
    uint64_t  landed   = m_seekCycleLanded.load   (memory_order_acquire);



    return posted != landed && GetTickCount64() - postedAt < kPatienceMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceHistoryThumbnails
//
//  CPU thread, between frames and while paused: the timeline's points are
//  laid out over the keyframes and the next wanted picture is handed to its
//  worker. Never during a reverse command, which runs from the command
//  queue. The timeline hears where the machine stands every turn, so its
//  live or replay state, its line and its labels follow the machine as it
//  runs, as does the caption's replay note.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ServiceHistoryThumbnails()
{
    HRESULT  hr = S_OK;



    if (!TryPublishHistoryPlayhead())
    {
        return;
    }

    UpdateReplayCaption();

    hr = m_historyThumbnails.Service (m_reverseHost->GetController().GetKeyframes());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPublishHistoryPlayhead
//
//  CPU thread: tells the timeline where the machine stands, whether that is
//  behind live, and where history begins. False while no history is kept.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryPublishHistoryPlayhead()
{
    EmuCpu                * cpu       = m_machine.GetCpu();
    uint64_t                cycle     = 0;
    bool                    isBehind  = false;
    const KeyframeStore   * keyframes = nullptr;



    if (m_reverseHost == nullptr || !m_reverseHost->IsRecording() || cpu == nullptr)
    {
        return false;
    }

    cycle     = cpu->GetTotalCycles();
    isBehind  = m_reverseHost->IsBehindLive();
    keyframes = &m_reverseHost->GetController().GetKeyframes();

    m_historyThumbnails.SetPlayhead     (m_machine.GetPosition(), isBehind);
    m_historyThumbnails.SetPlayheadTime (cycle, m_reverseHost->GetController().GetWallTimeAt (cycle), isBehind ? m_reverseHost->GetController().GetLiveEndCycle() : cycle);

    if (keyframes->GetCount() > 0)
    {
        m_historyThumbnails.SetBegin (keyframes->GetInfo (0).cycle, keyframes->GetInfo (0).wallTime);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReplayProgress
//
//  UI thread: how long the running reverse command has run and how much of
//  history it has searched, read from what the CPU thread sets as it goes.
//
////////////////////////////////////////////////////////////////////////////////

ReplayProgress EmulatorShell::GetReplayProgress()
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
//  GetHistoryStatus
//
//  The last reverse command's outcome stands until the machine moves from
//  where it landed, by running or by a step, and is dropped then.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus EmulatorShell::GetHistoryStatus()
{
    HistoryStatus  status;
    bool           hasMoved = !m_cpuManager.IsPaused() || m_machine.GetPosition() != m_lastReversePosition;



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
//  SyncHeatHistory
//
//  CPU thread. Attached, the debugger's heat map keeps its counts in the
//  history and is told what the history does; detached, before the debugger
//  goes, the two are unlinked.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncHeatHistory (bool isAttached)
{
    IDebugTarget       * target     = (m_debugSession != nullptr) ? &m_debugSession->GetTarget() : nullptr;
    ReverseController  * controller = (m_reverseHost != nullptr) ? &m_reverseHost->GetController() : nullptr;
    bool                 isLinked   = isAttached && target != nullptr && controller != nullptr;



    if (controller != nullptr)
    {
        controller->SetHistoryObserver (isLinked ? target->GetHistoryObserver() : nullptr);
    }

    if (target != nullptr)
    {
        target->AttachHistory       (isLinked ? &controller->GetKeyframes() : nullptr, isLinked ? &m_heatReplayer : nullptr);
        target->SetHeatAccessFinder (isLinked ? &m_heatFinder : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceCallHistory
//
//  CPU thread, once a pass while the debugger exists: the debugger's call
//  record is rebuilt from history whenever it starts mid-run. With no
//  history the two are unlinked, and a rebuild under way is dropped. The
//  debugger going drops one too: its call history unlinks itself as it is
//  destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ServiceCallHistory()
{
    ReverseController  * controller = (m_reverseHost != nullptr) ? &m_reverseHost->GetController() : nullptr;
    bool                 isLinked   = controller != nullptr;



    if (m_debugger == nullptr)
    {
        return;
    }

    m_debugger->GetCallHistory().Attach (controller, isLinked ? &m_callReplayer : nullptr);
    m_debugger->GetCallHistory().Service();
}





