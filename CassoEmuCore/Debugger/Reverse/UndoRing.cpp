#include "Pch.h"

#include "Debugger/Reverse/UndoRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Configure
//
//  Sets the checkpoint spacing and budget, and empties the ring. The record
//  capacity is sized now for the fewest checkpoints the ring may keep, and
//  grows once the first checkpoint gives the state's size.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::Configure (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles)
{
    m_settings                  = settings;
    m_settings.checkpointCycles = GetSpacing (settings, keyframeIntervalCycles);
    m_keyframeInterval          = keyframeIntervalCycles;

    m_stateBytes      = 0;
    m_checkpointLimit = GetCheckpointLimit (m_settings, m_keyframeInterval, 0);

    Clear();
    m_spareStates.clear();
    m_records.clear();
    ResizeRecords (GetRecordCapacity (m_checkpointLimit));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::Clear()
{
    while (!m_checkpoints.empty())
    {
        DropOldestCheckpoint();
    }

    m_head                = 0;
    m_count               = 0;
    m_firstPosition       = 0;
    m_nextCheckpointCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCheckpointLimit
//
//  One checkpoint more than the interval needs, so a target anywhere in the
//  oldest interval still has a checkpoint at or before it. Each checkpoint
//  costs its state and the records of one interval. A stateBytes of zero
//  gives the minimum alone.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetCheckpointLimit (
    const UndoRingSettings  & settings,
    uint64_t                  keyframeIntervalCycles,
    size_t                    stateBytes)
{
    uint64_t  spacing    = GetSpacing (settings, keyframeIntervalCycles);
    size_t    minimum    = static_cast<size_t> ((keyframeIntervalCycles + spacing - 1) / spacing) + 1;
    size_t    perEntry   = stateBytes + GetRecordBytesPerInterval (spacing);
    size_t    affordable = (stateBytes != 0) ? settings.budgetBytes / perEntry : 0;



    return std::max (minimum, affordable);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMinimumBudget
//
//  The smallest budget that still covers one keyframe interval, for the
//  setting that must not go below it.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetMinimumBudget (
    const UndoRingSettings  & settings,
    uint64_t                  keyframeIntervalCycles,
    size_t                    stateBytes)
{
    UndoRingSettings  unbudgeted = settings;
    uint64_t          spacing    = GetSpacing (settings, keyframeIntervalCycles);



    unbudgeted.budgetBytes = 0;

    return GetCheckpointLimit (unbudgeted, keyframeIntervalCycles, stateBytes) * (stateBytes + GetRecordBytesPerInterval (spacing));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSpacing
//
//  The checkpoint spacing in cycles: the setting, or the default for zero,
//  and never more than a keyframe interval, past which a checkpoint saves
//  nothing a keyframe does not.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t UndoRing::GetSpacing (
    const UndoRingSettings  & settings,
    uint64_t                  keyframeIntervalCycles)
{
    uint64_t  spacing = (settings.checkpointCycles != 0) ? settings.checkpointCycles : UndoRingSettings::kDefaultCheckpointCycles;



    if (keyframeIntervalCycles != 0)
    {
        spacing = std::min (spacing, keyframeIntervalCycles);
    }

    return spacing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecordBytesPerInterval
//
//  Enough records for every instruction of one checkpoint interval, each
//  taking at least two cycles; an interval can run long by the length of its
//  last instruction.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetRecordBytesPerInterval (uint64_t spacing)
{
    return static_cast<size_t> ((spacing + kMaxInstructionCycles) / kMinInstructionCycles) * sizeof (UndoRecord);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushSlow
//
//  Push when the record does not simply go on the end. A position that does
//  not continue the ring starts it over, since the ring only ever holds one
//  contiguous range; when it is full the oldest record goes.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::PushSlow (uint64_t position, const UndoRecord & record)
{
    size_t  capacity = m_records.size();



    if (capacity == 0)
    {
        return;
    }

    if (position != GetEndPosition())
    {
        m_head          = 0;
        m_count         = 0;
        m_firstPosition = position;
    }

    if (m_count == capacity)
    {
        m_head = (m_head + 1) % capacity;
        m_count--;
        m_firstPosition++;
    }

    m_records[(m_head + m_count) % capacity] = record;
    m_count++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddCheckpoint
//
//  The machine's whole state at the boundary before the instruction at
//  position, as its own bytes and the segments it shares. The first
//  checkpoint sizes the ring for the whole state, segments counted; past the
//  limit the oldest checkpoint goes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UndoRing::AddCheckpoint (
    uint64_t                      position,
    uint64_t                      cycle,
    size_t                        journalIndex,
    std::vector<Byte>          && state,
    std::vector<StateSegment>  && segments)
{
    HRESULT         hr         = S_OK;
    bool            isLater    = m_checkpoints.empty() || position > m_checkpoints.back().position;
    size_t          stateBytes = state.size();
    UndoCheckpoint  checkpoint;



    CBRAEx (isLater, E_INVALIDARG);

    for (const StateSegment & segment : segments)
    {
        stateBytes += segment.bytes->size();
    }

    if (stateBytes > m_stateBytes)
    {
        SizeFor (stateBytes);
    }

    while (m_checkpoints.size() >= m_checkpointLimit)
    {
        DropOldestCheckpoint();
    }

    checkpoint.position     = position;
    checkpoint.cycle        = cycle;
    checkpoint.journalIndex = journalIndex;
    checkpoint.state        = std::move (state);
    checkpoint.segments     = std::move (segments);

    // A buffer new to the ring grew by doubling; it is reused from here on,
    // so trim it once rather than carry the slack in every checkpoint.
    checkpoint.state.shrink_to_fit();

    m_checkpoints.push_back (std::move (checkpoint));

    m_nextCheckpointCycle = cycle + m_settings.checkpointCycles;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeSpareBuffer
//
//  The buffer of a dropped checkpoint, for the next state to be saved into,
//  or an empty one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> UndoRing::TakeSpareBuffer()
{
    std::vector<Byte>  buffer;



    if (!m_spareStates.empty())
    {
        buffer = std::move (m_spareStates.back());
        m_spareStates.pop_back();
    }

    return buffer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateAt
//
//  Drops every record at or after position and every checkpoint after it:
//  the machine was put back to position, and the ring must end where the
//  machine is. A checkpoint at position itself stays, being the state the
//  machine was just given.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::TruncateAt (uint64_t position)
{
    Truncate (position, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateFrom
//
//  As TruncateAt, but a checkpoint at position goes too: the machine was
//  changed there, so the state it holds is no longer what the machine had.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::TruncateFrom (uint64_t position)
{
    Truncate (position, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Truncate
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::Truncate (
    uint64_t  position,
    bool      keepCheckpointAt)
{
    while (!m_checkpoints.empty() && (m_checkpoints.back().position > position || (!keepCheckpointAt && m_checkpoints.back().position == position)))
    {
        m_spareStates.push_back (std::move (m_checkpoints.back().state));
        m_checkpoints.pop_back();
    }

    if (position < m_firstPosition)
    {
        m_head          = 0;
        m_count         = 0;
        m_firstPosition = position;
    }
    else if (position < GetEndPosition())
    {
        m_count = static_cast<size_t> (position - m_firstPosition);
    }

    m_nextCheckpointCycle = m_checkpoints.empty() ? 0 : m_checkpoints.back().cycle + m_settings.checkpointCycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetRecord
//
////////////////////////////////////////////////////////////////////////////////

bool UndoRing::TryGetRecord (
    uint64_t      position,
    UndoRecord  & outRecord) const
{
    bool  isHeld = position >= m_firstPosition && position < GetEndPosition();



    if (isHeld)
    {
        outRecord = m_records[(m_head + static_cast<size_t> (position - m_firstPosition)) % m_records.size()];
    }

    return isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindCheckpointAtOrBefore
//
//  The newest checkpoint whose position is at or before position.
//
////////////////////////////////////////////////////////////////////////////////

bool UndoRing::TryFindCheckpointAtOrBefore (
    uint64_t   position,
    size_t   & outIndex) const
{
    std::deque<UndoCheckpoint>::const_iterator  after = std::upper_bound (m_checkpoints.begin(),
                                                                          m_checkpoints.end(),
                                                                          position,
                                                                          [] (uint64_t target, const UndoCheckpoint & entry) { return target < entry.position; });
    bool                                        found = after != m_checkpoints.begin();



    if (found)
    {
        outIndex = static_cast<size_t> (std::distance (m_checkpoints.begin(), after)) - 1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindCheckpointByCycle
//
//  The newest checkpoint whose cycle is at or before cycle.
//
////////////////////////////////////////////////////////////////////////////////

bool UndoRing::TryFindCheckpointByCycle (
    uint64_t   cycle,
    size_t   & outIndex) const
{
    std::deque<UndoCheckpoint>::const_iterator  after = std::upper_bound (m_checkpoints.begin(),
                                                                          m_checkpoints.end(),
                                                                          cycle,
                                                                          [] (uint64_t target, const UndoCheckpoint & entry) { return target < entry.cycle; });
    bool                                        found = after != m_checkpoints.begin();



    if (found)
    {
        outIndex = static_cast<size_t> (std::distance (m_checkpoints.begin(), after)) - 1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetByteCount
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetByteCount() const
{
    size_t                  bytes    = m_records.size() * sizeof (UndoRecord);
    const UndoCheckpoint  * previous = nullptr;
    size_t                  i        = 0;
    bool                    isShared = false;



    for (const UndoCheckpoint & checkpoint : m_checkpoints)
    {
        bytes += checkpoint.state.capacity();

        for (i = 0; i < checkpoint.segments.size(); i++)
        {
            isShared = previous != nullptr
                       && i < previous->segments.size()
                       && previous->segments[i].bytes == checkpoint.segments[i].bytes;

            bytes += isShared ? 0 : checkpoint.segments[i].bytes->size();
        }

        previous = &checkpoint;
    }

    for (const std::vector<Byte> & spare : m_spareStates)
    {
        bytes += spare.capacity();
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeFor
//
//  Sets the checkpoint limit for a state of stateBytes and grows the record
//  capacity to cover every checkpoint interval the limit allows.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::SizeFor (size_t stateBytes)
{
    size_t  capacity = 0;



    m_stateBytes      = stateBytes;
    m_checkpointLimit = GetCheckpointLimit (m_settings, m_keyframeInterval, stateBytes);
    capacity          = GetRecordCapacity (m_checkpointLimit);

    if (capacity > m_records.size())
    {
        ResizeRecords (capacity);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecordCapacity
//
//  Records for every instruction across checkpointLimit intervals, and one
//  more interval for the one being filled.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetRecordCapacity (size_t checkpointLimit) const
{
    return (checkpointLimit + 1) * GetRecordBytesPerInterval (m_settings.checkpointCycles) / sizeof (UndoRecord);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResizeRecords
//
//  Reallocates the record buffer, keeping the records held, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::ResizeRecords (size_t capacity)
{
    std::vector<UndoRecord>  records (capacity);
    size_t                   kept    = std::min (m_count, capacity);
    size_t                   skipped = m_count - kept;
    size_t                   i       = 0;



    for (i = 0; i < kept; i++)
    {
        records[i] = m_records[(m_head + skipped + i) % m_records.size()];
    }

    m_records        = std::move (records);
    m_head           = 0;
    m_count          = kept;
    m_firstPosition += skipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOldestCheckpoint
//
//  Its buffer is kept for the next checkpoint, so a ring at its limit stops
//  allocating.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::DropOldestCheckpoint()
{
    m_spareStates.push_back (std::move (m_checkpoints.front().state));
    m_checkpoints.pop_front();
}
