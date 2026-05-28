#include "Pch.h"

#include "DiskMru.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EnforceCap
//
//  Trims the tail of `m_entries` until size <= k_capacity.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::EnforceCap ()
{
    while (m_entries.size() > k_capacity)
    {
        m_entries.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordMount
//
//  Move-to-front: drop any prior occurrence of `path`, insert at index 0,
//  evict the tail until size <= k_capacity. Empty paths are ignored.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::RecordMount (const std::filesystem::path & path)
{
    std::vector<std::filesystem::path>::iterator  it;



    if (!path.empty())
    {
        it = std::find (m_entries.begin(), m_entries.end(), path);
        if (it != m_entries.end())
        {
            m_entries.erase (it);
        }
        m_entries.insert (m_entries.begin(), path);
        EnforceCap();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Snapshot
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> DiskMru::Snapshot () const
{
    return m_entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Prune
//
//  Removes entries the predicate rejects, preserving order. A null
//  predicate is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> DiskMru::Prune (
    const std::function<bool (const std::filesystem::path &)> & existsPredicate)
{
    std::vector<std::filesystem::path>::iterator  last;



    if (existsPredicate)
    {
        last = std::remove_if (m_entries.begin(),
                               m_entries.end(),
                               [&] (const std::filesystem::path & p) { return !existsPredicate (p); });
        m_entries.erase (last, m_entries.end());
    }

    return m_entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceAll
//
//  Bulk replace (used at load time). De-dup is preserved if the caller
//  supplied de-duped entries; we still enforce the cap.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::ReplaceAll (std::vector<std::filesystem::path> entries)
{
    m_entries = std::move (entries);
    EnforceCap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FromUtf8
//
//  Constructs a DiskMru from the GlobalUserPrefs `recentDisks`
//  narrow-string list. Drops empty entries; preserves order; caps at
//  k_capacity.
//
////////////////////////////////////////////////////////////////////////////////

DiskMru DiskMru::FromUtf8 (const std::vector<std::string> & utf8Entries)
{
    DiskMru                             mru;
    std::vector<std::filesystem::path>  paths;
    size_t                              i = 0;



    paths.reserve (utf8Entries.size());
    for (i = 0; i < utf8Entries.size(); i++)
    {
        if (!utf8Entries[i].empty())
        {
            paths.emplace_back (std::filesystem::path (utf8Entries[i]));
        }
    }
    mru.ReplaceAll (std::move (paths));
    return mru;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUtf8
//
//  Serialises the snapshot into the `recentDisks` narrow-string list
//  shape used by GlobalUserPrefs JSON.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::ToUtf8 (std::vector<std::string> & outUtf8Entries) const
{
    size_t  i = 0;



    outUtf8Entries.clear();
    outUtf8Entries.reserve (m_entries.size());
    for (i = 0; i < m_entries.size(); i++)
    {
        outUtf8Entries.push_back (m_entries[i].string());
    }
}
