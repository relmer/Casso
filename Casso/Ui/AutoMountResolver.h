#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"






////////////////////////////////////////////////////////////////////////////////
//
//  AutoMountResolver
//
//  Pure-logic helper for the FR-047 auto-mount path (P6-T7). Given the
//  per-machine `lastMountedImages` list and an `IFileSystem` to stat
//  each path against, returns a `Decision` per drive saying whether to
//  mount, leave empty, or clear the stale remembered entry.
//
//  Extracting this out of `EmulatorShell` keeps the file existence test
//  injection-friendly: AutoMountTests passes an in-memory IFileSystem
//  instead of touching the real disk.
//
////////////////////////////////////////////////////////////////////////////////

class AutoMountResolver
{
public:
    enum class Action
    {
        LeaveEmpty,       // no remembered entry, or empty string
        Mount,            // file exists -- caller should mount it
        ClearStaleEntry,  // file missing -- caller should drop the entry
    };

    struct Decision
    {
        Action       action;
        std::wstring path;   // populated for Mount + ClearStaleEntry
    };

    // Resolve one drive. `rememberedPath` may be empty.
    static Decision                    Resolve                (const std::wstring & rememberedPath,
                                                              IFileSystem        & fs);
    static std::array<std::wstring, 2> SnapshotForPersistence (const std::wstring & drive0Path,
                                                              const std::wstring & drive1Path);
};
