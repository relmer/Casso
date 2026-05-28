#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMru
//
//  Pure most-recently-used list helper for mounted disk image paths.
//  Most-recent-first ordering, capped at k_capacity. Re-mounting an
//  existing entry moves it to index 0. New mount at cap evicts the
//  oldest. `Prune` takes an injected predicate so unit tests drive it
//  without touching the real file system.
//
//  Persistence is the owning singleton's job (GlobalUserPrefs writes /
//  reads the `recentDisks` JSON array); DiskMru itself is Win32-free
//  and IO-free.
//
////////////////////////////////////////////////////////////////////////////////



class DiskMru
{
public:
    static constexpr size_t  k_capacity = 16;

    void                                RecordMount  (const std::filesystem::path & path);
    std::vector<std::filesystem::path>  Snapshot     () const;

    // Removes entries the predicate returns false for, preserving the
    // surviving order. Returns the post-prune snapshot. Returns
    // unchanged copy when the predicate is null.
    std::vector<std::filesystem::path>  Prune        (const std::function<bool (const std::filesystem::path &)> & existsPredicate);

    // Replaces the list outright. Caller is responsible for ordering /
    // de-dup / cap; we still cap on the way in defensively.
    void                                ReplaceAll   (std::vector<std::filesystem::path> entries);

    size_t                              Size         () const { return m_entries.size(); }
    bool                                Empty        () const { return m_entries.empty(); }

    // Bridge helpers between DiskMru and the GlobalUserPrefs JSON
    // schema (which stores entries as narrow UTF-8 strings).
    static DiskMru  FromUtf8 (const std::vector<std::string> & utf8Entries);
    void            ToUtf8   (std::vector<std::string>       & outUtf8Entries) const;

private:
    void                                EnforceCap   ();

    std::vector<std::filesystem::path>  m_entries;   // index 0 == most recent
};
