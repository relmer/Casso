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

namespace
{
    constexpr const char * s_kpszVersionKey       = "$cassoMachineVersion";
    constexpr const char * s_kpszLegacyVersionKey = "$cassoDefault";
    constexpr const char * s_kpszUiPrefsKey       = "$cassoUiPrefs";
    constexpr const char * s_kpszGlobalKey        = "global";
    constexpr const char * s_kpszMachinesKey      = "machines";


    std::wstring Widen (const std::string & narrow)
    {
        std::wstring  out;


        out.reserve (narrow.size());
        for (char c : narrow)
        {
            out.push_back ((wchar_t) (unsigned char) c);
        }
        return out;
    }


    std::string Narrow (const std::wstring & wide)
    {
        std::string  out;


        out.reserve (wide.size());
        for (wchar_t c : wide)
        {
            out.push_back ((char) (unsigned char) c);
        }
        return out;
    }


    std::wstring JoinPath (
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


    std::wstring UserPrefsFilename()
    {
        return std::wstring (L"User") + L"Prefs" + L".json";
    }


    std::wstring LegacyGlobalPrefsFilename()
    {
        return std::wstring (L"Global") + L"User" + L"Prefs" + L".json";
    }


    std::wstring LegacyUserSuffix()
    {
        return std::wstring (L"_") + L"user" + L".json";
    }


    bool EndsWith (
        const std::wstring & text,
        const std::wstring & suffix)
    {
        if (text.size() < suffix.size())
        {
            return false;
        }

        return text.compare (text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }


    std::wstring StripSuffix (
        const std::wstring & text,
        const std::wstring & suffix)
    {
        return text.substr (0, text.size() - suffix.size());
    }


    int  FindObjectKey (
        const std::vector<std::pair<std::string, JsonValue>> & entries,
        const std::string                                    & key)
    {
        int  i = 0;


        for (i = 0; i < (int) entries.size(); ++i)
        {
            if (entries[(size_t) i].first == key)
            {
                return i;
            }
        }
        return -1;
    }


    const JsonValue * FindObjectValue (
        const JsonValue   & obj,
        const std::string & key)
    {
        int  idx = -1;


        if (obj.GetType() != JsonType::Object)
        {
            return nullptr;
        }

        idx = FindObjectKey (obj.GetObjectEntries(), key);
        if (idx < 0)
        {
            return nullptr;
        }

        return &obj.GetObjectEntries()[(size_t) idx].second;
    }

    int  ExtractVersion (const JsonValue & v)
    {
        const std::vector<std::pair<std::string, JsonValue>> * entries = nullptr;
        int                                                    found   = -1;

        if (v.GetType() != JsonType::Object)
        {
            return 0;
        }

        entries = &v.GetObjectEntries();
        found   = FindObjectKey (*entries, s_kpszVersionKey);
        if (found < 0)
        {
            return 0;
        }

        if ((*entries)[(size_t) found].second.GetType() != JsonType::Number)
        {
            return 0;
        }

        return (int) (*entries)[(size_t) found].second.GetNumber();
    }


    int  ExtractVersionForKey (
        const JsonValue   & v,
        const std::string & key)
    {
        const std::vector<std::pair<std::string, JsonValue>> * entries = nullptr;
        int                                                    found   = -1;


        if (v.GetType() != JsonType::Object)
        {
            return 0;
        }

        entries = &v.GetObjectEntries();
        found   = FindObjectKey (*entries, key);
        if (found < 0)
        {
            return 0;
        }

        if ((*entries)[(size_t) found].second.GetType() != JsonType::Number)
        {
            return 0;
        }

        return (int) (*entries)[(size_t) found].second.GetNumber();
    }


    bool HasLegacyVersionAlias (const JsonValue & v)
    {
        if (v.GetType() != JsonType::Object)
        {
            return false;
        }

        return FindObjectKey (v.GetObjectEntries(), s_kpszLegacyVersionKey) >= 0;
    }


    JsonValue CanonicalizeVersionStamp (
        const JsonValue & userJson,
        int               fallbackVersion)
    {
        std::vector<std::pair<std::string, JsonValue>>  out;
        int                                              canonicalVersion = 0;
        bool                                             fWroteVersion    = false;


        if (userJson.GetType() != JsonType::Object)
        {
            return userJson;
        }

        canonicalVersion = ExtractVersionForKey (userJson, s_kpszVersionKey);
        if (canonicalVersion <= 0)
        {
            canonicalVersion = ExtractVersionForKey (userJson, s_kpszLegacyVersionKey);
        }
        if (fallbackVersion > 0 && canonicalVersion < fallbackVersion)
        {
            canonicalVersion = fallbackVersion;
        }

        out.reserve (userJson.GetObjectEntries().size() + 1);

        for (const auto & kv : userJson.GetObjectEntries())
        {
            if (kv.first == s_kpszVersionKey)
            {
                if (!fWroteVersion && canonicalVersion > 0)
                {
                    out.emplace_back (s_kpszVersionKey, JsonValue ((double) canonicalVersion));
                    fWroteVersion = true;
                }
                continue;
            }

            if (kv.first == s_kpszLegacyVersionKey)
            {
                continue;
            }

            out.emplace_back (kv.first, kv.second);
        }

        if (!fWroteVersion && canonicalVersion > 0)
        {
            out.insert (out.begin(),
                        std::make_pair (std::string (s_kpszVersionKey),
                                        JsonValue ((double) canonicalVersion)));
        }

        return JsonValue (std::move (out));
    }


    bool TryGetBoolField (
        const JsonValue   & obj,
        const std::string & key,
        bool              & out)
    {
        int idx = -1;


        if (obj.GetType() != JsonType::Object)
        {
            return false;
        }

        idx = FindObjectKey (obj.GetObjectEntries(), key);
        if (idx < 0)
        {
            return false;
        }

        if (obj.GetObjectEntries()[(size_t) idx].second.GetType() != JsonType::Bool)
        {
            return false;
        }

        out = obj.GetObjectEntries()[(size_t) idx].second.GetBool();
        return true;
    }


    bool TryGetIntField (
        const JsonValue   & obj,
        const std::string & key,
        int               & out)
    {
        int idx = -1;


        if (obj.GetType() != JsonType::Object)
        {
            return false;
        }

        idx = FindObjectKey (obj.GetObjectEntries(), key);
        if (idx < 0)
        {
            return false;
        }

        if (obj.GetObjectEntries()[(size_t) idx].second.GetType() != JsonType::Number)
        {
            return false;
        }

        out = (int) obj.GetObjectEntries()[(size_t) idx].second.GetNumber();
        return true;
    }


    bool TryGetStringField (
        const JsonValue   & obj,
        const std::string & key,
        std::string       & out)
    {
        int idx = -1;


        if (obj.GetType() != JsonType::Object)
        {
            return false;
        }

        idx = FindObjectKey (obj.GetObjectEntries(), key);
        if (idx < 0)
        {
            return false;
        }

        if (obj.GetObjectEntries()[(size_t) idx].second.GetType() != JsonType::String)
        {
            return false;
        }

        out = obj.GetObjectEntries()[(size_t) idx].second.GetString();
        return true;
    }


    JsonValue BuildObjectWithEnabled (
        const JsonValue & src,
        bool              enabled)
    {
        std::vector<std::pair<std::string, JsonValue>> rebuilt;
        const auto * entries = &src.GetObjectEntries();


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


    JsonValue BuildUiPrefsDefaults()
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


    int FindInternalByType (
        const JsonValue   & arr,
        const std::string & type)
    {
        std::string candidate;


        if (arr.GetType() != JsonType::Array)
        {
            return -1;
        }

        for (size_t i = 0; i < arr.ArraySize(); ++i)
        {
            const JsonValue & e = arr.ArrayAt (i);
            if (e.GetType() != JsonType::Object)
            {
                continue;
            }

            candidate.clear();
            if (TryGetStringField (e, "type", candidate) && candidate == type)
            {
                return (int) i;
            }
        }

        return -1;
    }


    int FindSlotByNumber (
        const JsonValue & arr,
        int               slot)
    {
        int candidate = -1;


        if (arr.GetType() != JsonType::Array)
        {
            return -1;
        }

        for (size_t i = 0; i < arr.ArraySize(); ++i)
        {
            const JsonValue & e = arr.ArrayAt (i);
            if (e.GetType() != JsonType::Object)
            {
                continue;
            }

            candidate = -1;
            if (TryGetIntField (e, "slot", candidate) && candidate == slot)
            {
                return (int) i;
            }
        }

        return -1;
    }


    JsonValue MergeHardwareArray (
        const JsonValue & defaultArr,
        const JsonValue & userArr,
        bool              slotArray)
    {
        std::vector<JsonValue> merged;
        std::vector<bool>      userMatched;


        if (defaultArr.GetType() != JsonType::Array ||
            userArr.GetType()    != JsonType::Array)
        {
            return userArr;
        }

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

        return JsonValue (std::move (merged));
    }


    JsonValue BuildHardwareDeltaArray (
        const JsonValue & currentArr,
        const JsonValue & defaultArr,
        bool              slotArray)
    {
        std::vector<JsonValue> delta;


        if (currentArr.GetType() != JsonType::Array ||
            defaultArr.GetType() != JsonType::Array)
        {
            return currentArr;
        }

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
                std::vector<std::pair<std::string, JsonValue>> obj;
                std::string type;
                int slot = -1;

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

        return JsonValue (std::move (delta));
    }


    bool IsObjectArray (const JsonValue & v)
    {
        if (v.GetType() != JsonType::Array)
        {
            return false;
        }

        for (size_t i = 0; i < v.ArraySize(); ++i)
        {
            if (v.ArrayAt (i).GetType() != JsonType::Object)
            {
                return false;
            }
        }

        return true;
    }
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
    IFileSystem      & fs)
{
    HRESULT          hr     = S_OK;
    std::wstring     path   = UserPrefsFilePath();
    std::string      text;
    JsonValue        root;
    JsonParseError   err;



    m_prefs = &prefs;
    m_machinePrefs.clear();

    if (!fs.Exists (path))
    {
        hr = MigrateLegacyFiles (prefs, fs);
        if (hr == S_FALSE)
        {
            prefs = GlobalUserPrefs {};
        }
        BAIL_OUT_IF (true, hr);
    }

    hr = fs.ReadAllText (path, text);
    CHR (hr);

    hr = JsonParser::Parse (text, root, err);
    CHR (hr);

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
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::Load (
    const std::string  & machineName,
    const JsonValue    & defaultJson,
    IFileSystem        & fs,
    JsonValue          & outMerged) const
{
    HRESULT          hr            = S_OK;
    std::string      userContent;
    std::string      migrated;
    JsonValue        userJson;
    JsonValue        canonicalJson;
    JsonParseError   parseErr;
    JsonWriter::Options opts;
    int              defaultVer    = 0;
    int              userVer       = 0;
    bool             fNeedMigrate  = false;
    bool             fHasLegacyKey = false;
    auto             found         = m_machinePrefs.find (machineName);



    if (found == m_machinePrefs.end() && m_machinePrefs.empty() && fs.Exists (UserPrefsFilePath()))
    {
        GlobalUserPrefs  fallbackPrefs;
        JsonValue        root;


        hr = fs.ReadAllText (UserPrefsFilePath(), userContent);
        CHR (hr);

        hr = JsonParser::Parse (userContent, root, parseErr);
        CHR (hr);

        if (FindObjectValue (root, s_kpszMachinesKey) != nullptr)
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

    if (found == m_machinePrefs.end())
    {
        outMerged = defaultJson;
        return S_OK;
    }

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
        hr = MachineConfigUpgrade::MigrateUserConfig (userContent, migrated);
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
////////////////////////////////////////////////////////////////////////////////

JsonValue UserConfigStore::BuildCombinedJson (const GlobalUserPrefs & prefs) const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  machines;



    machines.reserve (m_machinePrefs.size());
    for (const auto & kv : m_machinePrefs)
    {
        machines.emplace_back (kv.first, kv.second);
    }

    root.emplace_back (s_kpszGlobalKey,   prefs.ToJson());
    root.emplace_back (s_kpszMachinesKey, JsonValue (std::move (machines)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::LoadCombinedJson
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

    global = FindObjectValue (root, s_kpszGlobalKey);
    if (global != nullptr)
    {
        hr = prefs.FromJson (*global);
        CHR (hr);
    }
    else
    {
        prefs = GlobalUserPrefs {};
    }

    machines = FindObjectValue (root, s_kpszMachinesKey);
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
    JsonValue            root = BuildCombinedJson (prefs);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT UserConfigStore::MigrateLegacyFiles (
    GlobalUserPrefs & prefs,
    IFileSystem     & fs) const
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

    fHaveLegacyUsers = !legacyUserFiles.empty();
    if (!fHaveLegacyGlobal && !fHaveLegacyUsers)
    {
        hr = S_FALSE;
        BAIL_OUT_IF (true, hr);
    }

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

    rootEntries.emplace_back (s_kpszGlobalKey,   legacyGlobalJson);
    rootEntries.emplace_back (s_kpszMachinesKey, JsonValue (std::move (machines)));

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
    std::vector<std::pair<std::string, JsonValue>>  merged;
    const std::vector<std::pair<std::string, JsonValue>> *  defaultEntries = nullptr;
    const std::vector<std::pair<std::string, JsonValue>> *  userEntries    = nullptr;
    int                                                     idx            = 0;
    size_t                                                  i              = 0;


    if (defaultV.GetType() != JsonType::Object ||
        userV.GetType()    != JsonType::Object)
    {
        // Scalar / array / type mismatch: user value wins (copy).
        return userV;
    }

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

    return JsonValue (std::move (merged));
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
    std::vector<std::pair<std::string, JsonValue>>  diff;
    const std::vector<std::pair<std::string, JsonValue>> *  curEntries     = nullptr;
    const std::vector<std::pair<std::string, JsonValue>> *  defEntries     = nullptr;
    int                                                     idx            = 0;
    size_t                                                  i              = 0;
    bool                                                    fIsVersionKey  = false;
    bool                                                    fSameType      = false;


    if (currentV.GetType() != JsonType::Object)
    {
        // Not an object: return an empty object (callers expect Object).
        return JsonValue (std::move (diff));
    }

    curEntries = &currentV.GetObjectEntries();

    if (defaultV.GetType() == JsonType::Object)
    {
        defEntries = &defaultV.GetObjectEntries();
    }

    for (i = 0; i < curEntries->size(); ++i)
    {
        const std::string & key = (*curEntries)[i].first;
        const JsonValue   & cv  = (*curEntries)[i].second;

        fIsVersionKey = (key == s_kpszVersionKey);

        if (defEntries == nullptr)
        {
            // Defaults isn't an object: keep everything from current.
            diff.emplace_back (key, cv);
            continue;
        }

        idx = FindObjectKey (*defEntries, key);

        if (idx < 0)
        {
            if (key == s_kpszUiPrefsKey && cv.GetType() == JsonType::Object)
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

        if (key == s_kpszUiPrefsKey && cv.GetType() == JsonType::Object)
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

    return JsonValue (std::move (diff));
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
    int  idx = 0;



    if (a.GetType() != b.GetType())
    {
        return false;
    }

    switch (a.GetType())
    {
        case JsonType::Null:
            return true;

        case JsonType::Bool:
            return a.GetBool() == b.GetBool();

        case JsonType::Number:
            return a.GetNumber() == b.GetNumber();

        case JsonType::String:
            return a.GetString() == b.GetString();

        case JsonType::Array:
        {
            if (a.ArraySize() != b.ArraySize())
            {
                return false;
            }
            for (size_t i = 0; i < a.ArraySize(); ++i)
            {
                if (!JsonEqual (a.ArrayAt (i), b.ArrayAt (i)))
                {
                    return false;
                }
            }
            return true;
        }

        case JsonType::Object:
        {
            const auto & ae = a.GetObjectEntries();
            const auto & be = b.GetObjectEntries();

            if (ae.size() != be.size())
            {
                return false;
            }

            for (size_t i = 0; i < ae.size(); ++i)
            {
                idx = FindObjectKey (be, ae[i].first);
                if (idx < 0)
                {
                    return false;
                }
                if (!JsonEqual (ae[i].second, be[(size_t) idx].second))
                {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}

