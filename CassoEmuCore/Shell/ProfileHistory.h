#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/ProfileEventProbe.h"
#include "Shell/ScratchEventReplayer.h"

class MachineHost;
class ReverseController;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory
//
//  A profile built from history: the stretches from the keyframe at or
//  before a period's first cycle to the machine's position are replayed on
//  a scratch machine with ProfileEventProbe, in the background, so neither
//  the machine nor the window waits on it. Consecutive stretches with no gap
//  between them replay as one, so a routine's calls and returns are seen in
//  order. A newer build replaces one under way.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileHistory
{
public:
    using IsStoppedFn = std::function<bool()>;

    ProfileHistory (MachineHost & machine, DebugEventHistory & history);
    ~ProfileHistory();

    ProfileHistory             (const ProfileHistory &) = delete;
    ProfileHistory & operator= (const ProfileHistory &) = delete;

    //  History begins again; null while recording is off.
    void  Start (ReverseController * controller);
    void  Stop  ();

    //  Whether the machine is stopped, so the stretch since the newest
    //  keyframe has an end; unset in a test.
    void  SetIsStopped (IsStoppedFn isStopped) { m_isStopped = std::move (isStopped); }
    void  SetWorkQueue (IWorkQueue * queue)    { m_runner.SetWorkQueue (queue); }
    void  WaitForWork  ()                      { m_runner.WaitForWork(); }

    //  The profile from fromCycle, or from history's start when it is
    //  earlier, to the machine's position. An error when there is no
    //  history to replay.
    HRESULT  Build (uint64_t fromCycle, std::string & error);

    //  The finished profile, once the last stretch is in.
    bool  Pump (ProfileCountsPtr & outCounts);

    //  The share replayed so far, 0 to 1, while a build is under way.
    std::optional<double>  GetProgress () const;

    //  The stretches a build from fromCycle replays, oldest first; for a test.
    void  GetBounds (uint64_t fromCycle, std::vector<DebugEventBounds> & outBounds) const;

private:
    bool  IsStopped () const;

    MachineHost                              & m_machine;
    DebugEventHistory                        & m_history;
    ReverseController                        * m_controller = nullptr;
    ProfileEventProbe                        * m_probe      = nullptr;
    ScratchEventReplayer<ProfileCountsPtr>     m_runner;
    uint64_t                                   m_generation = 0;
    size_t                                     m_remaining  = 0;
    ProfileCountsPtr                           m_latest;
    IsStoppedFn                                m_isStopped;
};
