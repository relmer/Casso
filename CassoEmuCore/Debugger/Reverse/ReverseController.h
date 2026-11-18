#pragma once

#include "Pch.h"

#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Debugger/Reverse/HistorySpan.h"
#include "Debugger/Reverse/HistoryTimeline.h"
#include "Debugger/Reverse/KeyframeCapturer.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/ReplayStepTable.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Debugger/Reverse/ReverseSeeker.h"
#include "Debugger/Reverse/ReverseStepper.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSettings
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseSettings
{
    KeyframeSettings  keyframes;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController
//
//  Records the machine's history and moves it backward and forward through
//  it. History is the keyframe store alone, one whole-machine snapshot every
//  few frames, plus the input journal of what the host fed in; any position
//  is reached by loading the keyframe at or before it and replaying forward
//  by the retired-instruction count. The positions from one keyframe to the
//  next are a stretch.
//
//  Step back needs the PC and stack pointer each instruction began with.
//  The first step into a stretch replays the whole stretch once and keeps
//  them in a table, so later steps in it are lookups; the table goes when
//  the machine leaves the stretch, becomes live, or changes. Step back over
//  and step back out search with the stack pointer alone, replaying one
//  stretch at a time and keeping nothing else; step back out seeks straight
//  to the call the debugger's call record holds, when it holds one.
//
//  Running backward leaves the recorded future in place: seeking forward
//  replays it, and reaching its end makes the machine live again. A change
//  made in the past (memory, registers, a disk) must be reported through
//  ReverseSeeker::OnMachineChanged, which moves the future into a branch
//  left behind and keyframes the changed state.
//  Running the machine from the past without a seek replays the recorded
//  future instead, inputs and all, until it reaches the end of it. A disk
//  change reaches it through OnMediaChanged, and a debugger edit through
//  OnMachineEdited: in the past at once, and while live once, before the next
//  instruction or reverse command, however many edits came first.
//
//  Recording continues at every speed. SetRecordingPaused pauses it,
//  leaving a gap in history from where it paused to the keyframe taken where
//  it resumed.
//
//  While the machine is behind live, the disk store holds the automatic
//  flushes (the motor stopping, a power cycle), so the image files are
//  written only on an eject, a machine switch, exit or a commit, each with
//  the disks as they stand at the current position. A reset is the
//  exception: the Disk II controller writes its dirty disks itself on a
//  reset, outside the store's hold, so a reset run or replayed behind live
//  writes the files. Live, the automatic flushes write as they do without
//  history.
//
//  The step commands work on positions; the scanline and frame steps and the
//  seek work on cycles and land on the first instruction boundary at or
//  after the target cycle. All of them run on the thread that runs the
//  machine, between instructions.
//
//  A history observer, when one is set, is told of each keyframe taken live
//  before it is stored, and may give it side bytes to keep, and of each
//  keyframe whose state a replay loads. Keyframe listeners are called with
//  each keyframe taken live once it is stored, and live listeners each time
//  the machine becomes live: at the end of history, or where a change made
//  in the past cut it.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseController : public HistoryRecorder
{
public:
    static constexpr uint64_t  kScanlineCycles = 65;
    static constexpr uint64_t  kFrameCycles    = KeyframeSettings::kFrameCycles;
    static constexpr uint64_t  kSkipCycles     = kFrameCycles * 600;   // ten seconds

    //  Called with the position each time the machine becomes live.
    using LiveListener = std::function<void (uint64_t position)>;

    explicit ReverseController (MachineHost & machine);
    ~ReverseController () override;

    ReverseController             (const ReverseController &) = delete;
    ReverseController & operator= (const ReverseController &) = delete;

    HRESULT   Start               (const ReverseSettings & settings);

    //  Saved history: the store and the journal laid out for a file, the
    //  machine's position and history cycle as the active path's live end,
    //  taken live; and recording started from saved history, in place of
    //  Start, on a machine loaded with the state at that live end.
    HRESULT   SaveHistory         (SavedKeyframes & outSaved);
    HRESULT   StartFromSaved      (const ReverseSettings & settings, const SavedKeyframes & saved);

    void      Stop                ();
    void      SetLiveListener     (const void * owner, LiveListener listener);
    bool      IsRecordingLive     () const;
    HRESULT   SetRecordingPaused  (bool isPaused);
    HRESULT   ChangeBudget        (size_t budgetBytes);

    void      OnMediaChanged      (MachineHost & machine) override;
    void      OnMachineEdited     (MachineHost & machine) override;

    KeyframeStore            & GetKeyframes ()       { return m_keyframes; }
    const KeyframeStore      & GetKeyframes () const { return m_keyframes; }
    const HistoryTimeline    & GetTimeline  () const { return m_timeline; }
    Replayer                 & GetReplayer  ()       { return m_replayer; }
    const HistorySpan        & GetSpan      () const { return m_span; }
    const ReplayStepTable    & GetStepTable () const { return m_stepTable; }
    KeyframeCapturer         & GetCapturer  ()       { return m_capturer; }
    ReverseSeeker            & GetSeeker    ()       { return m_seeker; }
    const ReverseSeeker      & GetSeeker    () const { return m_seeker; }
    ReverseStepper           & GetStepper   ()       { return m_stepper; }
    const ReverseStepper     & GetStepper   () const { return m_stepper; }

private:
    void  OnCaptureDue     (uint64_t cycle) override;
    void  OnArrived        () override;
    void  OnLiveChanged    (bool isLive);
    void  ScheduleCaptures ();

    MachineHost                                         & m_machine;
    ThreadPoolWorkQueue                                   m_workQueue;     // before the store, which waits on it as it goes
    KeyframeStore                                         m_keyframes;
    HistoryTimeline                                       m_timeline       { m_keyframes };
    Replayer                                              m_replayer;
    HistorySpan                                           m_span;
    ReplayStepTable                                       m_stepTable;
    KeyframeCapturer                                      m_capturer       { m_machine, m_keyframes, m_span, m_workQueue, m_pagesRun };
    ReverseSeeker                                         m_seeker         { m_machine, m_keyframes, m_timeline, m_replayer, m_span, m_stepTable, m_capturer };
    ReverseStepper                                        m_stepper        { m_machine, m_keyframes, m_timeline, m_replayer, m_span, m_stepTable, m_seeker };

    //  Each owner's live listener, in the order they were set.
    std::vector<std::pair<const void *, LiveListener>>    m_liveListeners;
};
