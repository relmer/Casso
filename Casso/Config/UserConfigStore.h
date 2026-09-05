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

    // What a load left behind. Four outcomes have to be told apart before the
    // caller can say anything true about the user's file, and a bare HRESULT
    // tells them apart from nothing: the file parsed; it did not parse and was
    // moved aside; it did not parse and could not be moved; there was no file
    // and the migration that would have made one did not finish.
    struct LoadReport
    {
        // Where the parse broke ("line 12, column 5: ..."). Empty unless the
        // file exists and will not parse.
        std::wstring                 parseDetail;

        // Where the unreadable file was moved to. A non-empty path holds every
        // byte the file had and the original is gone, so saving works normally
        // from here. Empty beside a failed load means the file was not moved.
        std::wstring                 preservedPath;

        // Legacy files the migration could not read and deliberately left on
        // disk. Non-empty beside a SUCCEEDED load: what came across came
        // across, and these did not.
        std::vector<std::wstring>    skippedLegacyFiles;

        // Whether a unified prefs file was on disk when the load began. False
        // means any failure came from the legacy migration, so there is no
        // file for the caller to point the user at.
        bool                         hadPrefsFile = false;
    };

    HRESULT      LoadAll           (GlobalUserPrefs  & prefs,
                                    IFileSystem      & fs,
                                    LoadReport       & outReport);
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
    // Removes the machine's saved overrides from the written file, not only
    // from this store's cache. Callers holding a merged config for it must
    // re-Load before their next SaveDelta.
    HRESULT      Reset             (const std::string & machineName,
                                    IFileSystem       & fs) const;
    std::wstring GetUserFilePath      (const std::string & machineName) const;
    std::wstring GetUserPrefsFilePath () const;

    // The clock the preserved copy's filename is stamped from. Tests install a
    // fixed source so the name is deterministic; unset, the wall clock.
    void  SetTimestampSource (std::function<time_t ()> source)
    {
        m_timestamp = std::move (source);
    }

    // What to tell the user when LoadAll failed. Lives here rather than in the
    // shell because the shell is not compiled into the test project, and the
    // outcomes it tells apart are exactly what a test should pin.
    static std::wstring ComposeLoadFailureMessage (const std::wstring & prefsPath,
                                                   const LoadReport   & report);

    // What to tell the user when a load SUCCEEDED but the migration left files
    // behind. Empty when nothing was skipped, so the caller shows nothing.
    static std::wstring ComposeSkippedLegacyMessage (const std::wstring & userDir,
                                                     const LoadReport   & report);

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

    // `UserPrefs.20260903-141530.original.json`. The stamp keeps one launch's
    // rescue from overwriting another's, and the trailing `.json` is what the
    // user double-clicks to go repair it. The name must not end in the legacy
    // `_user.json` suffix: MigrateLegacyFiles adopts every file that does,
    // fails the whole migration on one that will not parse, and deletes it.
    static std::wstring  GetPreservedPrefsFilename (time_t when);

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
    HRESULT      MigrateLegacyFiles  (GlobalUserPrefs           & prefs,
                                      IFileSystem               & fs,
                                      bool                      & outFoundLegacy,
                                      std::vector<std::wstring> & outSkipped) const;
    // A null `prefs` preserves the on-disk global section instead of writing
    // one. A non-null `omitMachine` leaves that machine out of the document,
    // which is how Reset removes an entry the on-disk read-back would
    // otherwise carry straight back in.
    HRESULT      SaveCombinedJson    (const GlobalUserPrefs * prefs,
                                      IFileSystem           & fs,
                                      const std::string     * omitMachine) const;
    HRESULT      BuildCombinedJson   (const GlobalUserPrefs * prefs,
                                      IFileSystem           & fs,
                                      const std::string     * omitMachine,
                                      JsonValue             & outRoot) const;

    // Moves an unreadable prefs file aside under a stamped name. The copy is
    // written before the original is removed, so a copy that did not land
    // costs nothing.
    HRESULT      PreserveUnreadableFile (const std::string & text,
                                         IFileSystem       & fs,
                                         std::wstring      & outPreservedPath) const;
    HRESULT      LoadCombinedJson    (const JsonValue & root,
                                      GlobalUserPrefs & prefs) const;

    std::wstring                                m_userDir;
    mutable std::map<std::string, JsonValue>    m_machinePrefs;

    // Whether this store has established what the document on disk holds,
    // either through LoadAll or through Load's first-touch read. The cache
    // being EMPTY is not the same question and was the wrong one to ask: it
    // made a re-read depend on how many machines happened to be cached rather
    // than on whether anything had been read at all.
    mutable bool                                m_hasReadFile    = false;

    // Set when a load failed and the file could NOT be moved aside, so what
    // this store holds is not a faithful continuation of what is on disk.
    // Every save is refused while it is set. Without it the refusal lasts only
    // as long as the file stays unreadable: a lock that clears mid-session
    // leaves the store holding whatever the caller reset it to, and the next
    // save writes that over settings that were intact the whole time.
    bool                                        m_loadUnresolved = false;

    GlobalUserPrefs                           * m_prefs          = nullptr;

    std::function<time_t ()>                    m_timestamp;
};
