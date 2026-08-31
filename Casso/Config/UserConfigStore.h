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

    // outParseDetail is empty unless the prefs file exists but does not
    // parse, in which case it carries a human-readable location ("line 12,
    // column 5: ...") so the caller can tell the user WHERE their JSON
    // broke rather than silently discarding their settings.
    HRESULT      LoadAll           (GlobalUserPrefs  & prefs,
                                    IFileSystem      & fs,
                                    std::wstring     & outParseDetail);
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
    std::wstring GetUserFilePath      (const std::string & machineName) const;
    std::wstring GetUserPrefsFilePath () const;

    // Pure helpers (exposed for testing)

    static JsonValue MergeJson    (const JsonValue & defaultV,
                                   const JsonValue & userV);
    static JsonValue DiffJson     (const JsonValue & currentV,
                                   const JsonValue & defaultV);
    static bool      AreJsonEqual    (const JsonValue & a,
                                      const JsonValue & b);

private:
    // Schema keys, string/path plumbing, and the JSON merge/delta helpers.
    // Every reader is a UserConfigStore method. These names in particular
    // -- Widen, Narrow, JoinPath, EndsWith, FindObjectKey -- are the kind
    // that collide across translation units, which is precisely what the
    // anonymous namespace was papering over rather than solving.
    static constexpr const char *  kpszVersionKey       = "$cassoMachineVersion";
    static constexpr const char *  kpszLegacyVersionKey = "$cassoDefault";
    static constexpr const char *  kpszUiPrefsKey       = "$cassoUiPrefs";
    static constexpr const char *  kpszGlobalKey        = "global";
    static constexpr const char *  kpszMachinesKey      = "machines";

    static std::wstring  Widen  (const std::string & narrow);
    static std::string   Narrow (const std::wstring & wide);

    static std::wstring  JoinPath (
        const std::wstring & baseDir,
        const std::wstring & filename);

    static std::wstring  GetUserPrefsFilename         ();
    static std::wstring  GetLegacyGlobalPrefsFilename ();
    static std::wstring  GetLegacyUserSuffix          ();

    static bool  EndsWith (
        const std::wstring & text,
        const std::wstring & suffix);

    static std::wstring  StripSuffix (
        const std::wstring & text,
        const std::wstring & suffix);

    static int  FindObjectKey (
        const std::vector<std::pair<std::string, JsonValue>> & entries,
        const std::string                                    & key);

    static const JsonValue *  FindObjectValue (
        const JsonValue   & obj,
        const std::string & key);

    static int  ExtractVersion (const JsonValue & v);

    static int  ExtractVersionForKey (
        const JsonValue   & v,
        const std::string & key);

    static bool  HasLegacyVersionAlias (const JsonValue & v);

    static JsonValue  CanonicalizeVersionStamp (
        const JsonValue & userJson,
        int               fallbackVersion);

    // Locates `key` in `obj` and confirms it holds `wanted`. The three
    // TryGet*Field helpers below differ only in the type they ask for and the
    // getter they call, so the lookup lives here once.
    static bool  TryFindTypedField (const JsonValue    &  obj,
                                 const std::string  &  key,
                                 JsonType              wanted,
                                 const JsonValue    *& outValue);

    static bool  TryGetBoolField   (const JsonValue & obj, const std::string & key, bool & out);
    static bool  TryGetIntField    (const JsonValue & obj, const std::string & key, int & out);
    static bool  TryGetStringField (const JsonValue & obj, const std::string & key, std::string & out);

    static JsonValue  BuildObjectWithEnabled (
        const JsonValue & src,
        bool              enabled);

    static JsonValue  BuildUiPrefsDefaults ();

    static int  FindInternalByType (const JsonValue & arr, const std::string & type);
    static int  FindSlotByNumber   (const JsonValue & arr, int slot);

    static JsonValue  MergeHardwareArray (
        const JsonValue & defaultArr,
        const JsonValue & userArr,
        bool              slotArray);

    static JsonValue  BuildHardwareDeltaArray (
        const JsonValue & currentArr,
        const JsonValue & defaultArr,
        bool              slotArray);

    static bool  IsObjectArray (const JsonValue & v);

    // One DiffJson step for a key present in BOTH current and defaults.
    static void  DiffMatchedKey (
        std::vector<std::pair<std::string, JsonValue>> & diff,
        const std::string                              & key,
        const JsonValue                                & cv,
        const JsonValue                                & dv);

    // `outFoundLegacy` is false when there was nothing from an older
    // layout to pull forward -- a first run, not a migration failure.
    HRESULT      MigrateLegacyFiles  (GlobalUserPrefs & prefs,
                                      IFileSystem     & fs,
                                      bool            & outFoundLegacy) const;
    HRESULT      SaveCombinedJson    (const GlobalUserPrefs & prefs,
                                      IFileSystem           & fs) const;
    JsonValue    BuildCombinedJson   (const GlobalUserPrefs & prefs,
                                      IFileSystem           & fs) const;
    HRESULT      LoadCombinedJson    (const JsonValue & root,
                                      GlobalUserPrefs & prefs) const;

    std::wstring                                m_userDir;
    mutable std::map<std::string, JsonValue>    m_machinePrefs;
    GlobalUserPrefs                           * m_prefs        = nullptr;
};
