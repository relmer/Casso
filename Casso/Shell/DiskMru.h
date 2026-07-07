#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMru
//
//  Pure most-recently-used list helper for mounted disk image paths.
//  Most-recent-first ordering, capped at k_capacity. Re-mounting an
//  existing entry moves it to index 0 and refreshes its load time. New
//  mount at cap evicts the oldest. `Prune` takes an injected predicate so
//  unit tests drive it without touching the real file system.
//
//  Each entry carries the wall-clock time the disk was last loaded
//  (Unix seconds; 0 == unknown, e.g. a legacy entry recorded before the
//  list tracked timestamps). The owning singleton persists this via the
//  parallel `recentDisks` / `recentDiskLoadedAt` GlobalUserPrefs arrays;
//  DiskMru itself is Win32-free and IO-free.
//
////////////////////////////////////////////////////////////////////////////////



class DiskMru
{
public:
    static constexpr size_t  k_capacity = 16;

    struct Entry
    {
        std::filesystem::path  path;
        std::int64_t           lastLoadedUnix = 0;   // 0 == unknown

        bool operator== (const Entry & other) const
        {
            return path == other.path && lastLoadedUnix == other.lastLoadedUnix;
        }
    };

    // Records a mount: move-to-front and stamp the load time (Unix
    // seconds; pass 0 when the time is unknown). Empty paths are ignored.
    void                 RecordMount  (const std::filesystem::path & path, std::int64_t lastLoadedUnix = 0);
    std::vector<Entry>   Snapshot     () const;

    // Removes entries the predicate returns false for, preserving the
    // surviving order. Returns the post-prune snapshot. Returns
    // unchanged copy when the predicate is null.
    std::vector<Entry>   Prune        (const std::function<bool (const std::filesystem::path &)> & existsPredicate);

    // Replaces the list outright. Caller is responsible for ordering /
    // de-dup / cap; we still cap on the way in defensively.
    void                 ReplaceAll   (std::vector<Entry> entries);

    size_t               Size         () const { return m_entries.size(); }
    bool                 Empty        () const { return m_entries.empty(); }

    // Bridge helpers between DiskMru and the GlobalUserPrefs JSON schema
    // (parallel arrays: `recentDisks` UTF-8 paths + `recentDiskLoadedAt`
    // Unix-second load times). A times vector shorter than the paths
    // vector (e.g. a legacy prefs file) leaves the missing entries at
    // load time 0.
    static DiskMru  FromUtf8 (const std::vector<std::string> & utf8Entries,
                              const std::vector<std::int64_t> & loadedAtUnix = {});
    void            ToUtf8   (std::vector<std::string>       & outUtf8Entries,
                              std::vector<std::int64_t>      & outLoadedAtUnix) const;

private:
    void                 EnforceCap   ();

    std::vector<Entry>   m_entries;   // index 0 == most recent
};
