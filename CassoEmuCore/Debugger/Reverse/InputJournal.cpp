#include "Pch.h"

#include "Debugger/Reverse/InputJournal.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Record
//
//  Appends one input, stamped with the given cycle and the current position.
//  The caller makes this call on the CPU thread before it applies the input,
//  so the stamp is the instruction boundary the input took effect at. With no
//  position source attached the position is zero; the cycle and the order
//  still place the input exactly.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::Record (
    uint64_t          cycle,
    InputKind         kind,
    Byte              value,
    uint16_t          detail,
    std::string_view  payload)
{
    InputRecord  * record = nullptr;



    if (!m_isOn)
    {
        return;
    }

    record = &AppendSlot();

    record->position   = (m_positionSource != nullptr) ? *m_positionSource : 0;
    record->cycle      = cycle;
    record->kind       = kind;
    record->value      = value;
    record->detail     = detail;
    record->data       = 0;
    record->isObserved = false;

    record->payload.assign (payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordObserved
//
//  Appends one input a device saw at a CPU-thread read: a value another
//  thread wrote, which this read is the first to see. The cycle is the one
//  the reading instruction began at, so a replay applies the value just
//  before that instruction and the read returns what it returned live.
//
//  While the CPU thread samples the devices at a slice boundary, the record
//  is a boundary record instead: no instruction has run yet, so a replay
//  landing on the position must already hold the value.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void InputJournal::RecordObserved (
    uint64_t   cycle,
    InputKind  kind,
    Byte       value,
    uint16_t   detail,
    uint64_t   data)
{
    InputRecord  * record = nullptr;



    if (!m_isOn)
    {
        return;
    }

    record = &AppendSlot();

    record->position   = (m_positionSource != nullptr) ? *m_positionSource : 0;
    record->cycle      = cycle;
    record->kind       = kind;
    record->value      = value;
    record->detail     = detail;
    record->data       = data;
    record->isObserved = !m_isSampling;

    record->payload.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendSlot
//
//  The slot for the next record: one a dropped record left past the live
//  ones, or a new one. Once as many slots lie dead before the live records
//  as there are live ones, the live records move to the front and the dead
//  slots go behind them, payload buffers and all, so the move is paid for by
//  the records that freed them and the vector stops growing.
//
////////////////////////////////////////////////////////////////////////////////

InputRecord & InputJournal::AppendSlot()
{
    size_t  end = m_head + m_count;



    if (end == m_records.size() && m_head != 0 && m_head >= m_count)
    {
        std::rotate (m_records.begin(), m_records.begin() + static_cast<ptrdiff_t> (m_head), m_records.end());

        m_head = 0;
        end    = m_count;
    }

    if (end == m_records.size())
    {
        m_records.emplace_back();
    }

    m_count++;

    return m_records[end];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecord
//
//  Takes an absolute index in [GetBeginIndex, GetEndIndex).
//
////////////////////////////////////////////////////////////////////////////////

const InputRecord & InputJournal::GetRecord (size_t index) const
{
    assert (index >= m_firstIndex && index < GetEndIndex());

    return m_records[m_head + (index - m_firstIndex)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Truncate
//
//  Drops every record at or after endIndex: the recorded future that a change
//  made in the past has made wrong. Their slots stay for reuse.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::Truncate (size_t endIndex)
{
    size_t  keep = 0;



    if (endIndex > m_firstIndex)
    {
        keep = endIndex - m_firstIndex;
    }

    m_count = std::min (m_count, keep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardBefore
//
//  Drops every record before beginIndex, once no snapshot can reach them any
//  more. Indices of the records kept do not change.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::DiscardBefore (size_t beginIndex)
{
    size_t  drop = 0;



    if (beginIndex <= m_firstIndex)
    {
        return;
    }

    drop = std::min (beginIndex - m_firstIndex, m_count);

    m_head       += drop;
    m_count      -= drop;
    m_firstIndex += drop;

    if (m_count == 0)
    {
        m_head = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
//  Empties the journal. Indices keep counting from where they were, so a
//  cursor held from before the clear can never land on a newer record.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::Clear()
{
    m_firstIndex += m_count;
    m_head        = 0;
    m_count       = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadRecords
//
//  The slots already held are written over, payloads and all, so a journal
//  loaded again and again stops allocating once it has held its largest.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::LoadRecords (
    size_t                             firstIndex,
    const std::vector<InputRecord>   & records)
{
    size_t  i = 0;



    if (m_records.size() < records.size())
    {
        m_records.resize (records.size());
    }

    for (i = 0; i < records.size(); i++)
    {
        m_records[i] = records[i];
    }

    m_head       = 0;
    m_count      = records.size();
    m_firstIndex = firstIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryCopyRecords
//
//  The records from absolute index firstIndex on, through the last made at
//  lastPosition, for a second machine replaying them. False when the
//  journal no longer holds firstIndex, or never held it.
//
////////////////////////////////////////////////////////////////////////////////

bool InputJournal::TryCopyRecords (
    size_t                       firstIndex,
    uint64_t                     lastPosition,
    std::vector<InputRecord>   & outRecords) const
{
    bool    isHeld = firstIndex >= GetBeginIndex() && firstIndex <= GetEndIndex();
    size_t  index  = firstIndex;



    outRecords.clear();

    for (index = firstIndex; isHeld && index < GetEndIndex() && GetRecord (index).position <= lastPosition; index++)
    {
        outRecords.push_back (GetRecord (index));
    }

    return isHeld;
}





