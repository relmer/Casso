#include "Pch.h"

#include "Debugger/DataBlockTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable::Add
//
////////////////////////////////////////////////////////////////////////////////

void DataBlockTable::Add (const std::string & name, Word first, Word last, DataBlockKind kind, int perLine)
{
    DataBlockEntry  entry;



    Remove (first, last);

    entry.name    = name.empty() ? MakeName (kind, first) : name;
    entry.first   = first;
    entry.last    = last;
    entry.kind    = kind;
    entry.perLine = perLine;

    m_entries.push_back (entry);
    std::sort (m_entries.begin(), m_entries.end(), [] (const DataBlockEntry & a, const DataBlockEntry & b) { return a.first < b.first; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable::Remove
//
//  A block inside the range goes; one straddling an end is trimmed; one
//  that contains the range is split around it, both halves keeping the
//  name.
//
////////////////////////////////////////////////////////////////////////////////

void DataBlockTable::Remove (Word first, Word last)
{
    std::vector<DataBlockEntry>  kept;



    for (const DataBlockEntry & entry : m_entries)
    {
        DataBlockEntry  head = entry;
        DataBlockEntry  tail = entry;



        if (entry.last < first || entry.first > last)
        {
            kept.push_back (entry);
            continue;
        }

        if (entry.first < first)
        {
            head.last = (Word) (first - 1);
            kept.push_back (head);
        }

        if (entry.last > last)
        {
            tail.first = (Word) (last + 1);
            kept.push_back (tail);
        }
    }

    m_entries = kept;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable::Clear
//
////////////////////////////////////////////////////////////////////////////////

void DataBlockTable::Clear()
{
    m_entries.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable::TryFindAt
//
////////////////////////////////////////////////////////////////////////////////

bool DataBlockTable::TryFindAt (Word address, DataBlockEntry & entry) const
{
    for (const DataBlockEntry & candidate : m_entries)
    {
        if (address >= candidate.first && address <= candidate.last)
        {
            entry = candidate;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable::MakeName
//
////////////////////////////////////////////////////////////////////////////////

std::string DataBlockTable::MakeName (DataBlockKind kind, Word first)
{
    static constexpr const char  kPrefixes[] = { 'B', 'W', 'A', 'T', 'F' };



    return std::format ("{}_{:04X}", kPrefixes[(int) kind], first);
}
