#pragma once

#include "Pch.h"

#include "Debugger/CallStack.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"

struct InputRecord;
class InputJournal;
struct ReplayDisk;
class ReplayDiskCopier;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRebuildPart
//
//  One stretch of a call-record rebuild: from a keyframe, still packed, with
//  its position, cycle and journal index, to where the next part begins or
//  to the job's target. The first part of a job that continues the last
//  holds no keyframe: the second machine replays on from where that job
//  left it.
//
//  The first part of a fresh job may hold a seed: the record as it stood at
//  its keyframe, packed (CallRecordCopies::Pack), which the record goes on
//  from rather than beginning there. seedFrom is the keyframe a rebuild
//  without the seed would have begun the record at.
//
////////////////////////////////////////////////////////////////////////////////

struct CallStackRebuildPart
{
    PackedKeyframe     start;
    bool               isLoaded      = true;
    uint64_t           startPosition = 0;
    uint64_t           startCycle    = 0;
    size_t             journalIndex  = 0;
    uint64_t           endPosition   = 0;
    std::vector<Byte>  seed;
    uint64_t           seedFrom      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopy
//
//  A keyframe a rebuild copies the record at, identified by its position,
//  cycle and checksum, so a keyframe taken later at the same position does
//  not match it; in a result, the record there, its last instruction
//  settled, packed (CallRecordCopies::Pack). A result leaves packed empty
//  where it would equal the copy before it in the same result.
//
////////////////////////////////////////////////////////////////////////////////

struct CallRecordCopy
{
    uint64_t           position = 0;
    uint64_t           cycle    = 0;
    uint64_t           checksum = 0;
    std::vector<Byte>  packed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRebuildJob
//
//  Everything a rebuild of the call record from history needs, copied out on
//  the thread that runs the machine so the replay can run on another: the
//  parts, oldest first, the last ending at the job's target; the
//  recorded inputs from the first part on, with the index of the first; the
//  disks in the bays now, which every part that loads a keyframe must have
//  been saved with; and the keyframes, oldest first, to copy the record at.
//  A job that continues takes up the second machine and its record where
//  the job before it, of the same generation, left them.
//
////////////////////////////////////////////////////////////////////////////////

struct CallStackRebuildJob
{
    uint64_t                           generation  = 0;
    bool                               isContinued = false;
    std::vector<CallStackRebuildPart>  parts;
    size_t                             inputsFrom  = 0;
    std::vector<InputRecord>           inputs;
    std::vector<ReplayDisk>            disks;
    std::vector<CallRecordCopy>        copyAt;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRebuildResult
//
//  Where a rebuild's replay stopped -- its position, the cycle count and the
//  registers there, and the journal index a job continuing it goes on from --
//  and the record as of there, the last instruction settled. recordFrom is
//  the position the record starts at: the oldest keyframe replayed, or a
//  later one where the record had to start again; for a seeded record, the
//  seed's seedFrom. Also the copies of the record made at the job's
//  keyframes, how many instructions it replayed, how long it took, and
//  whether it worked.
//
////////////////////////////////////////////////////////////////////////////////

struct CallStackRebuildResult
{
    uint64_t                     generation    = 0;
    HRESULT                      hr            = S_OK;
    uint64_t                     position      = 0;
    uint64_t                     cycle         = 0;
    Cpu6502Registers             registers     = {};
    size_t                       journalCursor = 0;
    CallRecord                   record;
    uint64_t                     recordFrom    = 0;
    std::vector<CallRecordCopy>  copies;
    uint64_t                     instructions  = 0;
    double                       ms            = 0.0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICallStackRebuilder
//
//  Rebuilds the call record away from the thread that runs the machine, one
//  job at a time. A job submitted while another waits replaces it, and one
//  running is abandoned at its next check, so only the newest finishes.
//  Results are taken on the machine's thread. Rebuild runs a job on the
//  calling thread instead, once the job running has finished. Progress is
//  how much of the newest job has been replayed, 0 to 1; a job abandoned or
//  cancelled shows none.
//
////////////////////////////////////////////////////////////////////////////////

class ICallStackRebuilder
{
public:
    virtual          ~ICallStackRebuilder() = default;

    virtual HRESULT  Submit        (std::shared_ptr<const CallStackRebuildJob> job) = 0;
    virtual bool     TryTakeResult (CallStackRebuildResult & outResult) = 0;
    virtual void     Cancel        () = 0;
    virtual HRESULT  Rebuild       (const CallStackRebuildJob & job, CallStackRebuildResult & outResult) = 0;
    virtual float    GetProgress   () const = 0;
};
