#pragma once

#include "Pch.h"

#include "IFileSystem.h"
#include "IRegistrySettings.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore
//
//  Per-machine `_user.json` shadow store. Operates at the JsonValue
//  layer — the caller passes parsed default-config JSON in, gets
//  parsed merged-config JSON back, and re-runs MachineConfigLoader on
//  the result if it needs a typed MachineConfig.
//
//  File layout:
//
//      <userDir>/<machineName>_user.json
//
//  Merge rules (FR-014, FR-017):
//      * Objects deep-merge — only keys present in the user file
//        override the default.
//      * Arrays replace wholesale.
//      * Scalars in the user file always win.
//
//  Diff rules (SaveDelta):
//      * Only keys whose effective value differs from the default are
//        written out.
//      * `$cassoMachineVersion` is always written.
//      * If no other keys differ, the on-disk file contains just the
//        version stamp (still a legal user file).
//
//  Migration:
//      * On Load, if the user file's `$cassoMachineVersion` is less than
//        the default's, MachineConfigUpgrade::MigrateUserConfig runs
//        and the upgraded result is written back via WriteAllText.
//      * Legacy `$cassoDefault` is accepted as an alias during migration
//        reads only. Any alias usage is rewritten to canonical
//        `$cassoMachineVersion` before merge/persist.
//
//  All I/O is funnelled through `IFileSystem` — no direct file APIs.
//
////////////////////////////////////////////////////////////////////////////////

class UserConfigStore
{
public:
    explicit UserConfigStore (const std::wstring & userDir);

    HRESULT      Load              (const std::string & machineName,
                                    const JsonValue   & defaultJson,
                                    IFileSystem       & fs,
                                    JsonValue         & outMerged) const;
    HRESULT      SaveDelta         (const std::string & machineName,
                                    const JsonValue   & currentJson,
                                    const JsonValue   & defaultJson,
                                    IFileSystem       & fs) const;
    HRESULT      Reset             (const std::string & machineName,
                                    IFileSystem       & fs) const;
    std::wstring UserFilePath      (const std::string & machineName) const;

    // One-shot migration: reads the legacy per-machine registry keys
    // (DriveAudioEnabled, DiskIIMechanism, DiskImage0, DiskImage1) and
    // writes them into `<machineName>_user.json`. No-op (S_FALSE) when
    // the user file already exists, or when no legacy registry values
    // are present. Returns S_OK after a successful write.
    HRESULT      MigrateFromRegistry (const std::string & machineName,
                                      IRegistrySettings & reg,
                                      IFileSystem       & fs) const;

    // ---- Pure helpers (exposed for testing) ----------------------------

    static JsonValue MergeJson    (const JsonValue & defaultV,
                                   const JsonValue & userV);
    static JsonValue DiffJson     (const JsonValue & currentV,
                                   const JsonValue & defaultV);
    static bool      JsonEqual    (const JsonValue & a,
                                   const JsonValue & b);

private:
    std::wstring m_userDir;
};
