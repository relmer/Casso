#include "Pch.h"

#include "Debugger/Reverse/ArenaExtents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtents::Reset
//
//  The list can grow to one more free extent than the extents allocated:
//  each allocation splits at most one free extent in two.
//
////////////////////////////////////////////////////////////////////////////////

void ArenaExtents::Reset (
    size_t  arenaBytes,
    size_t  extentCount)
{
    m_arenaBytes = arenaBytes;

    m_free.clear();
    m_free.reserve (extentCount + 2);

    Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtents::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ArenaExtents::Clear()
{
    m_free.clear();

    if (m_arenaBytes > 0)
    {
        m_free.push_back ({ 0, m_arenaBytes });
    }

    m_freeBytes = m_arenaBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtents::TryAllocate
//
//  The start of the first free extent that fits size is taken, and an extent
//  used up whole is removed from the list. False, with nothing taken, when
//  none fits. A size
//  of zero takes nothing and fits at the start of the first extent.
//
////////////////////////////////////////////////////////////////////////////////

bool ArenaExtents::TryAllocate (
    size_t    size,
    size_t  & outOffset)
{
    size_t  i     = 0;
    bool    isFit = false;



    if (size == 0)
    {
        outOffset = m_free.empty() ? 0 : m_free.front().offset;
        return true;
    }

    for (i = 0; i < m_free.size() && !isFit; i++)
    {
        isFit = m_free[i].size >= size;
    }

    if (!isFit)
    {
        return false;
    }

    i--;

    outOffset = m_free[i].offset;

    m_free[i].offset += size;
    m_free[i].size   -= size;
    m_freeBytes      -= size;

    if (m_free[i].size == 0)
    {
        m_free.erase (m_free.begin() + (ptrdiff_t) i);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtents::Free
//
//  The extent goes back into the list in offset order, merged with the free
//  extent that ends where it starts and the one that starts where it ends.
//
////////////////////////////////////////////////////////////////////////////////

void ArenaExtents::Free (
    size_t  offset,
    size_t  size)
{
    size_t  i          = 0;
    bool    isJoinLeft = false;
    bool    isJoinNext = false;



    if (size == 0)
    {
        return;
    }

    ASSERT (offset + size <= m_arenaBytes);

    while (i < m_free.size() && m_free[i].offset < offset)
    {
        i++;
    }

    isJoinLeft = i > 0 && m_free[i - 1].offset + m_free[i - 1].size == offset;
    isJoinNext = i < m_free.size() && offset + size == m_free[i].offset;

    if (isJoinLeft && isJoinNext)
    {
        m_free[i - 1].size += size + m_free[i].size;
        m_free.erase (m_free.begin() + (ptrdiff_t) i);
    }
    else if (isJoinLeft)
    {
        m_free[i - 1].size += size;
    }
    else if (isJoinNext)
    {
        m_free[i].offset  = offset;
        m_free[i].size   += size;
    }
    else
    {
        m_free.insert (m_free.begin() + (ptrdiff_t) i, { offset, size });
    }

    m_freeBytes += size;
}

