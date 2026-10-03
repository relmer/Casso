#pragma once

#include "Pch.h"

#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRingSettings
//
//  How often the ring takes a checkpoint, and how much memory it may use.
//  The ring never holds less than one keyframe interval (see
//  UndoRing::Configure), whatever the budget.
//
////////////////////////////////////////////////////////////////////////////////

struct UndoRingSettings
{
    static constexpr uint64_t  kDefaultCheckpointCycles = 17030;     // one video frame
    static constexpr size_t    kDefaultBudgetBytes      = 32ull * 1024 * 1024;

    uint64_t  checkpointCycles = kDefaultCheckpointCycles;
    size_t    budgetBytes      = kDefaultBudgetBytes;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRecord
//
//  The registers and cycle count one instruction began with.
//
////////////////////////////////////////////////////////////////////////////////

struct UndoRecord
{
    uint64_t  cycle = 0;
    Word      pc    = 0;
    Byte      a     = 0;
    Byte      x     = 0;
    Byte      y     = 0;
    Byte      sp    = 0;
    Byte      p     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UndoCheckpoint
//
//  The whole machine at one instruction boundary, unpacked, with the input
//  journal's end index at that moment. Taken by a sharing StateWriter, the
//  state is its own bytes and the segments it shares with other checkpoints
//  (see StateWriter::Flatten).
//
////////////////////////////////////////////////////////////////////////////////

struct UndoCheckpoint
{
    uint64_t                   position     = 0;
    uint64_t                   cycle        = 0;
    size_t                     journalIndex = 0;
    std::vector<Byte>          state;
    std::vector<StateSegment>  segments;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRing
//
//  The recent half of reverse execution's timeline. It holds the registers
//  every instruction began with, over a contiguous range of positions ending
//  where the machine is, and a whole-machine checkpoint every few thousand
//  cycles across the same range. Stepping back inside the ring loads the
//  checkpoint at or before the target and replays at most one checkpoint
//  interval, so no step back inside it waits on a keyframe's replay.
//
//  Checkpoints are whole states rather than per-write undo entries because
//  the Disk II and Mockingboard change on every instruction through their
//  cycle ticks, with no bus access to journal; loading a checkpoint is the
//  same LoadState a keyframe uses, so what the ring restores is exactly what
//  a replay would compute.
//
//  The register records are what step back over and step back out read to
//  find their target without replaying anything.
//
////////////////////////////////////////////////////////////////////////////////

class UndoRing
{
public:
    void      Configure (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles);
    void      Clear     ();

    //  The checkpoint count the ring keeps for a state of stateBytes: what the
    //  budget buys, counting the records each interval needs, but never fewer
    //  than cover one keyframe interval.
    static size_t  GetCheckpointLimit (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles, size_t stateBytes);
    static size_t  GetMinimumBudget   (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles, size_t stateBytes);

    //  Once per instruction, so the common case is inline and has one store
    //  to make: the next position goes in the slot after the newest, over
    //  the oldest record once the ring is full, which it is for nearly all
    //  of a long recording.
    void      Push (uint64_t position, const UndoRecord & record)
    {
        if (position != m_endPosition || m_capacity == 0)
        {
            PushSlow (position, record);
            return;
        }

        m_records[m_write] = record;

        m_write = (m_write + 1 == m_capacity) ? 0 : m_write + 1;
        m_endPosition++;
    }

    bool               IsCheckpointDue        (uint64_t cycle) const { return m_checkpoints.empty() || cycle >= m_nextCheckpointCycle; }
    HRESULT            AddCheckpoint          (uint64_t position, uint64_t cycle, size_t journalIndex, std::vector<Byte> && state) { return AddCheckpoint (position, cycle, journalIndex, std::move (state), {}); }
    HRESULT            AddCheckpoint          (uint64_t position, uint64_t cycle, size_t journalIndex, std::vector<Byte> && state, std::vector<StateSegment> && segments);
    std::vector<Byte>  TakeSpareBuffer        ();
    void               ReturnSpareBuffer      (std::vector<Byte> && buffer);
    void               TruncateAt             (uint64_t position);
    void               TruncateFrom           (uint64_t position);
    uint64_t           GetNextCheckpointCycle () const { return m_checkpoints.empty() ? 0 : m_nextCheckpointCycle; }

    bool      TryGetRecord                (uint64_t position, UndoRecord & outRecord) const;
    bool      TryFindCheckpointAtOrBefore (uint64_t position, size_t & outIndex) const;
    bool      TryFindCheckpointByCycle    (uint64_t cycle, size_t & outIndex) const;

    uint64_t                  GetFirstPosition   () const { return m_endPosition - GetRecordCount(); }
    uint64_t                  GetEndPosition     () const { return m_endPosition; }
    size_t                    GetCheckpointCount () const { return m_checkpoints.size(); }
    size_t                    GetCheckpointLimit () const { return m_checkpointLimit; }
    const UndoCheckpoint    & GetCheckpoint      (size_t index) const { return m_checkpoints[index]; }
    size_t                    GetByteCount       () const;
    const UndoRingSettings  & GetSettings        () const { return m_settings; }

private:
    static constexpr uint64_t  kMinInstructionCycles = 2;
    static constexpr uint64_t  kMaxInstructionCycles = 8;

    static uint64_t  GetSpacing                (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles);
    static size_t    GetRecordBytesPerInterval (uint64_t spacing);

    void      PushSlow             (uint64_t position, const UndoRecord & record);
    size_t    GetRecordCount       () const { return static_cast<size_t> (std::min<uint64_t> (m_endPosition - m_startPosition, m_capacity)); }
    size_t    GetSlot              (uint64_t position) const;
    void      SizeFor              (size_t stateBytes);
    size_t    GetRecordCapacity    (size_t checkpointLimit) const;
    void      ResizeRecords        (size_t capacity);
    void      DropOldestCheckpoint ();
    void      Truncate             (uint64_t position, bool keepCheckpointAt);

    UndoRingSettings                m_settings;
    uint64_t                        m_keyframeInterval    = 0;
    size_t                          m_checkpointLimit     = 0;
    size_t                          m_stateBytes          = 0;
    std::deque<UndoCheckpoint>      m_checkpoints;
    std::vector<std::vector<Byte>>  m_spareStates;                  // buffers of dropped checkpoints, for reuse
    std::vector<UndoRecord>         m_records;
    size_t                          m_capacity            = 0;      // m_records.size()
    size_t                          m_write               = 0;      // slot of m_endPosition
    uint64_t                        m_startPosition       = 0;      // where the run of records began; older ones may be overwritten
    uint64_t                        m_endPosition         = 0;
    uint64_t                        m_nextCheckpointCycle = 0;
};
