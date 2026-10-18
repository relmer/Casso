#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugLogDelta.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogRows
//
//  An event log pane's rows, in position order, built from the deltas the
//  machine's thread sends (DebugLogDelta): every row is dropped on a reset,
//  the rows from a cut on are dropped, and older rows go in front, newer at
//  the end. Each change moves the revision on, and a view of the rows built
//  at an older revision is built again.
//
//  UI thread only.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class EventLogRows
{
public:
    using Row = DebugLogRow<T>;

    //  The count of rows put in front; a view adds it to each of its
    //  indices.
    size_t  Apply (const DebugLogDelta<T> & delta)
    {
        size_t  prepended = 0;



        if (delta.isReset)
        {
            m_rows.clear();
        }

        if (delta.cutFrom != DebugLogDelta<T>::kNoCut)
        {
            auto  first = std::ranges::partition_point (m_rows, [&delta] (const Row & row) { return row.position < delta.cutFrom; });

            m_rows.erase (first, m_rows.end());
        }

        if (!delta.older.empty())
        {
            m_rows.insert (m_rows.begin(), delta.older.begin(), delta.older.end());
            prepended = delta.older.size();
        }

        m_rows.insert (m_rows.end(), delta.newer.begin(), delta.newer.end());

        m_heldFrom     = delta.heldFrom;
        m_isAtStart    = delta.isAtStart;
        m_isRebuilding = delta.isRebuilding;
        m_isReset      = delta.isReset;
        m_revision++;

        return prepended;
    }

    //  The oldest rows dropped down to the newest `retained`; the oldest row
    //  left is where the rows start now. The count of rows dropped.
    size_t  TrimFront (size_t retained)
    {
        size_t  dropped = (m_rows.size() > retained) ? m_rows.size() - retained : 0;



        if (dropped == 0)
        {
            return 0;
        }

        m_rows.erase (m_rows.begin(), m_rows.begin() + (ptrdiff_t) dropped);

        m_heldFrom  = m_rows.front().position;
        m_isAtStart = false;
        m_revision++;

        return dropped;
    }

    const std::vector<Row> &  GetRows       () const { return m_rows; }
    uint64_t                  GetHeldFrom   () const { return m_heldFrom; }
    bool                      IsAtStart     () const { return m_isAtStart; }
    bool                      IsRebuilding  () const { return m_isRebuilding; }
    bool                      WasReset      () const { return m_isReset; }
    uint64_t                  GetRevision   () const { return m_revision; }

private:
    std::vector<Row>  m_rows;
    uint64_t          m_heldFrom     = 0;
    bool              m_isAtStart    = true;
    bool              m_isRebuilding = false;
    bool              m_isReset      = false;
    uint64_t          m_revision     = 0;
};
