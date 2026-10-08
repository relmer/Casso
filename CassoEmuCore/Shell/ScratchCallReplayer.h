#pragma once

#include "Pch.h"

#include "Core/IWatchSink.h"
#include "Core/IWorkQueue.h"
#include "Core/MachineConfig.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Cpu.h"
#include "Debugger/CallStack.h"
#include "Debugger/CallStackRebuildJob.h"
#include "Debugger/DebugMemoryView.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Shell/MachineHost.h"
#include "Shell/ScratchReplayMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer
//
//  Rebuilds the debugger's call record by replaying history on a second
//  machine of its own (ScratchReplayMachine), so the running machine, its
//  sound and its picture are never touched and nobody waits on it. A
//  recorder of its own is fed by the second machine as the debugger's is fed
//  by the running one: the CPU reports the opcodes the record needs, and
//  every write to the stack page is offered to it, through the bus's watch
//  sink, which a replay hands to its stop test.
//
//  Each part of a job loads its keyframe over the job's disks, once the
//  keyframe is found to have been saved with those very disks, and replays
//  to its end with the recorded inputs, as reverse execution replays them.
//  The record begins at the first part loaded -- at power-on when that
//  keyframe is at cycle 0, and otherwise at the start of history -- and
//  begins again after a gap, or after a part whose disks were others; a
//  fresh job starts at the newest part that begins it again, since the
//  record keeps nothing from the parts before. A keyframe loaded on the way
//  is a change from outside, and the record goes on across it. A reset or
//  power cycle in the replay reaches the record as one on the running
//  machine reaches the debugger's. A job that continues starts where the
//  last left the machine and the record.
//
//  Jobs run one at a time on a pool thread of its own, or on a queue a test
//  hands in, and are abandoned within about kChunkCycles of replay when a
//  newer one comes in or they are cancelled.
//
//  SetMachine is called on the thread that builds the running machine;
//  Submit, TryTakeResult, Cancel and Rebuild on the thread that runs it;
//  the replay on one worker at a time, or in Rebuild.
//
////////////////////////////////////////////////////////////////////////////////

class ScratchCallReplayer : public ICallStackRebuilder,
                            public IOpcodeWatcher,
                            public IReverseStopTest,
                            public IWatchSink
{
public:
    //  Cycles replayed between checks for a newer job, about ten
    //  milliseconds of replay.
    static constexpr uint64_t  kChunkCycles = 1'000'000;

                ScratchCallReplayer  ();
                ~ScratchCallReplayer () override;

    ScratchCallReplayer             (const ScratchCallReplayer &) = delete;
    ScratchCallReplayer & operator= (const ScratchCallReplayer &) = delete;

    void        SetMachine     (const MachineConfig & config, const std::wstring & name);
    void        SetWorkQueue   (IWorkQueue * queue) { m_queue = queue; }
    void        WaitForWork    ();

    // ICallStackRebuilder
    HRESULT     Submit         (std::shared_ptr<const CallStackRebuildJob> job) override;
    bool        TryTakeResult  (CallStackRebuildResult & outResult) override;
    void        Cancel         () override;
    HRESULT     Rebuild        (const CallStackRebuildJob & job, CallStackRebuildResult & outResult) override;
    float       GetProgress    () const override { return m_progress.load (std::memory_order_relaxed); }

    // IOpcodeWatcher: the second machine's CPU, for the record.
    void        OnWatchedFetch (Word pc, Byte sp, Byte opcode) override;

    // IReverseStopTest: nothing stops a rebuild's replay; the watch sink is
    // where its stack writes come.
    bool          ShouldStopBefore (MachineHost & machine, Word pc) override;
    bool          TakePendingStop  () override;
    IWatchSink  * GetWatchSink     () override { return this; }

    // IWatchSink
    void        OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override;

private:
    static constexpr Word  kStackPage = 0x01;
    static constexpr int   kPageShift = 8;

    static void RunJob          (void * context);
    void        RunPending      ();
    HRESULT     Run             (const CallStackRebuildJob & job, CallStackRebuildResult & outResult);
    HRESULT     Start           (const CallStackRebuildJob & job);
    HRESULT     FindStartPart   (const CallStackRebuildJob & job, size_t & outStart);
    HRESULT     ReadPartMedia   (const CallStackRebuildPart & part, MachineHost::MediaIds & outSaved);
    HRESULT     RunPart         (const CallStackRebuildJob & job, const CallStackRebuildPart & part);
    HRESULT     LoadPart        (const CallStackRebuildJob & job, const CallStackRebuildPart & part, bool & outIsLoaded);
    HRESULT     ReplayPart      (const CallStackRebuildJob & job, const CallStackRebuildPart & part);
    void        Finish          (CallStackRebuildResult & outResult);
    void        AttachRecorder  ();
    void        DetachRecorder  ();
    void        SettleRecorder  ();
    void        OnReplayedReset (bool isPowerCycle);
    Byte        PeekByte        (Word address) const;
    HRESULT     UseQueue        ();
    bool        IsAbandoned     (const CallStackRebuildJob & job) const;

    static bool IsSavedWith     (const MachineHost::MediaIds & saved, const std::vector<ReplayDisk> & disks);

    //  The jobs, between the threads.
    std::mutex                                   m_lock;
    std::shared_ptr<const CallStackRebuildJob>   m_next;
    std::deque<CallStackRebuildResult>           m_results;
    bool                                         m_isRunning  = false;
    std::atomic<uint64_t>                        m_latest     = 0;      // the generation still wanted, 0 for none
    std::atomic<float>                           m_progress   = 0.0f;

    ThreadPoolWorkQueue                          m_ownQueue;
    IWorkQueue                                 * m_queue      = nullptr;

    //  The replaying thread's own.
    ScratchReplayMachine                         m_scratch;
    std::unique_ptr<DebugMemoryView>             m_view;
    KeyframeUnpacker                             m_unpacker;
    std::vector<Byte>                            m_state;
    CallStackRecorder                            m_recorder;
    std::array<bool, 0x100>                      m_opcodes      = {};
    uint64_t                                     m_ongoing      = 0;         // the generation the machine and record were left for, 0 for none
    bool                                         m_isBegun      = false;     // the record has begun
    bool                                         m_isRestartDue = false;     // a part was passed over: the next one loaded starts the record again
    uint64_t                                     m_recordFrom   = 0;
    uint64_t                                     m_jobStart     = 0;
    uint64_t                                     m_jobEnd       = 0;
};
