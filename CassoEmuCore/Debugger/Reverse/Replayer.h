#pragma once

#include "Pch.h"

#include "Core/StateWriter.h"

class EmuCpu;
class IReverseStopTest;
class KeyframeStore;
class MachineHost;
struct InputRecord;





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayTarget
//
//  Where a replay stops: the first instruction boundary at or past either
//  limit. A replay never runs past the end of recorded history.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayTarget
{
    uint64_t  position = UINT64_MAX;
    uint64_t  cycle    = UINT64_MAX;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayReport
//
//  How a replay ended. A divergence gives the keyframe whose checksum the
//  replayed machine did not match; the machine then stands at that keyframe's
//  position in the diverged state. The last hit is the latest position a stop
//  test fired at, before the replay's end position.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayReport
{
    bool      isDiverged       = false;
    size_t    divergedKeyframe = 0;
    bool      hasHit           = false;
    uint64_t  lastHit          = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStep
//
//  The registers, cycle count and instruction bytes one replayed instruction
//  began with; a replay that collects them gives one per position, in order.
//  The step commands read the PC and stack pointer, and the trace pane the
//  rest while the machine is behind live.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayStep
{
    uint64_t  cycles = 0;
    Word      pc     = 0;
    Byte      sp     = 0;
    Byte      a      = 0;
    Byte      x      = 0;
    Byte      y      = 0;
    Byte      p      = 0;
    Byte      opcode = 0;
    Byte      op1    = 0;
    Byte      op2    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Replayer
//
//  Puts the machine back to a keyframe and runs it forward with the
//  recorded inputs, so it reaches every later position in the state it had
//  live.
//
//  While it runs, the debug hook is detached, the input journal is off and
//  detached from the devices (so a replayed read records nothing), the bus
//  reports watched accesses to the stop test's sink alone (so the
//  debugger's own watchpoints see nothing), and the output gate is told, so
//  a host can mute audio and the printer. Inputs are applied in journal
//  order: before the instruction at a position, every record up to that
//  position; when the replay stops at a position, only the boundary records
//  there, since a device makes an observed record during the instruction at
//  its position. Each keyframe the replay reaches is checked against its
//  checksum, and the replay stops at the first that differs.
//
////////////////////////////////////////////////////////////////////////////////

class Replayer
{
public:
    Replayer (MachineHost & machine, KeyframeStore & keyframes);

    void      SetStateLoadedCallback (std::function<void()> callback)          { m_onStateLoaded = std::move (callback); }
    void      SetOutputGate          (std::function<void (bool)> gate)         { m_outputGate    = std::move (gate); }

    //  Told just after a recorded reset or power cycle is made again, true
    //  for a power cycle, as the debugger is told of one live.
    void      SetResetCallback       (std::function<void (bool)> callback)     { m_onReset       = std::move (callback); }

    HRESULT   RestoreKeyframe   (size_t index);
    HRESULT   RunTo             (const ReplayTarget & target, uint64_t endPosition, IReverseStopTest * stopTest, ReplayReport & report, std::vector<ReplayStep> * steps = nullptr, std::vector<Byte> * stackPointers = nullptr);
    HRESULT   PrepareStepHere   (ReplayReport & report, bool includeObserved);

    //  A state from outside the store, at position, its inputs from
    //  journalIndex on; for a second machine whose disks were mounted from
    //  files rather than kept, loaded over what is mounted.
    HRESULT   LoadFrom            (const std::vector<Byte> & state, uint64_t position, size_t journalIndex);
    void      SetOverMountedMedia (bool isOverMounted) { m_isOverMounted = isOverMounted; }

    bool      IsReplaying       () const          { return m_isReplaying; }
    uint64_t  GetReplayedCount  () const          { return m_replayedCount; }
    size_t    GetRestoreCount   () const          { return m_restoreCount; }
    size_t    GetJournalCursor  () const          { return m_journalCursor; }
    void      SetJournalCursor  (size_t cursor)   { m_journalCursor = cursor; }

private:
    //  The slot whose Disk II drives the journal's drive numbers refer to.
    static constexpr int  kDiskControllerSlot = 6;

    HRESULT   LoadState           (const std::vector<Byte> & state, uint64_t position, size_t journalIndex);
    HRESULT   ApplyInputs         (uint64_t position, bool includeObserved);
    HRESULT   ApplyInput          (const InputRecord & record);
    HRESULT   LoadBoundaryIfDue   ();
    HRESULT   CheckKeyframe       (ReplayReport & report);
    HRESULT   Step                (IReverseStopTest * stopTest, uint64_t endPosition, ReplayReport & report, std::vector<ReplayStep> * steps, std::vector<Byte> * stackPointers);
    void      FindNextKeyframe    (uint64_t afterPosition);

    ReplayStep         MakeStep   (const EmuCpu & cpu) const;

    MachineHost                  & m_machine;
    KeyframeStore                & m_keyframes;
    std::function<void()>          m_onStateLoaded;
    std::function<void (bool)>     m_outputGate;
    std::function<void (bool)>     m_onReset;
    std::vector<Byte>              m_scratch;
    StateWriter                    m_checkWriter;       // saves the machine at each keyframe a replay checks; kept for its capacity
    size_t                         m_journalCursor = 0;
    size_t                         m_nextKeyframe  = 0;
    uint64_t                       m_replayedCount = 0;                // instructions every replay has run, for tests and to tell whether a command replayed any
    size_t                         m_restoreCount  = 0;                // keyframes loaded, likewise
    bool                           m_isReplaying   = false;
    bool                           m_isOverMounted = false;
};