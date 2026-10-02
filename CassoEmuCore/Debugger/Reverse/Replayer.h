#pragma once

#include "Pch.h"

class IReverseStopTest;
class KeyframeStore;
class MachineHost;
class UndoRing;
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
//  Replayer
//
//  Puts the machine back to a keyframe or an undo ring checkpoint and runs it
//  forward with the recorded inputs, so it reaches every later position in
//  the state it had live.
//
//  While it runs, the debug hook is detached, the input journal is off and
//  detached from the devices (so a replayed read records nothing), and the
//  output gate is told, so a host can mute audio and the printer. Inputs are
//  applied in journal order: before the instruction at a position, every
//  record up to that position; when the replay stops at a position, only the
//  boundary records there, since a device makes an observed record during
//  the instruction at its position. Each keyframe the replay reaches is
//  checked against its checksum, and the replay stops at the first that
//  differs.
//
//  The ring is truncated to the restored position and refilled as the replay
//  runs, so its records and checkpoints always end where the machine is.
//
////////////////////////////////////////////////////////////////////////////////

class Replayer
{
public:
    Replayer (MachineHost & machine, KeyframeStore & keyframes, UndoRing & ring);

    void      SetStateLoadedCallback (std::function<void()> callback)          { m_onStateLoaded = std::move (callback); }
    void      SetOutputGate          (std::function<void (bool)> gate)         { m_outputGate    = std::move (gate); }

    HRESULT   RestoreKeyframe   (size_t index);
    HRESULT   RestoreCheckpoint (size_t index);
    HRESULT   RunTo             (const ReplayTarget & target, uint64_t endPosition, IReverseStopTest * stopTest, ReplayReport & report);

    bool      IsReplaying       () const          { return m_isReplaying; }
    size_t    GetJournalCursor  () const          { return m_journalCursor; }
    void      SetJournalCursor  (size_t cursor)   { m_journalCursor = cursor; }

private:
    HRESULT   LoadState           (const std::vector<Byte> & state, uint64_t position, size_t journalIndex);
    HRESULT   ApplyInputs         (uint64_t position, bool includeObserved);
    HRESULT   ApplyInput          (const InputRecord & record);
    HRESULT   TakeCheckpointIfDue ();
    HRESULT   CheckKeyframe       (ReplayReport & report);
    HRESULT   Step                (IReverseStopTest * stopTest, uint64_t endPosition, ReplayReport & report);
    void      FindNextKeyframe    (uint64_t afterPosition);

    MachineHost                  & m_machine;
    KeyframeStore                & m_keyframes;
    UndoRing                     & m_ring;
    std::function<void()>          m_onStateLoaded;
    std::function<void (bool)>     m_outputGate;
    std::vector<Byte>              m_scratch;
    size_t                         m_journalCursor = 0;
    size_t                         m_nextKeyframe  = 0;
    bool                           m_isReplaying   = false;
};
