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
//  WatchpointTable::SetAccessPc
//
//  Called before each instruction. A suppression covers only the instruction
//  it was set for, so moving on to another ends it.
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::SetAccessPc (Word pc)
{
    if (m_suppression.has_value() && m_suppression->pc != pc)
    {
        m_suppression.reset();
    }

    m_accessPc = pc;
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
//  WatchpointTable::TrySetFlags
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TrySetFlags (int id, bool temporary, bool stops)
{
    bool  isFound = false;



    for (Watchpoint & entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.temporary = temporary;
            entry.stops     = stops;
            isFound         = true;
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::TryAdopt
//
//  Inserts an entry under the id it carries, for BPEDIT. Fails if the id is
//  taken.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TryAdopt (const Watchpoint & entry)
{
    Watchpoint  existing;



    if (TryFind (entry.id, existing))
    {
        return false;
    }

    m_entries.push_back (entry);
    std::sort (m_entries.begin(), m_entries.end(), [] (const Watchpoint & a, const Watchpoint & b) { return a.id < b.id; });
    Publish();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::TryFind
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TryFind (int id, Watchpoint & entry) const
{
    for (const Watchpoint & candidate : m_entries)
    {
        if (candidate.id == id)
        {
            entry = candidate;
            return true;
        }
    }

    return false;
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
//  WatchpointTable::HasEnabledBefore
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::HasEnabledBefore() const
{
    return std::any_of (m_entries.begin(), m_entries.end(),
                        [] (const Watchpoint & entry) { return entry.enabled && entry.mode == WatchMode::Before; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::TryMatchBefore
//
//  A predicted read-modify-write matches a read, write or read-write
//  watchpoint; the hit's access is the watchpoint's own where it is single,
//  and Write for a read-write one.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::TryMatchBefore (Word pc, const AccessPrediction & prediction, WatchHit & hit)
{
    for (const PredictedTouch & touch : prediction.touches)
    {
        for (Watchpoint & entry : m_entries)
        {
            bool  isInRange = touch.address >= entry.first && touch.address <= entry.last;



            if (!entry.enabled || entry.mode != WatchMode::Before || !isInRange || !IsTouchMatch (entry.access, touch.access))
            {
                continue;
            }

            ++entry.hits;

            if (!entry.stops)
            {
                continue;
            }

            hit.id       = entry.id;
            hit.address  = touch.address;
            hit.value    = 0;
            hit.previous.reset();
            hit.access   = (entry.access == WatchAccess::ReadWrite)
                         ? (touch.access == PredictedAccess::Read ? WatchAccess::Read : WatchAccess::Write)
                         : entry.access;
            hit.accessPc = pc;
            hit.mode     = WatchMode::Before;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::SuppressAfterStopFor
//
////////////////////////////////////////////////////////////////////////////////

void WatchpointTable::SuppressAfterStopFor (Word pc, Word first, Word last)
{
    m_suppression = Suppression { pc, first, last };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::IsSuppressed
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::IsSuppressed (Word address) const
{
    return m_suppression.has_value()          &&
           m_suppression->pc == m_accessPc    &&
           address >= m_suppression->first    &&
           address <= m_suppression->last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTable::IsTouchMatch
//
////////////////////////////////////////////////////////////////////////////////

bool WatchpointTable::IsTouchMatch (WatchAccess watched, PredictedAccess predicted)
{
    return watched == WatchAccess::ReadWrite                                                                        ||
           predicted == PredictedAccess::ReadWrite                                                                  ||
           (watched == WatchAccess::Read  && predicted == PredictedAccess::Read)                                    ||
           (watched == WatchAccess::Write && predicted == PredictedAccess::Write);
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
    if (IsSuppressed (address))
    {
        return;
    }

    for (Watchpoint & entry : m_entries)
    {
        if (!entry.enabled || entry.mode != WatchMode::After || address < entry.first || address > entry.last || !IsAccessMatch (entry.access, access))
        {
            continue;
        }

        ++entry.hits;

        if (!entry.stops)
        {
            return;
        }

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
