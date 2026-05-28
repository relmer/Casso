#include "Pch.h"

#include "DiskMru.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordMount
//
//  Move-to-front semantics: if `path` is already present, drop the
//  prior occurrence; then insert at index 0; then evict the tail until
//  size <= k_capacity. Empty paths are silently ignored.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::RecordMount (const std::filesystem::path & path)
{
    if (path.empty())
    {
        return;
    }

    auto it = std::find (m_entries.begin(), m_entries.end(), path);
    if (it != m_entries.end())
    {
        m_entries.erase (it);
    }

    m_entries.insert (m_entries.begin(), path);

    while (m_entries.size() > k_capacity)
    {
        m_entries.pop_back();
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
//  Removes entries the predicate rejects. Preserves the surviving
//  order. A null predicate is a no-op (returns the current snapshot).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> DiskMru::Prune (
    const std::function<bool (const std::filesystem::path &)> & existsPredicate)
{
    if (!existsPredicate)
    {
        return m_entries;
    }

    auto last = std::remove_if (m_entries.begin(),
                                m_entries.end(),
                                [&] (const std::filesystem::path & p) { return !existsPredicate (p); });
    m_entries.erase (last, m_entries.end());

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

    while (m_entries.size() > k_capacity)
    {
        m_entries.pop_back();
    }
}
