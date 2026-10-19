#pragma once

#include "Pch.h"

#include "Cpu.h"
#include "Debugger/Events/DebugEventJob.h"
#include "Debugger/ProfileTable.h"
#include "Debugger/RoutineProfile.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileCounts
//
//  What the profiler counts: the opcode, penalty and address counts, and the
//  routines with their call tree and frames.
//
////////////////////////////////////////////////////////////////////////////////

struct ProfileCounts
{
    ProfileTable    table;
    RoutineProfile  routines;
};

using ProfileCountsPtr = std::shared_ptr<const ProfileCounts>;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEventProbe
//
//  The profiler as a rebuild's probe. While a stretch replays, each fetch
//  bills the instruction before it, as the live profiler does: its cycles
//  are the count at this fetch less the count at its own, and the stack
//  pointer after it is this fetch's. Only an instruction that starts within
//  the period counts. The counts add up across a job's stretches, and each
//  stretch's result is one entry with the counts so far, so the last entry
//  of a job has them all.
//
//  The share of the period replayed so far can be read from any thread.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileEventProbe : public IDebugEventProbe<ProfileCountsPtr>, public IOpcodeWatcher
{
public:
    ProfileEventProbe();

    //  The period, in CPU cycles, and the counts started again. Only while
    //  no job runs.
    void     Begin       (uint64_t fromCycle, uint64_t toCycle);
    double   GetProgress () const { return m_progress.load (std::memory_order_relaxed); }

    HRESULT  Attach (MachineHost & machine, std::vector<DebugEventEntry<ProfileCountsPtr>> & entries) override;
    void     Detach (MachineHost & machine) override;

    // IOpcodeWatcher
    void     OnWatchedFetch (Word pc, Byte sp, Byte opcode) override;

private:
    struct Pending
    {
        Word      pc         = 0;
        Byte      opcode     = 0;
        uint64_t  startCycle = 0;
    };

    static constexpr uint32_t  kProgressEvery = 4096;    // instructions between progress updates

    void  Bill (uint64_t cycle, Byte spAfter, Byte penalties);

    std::array<bool, 256>                               m_opcodes;
    ProfileCounts                                       m_counts;
    std::optional<Pending>                              m_pending;
    MachineHost                                       * m_machine   = nullptr;
    std::vector<DebugEventEntry<ProfileCountsPtr>>    * m_entries   = nullptr;
    uint64_t                                            m_fromCycle = 0;
    uint64_t                                            m_toCycle   = 0;
    uint32_t                                            m_untilNote = 0;
    std::atomic<double>                                 m_progress  { 0.0 };
};
