#pragma once

#include "Pch.h"

#include "IFileSystem.h"
#include "GlobalUserPrefs.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore
//
//  Unified user preferences store. Operates at the JsonValue layer for
//  per-machine overrides — the caller passes parsed default-config JSON
//  in, gets parsed merged-config JSON back, and re-runs
//  MachineConfigLoader on the result if it needs a typed MachineConfig.
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
//      * On Load, if a machine entry's `$cassoMachineVersion` is less
//        than the default's, MachineConfigUpgrade::MigrateUserConfig runs
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

    HRESULT      LoadAll           (GlobalUserPrefs  & prefs,
                                    IFileSystem      & fs);
    HRESULT      SaveAll           (const GlobalUserPrefs & prefs,
                                    IFileSystem           & fs) const;
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
    std::wstring UserPrefsFilePath () const;

    // ---- Pure helpers (exposed for testing) ----------------------------

    static JsonValue MergeJson    (const JsonValue & defaultV,
                                   const JsonValue & userV);
    static JsonValue DiffJson     (const JsonValue & currentV,
                                   const JsonValue & defaultV);
    static bool      JsonEqual    (const JsonValue & a,
                                   const JsonValue & b);

private:
    HRESULT      MigrateLegacyFiles  (GlobalUserPrefs & prefs,
                                      IFileSystem     & fs) const;
    HRESULT      SaveCombinedJson    (const GlobalUserPrefs & prefs,
                                      IFileSystem           & fs) const;
    JsonValue    BuildCombinedJson   (const GlobalUserPrefs & prefs,
                                      IFileSystem           & fs) const;
    HRESULT      LoadCombinedJson    (const JsonValue & root,
                                      GlobalUserPrefs & prefs) const;

    std::wstring                       m_userDir;
    mutable std::map<std::string, JsonValue>  m_machinePrefs;
    GlobalUserPrefs                  * m_prefs = nullptr;
};
