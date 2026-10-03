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
//  A keyframe interval or spacing whose records for one interval would pass
//  kMaxRecordBytes is rejected and leaves the ring as it was: the ring never
//  keeps fewer records than that, so it could not be allocated.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UndoRing::Configure (const UndoRingSettings & settings, uint64_t keyframeIntervalCycles)
{
    HRESULT   hr        = S_OK;
    uint64_t  spacing   = GetSpacing (settings, keyframeIntervalCycles);
    uint64_t  intervals = 0;
    uint64_t  perSpace  = 0;



    CBREx (spacing <= kMaxRecordBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    // The minimum checkpoint count, plus the interval being filled.
    intervals = keyframeIntervalCycles / spacing + ((keyframeIntervalCycles % spacing != 0) ? 1 : 0) + 2;
    perSpace  = (spacing + kMaxInstructionCycles) / kMinInstructionCycles * sizeof (UndoRecord);

    CBREx (perSpace <= kMaxRecordBytes / intervals, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_settings                  = settings;
    m_settings.checkpointCycles = GetSpacing (settings, keyframeIntervalCycles);
    m_keyframeInterval          = keyframeIntervalCycles;

    m_stateBytes      = 0;
    m_checkpointLimit = GetCheckpointLimit (m_settings, m_keyframeInterval, 0);

    Clear();
    m_spareStates.clear();
    m_spareSegments.clear();
    m_records.clear();

    m_largestOwnBytes = 0;

    ResizeRecords (GetRecordCapacity (m_checkpointLimit));

Error:
    return hr;
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

    m_heldBytes           = 0;
    m_write               = 0;
    m_startPosition       = 0;
    m_endPosition         = 0;
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
//  Push when the record does not continue the ring: it starts the ring over,
//  since the ring only ever holds one contiguous range. A ring with no room
//  keeps nothing.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::PushSlow (uint64_t position, const UndoRecord & record)
{
    if (m_capacity == 0)
    {
        return;
    }

    m_records[0] = record;

    m_write         = (m_capacity == 1) ? 0 : 1;
    m_startPosition = position;
    m_endPosition   = position + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddCheckpoint
//
//  The machine's whole state at the boundary before the instruction at
//  position, as its own bytes and the segments it shares. A checkpoint holds
//  its own bytes and the segments the one before it does not share, which
//  for a sharing save is the RAM chunks and disk tracks written since then.
//  The first checkpoint, which holds everything, sizes the ring; past the
//  limit, or past the budget counting what the checkpoints actually hold,
//  the oldest goes, and once the ring is full the limit grows when what they
//  hold leaves room for more.
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
    UndoCheckpoint  checkpoint;



    CBRAEx (isLater, E_INVALIDARG);

    checkpoint.position     = position;
    checkpoint.cycle        = cycle;
    checkpoint.journalIndex = journalIndex;
    checkpoint.state        = std::move (state);
    checkpoint.segments     = std::move (segments);
    checkpoint.heldBytes    = GetHeldBytes (checkpoint, m_checkpoints.empty() ? nullptr : &m_checkpoints.back());

    if (checkpoint.heldBytes > m_stateBytes)
    {
        SizeFor (checkpoint.heldBytes);
    }

    while (m_checkpoints.size() >= m_checkpointLimit)
    {
        DropOldestCheckpoint();
    }

    m_largestOwnBytes  = std::max (m_largestOwnBytes, checkpoint.state.size());
    m_heldBytes       += checkpoint.heldBytes;

    m_checkpoints.push_back (std::move (checkpoint));

    DropOverBudget();
    GrowLimitIfRoom();

    ScheduleAfter (cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleAfter
//
//  The next checkpoint falls due on the first multiple of the spacing after
//  cycle, as keyframes fall due on multiples of their interval, so where the
//  interval is a multiple of the spacing every keyframe falls due on the
//  same instruction boundary as a checkpoint and one save serves both.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::ScheduleAfter (uint64_t cycle)
{
    uint64_t  spacing = std::max<uint64_t> (m_settings.checkpointCycles, 1);



    m_nextCheckpointCycle = (cycle / spacing + 1) * spacing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeSpareBuffer
//
//  The buffer of a dropped checkpoint, for the next state to be saved into,
//  or a new one reserved to the largest checkpoint held so far, so it does
//  not grow by doubling and carry the slack for as long as it is reused.
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
    else
    {
        buffer.reserve (m_largestOwnBytes);
    }

    return buffer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeSpareSegments
//
//  The emptied segment list of a dropped checkpoint, or an empty one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<StateSegment> UndoRing::TakeSpareSegments()
{
    std::vector<StateSegment>  segments;



    if (!m_spareSegments.empty())
    {
        segments = std::move (m_spareSegments.back());
        m_spareSegments.pop_back();
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReturnSpareBuffer
//
//  A buffer taken for a save that made no checkpoint, back into the pool.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::ReturnSpareBuffer (std::vector<Byte> && buffer)
{
    if (buffer.capacity() != 0)
    {
        m_spareStates.push_back (std::move (buffer));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReturnSpareBuffer (with a segment list)
//
//  As ReturnSpareBuffer, and the save's segment list as well, emptied so the
//  tracks it shared are not kept alive by the pool.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::ReturnSpareBuffer (
    std::vector<Byte>          && buffer,
    std::vector<StateSegment>  && segments)
{
    ReturnSpareBuffer (std::move (buffer));

    if (segments.capacity() != 0)
    {
        segments.clear();
        m_spareSegments.push_back (std::move (segments));
    }
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
        m_heldBytes -= m_checkpoints.back().heldBytes;

        KeepSpares (m_checkpoints.back());
        m_checkpoints.pop_back();
    }

    if (position < GetFirstPosition())
    {
        m_write         = 0;
        m_startPosition = position;
        m_endPosition   = position;
    }
    else if (position < m_endPosition)
    {
        // The slots before the oldest record held now go to the dropped
        // future, so the run starts no earlier than that record.
        m_startPosition = GetFirstPosition();
        m_write         = GetSlot (position);
        m_endPosition   = position;
    }

    if (m_checkpoints.empty())
    {
        m_nextCheckpointCycle = 0;
    }
    else
    {
        ScheduleAfter (m_checkpoints.back().cycle);
    }
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
    bool  isHeld = position >= GetFirstPosition() && position < m_endPosition;



    if (isHeld)
    {
        outRecord = m_records[GetSlot (position)];
    }

    return isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSlot
//
//  The slot of position, which is at most one ring's length before the end;
//  the end position's own slot is m_write.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetSlot (uint64_t position) const
{
    size_t  back = static_cast<size_t> (m_endPosition - position);



    return (m_write >= back) ? m_write - back : m_write + m_capacity - back;
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
    size_t                   kept    = std::min (GetRecordCount(), capacity);
    uint64_t                 first   = m_endPosition - kept;
    size_t                   i       = 0;



    for (i = 0; i < kept; i++)
    {
        records[i] = m_records[GetSlot (first + i)];
    }

    m_records       = std::move (records);
    m_capacity      = capacity;
    m_write         = (kept == capacity) ? 0 : kept;
    m_startPosition = first;
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
    UndoCheckpoint  * front = nullptr;
    size_t            held  = 0;



    m_heldBytes -= m_checkpoints.front().heldBytes;

    KeepSpares (m_checkpoints.front());
    m_checkpoints.pop_front();

    // The new oldest now holds alone what it shared with the one dropped.
    if (!m_checkpoints.empty())
    {
        front = &m_checkpoints.front();
        held  = GetHeldBytes (*front, nullptr);

        m_heldBytes      += held - front->heldBytes;
        front->heldBytes  = held;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHeldBytes
//
//  What a checkpoint holds that the one before it does not: its own bytes,
//  and each segment that is not the very buffer the one before holds in the
//  same place. With no checkpoint before it, every segment.
//
////////////////////////////////////////////////////////////////////////////////

size_t UndoRing::GetHeldBytes (
    const UndoCheckpoint  & checkpoint,
    const UndoCheckpoint  * previous)
{
    size_t  held     = checkpoint.state.capacity();
    size_t  i        = 0;
    bool    isShared = false;



    for (i = 0; i < checkpoint.segments.size(); i++)
    {
        isShared = previous != nullptr
                   && i < previous->segments.size()
                   && previous->segments[i].bytes == checkpoint.segments[i].bytes;

        held += isShared ? 0 : checkpoint.segments[i].bytes->size();
    }

    return held;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOverBudget
//
//  Drops the oldest checkpoints while what they hold and the records their
//  intervals need pass the budget, down to the fewest that cover a keyframe
//  interval. When that leaves records for more than a thirty-second of the
//  budget allocated past the count, the limit and the record capacity come
//  down to the count, so records nothing will use do not hold the budget the
//  checkpoints now need. The margin keeps a count that wavers by one from
//  reallocating the records every time.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::DropOverBudget()
{
    constexpr size_t  kShrinkFraction = 32;
    UndoRingSettings  unbudgeted      = m_settings;
    size_t            minimum         = 0;
    size_t            perRecords      = GetRecordBytesPerInterval (m_settings.checkpointCycles);
    size_t            unused          = 0;
    bool              dropped         = false;



    unbudgeted.budgetBytes = 0;
    minimum                = GetCheckpointLimit (unbudgeted, m_keyframeInterval, 0);

    while (m_checkpoints.size() > minimum && m_heldBytes + m_checkpoints.size() * perRecords > m_settings.budgetBytes)
    {
        DropOldestCheckpoint();
        dropped = true;
    }

    unused = (m_checkpointLimit > m_checkpoints.size()) ? (m_checkpointLimit - m_checkpoints.size()) * perRecords : 0;

    if (dropped && unused > m_settings.budgetBytes / kShrinkFraction)
    {
        m_checkpointLimit = std::max (m_checkpoints.size(), minimum);

        ResizeRecords (GetRecordCapacity (m_checkpointLimit));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GrowLimitIfRoom
//
//  Once the ring is full, the checkpoints after the oldest show what one
//  more costs on average: the records of its interval and the bytes it holds
//  that the one before does not. When the budget left beside the oldest buys
//  a quarter more checkpoints than the limit, the limit and the record
//  capacity grow to that. The limit never shrinks here; DropOverBudget keeps
//  the bytes in the budget whatever the checkpoints come to hold.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::GrowLimitIfRoom()
{
    size_t  count      = m_checkpoints.size();
    size_t  perRecords = GetRecordBytesPerInterval (m_settings.checkpointCycles);
    size_t  first      = 0;
    size_t  mean       = 0;
    size_t  affordable = 0;
    size_t  capacity   = 0;



    if (count < m_checkpointLimit || count < 2)
    {
        return;
    }

    first = m_checkpoints.front().heldBytes;
    mean  = (m_heldBytes - first) / (count - 1);

    // The record capacity covers one interval more than the limit, so with
    // L checkpoints the ring holds (L + 1) intervals of records, the oldest
    // checkpoint and L - 1 more.
    if (first + perRecords >= m_settings.budgetBytes)
    {
        return;
    }

    affordable = (m_settings.budgetBytes - first - perRecords + mean) / (mean + perRecords);

    if (affordable <= m_checkpointLimit + m_checkpointLimit / 4)
    {
        return;
    }

    m_checkpointLimit = affordable;
    capacity          = GetRecordCapacity (affordable);

    if (capacity > m_records.size())
    {
        ResizeRecords (capacity);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeepSpares
//
//  A checkpoint about to go gives its buffer and its emptied segment list to
//  the pools.
//
////////////////////////////////////////////////////////////////////////////////

void UndoRing::KeepSpares (UndoCheckpoint & checkpoint)
{
    ReturnSpareBuffer (std::move (checkpoint.state), std::move (checkpoint.segments));
}
