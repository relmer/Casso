#include "Pch.h"

#include "UserConfigStore.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfigUpgrade.h"

#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::Widen (const std::string & narrow)
{
    std::wstring  out;



    out.reserve (narrow.size());
    for (char c : narrow)
    {
        out.push_back ((wchar_t) (unsigned char) c);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::Narrow
//
////////////////////////////////////////////////////////////////////////////////

std::string UserConfigStore::Narrow (const std::wstring & wide)
{
    std::string  out;



    out.reserve (wide.size());
    for (wchar_t c : wide)
    {
        out.push_back ((char) (unsigned char) c);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::JoinPath (
    const std::wstring & baseDir,
    const std::wstring & filename)
{
    std::wstring  result = baseDir;



    if (!result.empty() &&
        result.back() != L'\\' &&
        result.back() != L'/')
    {
        result += L'\\';
    }

    result += filename;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetUserPrefsFilename
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetUserPrefsFilename()
{
    return std::wstring (L"User") + L"Prefs" + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetPreservedPrefsFilename
//
//  Where an unreadable prefs file goes, stamped with the local time it was
//  set aside.
//
//  The stamp comes from PreservedCopy, which the disk layer already uses for
//  the same job, so both kinds of rescue copy sort as text in the order they
//  happened and read the same way in a directory listing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetPreservedPrefsFilename (time_t when)
{
    std::string  stamp = PreservedCopy::MakeStamp (when);



    return std::wstring (L"User") + L"Prefs" + L"." + Widen (stamp) + L".original" + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::ComposeLoadFailureMessage
//
//  Two outcomes, and the difference is the whole point of the message.
//
//  With a preserved path every byte survived under a file the user can open,
//  and saving works from here, so the message says where the copy went and how
//  to put it back. Without one the original is still in place and every save
//  refuses over it, so the message says that instead of implying the session
//  will keep anything.
//
//  The instruction to close Casso first is not decoration. A repaired copy
//  renamed back while the session runs is a readable file again, which is
//  exactly the condition that lets the next save overwrite it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::ComposeLoadFailureMessage (
    const std::wstring  & userDir,
    const std::wstring  & prefsPath,
    const LoadReport    & report)
{
    std::wstring  message;
    bool          wasPreserved = !report.preservedPath.empty();
    bool          hasDetail    = !report.parseDetail.empty();



    message = std::wstring (L"Settings file unreadable\n\n");

    if (!report.hadPrefsFile)
    {
        // No unified file, so the failure came from the migration that would
        // have written one. Pointing the user at UserPrefs.json here would
        // send them after a file that does not exist, and saving is not
        // refused either, since the gate only guards a file that is present.
        // The DIRECTORY, not prefsPath: this branch runs precisely because
        // that file does not exist, so printing it sends the user after
        // something they cannot find.
        message = std::wstring (L"Settings could not be carried forward\n\n")
                + L"Casso could not move your settings from an older layout "
                  L"and has started with defaults. Your old settings files are "
                  L"still in:\n\n"
                + userDir
                + L"\n\nCasso will try again the next time it starts.";
    }
    else if (wasPreserved)
    {
        message += L"Casso could not read your settings and has started with "
                   L"defaults. Your file was saved as:\n\n"
                 + report.preservedPath
                 + L"\n\nRepair that copy, close Casso, and rename it to "
                   L"UserPrefs.json to get your settings back.";
    }
    else
    {
        message += L"Casso could not read your settings and has started with "
                   L"defaults. Your file is still where it was:\n\n"
                 + prefsPath
                 + L"\n\nSettings will not be saved over it while it cannot be "
                   L"read. Repair or move it, then restart Casso.";
    }

    if (hasDetail)
    {
        message += L"\n\n" + report.parseDetail;
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::ComposeSkippedLegacyMessage
//
//  What to say when the migration carried forward what it could and left the
//  rest alone.
//
//  This is the SUCCESS path, which is why it needs saying at all. The load
//  worked, Casso is running, and nothing else will ever mention the files that
//  did not come across: the unified file now exists, so the migration gate
//  never opens again and those files sit unread forever. Reporting them once
//  is the difference between settings the user knows did not survive and
//  settings that silently reverted.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::ComposeSkippedLegacyMessage (
    const std::wstring  & userDir,
    const LoadReport    & report)
{
    HRESULT       hr         = S_OK;
    std::wstring  message;
    bool          hasSkipped = !report.skippedLegacyFiles.empty();



    BAIL_OUT_IF (!hasSkipped, S_OK);

    message = std::wstring (L"Some settings could not be carried forward\n\n")
            + L"Casso moved your settings from an older layout, but could not "
              L"read these files and has left them in place:\n";

    for (const auto & filename : report.skippedLegacyFiles)
    {
        message += L"\n    " + filename;
    }

    message += L"\n\nThey are in:\n\n"
             + userDir
             + L"\n\nWhatever was in them has been reset to defaults.";

Error:
    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetLegacyGlobalPrefsFilename
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetLegacyGlobalPrefsFilename()
{
    return std::wstring (L"Global") + L"User" + L"Prefs" + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetLegacyUserSuffix
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetLegacyUserSuffix()
{
    return std::wstring (L"_") + L"user" + L".json";
}


bool UserConfigStore::EndsWith (
    const std::wstring & text,
    const std::wstring & suffix)
{
    return text.size() >= suffix.size()
        && text.compare (text.size() - suffix.size(), suffix.size(), suffix) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::StripSuffix
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::StripSuffix (
    const std::wstring & text,
    const std::wstring & suffix)
{
    return text.substr (0, text.size() - suffix.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::FindObjectKey
//
////////////////////////////////////////////////////////////////////////////////

int  UserConfigStore::FindObjectKey (
    const std::vector<std::pair<std::string, JsonValue>> & entries,
    const std::string                                    & key)
{
    int  i     = 0;
    int  found = -1;



    for (i = 0; i < (int) entries.size() && found < 0; ++i)
    {
        if (entries[(size_t) i].first == key)
        {
            found = i;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::FindObjectValue
//
////////////////////////////////////////////////////////////////////////////////

const JsonValue * UserConfigStore::FindObjectValue (
    const JsonValue   & obj,
    const std::string & key)
{
    const JsonValue *  value = nullptr;
    int                idx   = -1;



    if (obj.GetType() == JsonType::Object)
    {
        idx = FindObjectKey (obj.GetObjectEntries(), key);

        if (idx >= 0)
        {
            value = &obj.GetObjectEntries()[(size_t) idx].second;
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::ExtractVersion
//
////////////////////////////////////////////////////////////////////////////////

int  UserConfigStore::ExtractVersion (const JsonValue & v)
{
    return ExtractVersionForKey (v, kpszVersionKey);
}


int  UserConfigStore::ExtractVersionForKey (
    const JsonValue   & v,
    const std::string & key)
{
    const std::vector<std::pair<std::string, JsonValue>> * entries = nullptr;
    int                                                    found   = -1;
    // 0 means "no usable version here" for all three misses: not an object,
    // no such key, or a key holding something that is not a number.
    int                                                    version = 0;



    if (v.GetType() == JsonType::Object)
    {
        entries = &v.GetObjectEntries();
        found   = FindObjectKey (*entries, key);

        if (found >= 0 && (*entries)[(size_t) found].second.GetType() == JsonType::Number)
        {
            version = (int) (*entries)[(size_t) found].second.GetNumber();
        }
    }

    return version;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::HasLegacyVersionAlias
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::HasLegacyVersionAlias (const JsonValue & v)
{
    return v.GetType() == JsonType::Object
        && FindObjectKey (v.GetObjectEntries(), kpszLegacyVersionKey) >= 0;
}


JsonValue UserConfigStore::CanonicalizeVersionStamp (
    const JsonValue & userJson,
    int               fallbackVersion)
{
    std::vector<std::pair<std::string, JsonValue>>  out;
    JsonValue                                       result           = userJson;
    int                                             canonicalVersion = 0;
    bool                                            fWroteVersion    = false;
    bool                                            isObject         = userJson.GetType() == JsonType::Object;
    HRESULT                                         hr               = S_OK;



    // A non-object round-trips unchanged.
    BAIL_OUT_IF (!isObject, S_OK);

    canonicalVersion = ExtractVersionForKey (userJson, kpszVersionKey);
    if (canonicalVersion <= 0)
    {
        canonicalVersion = ExtractVersionForKey (userJson, kpszLegacyVersionKey);
    }

    if (fallbackVersion > 0 && canonicalVersion < fallbackVersion)
    {
        canonicalVersion = fallbackVersion;
    }

    out.reserve (userJson.GetObjectEntries().size() + 1);

    for (const auto & kv : userJson.GetObjectEntries())
    {
        if (kv.first == kpszVersionKey)
        {
            if (!fWroteVersion && canonicalVersion > 0)
            {
                out.emplace_back (kpszVersionKey, JsonValue ((double) canonicalVersion));
                fWroteVersion = true;
            }

            continue;
        }

        if (kv.first == kpszLegacyVersionKey)
        {
            continue;
        }

        out.emplace_back (kv.first, kv.second);
    }

    if (!fWroteVersion && canonicalVersion > 0)
    {
        out.insert (out.begin(),
                    std::make_pair (std::string (kpszVersionKey),
                                    JsonValue ((double) canonicalVersion)));
    }

    result = JsonValue (std::move (out));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::TryFindTypedField
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::TryFindTypedField (
    const JsonValue    &  obj,
    const std::string  &  key,
    JsonType              wanted,
    const JsonValue    *& outValue)
{
    // FindObjectValue already answers null for "not an object" and "no such
    // key", so only the type still needs checking here.
    const JsonValue *  found   = FindObjectValue (obj, key);
    bool               isTyped = found != nullptr && found->GetType() == wanted;



    outValue = isTyped ? found : nullptr;

    return isTyped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::TryGetBoolField
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::TryGetBoolField (
    const JsonValue   & obj,
    const std::string & key,
    bool              & out)
{
    const JsonValue *  field = nullptr;
    bool               found = TryFindTypedField (obj, key, JsonType::Bool, field);



    if (found)
    {
        out = field->GetBool();
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::TryGetIntField
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::TryGetIntField (
    const JsonValue   & obj,
    const std::string & key,
    int               & out)
{
    const JsonValue *  field = nullptr;
    bool               found = TryFindTypedField (obj, key, JsonType::Number, field);



    if (found)
    {
        out = (int) field->GetNumber();
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::TryGetStringField
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::TryGetStringField (
    const JsonValue   & obj,
    const std::string & key,
    std::string       & out)
{
    const JsonValue *  field = nullptr;
    bool               found = TryFindTypedField (obj, key, JsonType::String, field);



    if (found)
    {
        out = field->GetString();
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::BuildObjectWithOverrides
//
//  Returns `src` carrying the two per-entry fields a user file is allowed to
//  restate: the `enabled` bit, and a card's `ports` connector list when the
//  user entry declares one.
//
//  Everything else deliberately comes from the default. A user file records a
//  DIFFERENCE, so `device`, `rom` and the capability flags have to keep
//  tracking the shipped machine rather than freezing at whatever they held
//  the last time the settings dialog wrote the file.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::BuildObjectWithOverrides (
    const JsonValue & src,
    bool              enabled,
    const JsonValue * ports)
{
    std::vector<std::pair<std::string, JsonValue>>    rebuilt;
    const auto                                      * entries = &src.GetObjectEntries();



    rebuilt.reserve (entries->size() + 2);
    for (size_t i = 0; i < entries->size(); ++i)
    {
        const std::string &  key = (*entries)[i].first;

        if (key == "enabled" || (key == "ports" && ports != nullptr))
        {
            continue;
        }

        rebuilt.emplace_back (key, (*entries)[i].second);
    }

    rebuilt.emplace_back ("enabled", JsonValue (enabled));

    if (ports != nullptr)
    {
        rebuilt.emplace_back ("ports", *ports);
    }

    return JsonValue (std::move (rebuilt));
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeepColorModeExplicit
//
//  Carries `colorMode` into a UI-prefs delta even when it matches the shared
//  default table.
//
//  EVERY OTHER UI PREFERENCE CAN BE STORED AS A DELTA against
//  BuildUiPrefsDefaults, because for those the table IS the default: dropping
//  a value that matches it loses nothing, since an absent key and the table's
//  value mean the same thing on the way back in.
//
//  The color mode stopped being one of those the moment its default became a
//  property of the machine's monitor. A green tube reads an absent key as
//  green, so a user who deliberately picks Color on a //c writes a delta that
//  is empty, saves nothing, and gets green back on the next launch -- their
//  choice silently discarded precisely because it agreed with a table that no
//  longer decides anything. A value whose default depends on the hardware
//  cannot be encoded as a difference from hardware-independent defaults, so
//  this one is always written out once the user has touched the machine's
//  settings at all.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char *  kpszColorModeKey = "colorMode";


static JsonValue  KeepColorModeExplicit (JsonValue uiDiff, const JsonValue & current)
{
    std::string  colorMode;
    bool         haveColor = current.HasString (kpszColorModeKey, colorMode);



    if (!haveColor || uiDiff.GetType() != JsonType::Object)
    {
        return uiDiff;
    }

    for (const auto & entry : uiDiff.GetObjectEntries())
    {
        if (entry.first == kpszColorModeKey)
        {
            return uiDiff;
        }
    }

    {
        std::vector<std::pair<std::string, JsonValue>>  entries = uiDiff.GetObjectEntries();

        entries.emplace_back (kpszColorModeKey, JsonValue (colorMode));

        return JsonValue (std::move (entries));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::BuildUiPrefsDefaults
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::BuildUiPrefsDefaults()
{
    std::vector<std::pair<std::string, JsonValue>> uiObj;
    std::vector<JsonValue>                         wp;



    uiObj.emplace_back ("speedMode",          JsonValue (std::string ("authentic")));
    uiObj.emplace_back (kpszColorModeKey,     JsonValue (std::string ("color")));
    uiObj.emplace_back ("writeMode",          JsonValue (std::string ("buffer-and-flush")));
    uiObj.emplace_back ("floppySoundEnabled", JsonValue (true));
    uiObj.emplace_back ("floppyMechanism",    JsonValue (std::string ("shugart")));
    wp.emplace_back (JsonValue (false));
    wp.emplace_back (JsonValue (false));
    uiObj.emplace_back ("writeProtect", JsonValue (std::move (wp)));

    return JsonValue (std::move (uiObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::FindInternalByType
//
////////////////////////////////////////////////////////////////////////////////

int UserConfigStore::FindInternalByType (
    const JsonValue   & arr,
    const std::string & type)
{
    std::string  candidate;
    size_t       i     = 0;
    int          found = -1;



    if (arr.GetType() == JsonType::Array)
    {
        for (i = 0; i < arr.GetArraySize() && found < 0; ++i)
        {
            const JsonValue & e = arr.GetArrayElement (i);

            if (e.GetType() == JsonType::Object)
            {
                candidate.clear();

                if (TryGetStringField (e, "type", candidate) && candidate == type)
                {
                    found = (int) i;
                }
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::FindSlotByNumber
//
////////////////////////////////////////////////////////////////////////////////

int UserConfigStore::FindSlotByNumber (
    const JsonValue & arr,
    int               slot)
{
    size_t  i         = 0;
    int     candidate = -1;
    int     found     = -1;



    if (arr.GetType() == JsonType::Array)
    {
        for (i = 0; i < arr.GetArraySize() && found < 0; ++i)
        {
            const JsonValue & e = arr.GetArrayElement (i);

            if (e.GetType() == JsonType::Object)
            {
                candidate = -1;

                if (TryGetIntField (e, "slot", candidate) && candidate == slot)
                {
                    found = (int) i;
                }
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::MergeHardwareArray
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::MergeHardwareArray (
    const JsonValue & defaultArr,
    const JsonValue & userArr,
    bool              slotArray)
{
    std::vector<JsonValue>  merged;
    std::vector<bool>       userMatched;
    JsonValue               result   = userArr;
    HRESULT                 hr       = S_OK;
    bool                    bothArrays = defaultArr.GetType() == JsonType::Array &&
                                         userArr.GetType()    == JsonType::Array;



    // Nothing to merge unless both sides really are arrays; the user's copy
    // wins unchanged.
    BAIL_OUT_IF (!bothArrays, S_OK);

    userMatched.resize (userArr.GetArraySize(), false);
    merged.reserve (defaultArr.GetArraySize() + userArr.GetArraySize());

    for (size_t i = 0; i < defaultArr.GetArraySize(); ++i)
    {
        const JsonValue & defEntry = defaultArr.GetArrayElement (i);
        int               userIdx  = -1;
        bool              enabled  = true;

        if (defEntry.GetType() == JsonType::Object)
        {
            if (slotArray)
            {
                int slot = -1;
                if (TryGetIntField (defEntry, "slot", slot))
                {
                    userIdx = FindSlotByNumber (userArr, slot);
                }
            }
            else
            {
                std::string type;
                if (TryGetStringField (defEntry, "type", type))
                {
                    userIdx = FindInternalByType (userArr, type);
                }
            }
        }

        if (userIdx >= 0)
        {
            const JsonValue & userEntry = userArr.GetArrayElement ((size_t) userIdx);
            const JsonValue * userPorts = nullptr;
            userMatched[(size_t) userIdx] = true;

            // Only a slot entry has connectors, and only the user's own list
            // may override the default's -- an absent or malformed `ports`
            // leaves the card describing the hardware it shipped with.
            if (slotArray)
            {
                (void) TryFindTypedField (userEntry, "ports", JsonType::Array, userPorts);
            }

            if (TryGetBoolField (userEntry, "enabled", enabled) &&
                defEntry.GetType() == JsonType::Object)
            {
                merged.emplace_back (BuildObjectWithOverrides (defEntry, enabled, userPorts));
            }
            else
            {
                merged.emplace_back (userEntry);
            }
        }
        else
        {
            merged.emplace_back (defEntry);
        }
    }

    for (size_t i = 0; i < userArr.GetArraySize(); ++i)
    {
        if (!userMatched[i])
        {
            merged.emplace_back (userArr.GetArrayElement (i));
        }
    }

    result = JsonValue (std::move (merged));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::BuildHardwareDeltaArray
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::BuildHardwareDeltaArray (
    const JsonValue & currentArr,
    const JsonValue & defaultArr,
    bool              slotArray)
{
    std::vector<JsonValue>  delta;
    JsonValue               result     = currentArr;
    HRESULT                 hr         = S_OK;
    bool                    bothArrays = currentArr.GetType() == JsonType::Array &&
                                         defaultArr.GetType() == JsonType::Array;



    // No delta to compute unless both sides are arrays; the current value
    // stands as its own delta.
    BAIL_OUT_IF (!bothArrays, S_OK);

    for (size_t i = 0; i < currentArr.GetArraySize(); ++i)
    {
        const JsonValue & curEntry    = currentArr.GetArrayElement (i);
        const JsonValue * curPorts    = nullptr;
        const JsonValue * defPorts    = nullptr;
        int               defIdx      = -1;
        bool              curEn       = true;
        bool              defEn       = true;
        bool              portsDiffer = false;

        if (curEntry.GetType() != JsonType::Object)
        {
            continue;
        }

        if (slotArray)
        {
            int slot = -1;
            if (TryGetIntField (curEntry, "slot", slot))
            {
                defIdx = FindSlotByNumber (defaultArr, slot);
            }
        }
        else
        {
            std::string type;
            if (TryGetStringField (curEntry, "type", type))
            {
                defIdx = FindInternalByType (defaultArr, type);
            }
        }

        (void) TryGetBoolField (curEntry, "enabled", curEn);
        if (defIdx >= 0 && defaultArr.GetArrayElement ((size_t) defIdx).GetType() == JsonType::Object)
        {
            const JsonValue &  defEntry = defaultArr.GetArrayElement ((size_t) defIdx);

            (void) TryGetBoolField (defEntry, "enabled", defEn);

            if (slotArray)
            {
                (void) TryFindTypedField (curEntry, "ports", JsonType::Array, curPorts);
                (void) TryFindTypedField (defEntry, "ports", JsonType::Array, defPorts);
            }
        }

        // A card's connector list is a setting in its own right: detaching
        // the second Disk ][ drive edits `ports` and leaves `enabled` alone.
        // Diffing the enabled bit by itself threw that edit away -- the
        // dialog applied it live, nothing reached the user file, and the
        // drive was back the next time the machine or the dialog loaded.
        portsDiffer = curPorts != nullptr &&
                      (defPorts == nullptr || !AreJsonEqual (*curPorts, *defPorts));

        if (curEn != defEn || portsDiffer)
        {
            std::vector<std::pair<std::string, JsonValue>>  obj;
            std::string                                     type;
            int                                             slot = -1;

            if (slotArray)
            {
                if (TryGetIntField (curEntry, "slot", slot))
                {
                    obj.emplace_back ("slot", JsonValue ((double) slot));
                }
            }
            else
            {
                if (TryGetStringField (curEntry, "type", type))
                {
                    obj.emplace_back ("type", JsonValue (type));
                }
            }

            obj.emplace_back ("enabled", JsonValue (curEn));

            if (portsDiffer)
            {
                obj.emplace_back ("ports", *curPorts);
            }

            delta.emplace_back (JsonValue (std::move (obj)));
        }
    }

    result = JsonValue (std::move (delta));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::IsObjectArray
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::IsObjectArray (const JsonValue & v)
{
    size_t  i         = 0;
    bool    allAreObj = v.GetType() == JsonType::Array;



    for (i = 0; allAreObj && i < v.GetArraySize(); ++i)
    {
        allAreObj = v.GetArrayElement (i).GetType() == JsonType::Object;
    }

    return allAreObj;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::UserConfigStore
//
////////////////////////////////////////////////////////////////////////////////

UserConfigStore::UserConfigStore (const std::wstring & userDir)
    : m_userDir (userDir)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetUserPrefsFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetUserPrefsFilePath() const
{
    return JoinPath (m_userDir, GetUserPrefsFilename());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::GetUserFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::GetUserFilePath (const std::string & machineName) const
{
    UNREFERENCED_PARAMETER (machineName);
    return GetUserPrefsFilePath();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LoadAll
//
//  Unified user prefs JSON shape:
//
//  {
//    "global": { ...GlobalUserPrefs fields... },
//    "machines": {
//      "Apple //e Enhanced": { "$cassoMachineVersion": 2, ...user overrides... },
//      "Apple ][+":          { "$cassoMachineVersion": 1, ...user overrides... }
//    }
//  }
//
//  Machine entries are keyed by display name, matching the existing
//  variantOverrides pattern. The per-machine version stamp remains
//  inside each entry so MachineConfigUpgrade::MigrateUserConfig can run
//  independently for each machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::LoadAll (
    GlobalUserPrefs  & prefs,
    IFileSystem      & fs,
    LoadReport       & outReport)
{
    HRESULT          hr         = S_OK;
    HRESULT          hrPreserve = S_OK;
    std::wstring     path       = GetUserPrefsFilePath();
    std::string      text;
    JsonValue        root;
    JsonParseError   err;
    bool             hasText    = false;



    m_prefs = &prefs;
    m_machinePrefs.clear();
    m_hasReadFile = false;
    outReport = LoadReport {};

    // Recorded before anything can change it, because a failure past this
    // point moves or replaces the file and the caller still has to say which
    // of the two states it started from.
    outReport.hadPrefsFile = fs.Exists (path);

    if (!outReport.hadPrefsFile)
    {
        bool  fFoundLegacy = false;

        hr = MigrateLegacyFiles (prefs, fs, fFoundLegacy, outReport.skippedLegacyFiles);
        CHR (hr);

        // The migration wrote the document, so this store knows its contents
        // as surely as if it had read them.
        m_hasReadFile = true;

        // No unified file and nothing legacy to carry forward: a genuine
        // first run, so start from struct defaults.
        if (!fFoundLegacy)
        {
            prefs = GlobalUserPrefs {};
        }

        BAIL_OUT_IF (true, S_OK);
    }

    hr = fs.ReadAllText (path, text);
    CHR (hr);

    // Only now. A read that never happened has established nothing, and a
    // store that believes otherwise stops going back for the file when a
    // transient lock clears -- which would leave every machine on shipped
    // defaults for the rest of the session over a file that reads fine.
    hasText       = true;
    m_hasReadFile = true;

    // A file that exists but will not parse is the case worth explaining:
    // the user still has settings, we just cannot read them. Capture where
    // the parse broke so the caller can say so instead of quietly starting
    // over with defaults.
    hr = JsonParser::Parse (text, root, err);
    CHRF (hr, outReport.parseDetail = std::format (L"{}\n\nline {}, column {}: {}",
                                                   path,
                                                   err.line,
                                                   err.column,
                                                   std::wstring (err.message.begin(), err.message.end())));

    hr = LoadCombinedJson (root, prefs);
    CHR (hr);

Error:
    // A file that read but did not load is the one case where the bytes are in
    // hand and the original is worth nothing where it is. Moving it aside is
    // what buys back the right to write: the user keeps every byte under a
    // name they can open, and the next save creates a fresh file rather than
    // overwriting settings they can still repair.
    //
    // A preservation that FAILED leaves the original in place, and the latch
    // set below refuses every save until a later load succeeds. That outlasts
    // the condition on purpose: the file being readable again is not the same
    // as this store holding what is in it, and the caller has by then reset
    // its prefs to defaults.
    if (FAILED (hr) && hasText)
    {
        hrPreserve = PreserveUnreadableFile (text, fs, outReport.preservedPath);
        IGNORE_RETURN_VALUE (hrPreserve, S_OK);
    }

    // A load that failed and left the file where it was means this store's
    // prefs are not a continuation of that file. Saves stay refused until a
    // later load succeeds, because the alternative is writing the caller's
    // fallback defaults over settings that were only ever unreachable.
    m_loadUnresolved = FAILED (hr) && outReport.preservedPath.empty();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::PreserveUnreadableFile
//
//  Sets the unreadable prefs file aside so a later save cannot destroy it.
//
//  The copy is written BEFORE the original is removed. Ordering it the other
//  way makes the copy worthless: the original is the thing being protected, so
//  a copy that did not land must not cost the user the file it was copying.
//
//  outPreservedPath is set only when both steps succeeded, so a caller can
//  read a non-empty path as "every byte is safe under this name".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::PreserveUnreadableFile (
    const std::string  & text,
    IFileSystem        & fs,
    std::wstring       & outPreservedPath) const
{
    HRESULT       hr        = S_OK;
    HRESULT       hrCleanup = S_OK;
    std::wstring  preserved;
    time_t        when      = 0;
    bool          wroteCopy = false;



    outPreservedPath.clear();

    when      = m_timestamp ? m_timestamp() : time (nullptr);
    preserved = JoinPath (m_userDir, GetPreservedPrefsFilename (when));

    hr = fs.WriteAllText (preserved, text);
    CHR (hr);

    wroteCopy = true;

    hr = fs.Delete (GetUserPrefsFilePath());
    CHR (hr);

    outPreservedPath = preserved;

Error:
    // The copy exists to make removing the original safe. An original that
    // could not be removed is still intact, so the copy now protects nothing,
    // and leaving it would stack up one more every launch for as long as the
    // file stays both unreadable and undeletable.
    if (FAILED (hr) && wroteCopy)
    {
        hrCleanup = fs.Delete (preserved);
        IGNORE_RETURN_VALUE (hrCleanup, S_OK);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::SaveAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::SaveAll (
    const GlobalUserPrefs & prefs,
    IFileSystem           & fs) const
{
    return SaveCombinedJson (&prefs, fs, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::Load
//
//  Produces the effective machine config: the shipped defaults with the user's
//  saved deltas merged over them, migrating an out-of-date delta on the way.
//
//  Deltas are stored rather than whole configs, which is the design decision
//  everything else here follows from. A user who changed one slot keeps
//  receiving every other improvement when a machine definition ships updated;
//  storing the full config would freeze them at whatever the file looked like
//  the day they touched it.
//
//  That is also why migration is needed at all. A delta names keys in a schema,
//  so when the shipped schema moves the delta has to move with it, or it starts
//  referring to keys that no longer exist. Three conditions trigger it: an
//  older version stamp, a MISSING stamp (which predates versioning), and a
//  legacy alias key.
//
//  Migration failure is deliberately non-fatal -- the un-migrated content is
//  used as-is. A config that fails to upgrade is far better than a machine
//  that will not load, and the canonicalize-and-save below is idempotent, so a
//  half-migrated file settles on the next run rather than compounding.
//
//  The lazy first-load path handles the case where nothing is cached yet and a
//  prefs file exists on disk. It accepts both shapes the file has had -- the
//  current combined form with a machines key, and the older bare per-machine
//  object -- so an upgrade from an older build finds its settings.
//
//  A machine with no saved prefs at all returns the defaults untouched, which
//  is the common case and costs nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::Load (
    const std::string  & machineName,
    const JsonValue    & defaultJson,
    IFileSystem        & fs,
    JsonValue          & outMerged) const
{
    HRESULT              hr            = S_OK;
    std::string          userContent;
    std::string          migrated;
    JsonValue            userJson;
    JsonValue            canonicalJson;
    JsonParseError       parseErr;
    JsonWriter::Options  opts;
    int                  defaultVer    = 0;
    int                  userVer       = 0;
    bool                 fNeedMigrate  = false;
    bool                 fRewritten    = false;
    bool                 fHasLegacyKey = false;
    bool                 hasUserPrefs  = false;
    auto                 found         = m_machinePrefs.find (machineName);



    if (found == m_machinePrefs.end() && !m_hasReadFile && fs.Exists (GetUserPrefsFilePath()))
    {
        GlobalUserPrefs  fallbackPrefs;
        JsonValue        root;


        hr = fs.ReadAllText (GetUserPrefsFilePath(), userContent);
        CHR (hr);

        m_hasReadFile = true;

        hr = JsonParser::Parse (userContent, root, parseErr);
        CHR (hr);

        if (FindObjectValue (root, kpszMachinesKey) != nullptr)
        {
            hr = LoadCombinedJson (root, fallbackPrefs);

            // The machines are cached by the time this returns, and the only
            // thing it can fail on here is the global section -- which this
            // path parses into a local and throws away. Failing over it would
            // cost a caller that came for one machine's delta the delta it
            // came for, and every such caller treats the failure as "nothing
            // saved for this machine" rather than "read the file again".
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else if (root.GetType() == JsonType::Object)
        {
            m_machinePrefs[machineName] = root;
        }

        found = m_machinePrefs.find (machineName);
    }

    hasUserPrefs = (found != m_machinePrefs.end());

    if (!hasUserPrefs)
    {
        // Nothing saved for this machine, so the defaults are the answer.
        outMerged = defaultJson;
    }

    BAIL_OUT_IF (!hasUserPrefs, S_OK);

    userJson = found->second;
    defaultVer = ExtractVersion (defaultJson);
    userVer    = ExtractVersion (userJson);
    fHasLegacyKey = HasLegacyVersionAlias (userJson);
    fNeedMigrate = (defaultVer > 0 && userVer > 0 && userVer < defaultVer)
                || (userVer == 0)
                || fHasLegacyKey;

    if (fNeedMigrate)
    {
        opts.fPretty = true;
        hr = JsonWriter::Write (userJson, opts, userContent);
        CHR (hr);

        migrated = userContent;

        // The default's port list is the template the external-drive fold
        // materializes: a user array replaces the default's wholesale, so a
        // delta naming one port would leave the machine with one connector.
        {
            const JsonValue *  defaultPorts = nullptr;

            if (!defaultJson.HasArray ("ports", defaultPorts))
            {
                defaultPorts = nullptr;
            }

            // Whether anything actually moved does not change what happens
            // next -- the canonicalize/save below is idempotent either way.
            hr = MachineConfigUpgrade::MigrateUserConfig (
                     userContent, defaultPorts, migrated, fRewritten);
        }

        if (FAILED (hr))
        {
            migrated = userContent;
            hr = S_OK;
        }

        hr = JsonParser::Parse (migrated, userJson, parseErr);
        CHR (hr);

        canonicalJson = CanonicalizeVersionStamp (userJson, defaultVer);
        m_machinePrefs[machineName] = canonicalJson;
        userJson = canonicalJson;

        // Non-fatal, as the banner above says: the canonicalize-and-save is
        // idempotent, so a write that could not land settles on the next run.
        // Failing here instead would turn an unreadable prefs file into a
        // failed machine load, which EmulatorShell answers with an assert.
        hr = SaveCombinedJson (m_prefs, fs, nullptr);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    outMerged = MergeJson (defaultJson, userJson);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::SaveDelta
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::SaveDelta (
    const std::string  & machineName,
    const JsonValue    & currentJson,
    const JsonValue    & defaultJson,
    IFileSystem        & fs) const
{
    HRESULT      hr       = S_OK;
    JsonValue    delta;
    JsonValue    saved;
    auto         found    = m_machinePrefs.find (machineName);
    bool         hasEntry = false;



    hasEntry = (found != m_machinePrefs.end());

    if (hasEntry)
    {
        saved = found->second;
    }

    delta = DiffJson (currentJson, defaultJson);
    m_machinePrefs[machineName] = delta;

    hr = SaveCombinedJson (m_prefs, fs, nullptr);
    CHR (hr);

Error:
    // A refused write leaves the old delta on disk, so the cache has to hold
    // the old delta too. Keeping the new one makes every later read answer
    // with a value that was never saved, and makes the next successful save
    // write it without the user asking again.
    if (FAILED (hr))
    {
        if (hasEntry)
        {
            m_machinePrefs[machineName] = saved;
        }
        else
        {
            m_machinePrefs.erase (machineName);
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::Reset
//
//  Discards a machine's saved overrides so it falls back to the shipped
//  defaults.
//
//  Erasing the cache entry is not enough on its own, which is the whole
//  reason this function is more than one line. BuildCombinedJson reads the
//  existing file back and merges every machine it finds there, so the entry
//  erased here returns from disk before the document is written and the file
//  comes out unchanged. The erasure set is what the read-back consults: the
//  machine goes in before the save and comes out after it, so exactly one
//  write suppresses exactly one entry.
//
//  The set is deliberately NOT session-lifetime state. After a successful
//  write neither the file nor the cache holds the entry, so every later save
//  already produces a document without it and a surviving tombstone would buy
//  nothing. It would cost something, though: anything that legitimately put
//  the machine back -- another store over the same directory, or the user
//  editing the file -- would be deleted again by the next save.
//
//  Two obligations fall on the caller, neither of which this function can
//  discharge. A merged config still held for this machine is stale, so saving
//  it back restores exactly what was erased; re-Load first. And another store
//  open over the same directory that still holds the machine in its own cache
//  will write it back on its next save, so reset through the store that owns
//  the machine's state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::Reset (
    const std::string  & machineName,
    IFileSystem        & fs) const
{
    HRESULT      hr       = S_OK;
    JsonValue    saved;
    auto         found    = m_machinePrefs.find (machineName);
    bool         hasEntry = false;
    bool         hasFile  = false;



    hasEntry = (found != m_machinePrefs.end());

    if (hasEntry)
    {
        saved = found->second;
        m_machinePrefs.erase (found);
    }

    // Nothing on disk is nothing to erase from, and writing an empty document
    // here would cost a user upgrading from an older build everything they
    // have. LoadAll gates the legacy-file migration on the unified file being
    // absent, so creating one strands GlobalUserPrefs.json and every
    // <machine>_user.json permanently unread.
    hasFile = fs.Exists (GetUserPrefsFilePath());
    BAIL_OUT_IF (!hasFile, S_OK);

    hr = SaveCombinedJson (m_prefs, fs, &machineName);
    CHR (hr);

Error:
    // The write is what removes the entry from the file, so a write that
    // failed leaves it there and the cache has to agree. Dropping it anyway
    // makes Load answer with the shipped defaults for the rest of the session
    // over overrides that are still on disk.
    if (FAILED (hr) && hasEntry)
    {
        m_machinePrefs[machineName] = saved;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::BuildCombinedJson
//
//  Assembles the whole prefs document -- global settings plus every machine's
//  deltas -- ready to write.
//
//  The on-disk file is READ BACK and merged under the in-memory entries, which
//  is the point of this function. m_machinePrefs is populated lazily, one
//  machine at a time as they are loaded, so a save that fires before some
//  machine has ever been loaded this session would otherwise write a document
//  omitting it -- silently deleting that machine's settings from disk. Reading
//  first preserves what this process never touched.
//
//  In-memory entries win over on-disk ones, since they are the newer state by
//  definition.
//
//  The global section is preserved the same way, and for the same reason. A
//  null `prefs` means this store never ran LoadAll, so it holds no global
//  state at all: most stores the app builds are short-lived helpers that only
//  read or rewrite one machine delta. Emitting struct defaults for those would
//  overwrite every global preference on disk with a value nobody chose, so the
//  on-disk object is carried through verbatim instead. With no prefs and
//  nothing on disk the key is omitted, which LoadCombinedJson already reads as
//  constructed defaults.
//
//  Reset is the one caller that needs an on-disk entry left out, which is what
//  `omitMachine` is for. The skip belongs to the on-disk pass alone: applying
//  it after the in-memory overlay below would let a Reset followed by a
//  SaveDelta of the same machine write the new delta and then drop it again,
//  which is the same silent loss in the other direction.
//
//  A read or parse failure REFUSES THE SAVE. This function reads the file back
//  precisely because nothing here records what was never loaded, so concluding
//  "there is nothing to preserve" from a failed read asserts the one thing it
//  exists because it does not know. Writing anyway replaces settings that are
//  still on disk and still repairable with a document built from whatever
//  happens to be in memory.
//
//  That does not trap the user. LoadAll moves an unreadable file aside under a
//  stamped name, after which the file is simply absent and saving works
//  normally. What this check cannot see is a store whose load failed over a
//  file it could not move, so SaveCombinedJson holds a separate latch for
//  that; the two are not redundant, and removing either reopens a path that
//  writes fallback defaults over settings that were never lost.
//
//  The merge goes through an ordered map, so machines land in a stable order
//  and the file does not churn between saves.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::BuildCombinedJson (
    const GlobalUserPrefs * prefs,
    IFileSystem           & fs,
    const std::string     * omitMachine,
    JsonValue             & outRoot) const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  machines;
    std::map<std::string, JsonValue>                merged;
    std::string                                     existingText;
    JsonValue                                       existing;
    JsonParseError                                  err;
    const JsonValue                               * existingMachines = nullptr;
    const JsonValue                               * existingGlobal   = nullptr;
    HRESULT                                         hr               = S_OK;
    bool                                            isObject         = false;



    // Preserve any machines present in the on-disk file that we haven't
    // touched in this process. m_machinePrefs is populated lazily; if a
    // save fires before a given machine has been Load'd, that machine
    // would otherwise be wiped from disk on the next write.
    if (fs.Exists (GetUserPrefsFilePath()))
    {
        hr = fs.ReadAllText (GetUserPrefsFilePath(), existingText);
        CHR (hr);

        hr = JsonParser::Parse (existingText, existing, err);
        CHREx (hr, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        isObject = (existing.GetType() == JsonType::Object);
        CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        existingGlobal = FindObjectValue (existing, kpszGlobalKey);

        existingMachines = FindObjectValue (existing, kpszMachinesKey);
        if (existingMachines != nullptr && existingMachines->GetType() == JsonType::Object)
        {
            for (const auto & kv : existingMachines->GetObjectEntries())
            {
                if (omitMachine != nullptr && kv.first == *omitMachine)
                {
                    continue;
                }

                if (kv.second.GetType() == JsonType::Object)
                {
                    merged[kv.first] = kv.second;
                }
            }
        }
    }

    // In-memory entries override on-disk entries.
    for (const auto & kv : m_machinePrefs)
    {
        merged[kv.first] = kv.second;
    }

    machines.reserve (merged.size());
    for (const auto & kv : merged)
    {
        machines.emplace_back (kv.first, kv.second);
    }

    if (prefs != nullptr)
    {
        root.emplace_back (kpszGlobalKey, prefs->ToJson());
    }
    else if (existingGlobal != nullptr)
    {
        root.emplace_back (kpszGlobalKey, *existingGlobal);
    }

    root.emplace_back (kpszMachinesKey, JsonValue (std::move (machines)));

    outRoot = JsonValue (std::move (root));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LoadCombinedJson
//
//  Reads a parsed prefs document into the global prefs and the per-machine
//  delta cache.
//
//  A global section that will not load is reported, but only AFTER the
//  machines are cached. The two sections are independent data in one document,
//  and the machines are readable on their own: LoadAll answers a failure by
//  setting the whole file aside, so anything cached here is what the next save
//  puts back. Reporting the failure first instead cost the user their slot
//  configuration and remembered disks over a fault in a different section.
//
//  Only the ROOT being a non-object is an error outright. Everything inside is
//  optional: a missing global section resets to constructed defaults, a
//  missing machines section leaves the cache alone, and any machine entry that
//  is not an object is skipped. A prefs file is user-writable and
//  version-skewed by nature, so one damaged entry must not cost the user every
//  other setting in the file.
//
//  Machine entries are MERGED into the cache rather than replacing it, so
//  loading does not discard deltas for machines this document happens not to
//  mention.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::LoadCombinedJson (
    const JsonValue & root,
    GlobalUserPrefs & prefs) const
{
    HRESULT            hr       = S_OK;
    HRESULT            hrGlobal = S_OK;
    const JsonValue  * global   = nullptr;
    const JsonValue  * machines = nullptr;
    bool               isObject = false;



    isObject = (root.GetType() == JsonType::Object);
    CBREx (isObject, E_INVALIDARG);

    global = FindObjectValue (root, kpszGlobalKey);
    if (global != nullptr)
    {
        hrGlobal = prefs.FromJson (*global);
    }

    if (global == nullptr || FAILED (hrGlobal))
    {
        prefs = GlobalUserPrefs {};
    }

    machines = FindObjectValue (root, kpszMachinesKey);
    if (machines != nullptr && machines->GetType() == JsonType::Object)
    {
        for (const auto & kv : machines->GetObjectEntries())
        {
            if (kv.second.GetType() == JsonType::Object)
            {
                m_machinePrefs[kv.first] = kv.second;
            }
        }
    }

    // Reported last, with the machines already cached.
    hr = hrGlobal;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::SaveCombinedJson
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::SaveCombinedJson (
    const GlobalUserPrefs * prefs,
    IFileSystem           & fs,
    const std::string     * omitMachine) const
{
    HRESULT              hr         = S_OK;
    JsonWriter::Options  opts;
    JsonValue            root;
    std::string          text;
    bool                 canAccount = !m_loadUnresolved;



    // A store whose load failed over a file it could not move aside holds
    // nothing it can honestly write. The file is intact and the caller has
    // since reset its prefs to defaults, so the document this would build is
    // those defaults over settings that were never actually lost.
    CBREx (canAccount, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    hr = BuildCombinedJson (prefs, fs, omitMachine, root);
    CHR (hr);

    opts.fPretty = true;
    hr = JsonWriter::Write (root, opts, text);
    CHR (hr);

    hr = fs.WriteAllText (GetUserPrefsFilePath(), text);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::MigrateLegacyFiles
//
//  One-time upgrade from the old layout -- a global prefs file plus one file
//  per machine -- into the single combined document.
//
//  The order is write-then-delete, and it is the difference between an
//  interrupted upgrade being harmless and being a data loss. The combined file
//  is fully written first; only then are the legacy files removed. A crash
//  between the two leaves both copies on disk, and the next run finds legacy
//  files still present and simply migrates again -- the operation is
//  idempotent. Deleting first would lose everything on any failure to write.
//
//  Legacy per-machine files are DISCOVERED by suffix rather than by asking
//  which machines exist, so a machine that has since been removed from the
//  product still has its settings carried forward instead of stranded.
//
//  Each legacy delta is version-stamped as it is read. Those files predate
//  versioning entirely, so without a stamp every one of them would look
//  un-migratable to Load forever after.
//
//  Finding nothing to migrate is a FIRST RUN, not a failure, and is reported
//  through outFoundLegacy rather than a second success code -- which keeps the
//  caller from having to distinguish two flavors of S_OK.
//
//  A missing legacy global with legacy machine files present still migrates:
//  the global section is written from constructed defaults so the combined
//  document is complete either way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::MigrateLegacyFiles (
    GlobalUserPrefs           & prefs,
    IFileSystem               & fs,
    bool                      & outFoundLegacy,
    std::vector<std::wstring> & outSkipped) const
{
    HRESULT                   hr                = S_OK;
    HRESULT                   hrGlobal          = S_OK;
    std::wstring              legacyGlobalPath  = JoinPath (m_userDir, GetLegacyGlobalPrefsFilename());
    std::wstring              legacySuffix      = GetLegacyUserSuffix();
    std::vector<std::wstring> filenames;
    std::vector<std::wstring> legacyUserFiles;
    std::vector<std::wstring> migratedFiles;
    std::string               text;
    std::string               combinedText;
    JsonValue                 parsed;
    JsonValue                 canonical;
    JsonValue                 legacyGlobalJson;
    JsonParseError            err;
    JsonWriter::Options       opts;
    bool                      fHaveLegacyGlobal = false;
    bool                      fHaveLegacyUsers  = false;
    std::wstring              trace;



    std::vector<std::pair<std::string, JsonValue>>  rootEntries;
    std::vector<std::pair<std::string, JsonValue>>  machines;



    outFoundLegacy    = false;
    outSkipped.clear();
    fHaveLegacyGlobal = fs.Exists (legacyGlobalPath);

    hr = fs.EnumerateFiles (m_userDir, filenames);
    if (FAILED (hr))
    {
        filenames.clear();
        hr = S_OK;
    }

    for (const auto & filename : filenames)
    {
        if (EndsWith (filename, legacySuffix))
        {
            legacyUserFiles.push_back (filename);
        }
    }

    // Nothing from an older layout to pull forward. That is a first run,
    // not a failure, so it leaves via outFoundLegacy rather than a
    // second success code.
    fHaveLegacyUsers = !legacyUserFiles.empty();
    BAIL_OUT_IF (!fHaveLegacyGlobal && !fHaveLegacyUsers, S_OK);

    outFoundLegacy = true;

    // A legacy file that will not read is skipped rather than fatal, and it
    // stays on disk. Failing the whole migration over one of them cost the user
    // every OTHER file too: nothing was written and nothing was deleted, and
    // once anything else created the unified file, the gate in LoadAll never
    // opened again and the readable files were stranded unread. Casso can do
    // nothing with a file it cannot parse, so leaving it in place is both the
    // most it can offer and the only record the user has left of it.
    if (fHaveLegacyGlobal)
    {
        hrGlobal = fs.ReadAllText (legacyGlobalPath, text);

        if (SUCCEEDED (hrGlobal))
        {
            hrGlobal = JsonParser::Parse (text, parsed, err);
        }

        if (SUCCEEDED (hrGlobal))
        {
            hrGlobal = prefs.FromJson (parsed);
        }

        if (SUCCEEDED (hrGlobal))
        {
            // What FromJson made of the document, not the document. The two
            // differ whenever loading converts something -- a v1 CRT block
            // becoming the sparse override map, most recently -- and passing
            // the original through would write the pre-conversion form into
            // the file that replaces it, then delete the only other copy.
            // Unknown keys still survive: FromJson keeps them and ToJson
            // emits them again.
            //
            // The branch below already does exactly this for a first run, so
            // agreeing with it is also what makes the two paths produce the
            // same document from the same preferences.
            legacyGlobalJson = prefs.ToJson();
        }
    }

    if (fHaveLegacyGlobal && FAILED (hrGlobal))
    {
        outSkipped.push_back (GetLegacyGlobalPrefsFilename());
    }

    if (!fHaveLegacyGlobal || FAILED (hrGlobal))
    {
        prefs = GlobalUserPrefs {};
        legacyGlobalJson = prefs.ToJson();
    }

    for (const auto & filename : legacyUserFiles)
    {
        std::wstring  path        = JoinPath (m_userDir, filename);
        std::string   machineName = Narrow (StripSuffix (filename, legacySuffix));
        HRESULT       hrFile      = fs.ReadAllText (path, text);


        if (SUCCEEDED (hrFile))
        {
            hrFile = JsonParser::Parse (text, parsed, err);
        }

        if (FAILED (hrFile))
        {
            outSkipped.push_back (filename);
            continue;
        }

        canonical = CanonicalizeVersionStamp (parsed, 1);
        m_machinePrefs[machineName] = canonical;
        migratedFiles.push_back (filename);
    }

    machines.reserve (m_machinePrefs.size());
    for (const auto & kv : m_machinePrefs)
    {
        machines.emplace_back (kv.first, kv.second);
    }

    rootEntries.emplace_back (kpszGlobalKey,   legacyGlobalJson);
    rootEntries.emplace_back (kpszMachinesKey, JsonValue (std::move (machines)));

    opts.fPretty = true;
    hr = JsonWriter::Write (JsonValue (std::move (rootEntries)), opts, combinedText);
    CHR (hr);

    hr = fs.WriteAllText (GetUserPrefsFilePath(), combinedText);
    CHR (hr);

    // Only what actually came across is removed. A skipped file is the user's
    // sole copy of whatever is in it.
    if (fHaveLegacyGlobal && SUCCEEDED (hrGlobal))
    {
        hr = fs.Delete (legacyGlobalPath);
        CHR (hr);
    }

    for (const auto & filename : migratedFiles)
    {
        std::wstring  path = JoinPath (m_userDir, filename);

        hr = fs.Delete (path);
        CHR (hr);
    }

    trace = L"[UserConfigStore] Migrated user prefs:";
    if (fHaveLegacyGlobal && SUCCEEDED (hrGlobal))
    {
        trace += L" global";
    }

    for (const auto & filename : migratedFiles)
    {
        trace += L" ";
        trace += filename;
    }

    if (!outSkipped.empty())
    {
        trace += L" -- left in place, unreadable:";

        for (const auto & filename : outSkipped)
        {
            trace += L" ";
            trace += filename;
        }
    }

    trace += L"\n";
    OutputDebugStringW (trace.c_str());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::MergeJson
//
//  Returns a new JsonValue equal to `defaultV` with every leaf that
//  appears in `userV` replaced. Object keys deep-merge; arrays in `userV`
//  replace the corresponding default array wholesale.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::MergeJson (
    const JsonValue & defaultV,
    const JsonValue & userV)
{
    std::vector<std::pair<std::string, JsonValue>>          merged;
    const std::vector<std::pair<std::string, JsonValue>>  * defaultEntries = nullptr;
    const std::vector<std::pair<std::string, JsonValue>>  * userEntries    = nullptr;
    int                                                     idx            = 0;
    size_t                                                  i              = 0;
    JsonValue                                               result         = userV;
    HRESULT                                                 hr             = S_OK;
    bool                                                    bothObjects    = defaultV.GetType() == JsonType::Object &&
                                                                             userV.GetType()    == JsonType::Object;



    // Scalar / array / type mismatch: user value wins (copy).
    BAIL_OUT_IF (!bothObjects, S_OK);

    defaultEntries = &defaultV.GetObjectEntries();
    userEntries    = &userV.GetObjectEntries();

    merged.reserve (defaultEntries->size() + userEntries->size());

    // Walk defaults in original order, replacing with merged-user value
    // when a corresponding user key exists.
    for (i = 0; i < defaultEntries->size(); ++i)
    {
        const std::string & key = (*defaultEntries)[i].first;

        idx = FindObjectKey (*userEntries, key);
        if (idx >= 0)
        {
            const JsonValue & dv = (*defaultEntries)[i].second;
            const JsonValue & uv = (*userEntries)[(size_t) idx].second;

            if (key == "internalDevices" && IsObjectArray (dv) && IsObjectArray (uv))
            {
                merged.emplace_back (key, MergeHardwareArray (dv, uv, false));
            }
            else if (key == "slots" && IsObjectArray (dv) && IsObjectArray (uv))
            {
                merged.emplace_back (key, MergeHardwareArray (dv, uv, true));
            }
            else
            {
                merged.emplace_back (key, MergeJson (dv, uv));
            }
        }
        else
        {
            merged.emplace_back (key, (*defaultEntries)[i].second);
        }
    }

    // Append user-only keys (preserves user-introduced fields like
    // `lastMountedImages` per FR-047).
    for (i = 0; i < userEntries->size(); ++i)
    {
        const std::string & key = (*userEntries)[i].first;

        idx = FindObjectKey (*defaultEntries, key);
        if (idx < 0)
        {
            merged.emplace_back (key, (*userEntries)[i].second);
        }
    }

    result = JsonValue (std::move (merged));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::DiffJson
//
//  Returns a JsonValue containing only keys/values from `currentV` that
//  differ from `defaultV`. Always returns an object. `$cassoMachineVersion`
//  is preserved from `currentV` even when equal.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::DiffJson (
    const JsonValue & currentV,
    const JsonValue & defaultV)
{
    std::vector<std::pair<std::string, JsonValue>>          diff;
    const std::vector<std::pair<std::string, JsonValue>>  * curEntries = nullptr;
    const std::vector<std::pair<std::string, JsonValue>>  * defEntries = nullptr;
    int                                                     idx        = 0;
    size_t                                                  i          = 0;
    JsonValue                                               result;
    HRESULT                                                 hr         = S_OK;
    bool                                                    isObject   = currentV.GetType() == JsonType::Object;



    // Not an object: the empty `diff` below still yields an object, which is
    // what callers expect.
    if (!isObject)
    {
        result = JsonValue (std::move (diff));
    }

    BAIL_OUT_IF (!isObject, S_OK);

    curEntries = &currentV.GetObjectEntries();

    if (defaultV.GetType() == JsonType::Object)
    {
        defEntries = &defaultV.GetObjectEntries();
    }

    for (i = 0; i < curEntries->size(); ++i)
    {
        const std::string & key = (*curEntries)[i].first;
        const JsonValue   & cv  = (*curEntries)[i].second;

        if (defEntries == nullptr)
        {
            // Defaults isn't an object: keep everything from current.
            diff.emplace_back (key, cv);
            continue;
        }

        idx = FindObjectKey (*defEntries, key);

        if (idx < 0)
        {
            if (key == kpszUiPrefsKey && cv.GetType() == JsonType::Object)
            {
                JsonValue uiDiff = KeepColorModeExplicit (DiffJson (cv, BuildUiPrefsDefaults()), cv);
                if (!uiDiff.GetObjectEntries().empty())
                {
                    diff.emplace_back (key, std::move (uiDiff));
                }

                continue;
            }

            // User-only key — always include.
            diff.emplace_back (key, cv);
            continue;
        }

        DiffMatchedKey (diff, key, cv, (*defEntries)[(size_t) idx].second);
    }

    result = JsonValue (std::move (diff));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::DiffMatchedKey
//
//  One DiffJson step for a key present in BOTH current and defaults. The
//  version key always passes through; the two hardware arrays and uiPrefs
//  diff through their dedicated builders; same-type objects recurse; and
//  anything else is kept only when it differs from the default. Factored
//  out of DiffJson's loop so `dv` binds at the top of a function rather
//  than mid-loop behind the existence guards.
//
////////////////////////////////////////////////////////////////////////////////

void UserConfigStore::DiffMatchedKey (
    std::vector<std::pair<std::string, JsonValue>> & diff,
    const std::string                              & key,
    const JsonValue                                & cv,
    const JsonValue                                & dv)
{
    bool  fIsVersionKey = (key == kpszVersionKey);
    bool  fSameType     = (cv.GetType() == dv.GetType());



    if (fIsVersionKey)
    {
        // Always pass through.
        diff.emplace_back (key, cv);
    }
    else if (key == "internalDevices" && IsObjectArray (cv) && IsObjectArray (dv))
    {
        JsonValue hwDelta = BuildHardwareDeltaArray (cv, dv, false);

        if (hwDelta.GetType() == JsonType::Array && hwDelta.GetArraySize() > 0)
        {
            diff.emplace_back (key, std::move (hwDelta));
        }
    }
    else if (key == "slots" && IsObjectArray (cv) && IsObjectArray (dv))
    {
        JsonValue hwDelta = BuildHardwareDeltaArray (cv, dv, true);

        if (hwDelta.GetType() == JsonType::Array && hwDelta.GetArraySize() > 0)
        {
            diff.emplace_back (key, std::move (hwDelta));
        }
    }
    else if (key == kpszUiPrefsKey && cv.GetType() == JsonType::Object)
    {
        JsonValue uiDiff = KeepColorModeExplicit (DiffJson (cv, BuildUiPrefsDefaults()), cv);

        if (!uiDiff.GetObjectEntries().empty())
        {
            diff.emplace_back (key, std::move (uiDiff));
        }
    }
    else if (fSameType && cv.GetType() == JsonType::Object)
    {
        JsonValue  nested = DiffJson (cv, dv);

        if (!nested.GetObjectEntries().empty())
        {
            diff.emplace_back (key, std::move (nested));
        }
    }
    else if (!AreJsonEqual (cv, dv))
    {
        diff.emplace_back (key, cv);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::AreJsonEqual
//
//  Structural equality. Object key order is ignored.
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::AreJsonEqual (
    const JsonValue & a,
    const JsonValue & b)
{
    size_t  i     = 0;
    int     idx   = 0;
    // Mismatched types are never equal, and that also makes every arm below
    // safe to read `b` with the same accessor it reads `a`.
    bool    equal = a.GetType() == b.GetType();



    if (equal)
    {
        switch (a.GetType())
        {
            case JsonType::Null:
                break;                                        // both null

            case JsonType::Bool:
                equal = a.GetBool() == b.GetBool();
                break;

            case JsonType::Number:
                equal = a.GetNumber() == b.GetNumber();
                break;

            case JsonType::String:
                equal = a.GetString() == b.GetString();
                break;

            case JsonType::Array:
                equal = a.GetArraySize() == b.GetArraySize();

                for (i = 0; equal && i < a.GetArraySize(); ++i)
                {
                    equal = AreJsonEqual (a.GetArrayElement (i), b.GetArrayElement (i));
                }

                break;

            case JsonType::Object:
            {
                const auto & ae = a.GetObjectEntries();
                const auto & be = b.GetObjectEntries();

                // Key ORDER is ignored: each of a's keys is looked up in b.
                // Equal sizes plus every a-key present makes that sufficient.
                equal = ae.size() == be.size();

                for (i = 0; equal && i < ae.size(); ++i)
                {
                    idx   = FindObjectKey (be, ae[i].first);
                    equal = idx >= 0 && AreJsonEqual (ae[i].second, be[(size_t) idx].second);
                }

                break;
            }
        }
    }

    return equal;
}

