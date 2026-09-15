#include "Pch.h"

#include "Debugger/WatchpointTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::WatchpointTable
//
////////////////////////////////////////////////////////////////////////////////

WatchpointTable::WatchpointTable (int & nextId) :
    m_nextId (nextId)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::SetTarget
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::SetTarget (IDebugTarget * target)
{
    m_target = target;
    Publish();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::Add
//
////////////////////////////////////////////////////////////////////////////////

int WatchpointTable::Add (WatchAccess access, Word first, Word last, WatchMode mode)
{
    Watchpoint  entry;



    entry.id     = m_nextId++;
    entry.access = access;
    entry.first  = first;
    entry.last   = last;
    entry.mode   = mode;

    m_entries.push_back (entry);
    Publish();
    return entry.id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::TryClear
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TryClear (int id)
{
    size_t  removed = std::erase_if (m_entries, [id] (const Watchpoint & entry) { return entry.id == id; });



    Publish();
    return removed > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::ClearAll
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::ClearAll()
{
    m_entries.clear();
    m_pendingHit.reset();
    Publish();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::TrySetEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TrySetEnabled (int id, bool enabled)
{
    bool  isFound = false;



    for (Watchpoint & entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.enabled = enabled;
            isFound       = true;
        }
    }

    Publish();
    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::HasEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::HasEnabled() const
{
    return std::any_of (m_entries.begin(), m_entries.end(), [] (const Watchpoint & entry) { return entry.enabled; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::GetWatchedPages
//
//  Every page an enabled after-mode watchpoint's range touches, and no other.
//
////////////////////////////////////////////////////////////////////////////////

WatchedPages WatchpointTable::GetWatchedPages() const
{
    WatchedPages  pages = {};



    for (const Watchpoint & entry : m_entries)
    {
        if (!entry.enabled || entry.mode != WatchMode::After)
        {
            continue;
        }

        for (int page = entry.first >> kPageShift; page <= (entry.last >> kPageShift); ++page)
        {
            pages[page] = true;
        }
    }

    return pages;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::OnWatchedAccess
//
//  The first matching hit since the last ClearPending is kept, except that a
//  write to the pending hit's address replaces it (see ShouldReplace).
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous)
{
    for (Watchpoint & entry : m_entries)
    {
        if (!entry.enabled || entry.mode != WatchMode::After || address < entry.first || address > entry.last || !IsAccessMatch (entry.access, access))
        {
            continue;
        }

        ++entry.hits;

        if (!m_pendingHit.has_value() || ShouldReplace (address, access))
        {
            m_pendingHit = WatchHit { entry.id,
                                      address,
                                      value,
                                      access == BusAccess::Write ? previous : std::nullopt,
                                      access == BusAccess::Read ? WatchAccess::Read : WatchAccess::Write,
                                      m_accessPc,
                                      WatchMode::After };
        }

        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::ShouldReplace
//
//  A write to the address already pending replaces the pending hit, whether
//  that hit was the pre-read of a store or an earlier write; a read never
//  replaces anything.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::ShouldReplace (Word address, BusAccess access) const
{
    return access == BusAccess::Write && m_pendingHit.has_value() && m_pendingHit->address == address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::IsAccessMatch
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::IsAccessMatch (WatchAccess watched, BusAccess actual)
{
    return watched == WatchAccess::ReadWrite                                ||
           (watched == WatchAccess::Read  && actual == BusAccess::Read)     ||
           (watched == WatchAccess::Write && actual == BusAccess::Write);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::Publish
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::Publish()
{
    if (m_target != nullptr)
    {
        m_target->SetWatchedPages (GetWatchedPages());
    }
}
