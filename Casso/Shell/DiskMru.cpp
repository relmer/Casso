#include "Pch.h"

#include "DiskMru.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EnforceCap
//
//  Trims the tail of `m_entries` until size <= k_capacity.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::EnforceCap()
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

void DiskMru::RecordMount (const std::filesystem::path & path, std::int64_t lastLoadedUnix)
{
    std::vector<Entry>::iterator  it;



    if (!path.empty())
    {
        it = std::find_if (m_entries.begin(),
                           m_entries.end(),
                           [&] (const Entry & e) { return e.path == path; });
        if (it != m_entries.end())
        {
            m_entries.erase (it);
        }
        m_entries.insert (m_entries.begin(), Entry { path, lastLoadedUnix });
        EnforceCap();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Snapshot
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskMru::Entry> DiskMru::Snapshot() const
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

std::vector<DiskMru::Entry> DiskMru::Prune (
    const std::function<bool (const std::filesystem::path &)> & existsPredicate)
{
    std::vector<Entry>::iterator  last;



    if (existsPredicate)
    {
        last = std::remove_if (m_entries.begin(),
                               m_entries.end(),
                               [&] (const Entry & e) { return !existsPredicate (e.path); });
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

void DiskMru::ReplaceAll (std::vector<Entry> entries)
{
    m_entries = std::move (entries);
    EnforceCap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FromUtf8
//
//  Constructs a DiskMru from the GlobalUserPrefs `recentDisks` /
//  `recentDiskLoadedAt` parallel lists. Drops empty paths; preserves
//  order; caps at k_capacity. A load-time entry is paired by index;
//  missing times (shorter `loadedAtUnix`) default to 0.
//
////////////////////////////////////////////////////////////////////////////////

DiskMru DiskMru::FromUtf8 (const std::vector<std::string> & utf8Entries,
                           const std::vector<std::int64_t> & loadedAtUnix)
{
    DiskMru             mru;
    std::vector<Entry>  entries;
    size_t              i = 0;



    entries.reserve (utf8Entries.size());
    for (i = 0; i < utf8Entries.size(); i++)
    {
        if (!utf8Entries[i].empty())
        {
            // Interpret the stored bytes as UTF-8 so non-ASCII filenames
            // (e.g. the o-slash in "Broderbund") round-trip intact rather
            // than being mangled by the platform-narrow path constructor.
            std::u8string  u8 (reinterpret_cast<const char8_t *> (utf8Entries[i].data()),
                               utf8Entries[i].size());
            std::int64_t   when = (i < loadedAtUnix.size()) ? loadedAtUnix[i] : 0;

            entries.push_back (Entry { std::filesystem::path (u8), when });
        }
    }
    mru.ReplaceAll (std::move (entries));
    return mru;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUtf8
//
//  Serialises the snapshot into the `recentDisks` / `recentDiskLoadedAt`
//  parallel narrow-string + load-time lists used by GlobalUserPrefs JSON.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMru::ToUtf8 (std::vector<std::string>  & outUtf8Entries,
                      std::vector<std::int64_t> & outLoadedAtUnix) const
{
    size_t  i = 0;



    outUtf8Entries.clear();
    outLoadedAtUnix.clear();
    outUtf8Entries.reserve (m_entries.size());
    outLoadedAtUnix.reserve (m_entries.size());
    for (i = 0; i < m_entries.size(); i++)
    {
        // Serialise as UTF-8 (not the platform-narrow encoding) so the
        // recentDisks JSON stays valid UTF-8 for non-ASCII filenames.
        std::u8string  u8 = m_entries[i].path.u8string();

        outUtf8Entries.emplace_back (reinterpret_cast<const char *> (u8.data()), u8.size());
        outLoadedAtUnix.push_back (m_entries[i].lastLoadedUnix);
    }
}
