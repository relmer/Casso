#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void BlockHistory::Record (uint32_t block)
{
    if (!m_entries.empty() && m_entries[m_current] == block)
    {
        return;
    }

    if (!m_entries.empty())
    {
        m_entries.resize (m_current + 1);
    }

    m_entries.push_back (block);

    if (m_entries.size() > kMaxEntries)
    {
        m_entries.erase (m_entries.begin());
    }

    m_current = m_entries.size() - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHistory::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHistory::GoBack (uint32_t & outBlock)
{
    if (!CanGoBack())
    {
        return false;
    }

    m_current--;
    outBlock = m_entries[m_current];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHistory::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHistory::GoForward (uint32_t & outBlock)
{
    if (!CanGoForward())
    {
        return false;
    }

    m_current++;
    outBlock = m_entries[m_current];

    return true;
}
