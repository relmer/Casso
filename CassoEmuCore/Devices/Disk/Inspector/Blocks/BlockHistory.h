#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHistory
//
//  The blocks a user stepped to, for Back and Forward as in a browser. A
//  block added after a step back replaces the ones ahead of it, the same
//  block twice in a row is added once, and the oldest is dropped past
//  kMaxEntries.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHistory
{
public:
    static constexpr size_t  kMaxEntries = 200;

    void  Record       (uint32_t block);
    bool  GoBack       (uint32_t & outBlock);
    bool  GoForward    (uint32_t & outBlock);
    bool  CanGoBack    () const { return m_current > 0; }
    bool  CanGoForward () const { return m_current + 1 < m_entries.size(); }
    void  Clear        ()       { m_entries.clear(); m_current = 0; }

private:
    vector<uint32_t>  m_entries;
    size_t            m_current = 0;
};
