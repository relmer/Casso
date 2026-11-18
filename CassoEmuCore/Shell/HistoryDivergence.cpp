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
//  The command queue's gate, on the posting thread. A command that changes
//  the machine, posted from the UI thread while the machine is behind live,
//  has a fork queued ahead of it, with no question. Every command goes ahead.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::AllowCommand (
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
//  HistoryDivergence::Diverge
//
//  CPU thread: the fork queued ahead of a change made behind live.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::Diverge()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_history.HasReverseHost(), S_OK);

    hr = m_history.Diverge();
    CHR (hr);

    m_history.UpdateCaption();
    m_viewModel.MarkDirty();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::GuardEdit
//
//  CPU thread, the debugger session's history guard. Behind live, history
//  forks first, with no question, and the edit is then made live.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryDivergence::GuardEdit (const DebugCommand & command)
{
    UNREFERENCED_PARAMETER (command);

    if (m_history.IsBehindLive())
    {
        Diverge();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::HoldInput
//
//  UI thread. The guest mouse's button or the //c's 80/40 switch, refused by
//  the host input gate. The button is deferred until the guest reads it;
//  the switch forks at once.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::HoldInput (HeldInput input)
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
        PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellForkForInput, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::RunForHeldInput
//
//  UI thread, for input deferred while the machine is paused behind live:
//  the replay runs, so the replayed guest reads the input where it would,
//  and history goes on from there. Not while the rewind key is down.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::RunForHeldInput()
{
    if (!m_cpuManager.IsPaused() || m_history.IsRewinding())
    {
        return;
    }

    m_cpuManager.SetPaused (false);
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, [] (IDebugCommandTarget & target) { target.NotifyDebugPauseChanged (false); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence::PublishHeldInput
//
//  UI thread, after any change to what is held: the lines the CPU thread
//  watches the guest's reads of, empty unless input is deferred.
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
//  UI thread: the replay stopped where the guest reads deferred input. With
//  input deferred, history forks there; with none -- let go of while the
//  message was on its way -- the replay runs on.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::OnHeldInputRead()
{
    bool  isForking = false;



    // A fork already on its way covers this read too, and runs the replay
    // on.
    if (m_divergenceGate.IsForking())
    {
        return;
    }

    isForking = m_divergenceGate.OnHeldInputRead();

    PublishHeldInput();

    if (isForking)
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
//  HistoryDivergence::ForkForHeldInput
//
//  UI thread: history forks for the input deferred, with no question. The
//  fork is queued; the machine going live then writes the game port as the
//  host has it now and posts back what was deferred, and a stopped replay
//  runs on from there.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryDivergence::ForkForHeldInput()
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
//  live history forks first, and it flips once the machine is live. The
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





