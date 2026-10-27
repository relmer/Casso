#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtents
//
//  The free space of one fixed arena, as a list of free extents ordered by
//  offset. An allocation takes the first extent it fits in, from its start;
//  a freed extent merges with free neighbors on either side. The list is
//  reserved once, so allocating and freeing allocate nothing.
//
//  Keyframes freed oldest first and allocated newest last reuse the arena as
//  a ring does; a branch of history dropped from the middle frees its
//  extents there, for the next allocations that fit.
//
////////////////////////////////////////////////////////////////////////////////

class ArenaExtents
{
public:
    //  An arena of arenaBytes, all free, with room in the list for at most
    //  extentCount extents allocated at once.
    void    Reset         (size_t arenaBytes, size_t extentCount);

    //  All of the arena free again.
    void    Clear         ();

    bool    TryAllocate   (size_t size, size_t & outOffset);
    void    Free          (size_t offset, size_t size);

    size_t  GetFreeBytes  () const { return m_freeBytes; }
    size_t  GetArenaBytes () const { return m_arenaBytes; }
    size_t  GetExtentCount() const { return m_free.size(); }

private:
    struct Extent
    {
        size_t  offset = 0;
        size_t  size   = 0;
    };

    std::vector<Extent>  m_free;
    size_t               m_arenaBytes = 0;
    size_t               m_freeBytes  = 0;
};
