#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryImageCache
//
//  Pictures of points in history by a key, least recently used first out:
//  once it holds its capacity, adding one drops the picture used longest
//  ago. Finding a picture counts as using it. A spare picture is kept only
//  while there is room, as the least recently used. Not thread safe; the
//  owner locks around it.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryImageCache
{
public:
    using Image = std::shared_ptr<const DxuiIconImage>;

    explicit  HistoryImageCache (size_t capacity) : m_capacity ((std::max) (capacity, (size_t) 1)) {}

    Image     Find           (uint64_t key);
    bool      Contains       (uint64_t key) const { return m_index.contains (key); }
    bool      TryFindNearest (uint64_t key, uint64_t & outKey) const;
    void      Put            (uint64_t key, Image image);
    void      PutSpare       (uint64_t key, Image image);
    void      Clear          ();
    void      SetCapacity    (size_t capacity);

    size_t    GetCount       () const { return m_entries.size(); }
    size_t    GetCapacity    () const { return m_capacity; }

private:
    using Entries = std::list<std::pair<uint64_t, Image>>;

    void      Trim        ();

    size_t                                           m_capacity = 1;
    Entries                                          m_entries;    // most recently used first
    std::unordered_map<uint64_t, Entries::iterator>  m_index;
};
