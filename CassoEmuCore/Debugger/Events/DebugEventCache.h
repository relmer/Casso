#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventEntry
//
//  One row of an event log: an event of type T; a gap where history
//  recording paused; or the start of a stretch of history whose events
//  cannot be rebuilt, its keyframe having been taken with other disks in the
//  bays. Each is stamped with the machine's position (instructions retired)
//  and CPU cycle at the time. An event's position is the index of the
//  instruction running at the time, so the events before position P are
//  those of the instructions before P.
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugEventEntryKind
{
    Event,
    Gap,
    Unavailable,
};

template <typename T>
struct DebugEventEntry
{
    DebugEventEntryKind  kind     = DebugEventEntryKind::Event;
    uint64_t             position = 0;
    uint64_t             cycle    = 0;
    T                    event    = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventCache
//
//  The newest events the running machine produced, in a ring of fixed-size
//  entries allocated once, so an append costs a copy and no allocation. When
//  the ring is full the oldest entry is overwritten. Entries are in position
//  order, oldest first.
//
//  The cache is complete from GetCoveredFrom on: from where it was reset
//  until the ring first overwrites an entry, then from just past the
//  overwritten entry's position, since others at that position may be gone.
//  A rewind or seek cuts it back to the machine's position (CutBack), so
//  every entry in it is from before the machine's position.
//
//  Each entry appended is given the next serial number. CutBack lowers the
//  end serial by the entries it drops, so the next entry appended is given
//  the first serial dropped. A reader that has read the entries before
//  serial S reads those from S on next.
//
//  It is used on one thread, the one that runs the machine; a view copies
//  spans out of it there.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class DebugEventCache
{
public:
    using Entry = DebugEventEntry<T>;

    explicit DebugEventCache (size_t capacity)
        : m_entries (std::max<size_t> (capacity, 1))
    {
    }

    //  Empties the cache, complete from position on.
    void  Reset (uint64_t position)
    {
        m_first       = 0;
        m_count       = 0;
        m_coveredFrom = position;
        m_hasDropped  = false;
    }

    void  Append (uint64_t position, uint64_t cycle, const T & event)
    {
        Push (DebugEventEntryKind::Event, position, cycle, event);
    }

    //  History recording paused before position, which it resumed at.
    void  MarkGap (uint64_t position, uint64_t cycle)
    {
        Push (DebugEventEntryKind::Gap, position, cycle, T {});
    }

    //  Drops every entry at or after position, newest first. After a cut to
    //  before where the cache is complete, the cache is empty and complete
    //  from position, since the machine runs on from there with every event
    //  appended.
    void  CutBack (uint64_t position)
    {
        while (m_count > 0 && GetAt (m_count - 1).position >= position)
        {
            m_count--;
            m_endSerial--;
        }

        m_coveredFrom = std::min (m_coveredFrom, position);
    }

    size_t          GetCount       () const             { return m_count; }
    size_t          GetCapacity    () const             { return m_entries.size(); }
    bool            HasDropped     () const             { return m_hasDropped; }
    uint64_t        GetCoveredFrom () const             { return m_coveredFrom; }
    uint64_t        GetFirstSerial () const             { return m_endSerial - m_count; }
    uint64_t        GetEndSerial   () const             { return m_endSerial; }
    const Entry &   GetAt          (size_t index) const { return m_entries[(m_first + index) % m_entries.size()]; }

    //  The entries from position from up to but not including to, oldest
    //  first, appended to out.
    void  CopySpan (uint64_t from, uint64_t to, std::vector<Entry> & out) const
    {
        size_t  i = FindFirstAtOrAfter (from);



        for (; i < m_count && GetAt (i).position < to; i++)
        {
            out.push_back (GetAt (i));
        }
    }

    //  The index of the first entry at or after position, or GetCount when
    //  none is.
    size_t  FindFirstAtOrAfter (uint64_t position) const
    {
        size_t  low  = 0;
        size_t  high = m_count;
        size_t  mid  = 0;



        while (low < high)
        {
            mid = low + (high - low) / 2;

            if (GetAt (mid).position < position)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }

        return low;
    }

private:
    void  Push (DebugEventEntryKind kind, uint64_t position, uint64_t cycle, const T & event)
    {
        size_t  slot = (m_first + m_count) % m_entries.size();



        //  The entry overwritten is the oldest; others at its position may
        //  remain, so the cache is complete only after it.
        if (m_count == m_entries.size())
        {
            m_coveredFrom = std::max (m_coveredFrom, m_entries[slot].position + 1);
            m_first       = (m_first + 1) % m_entries.size();
            m_hasDropped  = true;
            m_count--;
        }

        m_entries[slot] = Entry { kind, position, cycle, event };
        m_count++;
        m_endSerial++;
    }

    std::vector<Entry>  m_entries;
    size_t              m_first       = 0;
    size_t              m_count       = 0;
    uint64_t            m_coveredFrom = 0;
    uint64_t            m_endSerial   = 0;
    bool                m_hasDropped  = false;
};
