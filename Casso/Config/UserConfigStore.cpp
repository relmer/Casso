#include "Pch.h"

#include "UserConfigStore.h"

#include "../../CassoCore/Ehm.h"

#include "../../CassoEmuCore/Core/JsonParser.h"
#include "../../CassoEmuCore/Core/JsonWriter.h"
#include "../../CassoEmuCore/Core/MachineConfigUpgrade.h"

#include <algorithm>





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char * s_kpszVersionKey = "$cassoMachineVersion";


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
//  UserConfigStore::UserFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UserConfigStore::UserFilePath (const std::string & machineName) const
{
    std::wstring  result = m_userDir;

    if (!result.empty() &&
        result.back() != L'\\' &&
        result.back() != L'/')
    {
        result += L'\\';
    }

    result += Widen (machineName);
    result += L"_user.json";

    return result;
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
    std::wstring     path          = UserFilePath (machineName);
    std::string      userContent;
    std::string      migrated;
    JsonValue        userJson;
    JsonParseError   parseErr;
    int              defaultVer    = 0;
    int              userVer       = 0;
    bool             fNeedMigrate  = false;


    if (!fs.Exists (path))
    {
        // No user file -> merged == default (copy).
        outMerged = defaultJson;
        return S_OK;
    }

    hr = fs.ReadAllText (path, userContent);
    CHR (hr);

    hr = JsonParser::Parse (userContent, userJson, parseErr);
    CHR (hr);

    defaultVer = ExtractVersion (defaultJson);
    userVer    = ExtractVersion (userJson);
    fNeedMigrate = (defaultVer > 0 && userVer > 0 && userVer < defaultVer)
                || (userVer == 0);  // pre-versioned $cassoDefault path

    if (fNeedMigrate)
    {
        hr = MachineConfigUpgrade::MigrateUserConfig (userContent, migrated);
        if (SUCCEEDED (hr) && !migrated.empty())
        {
            hr = JsonParser::Parse (migrated, userJson, parseErr);
            CHR (hr);

            // Write the upgraded text back to disk.
            hr = fs.WriteAllText (path, migrated);
            CHR (hr);
        }
        else if (hr == S_FALSE)
        {
            // Already at current schema per migrator — keep userJson as parsed.
            hr = S_OK;
        }
        else
        {
            // Migration failure is non-fatal here; fall through and merge as-is.
            hr = S_OK;
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
    std::wstring         path    = UserFilePath (machineName);
    std::string          text;
    JsonWriter::Options  opts;


    delta = DiffJson (currentJson, defaultJson);

    opts.fPretty = true;
    hr = JsonWriter::Write (delta, opts, text);
    CHR (hr);

    hr = fs.WriteAllText (path, text);
    CHR (hr);

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
    return fs.Delete (UserFilePath (machineName));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStore::MergeJson
//
//  Deep merge: when both `defaultV` and `userV` are objects, recurse
//  per-key. Otherwise `userV` wins (including arrays).
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
            merged.emplace_back (
                key,
                MergeJson (
                    (*defaultEntries)[i].second,
                    (*userEntries)[(size_t) idx].second));
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
//  Produces an Object containing only the differences between
//  `currentV` and `defaultV`. Always preserves `$cassoMachineVersion`
//  from `currentV` if present (even when equal).
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
