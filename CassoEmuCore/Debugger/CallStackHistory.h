#pragma once

#include "Pch.h"

#include "Debugger/CallRecordCopies.h"
#include "Debugger/CallStackRebuildJob.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/ReplayDiskCopier.h"

class DebugSession;
class MachineHost;
class ReverseController;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory
//
//  Rebuilds the debugger's call record from the machine's history, so the
//  call stack holds the calls made before the debugger attached as well as
//  those recorded since.
//
//  Whenever the record starts mid-run -- the debugger attached, or the
//  machine moved through history -- the record is rebuilt by replaying
//  history to where the machine stands, on a second machine
//  (ICallStackRebuilder), so neither the machine nor the debugger waits. The
//  live record goes on meanwhile, and the call stack shows that the rebuild
//  is under way and how far along it is.
//
//  A copy of the record is kept at each keyframe (CallRecordCopies): taken
//  live as each keyframe is, while the record holds what a rebuild would,
//  and made by every rebuild at the keyframes it replays past. A rebuild
//  starts at the newest copy at or before where the machine stands, with
//  the record the copy holds, and replays at most from that keyframe on;
//  without one, at the newest point the record would start again from: the
//  oldest keyframe, the newest keyframe after a gap, or the keyframe before
//  the last power cycle. A copy goes when history drops its keyframe, and
//  the oldest go when the copies outgrow an eighth of history's budget.
//
//  The machine may have run on by the time the rebuild comes in. The
//  second machine then goes on, from where it stopped to where the
//  machine now stands, until it is close enough to finish here, at once,
//  and the record it holds -- what a recorder that had watched from the
//  start of history would hold, resets and power cycles included -- becomes
//  the live one, which goes on from it. A record from power-on, or from a
//  reset, holds every call there is, and is not rebuilt.
//
//  History that does not reach power-on gives a record that begins where
//  history does, which the call stack shows. A record that would begin later
//  than the live one -- history cut by a gap since the live one began -- is
//  of no use, and the live one stays. So does it when the rebuild fails, or
//  the machine it finished on does not match the running one in its cycle
//  count and every register. A record begun again after a move that no
//  rebuild replaces -- history was turned off, or one of these -- then marks
//  only that the calls before it are not available.
//
//  A new start of the record (a reset, a move, the debugger closing and
//  opening again) drops any rebuild under way; a reverse command that left
//  the machine where it was, and replayed nothing on it, is not a move. Nor
//  is a keyframe loaded under the machine as it runs on from the past --
//  past a gap, for one -- but the record was fed across it, so it starts
//  again there too. During a drag of the history timeline the rebuild waits
//  for the drag to end.
//
//  Everything here runs on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class CallStackHistory
{
public:
    //  How far behind the machine the second machine may stand and still
    //  catch up here, at once, rather than on its worker: at first a frame or
    //  two, and once the worker has caught up a round, what the machine runs
    //  in a pass at double speed, with room to spare.
    static constexpr uint64_t  kCatchUpFrames     = 2;
    static constexpr uint64_t  kLateCatchUpFrames = 8;
    static constexpr uint64_t  kCatchUpCycles     = kCatchUpFrames * KeyframeSettings::kFrameCycles;
    static constexpr uint64_t  kLateCatchUpCycles = kLateCatchUpFrames * KeyframeSettings::kFrameCycles;

    //  The copies of the record may hold one part in this many of the
    //  history's budget.
    static constexpr size_t    kCopyBudgetParts   = 8;

    CallStackHistory (MachineHost & machine, DebugSession & session);
    ~CallStackHistory();

    CallStackHistory             (const CallStackHistory &) = delete;
    CallStackHistory & operator= (const CallStackHistory &) = delete;

    //  The history to rebuild from and what rebuilds; either null for none,
    //  which drops a rebuild under way.
    void      Attach       (ReverseController * history, ICallStackRebuilder * rebuilder);

    //  A reverse command is about to run: where the machine stands, so
    //  OnMoved can compare.
    void      OnMoving     ();

    //  The reverse command ran, and the machine stands where it landed;
    //  isInterim while a drag of the timeline goes on.
    void      OnMoved      (bool isInterim);

    //  Takes in a rebuild that has come in, continues it or finishes it,
    //  and requests one now due. Once a pass.
    void      Service      ();

    bool      IsRebuilding () const { return m_awaited != 0; }

    //  The copies of the record kept at keyframes.
    const CallRecordCopies & GetCopies () const { return m_copies; }

    //  The job that rebuilds the record up to target: from the newest copy
    //  of the record, or the newest point the record would start again
    //  from, or, continuing, from where the last job stopped. False in
    //  outHasJob when history holds nothing to replay up to target, or a gap
    //  lies after where a continuing job starts.
    HRESULT   MakeJob      (bool isContinued, uint64_t from, size_t journalCursor, uint64_t target, CallStackRebuildJob & outJob, bool & outHasJob);

private:
    //  Where the machine stood before a reverse command, and how many
    //  instructions and keyframes the history's replayer had replayed and
    //  loaded by then.
    struct MoveMark
    {
        uint64_t  position = 0;
        uint64_t  cycle    = 0;
        uint64_t  replayed = 0;
        size_t    restores = 0;
    };

    void      Stop             ();
    void      Abandon          ();
    void      Request          ();
    void      TakeResults      ();
    HRESULT   Land             (CallStackRebuildResult & result);
    HRESULT   Continue         (const CallStackRebuildResult & result, bool & outIsSettled);
    bool      TryInstall       (const CallStackRebuildResult & result);
    void      NoteWhole        ();
    void      KeepCopies       (std::vector<CallRecordCopy> & copies);
    void      CommitCopies     ();
    void      CheckLoads       ();
    void      OnRecordChanged  (uint64_t record, bool isOn);
    void      OnKeyframeTaken  (uint64_t position);
    void      OnKeyframeDrop   (KeyframeDrop drop);
    bool      IsCopying        () const;
    HRESULT   CopyPart         (size_t index, CallStackRebuildPart & outPart);
    void      AddCopyAt        (const KeyframeInfo & info, CallStackRebuildJob & ioJob) const;
    bool      TryFindSeed      (const KeyframeStore & keyframes, uint64_t from, uint64_t target, size_t & outIndex) const;
    size_t    FindFreshStart   (const KeyframeStore & keyframes, uint64_t target) const;
    bool      TryMarkMove      (MoveMark & outMark) const;
    size_t    GetRestoreCount  () const;
    void      Publish          ();

    static uint64_t  GetRecordFrom    (const CallStackRebuildPart & first);
    static bool      AreSameRegisters (const Cpu6502Registers & a, const Cpu6502Registers & b);

    MachineHost                    & m_machine;
    DebugSession                   & m_session;
    ReverseController              * m_history        = nullptr;
    ICallStackRebuilder            * m_rebuilder      = nullptr;
    ReplayDiskCopier                 m_diskCopier;
    KeyframeUnpacker                 m_noUnpacker;               // holds nothing, so every copy includes its whole snapshot

    uint64_t                         m_seenRecord     = 0;       // the session's record generation last seen
    uint64_t                         m_began          = 0;       // where the live record began
    uint64_t                         m_generation     = 0;
    uint64_t                         m_awaited        = 0;       // the rebuild in flight, 0 for none
    uint64_t                         m_rebuildFrom    = 0;       // where the rebuild in flight started replaying
    uint64_t                         m_roundFrom      = 0;       // where its round on the worker starts
    uint64_t                         m_roundTo        = 0;       // and ends
    size_t                           m_continuations  = 0;       // the rounds the rebuild in flight has been continued on the worker
    bool                             m_isDue          = false;
    bool                             m_isDragging     = false;
    std::optional<MoveMark>          m_moveMark;                 // taken by OnMoving, used by OnMoved

    //  The live record holds what a rebuild to the machine would hold, for
    //  record generation m_wholeRecord; and the history replayer's keyframe
    //  loads accounted for, past which the record was fed across a load.
    bool                             m_isWhole        = false;
    uint64_t                         m_wholeRecord    = 0;
    size_t                           m_restoresSeen   = 0;

    CallRecordCopies                 m_copies;
    std::vector<CallRecordCopy>      m_pendingCopies;            // the rebuild in flight's, kept once its record is taken
    std::vector<Byte>                m_packed;
};
