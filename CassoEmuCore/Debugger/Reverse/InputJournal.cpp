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
    InputRecord  record;



    if (!m_isOn)
    {
        return;
    }

    record.position = (m_positionSource != nullptr) ? *m_positionSource : 0;
    record.cycle    = cycle;
    record.kind     = kind;
    record.value    = value;
    record.detail   = detail;
    record.payload  = payload;

    m_records.push_back (std::move (record));
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

    return m_records[index - m_firstIndex];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Truncate
//
//  Drops every record at or after endIndex: the recorded future that a change
//  made in the past has made wrong.
//
////////////////////////////////////////////////////////////////////////////////

void InputJournal::Truncate (size_t endIndex)
{
    size_t  keep = 0;



    if (endIndex > m_firstIndex)
    {
        keep = endIndex - m_firstIndex;
    }

    if (keep < m_records.size())
    {
        m_records.resize (keep);
    }
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

    drop = std::min (beginIndex - m_firstIndex, m_records.size());

    m_records.erase (m_records.begin(), m_records.begin() + static_cast<ptrdiff_t> (drop));
    m_firstIndex += drop;
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
    m_firstIndex += m_records.size();
    m_records.clear();
}





