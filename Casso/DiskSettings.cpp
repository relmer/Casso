#include "Pch.h"

#include "DiskSettings.h"

#include "Config/IFileSystem.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/PathResolver.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // Load the per-machine default JSON (the on-disk Machines/<Name>/
    // <Name>.json) into `outDefault`. Returns S_FALSE when the file
    // isn't found, S_OK on a clean parse, error HRESULT otherwise.
    HRESULT LoadMachineDefaultJson (const std::wstring  & machineName,
                                    JsonValue           & outDefault)
    {
        std::vector<fs::path>  searchPaths;
        fs::path               configRelPath;
        fs::path               configPath;
        std::ifstream          configFile;
        std::stringstream      ss;
        std::string            jsonText;
        JsonParseError         parseErr;
        HRESULT                hr            = S_OK;


        searchPaths   = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                          PathResolver::GetWorkingDirectory());
        configRelPath = fs::path ("Machines") / fs::path (machineName).string()
                                              / (fs::path (machineName).string() + ".json");
        configPath    = PathResolver::FindFile (searchPaths, configRelPath);

        if (configPath.empty())
        {
            return S_FALSE;
        }

        configFile.open (configPath);
        if (!configFile.good())
        {
            return S_FALSE;
        }

        ss << configFile.rdbuf();
        jsonText = ss.str();
        hr = JsonParser::Parse (jsonText, outDefault, parseErr);
        return hr;
    }


    std::string  WideToNarrowAscii (const std::wstring & w)
    {
        std::string  narrow;

        narrow.reserve (w.size());
        for (wchar_t c : w)
        {
            narrow.push_back (static_cast<char> (static_cast<unsigned char> (c)));
        }
        return narrow;
    }


    std::wstring NarrowToWideAscii (const std::string & s)
    {
        return std::wstring (s.begin(), s.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSavedDiskPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::ReadSavedDiskPath (
    UserConfigStore    & store,
    IFileSystem        & fs,
    int                  drive,
    const std::wstring & machineName,
    std::wstring       & outPath)
{
    HRESULT             hr            = S_OK;
    JsonValue           defaultJson;
    JsonValue           mergedJson;
    const JsonValue *   uiPrefs       = nullptr;
    std::string         pathNarrow;
    const char *        keyName       = (drive == 0) ? "disk1Path" : "disk2Path";


    outPath.clear();

    if (drive < 0 || drive > 1 || machineName.empty())
    {
        return S_FALSE;
    }

    hr = LoadMachineDefaultJson (machineName, defaultJson);
    if (hr != S_OK)
    {
        return S_FALSE;
    }

    hr = store.Load (WideToNarrowAscii (machineName), defaultJson, fs, mergedJson);
    if (FAILED (hr) || mergedJson.GetType() != JsonType::Object)
    {
        return S_FALSE;
    }

    hr = mergedJson.GetObject ("$cassoUiPrefs", uiPrefs);
    if (FAILED (hr) || uiPrefs == nullptr)
    {
        return S_FALSE;
    }
    _Analysis_assume_ (uiPrefs != nullptr);

    hr = uiPrefs->GetString (keyName, pathNarrow);
    if (FAILED (hr) || pathNarrow.empty())
    {
        return S_FALSE;
    }

    outPath = PathResolver::ResolveExeRelativePath (NarrowToWideAscii (pathNarrow));
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSavedDiskPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::WriteSavedDiskPath (
    UserConfigStore    & store,
    IFileSystem        & fs,
    int                  drive,
    const std::wstring & machineName,
    const std::wstring & path)
{
    HRESULT          hr            = S_OK;
    JsonValue        defaultJson;
    JsonValue        mergedJson;
    JsonValue        updatedJson;
    std::wstring     stored;
    std::string      storedNarrow;
    const char *     keyName       = (drive == 0) ? "disk1Path" : "disk2Path";
    std::vector<std::pair<std::string, JsonValue>>  rootEntries;
    std::vector<std::pair<std::string, JsonValue>>  uiPrefsEntries;
    int              uiPrefsIdx    = -1;
    int              i             = 0;


    if (drive < 0 || drive > 1 || machineName.empty())
    {
        return E_INVALIDARG;
    }

    hr = LoadMachineDefaultJson (machineName, defaultJson);
    if (hr != S_OK)
    {
        return S_FALSE;
    }

    hr = store.Load (WideToNarrowAscii (machineName), defaultJson, fs, mergedJson);
    CHR (hr);

    if (mergedJson.GetType() != JsonType::Object)
    {
        return S_FALSE;
    }

    stored       = PathResolver::MakeExeRelativePath (path);
    storedNarrow = WideToNarrowAscii (stored);

    // Splice the new diskNPath into the merged JSON's $cassoUiPrefs
    // block. The JsonValue API is read-mostly; we rebuild the relevant
    // sub-objects via swap-and-replace.
    rootEntries = mergedJson.GetObjectEntries();

    for (i = 0; i < (int) rootEntries.size(); ++i)
    {
        if (rootEntries[(size_t) i].first == "$cassoUiPrefs")
        {
            uiPrefsIdx = i;
            if (rootEntries[(size_t) i].second.GetType() == JsonType::Object)
            {
                uiPrefsEntries = rootEntries[(size_t) i].second.GetObjectEntries();
            }
            break;
        }
    }

    // Replace or append the key inside the $cassoUiPrefs block.
    {
        bool  replaced = false;
        for (i = 0; i < (int) uiPrefsEntries.size(); ++i)
        {
            if (uiPrefsEntries[(size_t) i].first == keyName)
            {
                uiPrefsEntries[(size_t) i].second = JsonValue (storedNarrow);
                replaced = true;
                break;
            }
        }
        if (!replaced)
        {
            uiPrefsEntries.emplace_back (keyName, JsonValue (storedNarrow));
        }
    }

    if (uiPrefsIdx < 0)
    {
        rootEntries.emplace_back ("$cassoUiPrefs", JsonValue (std::move (uiPrefsEntries)));
    }
    else
    {
        rootEntries[(size_t) uiPrefsIdx].second = JsonValue (std::move (uiPrefsEntries));
    }

    updatedJson = JsonValue (std::move (rootEntries));

    hr = store.SaveDelta (WideToNarrowAscii (machineName), updatedJson, defaultJson, fs);
    CHR (hr);

Error:
    return hr;
}
