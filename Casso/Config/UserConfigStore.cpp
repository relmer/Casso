#include "Pch.h"

#include "UserConfigStore.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfigUpgrade.h"





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
//  UserConfigStore::UserPrefsFilename
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::UserPrefsFilename()
{
    return std::wstring (L"User") + L"Prefs" + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LegacyGlobalPrefsFilename
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::LegacyGlobalPrefsFilename()
{
    return std::wstring (L"Global") + L"User" + L"Prefs" + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LegacyUserSuffix
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::LegacyUserSuffix()
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


std::wstring UserConfigStore::StripSuffix (
    const std::wstring & text,
    const std::wstring & suffix)
{
    return text.substr (0, text.size() - suffix.size());
}


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


JsonValue UserConfigStore::BuildObjectWithEnabled (
    const JsonValue & src,
    bool              enabled)
{
    std::vector<std::pair<std::string, JsonValue>>    rebuilt;
    const auto                                      * entries = &src.GetObjectEntries();


    rebuilt.reserve (entries->size() + 1);
    for (size_t i = 0; i < entries->size(); ++i)
    {
        if ((*entries)[i].first == "enabled")
        {
            continue;
        }

        rebuilt.emplace_back ((*entries)[i].first, (*entries)[i].second);
    }

    rebuilt.emplace_back ("enabled", JsonValue (enabled));

    return JsonValue (std::move (rebuilt));
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
    uiObj.emplace_back ("colorMode",          JsonValue (std::string ("color")));
    uiObj.emplace_back ("writeMode",          JsonValue (std::string ("buffer-and-flush")));
    uiObj.emplace_back ("floppySoundEnabled", JsonValue (true));
    uiObj.emplace_back ("floppyMechanism",    JsonValue (std::string ("shugart")));
    wp.emplace_back (JsonValue (false));
    wp.emplace_back (JsonValue (false));
    uiObj.emplace_back ("writeProtect", JsonValue (std::move (wp)));

    return JsonValue (std::move (uiObj));
}


int UserConfigStore::FindInternalByType (
    const JsonValue   & arr,
    const std::string & type)
{
    std::string  candidate;
    size_t       i     = 0;
    int          found = -1;


    if (arr.GetType() == JsonType::Array)
    {
        for (i = 0; i < arr.ArraySize() && found < 0; ++i)
        {
            const JsonValue & e = arr.ArrayAt (i);

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


int UserConfigStore::FindSlotByNumber (
    const JsonValue & arr,
    int               slot)
{
    size_t  i         = 0;
    int     candidate = -1;
    int     found     = -1;


    if (arr.GetType() == JsonType::Array)
    {
        for (i = 0; i < arr.ArraySize() && found < 0; ++i)
        {
            const JsonValue & e = arr.ArrayAt (i);

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

    userMatched.resize (userArr.ArraySize(), false);
    merged.reserve (defaultArr.ArraySize() + userArr.ArraySize());

    for (size_t i = 0; i < defaultArr.ArraySize(); ++i)
    {
        const JsonValue & defEntry = defaultArr.ArrayAt (i);
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
            const JsonValue & userEntry = userArr.ArrayAt ((size_t) userIdx);
            userMatched[(size_t) userIdx] = true;

            if (TryGetBoolField (userEntry, "enabled", enabled) &&
                defEntry.GetType() == JsonType::Object)
            {
                merged.emplace_back (BuildObjectWithEnabled (defEntry, enabled));
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

    for (size_t i = 0; i < userArr.ArraySize(); ++i)
    {
        if (!userMatched[i])
        {
            merged.emplace_back (userArr.ArrayAt (i));
        }
    }

    result = JsonValue (std::move (merged));

Error:
    return result;
}


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

    for (size_t i = 0; i < currentArr.ArraySize(); ++i)
    {
        const JsonValue & curEntry = currentArr.ArrayAt (i);
        int               defIdx   = -1;
        bool              curEn    = true;
        bool              defEn    = true;

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
        if (defIdx >= 0 && defaultArr.ArrayAt ((size_t) defIdx).GetType() == JsonType::Object)
        {
            (void) TryGetBoolField (defaultArr.ArrayAt ((size_t) defIdx), "enabled", defEn);
        }

        if (curEn != defEn)
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



    for (i = 0; allAreObj && i < v.ArraySize(); ++i)
    {
        allAreObj = v.ArrayAt (i).GetType() == JsonType::Object;
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
//  UserConfigStore::UserPrefsFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::UserPrefsFilePath() const
{
    return JoinPath (m_userDir, UserPrefsFilename());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::UserFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::UserFilePath (const std::string & machineName) const
{
    UNREFERENCED_PARAMETER (machineName);
    return UserPrefsFilePath();
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
    std::wstring     & outParseDetail)
{
    HRESULT          hr     = S_OK;
    std::wstring     path   = UserPrefsFilePath();
    std::string      text;
    JsonValue        root;
    JsonParseError   err;



    m_prefs = &prefs;
    m_machinePrefs.clear();
    outParseDetail.clear();

    if (!fs.Exists (path))
    {
        bool  fFoundLegacy = false;

        hr = MigrateLegacyFiles (prefs, fs, fFoundLegacy);
        CHR (hr);

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

    // A file that exists but will not parse is the case worth explaining:
    // the user still has settings, we just cannot read them. Capture where
    // the parse broke so the caller can say so instead of quietly starting
    // over with defaults.
    hr = JsonParser::Parse (text, root, err);
    CHRF (hr, outParseDetail = std::format (L"{}\n\nline {}, column {}: {}",
                                            path,
                                            err.line,
                                            err.column,
                                            std::wstring (err.message.begin(), err.message.end())));

    hr = LoadCombinedJson (root, prefs);
    CHR (hr);

Error:
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
    return SaveCombinedJson (prefs, fs);
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



    if (found == m_machinePrefs.end() && m_machinePrefs.empty() && fs.Exists (UserPrefsFilePath()))
    {
        GlobalUserPrefs  fallbackPrefs;
        JsonValue        root;


        hr = fs.ReadAllText (UserPrefsFilePath(), userContent);
        CHR (hr);

        hr = JsonParser::Parse (userContent, root, parseErr);
        CHR (hr);

        if (FindObjectValue (root, kpszMachinesKey) != nullptr)
        {
            hr = LoadCombinedJson (root, fallbackPrefs);
            CHR (hr);
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
        // Whether anything actually moved does not change what happens
        // next -- the canonicalize/save below is idempotent either way.
        hr = MachineConfigUpgrade::MigrateUserConfig (userContent, migrated, fRewritten);
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

        if (m_prefs != nullptr)
        {
            hr = SaveCombinedJson (*m_prefs, fs);
            CHR (hr);
        }
        else
        {
            GlobalUserPrefs  fallbackPrefs;


            hr = SaveCombinedJson (fallbackPrefs, fs);
            CHR (hr);
        }
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
    HRESULT              hr      = S_OK;
    JsonValue            delta;
    GlobalUserPrefs      fallbackPrefs;



    delta = DiffJson (currentJson, defaultJson);
    m_machinePrefs[machineName] = delta;

    if (m_prefs != nullptr)
    {
        hr = SaveCombinedJson (*m_prefs, fs);
        CHR (hr);
    }
    else
    {
        hr = SaveCombinedJson (fallbackPrefs, fs);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::Reset
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::Reset (
    const std::string  & machineName,
    IFileSystem        & fs) const
{
    HRESULT          hr = S_OK;
    GlobalUserPrefs  fallbackPrefs;



    m_machinePrefs.erase (machineName);

    if (m_prefs != nullptr)
    {
        hr = SaveCombinedJson (*m_prefs, fs);
        CHR (hr);
    }
    else
    {
        hr = SaveCombinedJson (fallbackPrefs, fs);
        CHR (hr);
    }

Error:
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
//  A read or parse failure is ignored rather than propagated: an unreadable or
//  corrupt existing file means there is nothing to preserve, and refusing to
//  save would leave the user unable to fix it by changing a setting.
//
//  The merge goes through an ordered map, so machines land in a stable order
//  and the file does not churn between saves.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::BuildCombinedJson (
    const GlobalUserPrefs & prefs,
    IFileSystem           & fs) const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  machines;
    std::map<std::string, JsonValue>                merged;
    std::string                                     existingText;
    JsonValue                                       existing;
    JsonParseError                                  err;
    const JsonValue                               * existingMachines = nullptr;
    HRESULT                                         hr               = S_OK;



    // Preserve any machines present in the on-disk file that we haven't
    // touched in this process. m_machinePrefs is populated lazily; if a
    // save fires before a given machine has been Load'd, that machine
    // would otherwise be wiped from disk on the next write.
    if (fs.Exists (UserPrefsFilePath()))
    {
        hr = fs.ReadAllText (UserPrefsFilePath(), existingText);
        if (SUCCEEDED (hr))
        {
            hr = JsonParser::Parse (existingText, existing, err);
            if (SUCCEEDED (hr))
            {
                existingMachines = FindObjectValue (existing, kpszMachinesKey);
                if (existingMachines != nullptr && existingMachines->GetType() == JsonType::Object)
                {
                    for (const auto & kv : existingMachines->GetObjectEntries())
                    {
                        if (kv.second.GetType() == JsonType::Object)
                        {
                            merged[kv.first] = kv.second;
                        }
                    }
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

    root.emplace_back (kpszGlobalKey,   prefs.ToJson());
    root.emplace_back (kpszMachinesKey, JsonValue (std::move (machines)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LoadCombinedJson
//
//  Reads a parsed prefs document into the global prefs and the per-machine
//  delta cache.
//
//  Only the ROOT being a non-object is an error. Everything inside is
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
    const JsonValue  * global   = nullptr;
    const JsonValue  * machines = nullptr;



    if (root.GetType() != JsonType::Object)
    {
        hr = E_INVALIDARG;
        CHR (hr);
    }

    global = FindObjectValue (root, kpszGlobalKey);
    if (global != nullptr)
    {
        hr = prefs.FromJson (*global);
        CHR (hr);
    }
    else
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::SaveCombinedJson
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::SaveCombinedJson (
    const GlobalUserPrefs & prefs,
    IFileSystem           & fs) const
{
    HRESULT              hr   = S_OK;
    JsonWriter::Options  opts;
    JsonValue            root = BuildCombinedJson (prefs, fs);
    std::string          text;



    opts.fPretty = true;
    hr = JsonWriter::Write (root, opts, text);
    CHR (hr);

    hr = fs.WriteAllText (UserPrefsFilePath(), text);
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
    GlobalUserPrefs & prefs,
    IFileSystem     & fs,
    bool            & outFoundLegacy) const
{
    HRESULT                   hr                = S_OK;
    std::wstring              legacyGlobalPath  = JoinPath (m_userDir, LegacyGlobalPrefsFilename());
    std::wstring              legacySuffix      = LegacyUserSuffix();
    std::vector<std::wstring> filenames;
    std::vector<std::wstring> legacyUserFiles;
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

    if (fHaveLegacyGlobal)
    {
        hr = fs.ReadAllText (legacyGlobalPath, text);
        CHR (hr);

        hr = JsonParser::Parse (text, parsed, err);
        CHR (hr);

        legacyGlobalJson = parsed;

        hr = prefs.FromJson (parsed);
        CHR (hr);
    }
    else
    {
        prefs = GlobalUserPrefs {};
        legacyGlobalJson = prefs.ToJson();
    }

    for (const auto & filename : legacyUserFiles)
    {
        std::wstring  path        = JoinPath (m_userDir, filename);
        std::string   machineName = Narrow (StripSuffix (filename, legacySuffix));

        hr = fs.ReadAllText (path, text);
        CHR (hr);

        hr = JsonParser::Parse (text, parsed, err);
        CHR (hr);

        canonical = CanonicalizeVersionStamp (parsed, 1);
        m_machinePrefs[machineName] = canonical;
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

    hr = fs.WriteAllText (UserPrefsFilePath(), combinedText);
    CHR (hr);

    if (fHaveLegacyGlobal)
    {
        hr = fs.Delete (legacyGlobalPath);
        CHR (hr);
    }

    for (const auto & filename : legacyUserFiles)
    {
        std::wstring  path = JoinPath (m_userDir, filename);

        hr = fs.Delete (path);
        CHR (hr);
    }

    trace = L"[UserConfigStore] Migrated user prefs:";
    if (fHaveLegacyGlobal)
    {
        trace += L" global";
    }

    for (const auto & filename : legacyUserFiles)
    {
        trace += L" ";
        trace += filename;
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
    const std::vector<std::pair<std::string, JsonValue>>  * curEntries    = nullptr;
    const std::vector<std::pair<std::string, JsonValue>>  * defEntries    = nullptr;
    int                                                     idx           = 0;
    size_t                                                  i             = 0;
    bool                                                    fIsVersionKey = false;
    bool                                                    fSameType     = false;
    JsonValue                                               result;
    HRESULT                                                 hr            = S_OK;
    bool                                                    isObject      = currentV.GetType() == JsonType::Object;


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

        fIsVersionKey = (key == kpszVersionKey);

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
                JsonValue uiDiff = DiffJson (cv, BuildUiPrefsDefaults());
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

        const JsonValue & dv = (*defEntries)[(size_t) idx].second;

        fSameType = (cv.GetType() == dv.GetType());

        if (fIsVersionKey)
        {
            // Always pass through.
            diff.emplace_back (key, cv);
            continue;
        }

        if (key == "internalDevices" &&
            IsObjectArray (cv) &&
            IsObjectArray (dv))
        {
            JsonValue hwDelta = BuildHardwareDeltaArray (cv, dv, false);
            if (hwDelta.GetType() == JsonType::Array && hwDelta.ArraySize() > 0)
            {
                diff.emplace_back (key, std::move (hwDelta));
            }

            continue;
        }

        if (key == "slots" &&
            IsObjectArray (cv) &&
            IsObjectArray (dv))
        {
            JsonValue hwDelta = BuildHardwareDeltaArray (cv, dv, true);
            if (hwDelta.GetType() == JsonType::Array && hwDelta.ArraySize() > 0)
            {
                diff.emplace_back (key, std::move (hwDelta));
            }

            continue;
        }

        if (key == kpszUiPrefsKey && cv.GetType() == JsonType::Object)
        {
            JsonValue uiDiff = DiffJson (cv, BuildUiPrefsDefaults());
            if (!uiDiff.GetObjectEntries().empty())
            {
                diff.emplace_back (key, std::move (uiDiff));
            }

            continue;
        }

        if (fSameType && cv.GetType() == JsonType::Object)
        {
            JsonValue  nested = DiffJson (cv, dv);
            if (!nested.GetObjectEntries().empty())
            {
                diff.emplace_back (key, std::move (nested));
            }

            continue;
        }

        if (!JsonEqual (cv, dv))
        {
            diff.emplace_back (key, cv);
        }
    }

    result = JsonValue (std::move (diff));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::JsonEqual
//
//  Structural equality. Object key order is ignored.
//
////////////////////////////////////////////////////////////////////////////////

bool UserConfigStore::JsonEqual (
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
                equal = a.ArraySize() == b.ArraySize();

                for (i = 0; equal && i < a.ArraySize(); ++i)
                {
                    equal = JsonEqual (a.ArrayAt (i), b.ArrayAt (i));
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
                    equal = idx >= 0 && JsonEqual (ae[i].second, be[(size_t) idx].second);
                }

                break;
            }
        }
    }

    return equal;
}

