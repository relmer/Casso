#include "Pch.h"

#include "Debugger/Reverse/HistoryImageCache.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache::Find
//
//  The picture moves to the front, as the most recently used.
//
////////////////////////////////////////////////////////////////////////////////

HistoryImageCache::Image HistoryImageCache::Find (uint64_t key)
{
    auto  found = m_index.find (key);



    if (found == m_index.end())
    {
        return nullptr;
    }

    m_entries.splice (m_entries.begin(), m_entries, found->second);

    return found->second->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache::Put
//
//  A key already held takes the new picture and moves to the front.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryImageCache::Put (
    uint64_t  key,
    Image     image)
{
    auto  found = m_index.find (key);



    if (found != m_index.end())
    {
        found->second->second = std::move (image);
        m_entries.splice (m_entries.begin(), m_entries, found->second);
        return;
    }

    m_entries.emplace_front (key, std::move (image));
    m_index[key] = m_entries.begin();

    Trim();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache::Clear
//
////////////////////////////////////////////////////////////////////////////////

void HistoryImageCache::Clear()
{
    m_entries.clear();
    m_index.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache::SetCapacity
//
////////////////////////////////////////////////////////////////////////////////

void HistoryImageCache::SetCapacity (size_t capacity)
{
    m_capacity = (std::max) (capacity, (size_t) 1);

    Trim();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache::Trim
//
////////////////////////////////////////////////////////////////////////////////

void HistoryImageCache::Trim()
{
    while (m_entries.size() > m_capacity)
    {
        m_index.erase (m_entries.back().first);
        m_entries.pop_back();
    }
}
