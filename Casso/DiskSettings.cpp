#include "Pch.h"

#include "DiskSettings.h"

#include "Config/IFileSystem.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope helpers
//
//  Loading a machine's default config and the narrow / wide conversions the
//  JSON layer needs.
//
//  A MISSING config is reported distinctly, as
//  HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), rather than as a generic failure.
//  Callers read it as "this machine has no saved config" -- an ordinary first
//  run -- while any other error is a real load or parse problem worth
//  surfacing. Collapsing the two would make a fresh install look broken.
//
//  Paths are resolved through the shared search-path builder rather than
//  assembled from the executable directory, so a machine found in a
//  development tree loads from that tree instead of an installed copy.
//
//  The string conversions exist because the JSON layer is narrow while the
//  Windows path API is wide; they are kept here rather than shared so the
//  encoding assumption stays visible at its point of use.
//
////////////////////////////////////////////////////////////////////////////////

// Load the per-machine default JSON (the on-disk Machines/<Name>/
// <Name>.json) into `outDefault`. S_OK on a clean parse;
// HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) when there is no such file,
// which callers treat as "this machine has no saved config" rather than a
// failure; any other error HRESULT is a real load/parse failure.
HRESULT DiskSettings::LoadMachineDefaultJson (const std::wstring  & machineName,
                                JsonValue           & outDefault)
{
    std::vector<fs::path> searchPaths;
    fs::path              configRelPath;
    fs::path              configPath;
    std::ifstream         configFile;
    std::stringstream     ss;
    std::string           jsonText;
    JsonParseError        parseErr;
    HRESULT               hr            = S_OK;
    bool                  wasFound      = false;
    bool                  isOpen        = false;


    searchPaths   = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                      PathResolver::GetWorkingDirectory());
    configRelPath = fs::path ("Machines") / fs::path (machineName).string()
                                          / (fs::path (machineName).string() + ".json");
    configPath    = PathResolver::FindFile (searchPaths, configRelPath);
    wasFound      = !configPath.empty();

    CBREx (wasFound, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    configFile.open (configPath);
    isOpen = configFile.good();

    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = JsonParser::Parse (jsonText, outDefault, parseErr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WideToUtf8
//
////////////////////////////////////////////////////////////////////////////////

std::string  DiskSettings::WideToUtf8 (const std::wstring & w)
{
    HRESULT      hr   = S_OK;
    std::string  utf8;
    int          len  = 0;



    BAIL_OUT_IF (w.empty(), S_OK);

    len = WideCharToMultiByte (CP_UTF8, 0, w.c_str(), static_cast<int> (w.size()), nullptr, 0, nullptr, nullptr);
    CWRA (len);

    utf8.resize (static_cast<size_t> (len));

    len = WideCharToMultiByte (CP_UTF8, 0, w.c_str(), static_cast<int> (w.size()), utf8.data(), len, nullptr, nullptr);
    CWRA (len);

Error:
    return utf8;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Utf8ToWide
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskSettings::Utf8ToWide (const std::string & s)
{
    HRESULT       hr   = S_OK;
    std::wstring  wide;
    int           len  = 0;



    BAIL_OUT_IF (s.empty(), S_OK);

    len = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), static_cast<int> (s.size()), nullptr, 0);
    CWRA (len);

    wide.resize (static_cast<size_t> (len));

    len = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), static_cast<int> (s.size()), wide.data(), len);
    CWRA (len);

Error:
    return wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSavedDiskPath
//
//  Reads the disk image a machine last had mounted in the given drive.
//
//  "Nothing saved" is reported by an EMPTY outPath and S_OK, not by a distinct
//  result code -- and there are four different ways to arrive there: no config
//  file, a non-object merge result, no $cassoUiPrefs block, or no key. Every
//  caller already tests the path, so collapsing all four onto that one signal
//  keeps them from having to distinguish cases that mean the same thing. Only
//  a genuine load or parse failure propagates.
//
//  The path is stored EXE-RELATIVE and resolved back on read, so a Casso
//  folder moved or copied to another machine keeps finding its disks.
//
//  Reading goes through the user-config merge rather than the raw file, so a
//  disk recorded as a machine default and one the user mounted are found by
//  the same lookup.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::ReadSavedDiskPath (
    UserConfigStore    & store,
    IFileSystem        & fs,
    int                  drive,
    const std::wstring & machineName,
    std::wstring       & outPath)
{
    HRESULT           hr            = S_OK;
    JsonValue         defaultJson;
    JsonValue         mergedJson;
    const JsonValue * uiPrefs       = nullptr;
    std::string       pathNarrow;
    bool              hasMachine    = false;
    const char      * keyName       = (drive == 0) ? "disk1Path" : "disk2Path";



    outPath.clear();

    hasMachine = !machineName.empty();
    CBRAEx (drive >= 0 && drive <= 1 && hasMachine, E_INVALIDARG);

    hr = LoadMachineDefaultJson (machineName, defaultJson);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), S_OK);
    CHR (hr);

    // A real load/parse failure propagates (CHR above); a missing file or a
    // non-object result just means this machine has nothing saved, and that
    // is reported by leaving outPath empty rather than by a second result
    // code -- which is what every caller already tests.
    hr = store.Load (WideToUtf8 (machineName), defaultJson, fs, mergedJson);
    CHR (hr);

    BAIL_OUT_IF (mergedJson.GetType() != JsonType::Object, S_OK);

    // The remaining lookups are for optional keys; absent = nothing saved.
    hr = mergedJson.GetObject ("$cassoUiPrefs", uiPrefs);
    BAIL_OUT_IF (FAILED (hr) || uiPrefs == nullptr, S_OK);
    _Analysis_assume_ (uiPrefs != nullptr);

    hr = uiPrefs->GetString (keyName, pathNarrow);
    BAIL_OUT_IF (FAILED (hr) || pathNarrow.empty(), S_OK);

    outPath = PathResolver::ResolveExeRelativePath (Utf8ToWide (pathNarrow));
    hr      = S_OK;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSavedDiskPath
//
//  Records the disk image mounted in a drive, so the machine remounts it on
//  the next launch.
//
//  The path is made EXE-RELATIVE before storing, mirroring what
//  ReadSavedDiskPath resolves, so a Casso folder that is moved or copied keeps
//  finding its disks.
//
//  The write is a SPLICE, not a rewrite. The merged document is decomposed
//  into entries, one key inside $cassoUiPrefs is replaced or appended, and the
//  whole is rebuilt -- so every other preference in that block survives
//  untouched. Writing a fresh object with just this key would silently discard
//  the user's color mode, speed, and peripheral settings.
//
//  The rebuild is verbose because the JsonValue API is read-mostly: there is
//  no in-place mutation, so swap-and-replace is the available shape.
//
//  A machine with no config on disk is skipped rather than having one created,
//  since there is nothing for the delta to be a delta against.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::WriteSavedDiskPath (
    UserConfigStore    & store,
    IFileSystem        & fs,
    int                  drive,
    const std::wstring & machineName,
    const std::wstring & path)
{
    HRESULT                                         hr             = S_OK;
    JsonValue                                       defaultJson;
    JsonValue                                       mergedJson;
    JsonValue                                       updatedJson;
    std::wstring                                    stored;
    std::string                                     storedNarrow;
    std::vector<std::pair<std::string, JsonValue>>  rootEntries;
    std::vector<std::pair<std::string, JsonValue>>  uiPrefsEntries;
    int                                             uiPrefsIdx     = 0;
    int                                             i              = 0;
    bool                                            hasMachine     = false;
    const char                                     * keyName        = (drive == 0) ? "disk1Path" : "disk2Path";
    uiPrefsIdx = -1;



    hasMachine = !machineName.empty();
    CBRAEx (drive >= 0 && drive <= 1 && hasMachine, E_INVALIDARG);

    hr = LoadMachineDefaultJson (machineName, defaultJson);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), S_OK);
    CHR (hr);

    hr = store.Load (WideToUtf8 (machineName), defaultJson, fs, mergedJson);
    CHR (hr);

    BAIL_OUT_IF (mergedJson.GetType() != JsonType::Object, S_OK);

    stored       = PathResolver::MakeExeRelativePath (path);
    storedNarrow = WideToUtf8 (stored);

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

    hr = store.SaveDelta (WideToUtf8 (machineName), updatedJson, defaultJson, fs);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSavedUiPrefBool
//
//  Splice a boolean into the merged config's $cassoUiPrefs block and persist
//  the delta. Mirrors WriteSavedDiskPath's read-modify-write, keyed on an
//  arbitrary $cassoUiPrefs field name rather than diskNPath.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::WriteSavedUiPrefBool (
    UserConfigStore    & store,
    IFileSystem        & fs,
    const std::string  & key,
    const std::wstring & machineName,
    bool                 value)
{
    HRESULT                                          hr             = S_OK;
    JsonValue                                        defaultJson;
    JsonValue                                        mergedJson;
    JsonValue                                        updatedJson;
    std::vector<std::pair<std::string, JsonValue>>   rootEntries;
    std::vector<std::pair<std::string, JsonValue>>   uiPrefsEntries;
    int                                              uiPrefsIdx     = -1;
    int                                              i              = 0;
    bool                                             hasKey         = false;
    bool                                             hasMachine     = false;



    hasKey     = !key.empty();
    hasMachine = !machineName.empty();

    CBRAEx (hasKey && hasMachine, E_INVALIDARG);

    hr = LoadMachineDefaultJson (machineName, defaultJson);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), S_OK);
    CHR (hr);

    hr = store.Load (WideToUtf8 (machineName), defaultJson, fs, mergedJson);
    CHR (hr);

    BAIL_OUT_IF (mergedJson.GetType() != JsonType::Object, S_OK);

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

    {
        bool  replaced = false;
        for (i = 0; i < (int) uiPrefsEntries.size(); ++i)
        {
            if (uiPrefsEntries[(size_t) i].first == key)
            {
                uiPrefsEntries[(size_t) i].second = JsonValue (value);
                replaced = true;
                break;
            }
        }

        if (!replaced)
        {
            uiPrefsEntries.emplace_back (key, JsonValue (value));
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

    hr = store.SaveDelta (WideToUtf8 (machineName), updatedJson, defaultJson, fs);
    CHR (hr);

Error:
    return hr;
}

