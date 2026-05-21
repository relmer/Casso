#pragma once

#include "Pch.h"

#include "IFileSystem.h"

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
//
//  All I/O is funnelled through `IFileSystem` — no direct file APIs.
//
////////////////////////////////////////////////////////////////////////////////

class UserConfigStore
{
public:
    explicit UserConfigStore (const std::wstring & userDir);

    // Load + merge. `defaultJson` is the parsed embedded default for the
    // machine. On return, `outMerged` contains the merged JsonValue
    // ready to feed to MachineConfigLoader. If a user file exists with
    // an older `$cassoMachineVersion`, this method runs the migration
    // and writes the upgraded text back to disk before merging.
    HRESULT Load (
        const std::string  & machineName,
        const JsonValue    & defaultJson,
        IFileSystem        & fs,
        JsonValue          & outMerged) const;

    // Diff `currentJson` vs `defaultJson` and write only the differences
    // (plus `$cassoMachineVersion`) to <userDir>/<machineName>_user.json.
    HRESULT SaveDelta (
        const std::string  & machineName,
        const JsonValue    & currentJson,
        const JsonValue    & defaultJson,
        IFileSystem        & fs) const;

    // Delete the per-machine user file. Succeeds (S_OK) even if the file
    // did not exist.
    HRESULT Reset (
        const std::string  & machineName,
        IFileSystem        & fs) const;

    // Resolve the absolute on-disk path of the user file for a machine.
    // Exposed so callers and tests can stat the file directly.
    std::wstring UserFilePath (const std::string & machineName) const;

    // ---- Pure helpers (exposed for testing) ----------------------------

    // Returns a new JsonValue equal to `defaultV` with every leaf that
    // appears in `userV` replaced. Object keys deep-merge; arrays in
    // `userV` replace the corresponding default array wholesale.
    static JsonValue MergeJson (
        const JsonValue & defaultV,
        const JsonValue & userV);

    // Returns a JsonValue containing only keys/values from `currentV`
    // that differ from `defaultV`. Always returns an object (possibly
    // empty). `$cassoMachineVersion` is preserved if present in
    // `currentV` even when equal.
    static JsonValue DiffJson (
        const JsonValue & currentV,
        const JsonValue & defaultV);

    // Structural equality. Object key order is ignored.
    static bool JsonEqual (
        const JsonValue & a,
        const JsonValue & b);

private:
    std::wstring m_userDir;
};
