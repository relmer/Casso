#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryTimeline.h"
#include "Debugger/Reverse/IReverseSeeker.h"

class HistorySpan;
class IReverseStopTest;
class KeyframeCapturer;
class KeyframeStore;
class MachineHost;
class Replayer;
class ReplayStepTable;
struct ReplayControl;
struct ReplayTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker
//
//  Moves the machine between live and history: a seek to a position or a
//  cycle, which loads the keyframe at or before it and replays forward; a
//  reverse continue, which replays each stretch with a stop test; the run
//  on from a point in history, which replays the future in history as it
//  goes; a change from outside the inputs in the journal, which behind live
//  moves the history after it into a branch left behind; and a switch to
//  such a branch. A replay that diverges from a keyframe's checksum cuts
//  history at the last good keyframe, and the machine is live there.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseSeeker : public IReverseSeeker
{
public:
    //  Called when the machine becomes live (true) or goes behind live (false).
    using LiveChangedCallback = std::function<void (bool isLive)>;

    ReverseSeeker (MachineHost      & machine,
                   KeyframeStore    & keyframes,
                   HistoryTimeline  & timeline,
                   Replayer         & replayer,
                   HistorySpan      & span,
                   ReplayStepTable  & stepTable,
                   KeyframeCapturer & capturer);

    ReverseSeeker (const ReverseSeeker &)             = delete;
    ReverseSeeker & operator= (const ReverseSeeker &) = delete;

    //  The shell's control, read and set from another thread: a search's
    //  progress, and the user's request to stop it.
    void  SetReplayControl       (ReplayControl * control)       { m_control = control; }
    void  SetLiveChangedCallback (LiveChangedCallback callback) { m_onLiveChanged = std::move (callback); }

    HRESULT  StepForward     (ReverseResult & result);
    HRESULT  ReverseContinue (IReverseStopTest & stopTest, ReverseResult & result);
    HRESULT  SwitchBranch    (uint32_t branchId, ReverseResult & result);

    //  Something outside the inputs in the journal changed the machine.
    HRESULT  OnMachineChanged();

    //  The machine runs the instruction at its position itself, behind
    //  live, or arrived after one.
    HRESULT  ReplayHere (bool isStarting);

    void     BecomeLive();

    //  IReverseSeeker.
    HRESULT  LeaveLive        () override;
    HRESULT  LandAt           (uint64_t position, ReverseOutcome outcome, ReverseResult & result) override;
    HRESULT  SeekToPosition   (uint64_t position, ReverseResult & result) override;
    HRESULT  SeekToCycle      (uint64_t cycle, ReverseResult & result) override;
    HRESULT  HandleDivergence (const ReplayReport & report, ReverseResult & result) override;
    bool     IsStopDue        (uint64_t from, uint64_t reached) override;

    //  The positions history spans, with the machine where it is now.
    uint64_t  GetOldestPosition  () const;
    uint64_t  GetLiveEndPosition () const;
    uint64_t  GetLiveEndCycle    () const;
    uint64_t  GetRecordedEnd     () const;

    //  The last replay that diverged, and how many have.
    const std::string  & GetLastDivergence () const { return m_lastDivergence; }
    size_t               GetCutCount       () const { return m_cutCount; }

    static void  FillResult (const MachineHost & machine, ReverseResult & result);

private:
    using Stretch = HistoryTimeline::Stretch;

    HRESULT  Seek              (const ReplayTarget & target, bool isByCycle, IReverseStopTest * stopTest, ReplayReport & report, ReverseResult & result);
    HRESULT  RestoreAtOrBefore (uint64_t position, uint64_t cycle, bool isByCycle, Stretch & outStretch);
    bool     TryFindStretch    (uint64_t position, Stretch & outStretch) const;
    void     MarkReplaying     ();
    HRESULT  CaptureLiveEnd    ();

    MachineHost          & m_machine;
    KeyframeStore        & m_keyframes;
    HistoryTimeline      & m_timeline;
    Replayer             & m_replayer;
    HistorySpan          & m_span;
    ReplayStepTable      & m_stepTable;
    KeyframeCapturer     & m_capturer;
    ReplayControl        * m_control  = nullptr;
    LiveChangedCallback    m_onLiveChanged;
    std::string            m_lastDivergence;
    size_t                 m_cutCount = 0;
};
