#include "Pch.h"

#include "Shell/HistoryDivergence.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/DebugViewModel.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/HeldHostInputs.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/IShellDebugContext.h"
#include "Shell/MachineHost.h"
#include "Shell/ShellHistory.h"
#include "Core/AppTimerIds.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Debugger/CpuManagerRunDriver.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::HistoryDivergence
//
////////////////////////////////////////////////////////////////////////////////

HistoryDivergence::HistoryDivergence (
    IDebuggerHost        & host,
    MachineHost          & machine,
    CpuManager           & cpuManager,
    ShellHistory         & history,
    IShellDebugContext   & context,
    DebugViewModel       & viewModel) :
    m_host       (host),
    m_machine    (machine),
    m_cpuManager (cpuManager),
    m_history    (history),
    m_context    (context),
    m_viewModel  (viewModel)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::IsBehindLiveForUi
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::IsBehindLiveForUi()
{
    return m_machine.GetHostInput().GetGate().IsHeld();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::AllowCommand
//
//  The command queue's gate, asked on the posting thread. A command that
//  changes the machine, posted from the UI thread while the machine is behind
//  live, asks first; a yes queues the cut ahead of it, a no drops it. The CPU
//  thread's own commands, and the rest, go ahead.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::AllowCommand (
    WORD                 id,
    const std::string  & payload)
{
    bool               isUiThread = m_host.GetMainWindow() != nullptr && GetWindowThreadProcessId (m_host.GetMainWindow(), nullptr) == GetCurrentThreadId();
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
//  HistoryDivergence::AskToDiverge
//
//  UI thread. Whether the user agrees to discard the recorded history after
//  where the machine stands. The message box's loop runs the window's input,
//  so a change made while the question is up would raise it again over the
//  first; that change is dropped instead, as a no drops it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::AskToDiverge()
{
    int                  choice = IDNO;
    HWND                 owner  = nullptr;
    const IDxuiTheme   * theme  = nullptr;



    if (m_isAskingToDiverge)
    {
        return false;
    }

    m_isAskingToDiverge = true;

    //  With the debugger's window active, the box is over it and in its theme.
    m_context.GetQuestionOwner (true, owner, theme);

    choice = DxuiMessageBox (owner, theme, DivergenceGate::kpszQuestion, DivergenceGate::kpszTitle,
                             MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);

    m_isAskingToDiverge = false;

    return choice == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::Diverge
//
//  CPU thread: the yes, acted on ahead of the change that asked.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::Diverge()
{
    HRESULT                    hr       = S_OK;
    std::vector<DebugCommand>  edits;
    std::vector<std::string>   failures;
    Reply                      reply;



    BAIL_OUT_IF (!m_history.HasReverseHost(), S_OK);

    hr = m_history.Diverge();
    CHR (hr);

    m_history.UpdateCaption();

    //  The edits deferred for the question run now, live, in the order made.
    edits = m_deferredEdits.TakeAll();

    for (const DebugCommand & edit : edits)
    {
        BAIL_OUT_IF (m_context.GetDebugController() == nullptr, S_OK);

        reply = m_context.GetDebugController()->GetSession().Execute (edit);

        if (reply.status != CommandStatus::Ok)
        {
            failures.push_back (reply.error.detail);
        }
    }

    m_context.AddConsoleLines (failures);
    m_viewModel.MarkDirty();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::GuardEdit
//
//  CPU thread, the debugger session's history guard. Live, the edit is
//  made. Behind live the command is deferred with any others deferred
//  since, and the first one posts the question to the UI thread; on a yes
//  the cut runs every deferred command, live.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::GuardEdit (const DebugCommand & command)
{
    bool  isBehind = m_history.IsBehindLive();
    bool  isFirst  = false;



    if (!isBehind)
    {
        return true;
    }

    isFirst = m_deferredEdits.Defer (command);

    if (isFirst)
    {
        PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellConfirmDiverge, 0, 0);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::OnConfirmDiverge
//
//  UI thread: puts the question for the deferred debugger edits. A yes
//  queues the cut, which runs them; a no drops them.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::OnConfirmDiverge()
{
    if (!m_deferredEdits.HasAny())
    {
        return;
    }

    if (!AskToDiverge())
    {
        m_deferredEdits.Clear();
        return;
    }

    m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::HoldInput
//
//  UI thread. The guest mouse's button or the //c's 80/40 switch, refused by
//  the host input gate. The button is held until the guest reads it; the
//  switch asks at once.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::HoldInput (HeldInput input)
{
    DivergenceVerdict  verdict = m_divergenceGate.JudgeInput (true, input == HeldInput::MousePress);



    if (verdict == DivergenceVerdict::Ask || verdict == DivergenceVerdict::Hold)
    {
        m_divergenceGate.HoldInput (input);
    }

    PublishHeldInput();

    if (verdict == DivergenceVerdict::Ask)
    {
        PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellConfirmInput, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::PublishHeldInput
//
//  UI thread, after any change to what is held: the lines the CPU thread
//  watches the guest's reads of, empty unless input is held unasked.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::PublishHeldInput()
{
    m_heldInputWatch.Publish (m_divergenceGate.GetHeldLines());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::StopForHeldInputRead
//
//  CPU thread, after a slice that ended on a read the held input would
//  change. The machine stops, a debugger run ending as a pause does, and is
//  put back where the reading instruction began, so a yes makes the input
//  live before that instruction runs. Whether it was running decides
//  whether it runs on after the answer; a debugger step stays stopped.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::StopForHeldInputRead()
{
    uint64_t  position  = m_heldInputWatch.GetHitPosition();
    bool      isStep    = m_context.GetDebugRunDriver() != nullptr && m_context.GetDebugRunDriver()->IsSilent();
    bool      isRunning = !m_cpuManager.IsPaused() && !isStep;



    m_heldInputWatch.ClearHit();

    m_cpuManager.SetPaused (true);
    m_context.NotifyDebugPauseChanged (true);

    m_history.RunCommand (ReverseCommand::Seek, position);

    m_isResumeOwedAfterHeldRead.store (isRunning, memory_order_release);

    m_host.UpdateWindowTitle();
    PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellHeldInputRead, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::OnHeldInputRead
//
//  UI thread: the replay stopped where the guest reads held input. With
//  input still held, that asks; with nothing held any more -- let go of
//  while the message was on its way -- the replay runs on.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::OnHeldInputRead()
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
//  HistoryDivergence::ResumeAfterHeldInput
//
//  UI thread: a replay stopped at a held read runs on, as the pause button
//  would run it, once the question is answered and, after a yes, the input
//  has gone in.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ResumeAfterHeldInput()
{
    bool  isOwed = m_isResumeOwedAfterHeldRead.exchange (false, memory_order_acq_rel);



    if (!isOwed)
    {
        return;
    }

    m_cpuManager.SetPaused (false);
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, [] (IDebugCommandTarget & target) { target.NotifyDebugPauseChanged (false); });

    m_host.UpdateWindowTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::OnConfirmInputDiverge
//
//  UI thread: asks about the input held back. A yes queues the cut; the
//  machine going live then writes the game port as the host has it now and
//  hands back what was held, and a stopped replay runs on from there. A no
//  drops it, a paste included, and the replay goes on.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::OnConfirmInputDiverge()
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
        if (hasHeldPaste)
        {
            m_host.TruncatePaste (m_pasteLengthBeforeHold);
        }

        ResumeAfterHeldInput();
        return;
    }

    m_cpuManager.PostCommand (IDM_DEBUG_DIVERGE);

    // Live already, the replay having caught up while the question was
    // open: no return to live will hand the input back, so hand it now.
    if (!IsBehindLiveForUi())
    {
        PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellGameportFlush, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::ApplyHeldInputs
//
//  UI thread, the machine live: the inputs a yes kept, in order.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ApplyHeldInputs (const std::vector<HeldInput> & inputs)
{
    for (HeldInput input : inputs)
    {
        switch (input)
        {
            case HeldInput::MousePress:
                PressHeldMouse();
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
//  HistoryDivergence::ApplyHeldMouseTarget
//
//  UI thread, the machine live: the pointer's place over the picture, held
//  behind live, becomes the guest mouse's target, as a move there would.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ApplyHeldMouseTarget (uint32_t target)
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;



    BAIL_OUT_IF (!lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInput().GetGate().TryEnter (gate);
    BAIL_OUT_IF (!isGateOpen, S_OK);

    m_machine.GetMouse()->SetHostTargetFraction (static_cast<uint16_t> (target >> 16), static_cast<uint16_t> (target & 0xFFFF));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::PressHeldMouse
//
//  The press goes down as a real one would. The question may have taken the
//  release, which went to the message box, so a button no longer down is
//  let go of after kClickHoldMs, long enough for the guest to see a click.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::PressHeldMouse()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = DxuiInputState::IsKeyDown (VK_LBUTTON);



    BAIL_OUT_IF (!lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInput().GetGate().TryEnter (gate);
    BAIL_OUT_IF (!isGateOpen, S_OK);

    m_machine.GetMouse()->SetButton (true);
    m_host.GetHeldHostInputs().OnPress (HeldHostInputs::kMouseButton, true);

    if (!isHostDown)
    {
        hr = m_host.SetUiTimer ((UINT_PTR) AppTimerId::ShellClickRelease, kClickHoldMs);
        CHRA (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::ReleaseMouseAfterClick
//
//  The end of the click a held press made, unless the user has the button
//  down again by now, when its own release lets go.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ReleaseMouseAfterClick()
{
    HRESULT                              hr         = S_OK;
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 isHostDown = DxuiInputState::IsKeyDown (VK_LBUTTON);



    hr = m_host.KillUiTimer ((UINT_PTR) AppTimerId::ShellClickRelease);
    IGNORE_RETURN_VALUE (hr, S_OK);

    BAIL_OUT_IF (isHostDown || !lifetime.owns_lock() || m_machine.GetMouse() == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInput().GetGate().TryEnter (gate);
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
//  HistoryDivergence::ToggleHeldEightyColumnSwitch
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ToggleHeldEightyColumnSwitch()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);



    if (lifetime.owns_lock())
    {
        ToggleEightyColumnSwitch (m_machine.GetRefs().iieKeyboard);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::ToggleEightyColumnSwitch
//
//  The //c's 80/40 switch, flipped and kept with the preferences. Behind
//  live it asks first, and on a yes flips once the machine is live. The
//  caller holds the lifetime lock.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ToggleEightyColumnSwitch (Apple2eKeyboard * iieKbd)
{
    HRESULT                              hr         = S_OK;
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;
    bool                                 newIn      = false;



    BAIL_OUT_IF (iieKbd == nullptr, S_OK);

    isGateOpen = m_machine.GetHostInput().GetGate().TryEnter (gate);

    if (!isGateOpen)
    {
        HoldInput (HeldInput::EightyColumnToggle);
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
//  HistoryDivergence::TakeJoyportSyncOwed
//
//  UI thread, once live: whether a players' change made behind live is
//  owed to the Joyport, cleared as it is read.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::TakeJoyportSyncOwed()
{
    bool  isOwed = m_isJoyportSyncOwed;



    m_isJoyportSyncOwed = false;

    return isOwed;
}





