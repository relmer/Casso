#pragma once

#include "Pch.h"





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
//  journal's end index at that moment.
//
////////////////////////////////////////////////////////////////////////////////

struct UndoCheckpoint
{
    uint64_t           position     = 0;
    uint64_t           cycle        = 0;
    size_t             journalIndex = 0;
    std::vector<Byte>  state;
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

    //  Once per instruction, so the common case is inline: the next position,
    //  with room.
    void      Push (uint64_t position, const UndoRecord & record)
    {
        size_t  slot = m_head + m_count;



        if (position != m_firstPosition + m_count || m_count == m_records.size())
        {
            PushSlow (position, record);
            return;
        }

        if (slot >= m_records.size())
        {
            slot -= m_records.size();
        }

        m_records[slot] = record;
        m_count++;
    }

    bool               IsCheckpointDue        (uint64_t cycle) const { return m_checkpoints.empty() || cycle >= m_nextCheckpointCycle; }
    HRESULT            AddCheckpoint          (uint64_t position, uint64_t cycle, size_t journalIndex, std::vector<Byte> && state);
    std::vector<Byte>  TakeSpareBuffer        ();
    void               TruncateAt             (uint64_t position);
    void               TruncateFrom           (uint64_t position);
    uint64_t           GetNextCheckpointCycle () const { return m_checkpoints.empty() ? 0 : m_nextCheckpointCycle; }

    bool      TryGetRecord                (uint64_t position, UndoRecord & outRecord) const;
    bool      TryFindCheckpointAtOrBefore (uint64_t position, size_t & outIndex) const;
    bool      TryFindCheckpointByCycle    (uint64_t cycle, size_t & outIndex) const;

    uint64_t                  GetFirstPosition   () const { return m_firstPosition; }
    uint64_t                  GetEndPosition     () const { return m_firstPosition + m_count; }
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
    size_t                          m_head                = 0;      // slot of m_firstPosition
    size_t                          m_count               = 0;
    uint64_t                        m_firstPosition       = 0;
    uint64_t                        m_nextCheckpointCycle = 0;
};
