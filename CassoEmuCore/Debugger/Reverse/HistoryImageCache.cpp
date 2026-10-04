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
//  HistoryImageCache::TryFindNearest
//
//  The key held closest to `key`, the earlier of two as close. Looking does
//  not count as using it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryImageCache::TryFindNearest (
    uint64_t    key,
    uint64_t  & outKey) const
{
    uint64_t  best     = 0;
    uint64_t  distance = 0;
    bool      isFound  = false;



    outKey = 0;

    for (const auto & entry : m_entries)
    {
        distance = (entry.first > key) ? entry.first - key : key - entry.first;

        if (!isFound || distance < best || (distance == best && entry.first < outKey))
        {
            best    = distance;
            outKey  = entry.first;
            isFound = true;
        }
    }

    return isFound;
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
