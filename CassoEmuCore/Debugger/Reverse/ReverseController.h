#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryRecorder.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/UndoRing.h"

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
    UndoRingSettings  ring;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOutcome
//
//  How a reverse command ended. Moved: the machine is at the target.
//  AtHistoryStart: no earlier position satisfied the command, so the machine
//  is at the oldest position history holds. HistoryCut: a replay diverged
//  from a keyframe's checksum, history after the last good keyframe was
//  dropped, and the machine is live at that keyframe.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReverseOutcome
{
    Moved,
    AtHistoryStart,
    HistoryCut,
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
//  it. Keyframes hold the long range, the undo ring the recent one, and the
//  input journal what the host fed in; any position in history is reached by
//  loading the nearest earlier snapshot and replaying.
//
//  Running backward leaves the recorded future in place: seeking forward
//  replays it, and reaching its end makes the machine live again. A change
//  made in the past (memory, registers, a disk) must be reported through
//  OnMachineChanged, which drops the future and keyframes the changed state;
//  running the machine from the past without a seek does the same. A disk
//  change reaches it through OnMediaChanged.
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

    explicit ReverseController (MachineHost & machine);
    ~ReverseController () override;

    ReverseController             (const ReverseController &) = delete;
    ReverseController & operator= (const ReverseController &) = delete;

    HRESULT   Start            (const ReverseSettings & settings);
    void      Stop             ();
    bool      IsRecording      () const { return m_isRecording; }

    void      OnMediaChanged   (MachineHost & machine) override;
    HRESULT   OnMachineChanged ();

    HRESULT   StepBack         (ReverseResult & result);
    HRESULT   StepBackOver     (ReverseResult & result);
    HRESULT   StepBackOut      (ReverseResult & result);
    HRESULT   StepBackCycles   (uint64_t cycles, ReverseResult & result);
    HRESULT   ReverseContinue  (IReverseStopTest & stopTest, ReverseResult & result);
    HRESULT   SeekToCycle      (uint64_t cycle, ReverseResult & result);
    HRESULT   SeekToPosition   (uint64_t position, ReverseResult & result);

    bool      IsInHistory        () const;
    uint64_t  GetOldestPosition  () const;
    uint64_t  GetLiveEndPosition () const;

    KeyframeStore        & GetKeyframes ()       { return m_keyframes; }
    UndoRing             & GetRing      ()       { return m_ring; }
    Replayer             & GetReplayer  ()       { return m_replayer; }
    const KeyframeStore  & GetKeyframes () const { return m_keyframes; }
    const UndoRing       & GetRing      () const { return m_ring; }

private:
    HRESULT   Seek               (const ReplayTarget & target, bool isByCycle, IReverseStopTest * stopTest, ReplayReport & report, ReverseResult & result);
    HRESULT   RestoreAtOrBefore  (uint64_t position, uint64_t cycle, bool isByCycle);
    HRESULT   HandleDivergence   (const ReplayReport & report, ReverseResult & result);
    HRESULT   GetRecord          (uint64_t position, UndoRecord & outRecord);
    HRESULT   FindStepOverTarget (uint64_t current, bool & outFound, uint64_t & outTarget);
    HRESULT   FindStepOutTarget  (uint64_t current, bool & outFound, uint64_t & outTarget);
    HRESULT   LandAt             (uint64_t position, bool isFound, ReverseResult & result);
    HRESULT   CaptureNow         (bool takeKeyframe, bool takeCheckpoint);
    void      OnCaptureDue       (uint64_t cycle) override;
    void      LeaveLive          ();
    void      BecomeLive         ();
    void      ScheduleCaptures   ();
    void      PruneRetainedMedia ();
    void      FillResult         (ReverseResult & result) const;

    bool      TryFindKeyframeAtOrBefore (uint64_t position, size_t & outIndex) const;

    MachineHost        & m_machine;
    KeyframeStore        m_keyframes;
    Replayer             m_replayer;
    StateWriter          m_writer;                 // kept so its section and segment lists keep their capacity
    StateWriter          m_hostWriter;             // the host input state taken with every capture
    std::vector<Byte>    m_keyframeState;          // a keyframe's whole blob, kept at full size
    bool                 m_isRecording     = false;
    bool                 m_isLive          = true;
    uint64_t             m_liveEndPosition = 0;
    uint64_t             m_liveEndCycle    = 0;
};
