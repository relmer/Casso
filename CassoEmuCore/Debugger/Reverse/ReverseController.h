#pragma once

#include "Pch.h"

#include "Core/StateWriter.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/CallerLink.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/ReverseOutcome.h"

class IReverseStopTest;
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
//  ReverseResult
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseResult
{
    ReverseOutcome  outcome  = ReverseOutcome::Moved;
    uint64_t        position = 0;
    uint64_t        cycle    = 0;
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
//  OnMachineChanged, which drops the future and keyframes the changed state.
//  Running the machine from the past without a seek replays the recorded
//  future instead, inputs and all, until it reaches the end of it. A disk
//  change reaches it through OnMediaChanged, and a debugger edit through
//  OnMachineEdited: in the past at once, and while live once, before the next
//  instruction or reverse command, however many edits came first.
//
//  While the user has chosen Maximum speed, recording pauses, leaving a gap
//  in history from where it paused to the keyframe taken where it resumed;
//  SetUserMaximumSpeed carries the choice. A speed raised automatically is
//  not the user's choice and recording goes on.
//
//  While recording, the disk store holds the automatic flushes, so the image
//  files are written only on an eject, a machine switch, exit or a commit,
//  each with the disks as they stand at the current position.
//
//  The step commands work on positions; the scanline and frame steps and the
//  seek work on cycles and land on the first instruction boundary at or
//  after the target cycle. All of them run on the thread that runs the
//  machine, between instructions.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseController : public HistoryRecorder
{
public:
    static constexpr uint64_t  kScanlineCycles = 65;
    static constexpr uint64_t  kFrameCycles    = KeyframeSettings::kFrameCycles;

    //  Asked, live, whether the call record shows that no call made at or
    //  after the given cycle, where history begins, entered the code now
    //  running. False when the record cannot tell.
    using CallerProbe = std::function<bool (uint64_t historyStartCycle)>;

    //  Asked, live, for the calls the code now running is still inside of,
    //  so step back out can seek straight to the innermost.
    using CallerLinksProbe = std::function<void (std::vector<CallerLink> & outLinks)>;

    explicit ReverseController (MachineHost & machine);
    ~ReverseController () override;

    ReverseController             (const ReverseController &) = delete;
    ReverseController & operator= (const ReverseController &) = delete;

    HRESULT   Start               (const ReverseSettings & settings);
    void      SetWorkQueue        (IWorkQueue * queue) { m_workQueueOverride = queue; }
    void      SetReplayControl    (ReplayControl * control) { m_control = control; }
    void      SetCallerProbe      (CallerProbe probe) { m_callerProbe = std::move (probe); }
    void      SetCallerLinksProbe (CallerLinksProbe probe) { m_callerLinksProbe = std::move (probe); }
    void      Stop                ();
    bool      IsRecording         () const { return m_isRecording; }
    HRESULT   SetUserMaximumSpeed (bool isMaximum);
    bool      IsPaused            () const { return m_isPaused; }

    void      OnMediaChanged      (MachineHost & machine) override;
    void      OnMachineEdited     (MachineHost & machine) override;
    HRESULT   OnMachineChanged    ();

    HRESULT   StepBack            (ReverseResult & result);
    HRESULT   StepForward         (ReverseResult & result);
    HRESULT   StepBackOver        (ReverseResult & result);
    HRESULT   StepBackOut         (ReverseResult & result);
    HRESULT   StepBackCycles      (uint64_t cycles, ReverseResult & result);
    HRESULT   ReverseContinue     (IReverseStopTest & stopTest, ReverseResult & result);
    HRESULT   SeekToCycle         (uint64_t cycle, ReverseResult & result);
    HRESULT   SeekToPosition      (uint64_t position, ReverseResult & result);

    bool      IsInHistory         () const;
    uint64_t  GetOldestPosition   () const;
    uint64_t  GetLiveEndPosition  () const;
    uint64_t  GetLiveEndCycle     () const { return m_liveEndCycle; }
    uint64_t  GetWallTimeAt       (uint64_t cycle) const;
    size_t    GetTableBuildCount  () const { return m_tableBuilds; }
    bool      HasStepTable        () const { return m_hasSteps; }

    HRESULT   PrepareRecentSteps  (ReverseResult & result);
    void      GetRecentSteps      (size_t count, std::vector<ReplayStep> & outSteps) const;

    KeyframeStore        & GetKeyframes ()       { return m_keyframes; }
    Replayer             & GetReplayer  ()       { return m_replayer; }
    const KeyframeStore  & GetKeyframes () const { return m_keyframes; }

private:
    //  The positions one keyframe's replay covers: from the keyframe to the
    //  next one, or to where recording paused, or to the end of history.
    struct Stretch
    {
        size_t    keyframe = 0;
        uint64_t  start    = 0;
        uint64_t  end      = 0;
    };

    HRESULT   Seek               (const ReplayTarget & target, bool isByCycle, IReverseStopTest * stopTest, ReplayReport & report, ReverseResult & result);
    HRESULT   RestoreAtOrBefore  (uint64_t position, uint64_t cycle, bool isByCycle, Stretch & outStretch);
    HRESULT   HandleDivergence   (const ReplayReport & report, ReverseResult & result);
    HRESULT   GetStep            (uint64_t position, ReplayStep & outStep, bool & outIsInGap, uint64_t & outGapStart);
    HRESULT   BuildStepTable     (const Stretch & stretch);
    HRESULT   ReplayStackLevels  (uint64_t end, Stretch & outStretch, bool & outIsInGap);
    HRESULT   FindStepOverTarget (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    HRESULT   FindStepOutTarget  (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    bool      HasNoCaller        () const;
    bool      TryGetCallerLinks  (size_t & outCount);
    bool      HasGapSince        (uint64_t cycle, uint64_t position) const;
    HRESULT   StepOutByRecord    (bool & outIsDone, ReverseResult & result);
    bool      IsAtCallerLink     (const CallerLink & link);
    void      DiscardCallerLinks ();
    HRESULT   LandAt             (uint64_t position, ReverseOutcome outcome, ReverseResult & result);
    HRESULT   CaptureNow         ();
    HRESULT   CaptureBoundary    (bool isAfterGap, uint64_t gapStart);
    void      OnCaptureDue       (uint64_t cycle) override;
    void      OnArrived          () override;
    HRESULT   ReplayHere         (bool isStarting);
    HRESULT   LeaveLive          ();
    bool      IsStopDue          (uint64_t from, uint64_t reached);
    void      BecomeLive         ();
    void      ScheduleCaptures   ();
    void      DiscardStepTable   ();
    void      PruneRetainedMedia ();
    HRESULT   UseWorkQueue       ();
    void      FillResult         (ReverseResult & result) const;

    bool      TryFindStretch     (uint64_t position, Stretch & outStretch) const;
    uint64_t  GetSegmentEnd      (size_t keyframe) const;

    MachineHost              & m_machine;
    ThreadPoolWorkQueue        m_workQueue;              // before the store, which waits on it as it goes
    KeyframeStore              m_keyframes;
    Replayer                   m_replayer;
    StateWriter                m_writer;                 // kept so its section and segment lists keep their capacity
    StateWriter                m_hostWriter;             // the host input state taken with every capture
    IWorkQueue               * m_workQueueOverride = nullptr;  // set by a test
    ReplayControl            * m_control           = nullptr;  // the shell's, read and set from another thread
    CallerProbe                m_callerProbe;            // the debugger's call record, when one is attached
    CallerLinksProbe           m_callerLinksProbe;       // the same record's calls still entered
    std::vector<CallerLink>    m_callerLinks;            // the chain step back out last read live, outermost first
    size_t                     m_callerDepth       = 0;      // the links outside the call step back out last landed on
    uint64_t                   m_callerLanding     = UINT64_MAX;  // where it landed; any other position uses no link
    std::vector<Byte>          m_stackPointers;          // a search's stack pointer per position, for one stretch
    std::vector<ReplayStep>    m_steps;                  // one stretch's registers and bytes per position
    uint64_t                   m_stepsStart        = 0;
    uint64_t                   m_stepsEnd          = 0;
    size_t                     m_tableBuilds       = 0;
    bool                       m_hasSteps          = false;
    bool                       m_isCut             = false;  // a table's replay diverged and cut history; m_cutResult says where
    bool                       m_isStopped         = false;  // a search for a step's target was stopped short
    ReverseResult              m_cutResult;
    bool                       m_isRecording       = false;
    bool                       m_isLive            = true;
    bool                       m_isPaused          = false;
    bool                       m_isEditPending     = false;  // a debugger edit while live, kept as a boundary before the next instruction
    uint64_t                   m_pauseStart        = 0;      // while paused and live: where recording stopped
    uint64_t                   m_liveEndPosition   = 0;
    uint64_t                   m_liveEndCycle      = 0;
    uint64_t                   m_hereStart         = 0;      // the stretch the machine last ran an instruction in behind live
    uint64_t                   m_hereEnd           = 0;
};
