#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSink
//
//  Points the mixer at the machine it writes to, or at nothing. A different
//  sink may hold any state, so the next write covers every field.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::SetSink (IGamePortSink * sink)
{
    std::unique_lock<std::mutex>  lock (m_mutex);



    m_sink       = sink;
    m_hasApplied = false;

    lock.unlock();
    ScheduleApply();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetApplyThread
//
//  Names the one thread allowed to write the machine, and how to wake it.
//  A submission from any other thread records its values and calls
//  requestFlush, at most once until that thread flushes; the woken thread
//  then calls FlushPending. With no apply thread set, every caller writes
//  inline, which is what single-threaded tests want.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::SetApplyThread (std::thread::id applyThread, std::function<void()> requestFlush)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_applyThread    = applyThread;
    m_requestFlush   = std::move (requestFlush);
    m_flushRequested = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisOwner
//
//  Chooses which source drives PDL0 and PDL1. The new owner's last
//  contribution takes effect immediately, so a source that has been
//  submitting all along does not wait for its next change.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::SetAxisOwner (AxisOwner owner)
{
    std::unique_lock<std::mutex>  lock    (m_mutex);
    bool                          changed = m_owner != owner;



    m_owner = owner;
    lock.unlock();

    if (changed)
    {
        ScheduleApply();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Submit
//
//  Records what one source currently asks of the game port. Nothing is
//  written when the contribution is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::Submit (GamePortSource source, const GamePortContribution & contribution)
{
    HRESULT                       hr      = S_OK;
    std::unique_lock<std::mutex>  lock    (m_mutex);
    size_t                        index   = static_cast<size_t> (source);
    bool                          changed = false;



    CBRA (index < kSourceCount);

    changed = !(m_contributions[index] == contribution);
    m_contributions[index] = contribution;
    lock.unlock();

    if (changed)
    {
        ScheduleApply();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseSource
//
//  Returns one source to rest: no axes, no buttons.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::ReleaseSource (GamePortSource source)
{
    Submit (source, GamePortContribution());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyMachineRebuilt
//
//  The machine's devices were replaced, so nothing the mixer wrote before is
//  in them. The next write covers every field, and it is scheduled now rather
//  than waiting for the next input change. This is also what delivers a write
//  the sink refused while the rebuild held the machine.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::NotifyMachineRebuilt()
{
    std::unique_lock<std::mutex>  lock (m_mutex);



    m_hasApplied = false;
    lock.unlock();

    ScheduleApply();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushPending
//
//  Apply thread. Writes the target state if it differs from what was last
//  written. Returns true when the machine holds the target state afterward.
//  The sink runs outside the lock, so a submission from another thread during
//  the write is recorded and requests another flush rather than blocking.
//
////////////////////////////////////////////////////////////////////////////////

bool GamePortInputMixer::FlushPending()
{
    std::unique_lock<std::mutex>    lock       (m_mutex);
    IGamePortSink                 * sink       = m_sink;
    GamePortState                   target     = ComputeTargetLocked();
    GamePortState                   last       = m_lastApplied;
    bool                            hasApplied = m_hasApplied;
    bool                            applied    = false;
    bool                            upToDate   = false;



    m_flushRequested = false;
    lock.unlock();

    if (sink != nullptr && !(hasApplied && target == last))
    {
        applied = sink->TryApply (target, hasApplied ? &last : nullptr);
    }

    lock.lock();

    if (applied)
    {
        m_lastApplied = target;
        m_hasApplied  = true;
    }

    upToDate = m_hasApplied && ComputeTargetLocked() == m_lastApplied;

    return upToDate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasPendingWrite
//
//  True while the machine does not yet hold the target state: before the
//  first write, after a refused write, and between a submission from another
//  thread and the flush it requested.
//
////////////////////////////////////////////////////////////////////////////////

bool GamePortInputMixer::HasPendingWrite() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return !m_hasApplied || !(ComputeTargetLocked() == m_lastApplied);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetState
//
////////////////////////////////////////////////////////////////////////////////

GamePortState GamePortInputMixer::GetTargetState() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return ComputeTargetLocked();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleApply
//
//  Writes now when called on the apply thread; otherwise asks that thread to
//  flush, once per flush.
//
////////////////////////////////////////////////////////////////////////////////

void GamePortInputMixer::ScheduleApply()
{
    std::unique_lock<std::mutex>  lock         (m_mutex);
    bool                          applyHere    = m_applyThread == std::thread::id() ||
                                                 m_applyThread == std::this_thread::get_id();
    bool                          shouldWake   = false;
    std::function<void()>         requestFlush;



    if (!applyHere && !m_flushRequested && m_requestFlush)
    {
        m_flushRequested = true;
        shouldWake       = true;
        requestFlush     = m_requestFlush;
    }

    lock.unlock();

    if (applyHere)
    {
        FlushPending();
    }
    else if (shouldWake)
    {
        requestFlush();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeTargetLocked
//
//  Buttons OR across every source, so releasing one never releases a button
//  another source still holds. The axes come from the owner alone, or rest
//  at center when the owner has nothing to say.
//
////////////////////////////////////////////////////////////////////////////////

GamePortState GamePortInputMixer::ComputeTargetLocked() const
{
    GamePortState                   state;
    const GamePortContribution    * axisSource = GetAxisContributionLocked();



    for (const GamePortContribution & contribution : m_contributions)
    {
        state.buttons |= contribution.buttons;
    }

    if (axisSource != nullptr && axisSource->paddle.has_value())
    {
        state.paddle = *axisSource->paddle;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxisContributionLocked
//
////////////////////////////////////////////////////////////////////////////////

const GamePortContribution * GamePortInputMixer::GetAxisContributionLocked() const
{
    const GamePortContribution  * contribution = nullptr;



    switch (m_owner)
    {
        case AxisOwner::ArrowKeys:
            contribution = &m_contributions[static_cast<size_t> (GamePortSource::ArrowKeys)];
            break;

        case AxisOwner::MousePaddle:
            contribution = &m_contributions[static_cast<size_t> (GamePortSource::MousePaddle)];
            break;

        case AxisOwner::Controller:
            contribution = &m_contributions[static_cast<size_t> (GamePortSource::Controller)];
            break;

        case AxisOwner::None:
        default:
            break;
    }

    return contribution;
}
