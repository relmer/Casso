#include "Pch.h"

#include "AssetBootstrap.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Ehm.h"
#include "resource.h"
#include "UnicodeSymbols.h"

#pragma comment(lib, "winhttp.lib")


static constexpr LPCWSTR       s_kpszAppleWinHost = L"raw.githubusercontent.com";
static constexpr LPCWSTR       s_kpszUserAgent    = L"Casso/1.0";
static constexpr LPCWSTR       s_kpszUrlPrefix    = L"/AppleWin/AppleWin/master/resource/";

static constexpr LPCWSTR       s_kpszAsimovHost   = L"www.apple.asimov.net";





////////////////////////////////////////////////////////////////////////////////
//
//  RomSpec
//
//  Static map: (machineName, Casso ROM filename) -> AppleWin source
//  basename + size + on-disk relative directory. Per spec
//  005-disk-ii-audio Phase 12 (T122 + Q1), machine-specific ROMs are
//  keyed by their owning machine so a shared upstream file
//  (Apple2_Video.rom, Apple2e_Enhanced_Video.rom) downloads once per
//  machine into Machines/<MachineName>/. The handful of bytes
//  duplicated on disk is invisible next to the convenience of having
//  every machine's assets self-contained. Mirrors
//  scripts/FetchRoms.ps1.
//
//  Entries with an empty `machineName` are shared device ROMs that
//  live under Devices/<Family>/ rather than a specific machine
//  folder (currently: the Disk II controller boot ROMs).
//
////////////////////////////////////////////////////////////////////////////////

struct RomSpec
{
    string_view  machineName;        // "Apple2"/"Apple2Plus"/... or "" for device-shared
    string_view  cassoName;
    string_view  appleWinName;
    string_view  localRelDir;        // e.g. "Machines/Apple2e" or "Devices/DiskII"
    size_t       expectedSize;
    string_view  description;
};

static constexpr RomSpec s_kRomCatalog[] =
{
    { "Apple2",           "Apple2.rom",            "Apple2.rom",                 "Machines/Apple2",           12288, "Apple ][ ROM (Integer BASIC)"              },
    { "Apple2",           "Apple2_Video.rom",      "Apple2_Video.rom",           "Machines/Apple2",            2048, "Apple ][/][+ Character Generator"          },
    { "Apple2Plus",       "Apple2Plus.rom",        "Apple2_Plus.rom",            "Machines/Apple2Plus",       12288, "Apple ][+ ROM (Applesoft BASIC)"           },
    { "Apple2Plus",       "Apple2_Video.rom",      "Apple2_Video.rom",           "Machines/Apple2Plus",        2048, "Apple ][/][+ Character Generator"          },
    { "Apple2e",          "Apple2e.rom",           "Apple2e.rom",                "Machines/Apple2e",          16384, "Apple //e ROM"                             },
    { "Apple2e",          "Apple2e_Video.rom",     "Apple2e_Enhanced_Video.rom", "Machines/Apple2e",           4096, "Apple //e Character Generator + MouseText" },
    { "Apple2eEnhanced",  "Apple2eEnhanced.rom",   "Apple2e_Enhanced.rom",       "Machines/Apple2eEnhanced",  16384, "Apple //e Enhanced ROM"                    },
    { "Apple2eEnhanced",  "Apple2e_Video.rom",     "Apple2e_Enhanced_Video.rom", "Machines/Apple2eEnhanced",   4096, "Apple //e Character Generator + MouseText" },
    { "",                 "Disk2.rom",             "DISK2.rom",                  "Devices/DiskII",              256, "Disk ][ Boot ROM (slot 6)"                 },
    { "",                 "Disk2_13Sector.rom",    "DISK2-13sector.rom",         "Devices/DiskII",              256, "Disk ][ Boot ROM (13-sector)"              },
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskSpec
//
//  Apple master disk images we offer to download from the Asimov
//  mirror when a user starts a machine with a Disk ][ controller and
//  no disk has ever been mounted in drive 1. The on-disk filename
//  matches the canonical Asimov filename so a savvy user can drop
//  their own copy into Disks/ and have it picked up.
//
////////////////////////////////////////////////////////////////////////////////

struct BootDiskSpec
{
    string_view  cassoName;          // local filename under Disks/
    LPCWSTR      asimovUrlPath;      // path component on the Asimov mirror
    size_t       expectedSize;       // exact byte count for integrity check
    string_view  shortLabel;         // text on the choice button
    string_view  description;        // longer text in the dialog body
};


// SHA1 27EA2EE7114EBFA91DA0A16B7B8EBFF24EB8EE88; verified against
// `Disks/Apple/dos33-master.dsk` already used by the developer's local
// CatalogReproductionTest.
static constexpr BootDiskSpec s_kDos33Disk =
{
    "DOS 3.3 System Master.dsk",
    L"/images/masters/DOS%203.3%20System%20Master%20-%20680-0210-A%20%281982%29.dsk",
    143360,
    "DOS 3.3",
    "Apple DOS 3.3 System Master (680-0210-A, 1982). Boots Applesoft BASIC; "
    "type CATALOG to list files."
};

// SHA1 40DC1A16E3F234857A29B49CA0B996E1B14D38B9.
static constexpr BootDiskSpec s_kProDOSDisk =
{
    "ProDOS Users Disk.dsk",
    L"/images/masters/prodos/ProDOS%20Users%20Disk%20-%20680-0224-C.dsk",
    143360,
    "ProDOS",
    "Apple ProDOS Users Disk (680-0224-C). Boots ProDOS 8 with the BASIC.SYSTEM "
    "shell."
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedConfig
//
////////////////////////////////////////////////////////////////////////////////

struct EmbeddedConfig
{
    int          resourceId;
    string_view  machineName;        // "Apple2", "Apple2Plus", "Apple2e"
    string_view  fileName;           // "<machineName>.json"
};


static constexpr EmbeddedConfig s_kEmbeddedConfigs[] =
{
    { IDR_MACHINE_APPLE2,     "Apple2",     "Apple2.json"     },
    { IDR_MACHINE_APPLE2PLUS, "Apple2Plus", "Apple2Plus.json" },
    { IDR_MACHINE_APPLE2E,    "Apple2e",    "Apple2e.json"    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  AsciiToWide
//
//  Compile-time-friendly widening for ASCII string literals (descriptions,
//  filenames). Only safe for 7-bit input; anything else would need
//  MultiByteToWideChar.
//
////////////////////////////////////////////////////////////////////////////////

static wstring AsciiToWide (string_view s)
{
    return wstring (s.begin(), s.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractResource
//
//  Locks an RCDATA resource and returns a non-owning span into the
//  module image (no free required). Returns an empty span on failure.
//
////////////////////////////////////////////////////////////////////////////////

static span<const Byte> ExtractResource (HINSTANCE hInstance, int resourceId)
{
    HRESULT           hr     = S_OK;
    HRSRC             hRes   = nullptr;
    HGLOBAL           hMem   = nullptr;
    DWORD             size   = 0;
    void            * data   = nullptr;
    span<const Byte>  result;



    hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
    CWR (hRes != nullptr);

    size = SizeofResource (hInstance, hRes);
    CBR (size > 0);

    hMem = LoadResource (hInstance, hRes);
    CWR (hMem != nullptr);

    data = LockResource (hMem);
    CPR (data);

    result = span<const Byte> (static_cast<const Byte *> (data), static_cast<size_t> (size));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFileBytes
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteFileBytes (const fs::path & path, span<const Byte> bytes)
{
    HRESULT     hr  = S_OK;
    ofstream    out;



    out.open (path, ios::binary | ios::trunc);
    CBRA (out.good());

    out.write (reinterpret_cast<const char *> (bytes.data()), static_cast<streamsize> (bytes.size()));
    CBRA (out.good());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureMachineConfigs
//
//  Make sure at least one Machines/ directory exists with the stock
//  JSON configs. Existing configs are never overwritten.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::EnsureMachineConfigs (
    HINSTANCE                hInstance,
    const vector<fs::path> & searchPaths,
    const fs::path         & exeDir)
{
    HRESULT     hr           = S_OK;
    fs::path    machinesDir;
    error_code  ec;



    machinesDir = PathResolver::FindOrCreateAssetDir (searchPaths,
                                                      fs::path ("Machines"),
                                                      exeDir);

    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        // Per spec 005-disk-ii-audio Phase 12 (T120/T124): embedded
        // defaults extract into the per-machine subdir,
        // Machines/<MachineName>/<MachineName>.json, so each machine's
        // assets (config + ROMs + future per-machine extras) live
        // together.
        fs::path          machineSubdir = machinesDir / cfg.machineName;
        fs::path          target        = machineSubdir / cfg.fileName;
        span<const Byte>  bytes;
        HRESULT           hrItem        = S_OK;



        fs::create_directories (machineSubdir, ec);

        if (fs::exists (target, ec))
        {
            continue;
        }

        bytes = ExtractResource (hInstance, cfg.resourceId);

        if (bytes.empty())
        {
            hr = E_FAIL;
            continue;
        }

        hrItem = WriteFileBytes (target, bytes);

        if (FAILED (hrItem))
        {
            hr = hrItem;
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAssetBaseDirectory
//
//  Returns the install root that contains (or should contain) the
//  per-machine `Machines/` and per-device `Devices/` subtrees. We
//  reuse the existing `Machines/` directory locator -- whichever
//  search path holds Machines/ is, by construction, the install root.
//  Falls back to the exe directory when no Machines/ exists yet
//  (which `EnsureMachineConfigs` will then populate).
//
////////////////////////////////////////////////////////////////////////////////

fs::path AssetBootstrap::GetAssetBaseDirectory (
    const vector<fs::path> & searchPaths,
    const fs::path         & exeDir)
{
    fs::path  machinesDir;



    machinesDir = PathResolver::FindOrCreateAssetDir (searchPaths,
                                                      fs::path ("Machines"),
                                                      exeDir);
    return machinesDir.parent_path ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDiskDirectory
//
//  Returns the directory where downloaded disk images land. Mirrors
//  GetAssetBaseDirectory: an existing Disks/ found via the search
//  paths wins, otherwise we create one next to the exe.
//
////////////////////////////////////////////////////////////////////////////////

fs::path AssetBootstrap::GetDiskDirectory (
    const vector<fs::path> & searchPaths,
    const fs::path         & exeDir)
{
    return PathResolver::FindOrCreateAssetDir (searchPaths,
                                               fs::path ("Disks"),
                                               exeDir);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindRomSpec
//
//  Look up a (machineName, cassoName) pair against the catalog. Falls
//  back to a name-only match (machine-agnostic shared device ROMs
//  like the Disk II controller boot ROM) when no per-machine entry
//  exists.
//
////////////////////////////////////////////////////////////////////////////////

static const RomSpec * FindRomSpec (string_view machineName, string_view cassoName)
{
    const RomSpec  * result = nullptr;



    for (const RomSpec & spec : s_kRomCatalog)
    {
        if (spec.cassoName == cassoName && spec.machineName == machineName)
        {
            result = &spec;
            break;
        }
    }

    if (result == nullptr)
    {
        for (const RomSpec & spec : s_kRomCatalog)
        {
            if (spec.cassoName == cassoName && spec.machineName.empty ())
            {
                result = &spec;
                break;
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DownloadHttp
//
//  Fetches `urlPath` from `host` over HTTPS into `outBytes`. Validates
//  HTTP status and exact size. `displayName` is woven into the error
//  text on failure so the user sees which asset failed.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DownloadHttp (
    HINTERNET        hSession,
    LPCWSTR          host,
    LPCWSTR          urlPath,
    size_t           expectedSize,
    string_view      displayName,
    vector<Byte>   & outBytes,
    string         & outError)
{
    HRESULT      hr           = S_OK;
    HINTERNET    hConnect     = nullptr;
    HINTERNET    hRequest     = nullptr;
    BOOL         fOk          = FALSE;
    DWORD        statusCode   = 0;
    DWORD        statusSize   = sizeof (statusCode);
    DWORD        bytesAvail   = 0;
    DWORD        bytesRead    = 0;
    string       narrowHost;


    outBytes.clear();
    outBytes.reserve (expectedSize);

    for (LPCWSTR p = host; *p; p++)
    {
        narrowHost.push_back (static_cast<char> (*p & 0x7F));
    }

    hConnect = WinHttpConnect (hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    CBRF (hConnect != nullptr,
          outError = format ("Cannot connect to {}", narrowHost));

    hRequest = WinHttpOpenRequest (hConnect,
                                   L"GET",
                                   urlPath,
                                   nullptr,
                                   WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE);
    CBRF (hRequest != nullptr,
          outError = format ("Cannot open HTTPS request for {}", displayName));

    fOk = WinHttpSendRequest (hRequest,
                              WINHTTP_NO_ADDITIONAL_HEADERS,
                              0,
                              WINHTTP_NO_REQUEST_DATA,
                              0,
                              0,
                              0);
    CBRF (fOk,
          outError = format ("Network send failed for {}", displayName));

    fOk = WinHttpReceiveResponse (hRequest, nullptr);
    CBRF (fOk,
          outError = format ("No response from server for {}", displayName));

    fOk = WinHttpQueryHeaders (hRequest,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &statusCode,
                               &statusSize,
                               WINHTTP_NO_HEADER_INDEX);
    CBRF (fOk && statusCode == 200,
          outError = format ("HTTP {} fetching {}", statusCode, displayName));

    while (true)
    {
        vector<Byte>  chunk;

        bytesAvail = 0;
        fOk = WinHttpQueryDataAvailable (hRequest, &bytesAvail);
        CBRF (fOk,
              outError = format ("Read failed for {}", displayName));

        if (bytesAvail == 0)
        {
            break;
        }

        chunk.resize (bytesAvail);
        bytesRead = 0;
        fOk = WinHttpReadData (hRequest, chunk.data(), bytesAvail, &bytesRead);
        CBRF (fOk,
              outError = format ("Read failed for {}", displayName));

        if (bytesRead == 0)
        {
            break;
        }

        outBytes.insert (outBytes.end(), chunk.begin(), chunk.begin() + bytesRead);
    }

    CBRF (outBytes.size() == expectedSize,
          outError = format ("Downloaded {} has wrong size: got {}, expected {}",
                             displayName, outBytes.size(), expectedSize));

Error:
    if (hRequest != nullptr)
    {
        WinHttpCloseHandle (hRequest);
    }

    if (hConnect != nullptr)
    {
        WinHttpCloseHandle (hConnect);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DownloadOne
//
//  Downloads a single ROM from raw.githubusercontent.com into `outBytes`.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DownloadOne (
    HINTERNET        hSession,
    const RomSpec  & spec,
    vector<Byte>   & outBytes,
    string         & outError)
{
    wstring  wPath = wstring (s_kpszUrlPrefix) + AsciiToWide (spec.appleWinName);


    return DownloadHttp (hSession,
                         s_kpszAppleWinHost,
                         wPath.c_str(),
                         spec.expectedSize,
                         spec.cassoName,
                         outBytes,
                         outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptUser
//
////////////////////////////////////////////////////////////////////////////////

static bool PromptUser (HWND hwndParent, const vector<const RomSpec *> & missing)
{
    wstring  message;
    wstring  title;
    int      response = 0;


    message = L"Casso needs the following Apple ROM image(s):\n\n";

    for (const RomSpec * spec : missing)
    {
        message += L"    ";
        message += s_kchBullet;
        message += L' ';
        message += AsciiToWide (spec->cassoName);
        message += L"  ";
        message += s_kchEmDash;
        message += L"  ";
        message += AsciiToWide (spec->description);
        message += L'\n';
    }

    message += L"\nThese files are not bundled with Casso but are available from the "
               L"AppleWin open-source emulator project (https://github.com/AppleWin/AppleWin)."
               L"\n\nWould you like to download them now? ";

    title  = L"Casso ";
    title += s_kchEmDash;
    title += L" Download ROM Images";

    response = MessageBoxW (hwndParent,
                            message.c_str(),
                            title.c_str(),
                            MB_YESNO | MB_ICONQUESTION);

    return response == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindEmbeddedConfig
//
//  Case-insensitive match: the machine name can arrive from the
//  registry, the picker, or a `--machine` CLI flag with arbitrary
//  casing (filesystem lookups are case-insensitive on NTFS, so the
//  user has no reason to think otherwise).
//
////////////////////////////////////////////////////////////////////////////////

static const EmbeddedConfig * FindEmbeddedConfig (const wstring & machineName)
{
    const EmbeddedConfig  * result = nullptr;
    wstring                 wide;


    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        wide = AsciiToWide (cfg.machineName);

        if (_wcsicmp (wide.c_str(), machineName.c_str()) == 0)
        {
            result = &cfg;
            break;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadEmbeddedJson
//
//  Find the embedded RCDATA resource for `machineName`, narrow-copy a
//  best-effort ASCII version of the name into `outNarrowName` (for
//  use in error messages), and return the JSON bytes as a string.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT LoadEmbeddedJson (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    string          & outJsonText,
    string          & outNarrowName,
    string          & outError)
{
    HRESULT                 hr    = S_OK;
    const EmbeddedConfig  * cfg   = nullptr;
    span<const Byte>        bytes;


    outJsonText.clear();
    outNarrowName.clear();
    outNarrowName.reserve (machineName.size());

    for (wchar_t wch : machineName)
    {
        outNarrowName.push_back (static_cast<char> (wch & 0x7F));
    }

    cfg = FindEmbeddedConfig (machineName);
    CBRF (cfg != nullptr,
          outError = format ("No embedded config for machine '{}'", outNarrowName));

    bytes = ExtractResource (hInstance, cfg->resourceId);
    CBRF (!bytes.empty(),
          outError = format ("Embedded config resource for '{}' is empty",
                             string (cfg->machineName)));

    outJsonText.assign (reinterpret_cast<const char *> (bytes.data()),
                        bytes.size());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequiredRoms
//
//  Returns the relative ROM filenames referenced by the embedded
//  default config for `machineName`. The embedded config is the
//  source of truth for the in-app downloader: any ROMs the user
//  added by editing their on-disk Machines/<name>.json are their
//  responsibility to source manually. Returns an error if
//  `machineName` doesn't match a known embedded config.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::GetRequiredRoms (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    vector<string>  & outRomFiles,
    string          & outError)
{
    HRESULT  hr        = S_OK;
    string   jsonText;
    string   narrowName;


    outRomFiles.clear();

    hr = LoadEmbeddedJson (hInstance, machineName, jsonText, narrowName, outError);
    CHR (hr);

    hr = MachineConfigLoader::CollectRomFiles (jsonText, outRomFiles, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasDiskController
//
//  True iff the embedded config for `machineName` declares any slot
//  with `device == "disk-ii"`. Used to decide whether to offer the
//  user a boot-disk download — there's no point doing so for a
//  machine that has no controller wired up.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::HasDiskController (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    bool            & outHasDiskController,
    string          & outError)
{
    HRESULT             hr           = S_OK;
    string              jsonText;
    string              narrowName;
    JsonValue           root;
    JsonParseError      parseError;
    const JsonValue   * pSlots       = nullptr;
    HRESULT             hrOpt        = S_OK;
    size_t              idx          = 0;
    string              device;


    outHasDiskController = false;

    hr = LoadEmbeddedJson (hInstance, machineName, jsonText, narrowName, outError);
    CHR (hr);

    hr = JsonParser::Parse (jsonText, root, parseError);
    CHRF (hr,
          outError = format ("Embedded config for '{}' is malformed: {} at line {}",
                             narrowName, parseError.message, parseError.line));

    // `slots` is optional (][/][+ omit it); a missing slots array
    // simply means there is no Disk ][ controller for this machine.
    hrOpt = root.GetArray ("slots", pSlots);
    BAIL_OUT_IF (FAILED (hrOpt), S_OK);

    for (idx = 0; idx < pSlots->ArraySize(); idx++)
    {
        const JsonValue &  entry  = pSlots->ArrayAt (idx);
        HRESULT            hrDev  = entry.GetString ("device", device);

        if (SUCCEEDED (hrDev) && device == "disk-ii")
        {
            outHasDiskController = true;
            break;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckAndFetchRoms
//
//  If any ROM files required by the embedded config for `machineName`
//  are missing from `searchPaths`, prompt the user and download them
//  from the AppleWin project. Returns S_OK if all ROMs are present
//  (or were just downloaded), S_FALSE if the user declined the
//  download (caller should bail out cleanly), or an error HRESULT
//  with `outError` set on hard failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::CheckAndFetchRoms (
    HINSTANCE                hInstance,
    const wstring          & machineName,
    HWND                     hwndParent,
    const vector<fs::path> & searchPaths,
    const fs::path         & assetBaseDir,
    string                 & outError)
{
    HRESULT                  hr             = S_OK;
    vector<string>           romFiles;
    vector<const RomSpec *>  missing;
    HINTERNET                hSession       = nullptr;
    error_code               ec;
    bool                     userOk         = false;
    string                   narrowMachine;



    narrowMachine.reserve (machineName.size ());

    for (wchar_t wch : machineName)
    {
        narrowMachine.push_back (static_cast<char> (wch & 0x7F));
    }

    hr = GetRequiredRoms (hInstance, machineName, romFiles, outError);
    CHR (hr);

    for (const string & romFile : romFiles)
    {
        const RomSpec * spec    = FindRomSpec (narrowMachine, romFile);
        fs::path        relPath;
        fs::path        found;

        CBRF (spec != nullptr,
              outError = format ("ROM '{}' is missing and Casso has no download "
                                 "URL for it. Place the file under {} and try again.",
                                 romFile, assetBaseDir.string ()));

        relPath = fs::path (string (spec->localRelDir)) / spec->cassoName;
        found   = PathResolver::FindFile (searchPaths, relPath);

        if (!found.empty ())
        {
            continue;
        }

        missing.push_back (spec);
    }

    BAIL_OUT_IF (missing.empty(), S_OK);

    userOk = PromptUser (hwndParent, missing);
    BAIL_OUT_IF (!userOk, S_FALSE);

    hSession = WinHttpOpen (s_kpszUserAgent,
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
    CBRF (hSession != nullptr,
          outError = "Cannot initialize WinHTTP session");

    for (const RomSpec * spec : missing)
    {
        fs::path      destDir  = assetBaseDir / string (spec->localRelDir);
        fs::path      destPath = destDir / spec->cassoName;
        vector<Byte>  payload;

        fs::create_directories (destDir, ec);

        hr = DownloadOne (hSession, *spec, payload, outError);
        CHR (hr);

        hr = WriteFileBytes (destPath, payload);
        CHRF (hr,
              outError = format ("Cannot write {}", destPath.string()));
    }



Error:
    if (hSession != nullptr)
    {
        WinHttpCloseHandle (hSession);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEmbeddedDisplayName
//
//  Pulls the styled top-level "name" field (e.g. "Apple //e") out of
//  the embedded JSON for `machineName`. Falls back to `machineName`
//  itself if the JSON can't be loaded or doesn't have a name field.
//
////////////////////////////////////////////////////////////////////////////////

static wstring GetEmbeddedDisplayName (HINSTANCE hInstance, const wstring & machineName)
{
    HRESULT         hr        = S_OK;
    string          jsonText;
    string          narrowName;
    string          dummyError;
    JsonValue       root;
    JsonParseError  parseError;
    string          name;
    wstring         result    = machineName;


    hr = LoadEmbeddedJson (hInstance, machineName, jsonText, narrowName, dummyError);

    if (SUCCEEDED (hr))
    {
        hr = JsonParser::Parse (jsonText, root, parseError);
    }

    if (SUCCEEDED (hr) && SUCCEEDED (root.GetString ("name", name)) && !name.empty())
    {
        result.assign (name.begin(), name.end());
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptBootDisk
//
//  Three-button TaskDialog: DOS 3.3 / ProDOS / Cancel. Returns the
//  chosen disk spec, or nullptr if the user cancelled. Falls back to
//  a Yes/No/Cancel MessageBox if comctl32 v6's TaskDialogIndirect
//  isn't available.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int  s_kIdDos33  = 1001;
static constexpr int  s_kIdProDOS = 1002;
static constexpr int  s_kIdSkip   = IDCANCEL;


static const BootDiskSpec * PromptBootDisk (HWND hwndParent, const wstring & displayName)
{
    HRESULT               hr            = S_OK;
    int                   chosen        = 0;
    wstring               body;
    wstring               title;
    TASKDIALOGCONFIG      cfg           = { sizeof (TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON     buttons[2]    = {};
    const BootDiskSpec  * result        = nullptr;


    body  = L"The ";
    body += displayName;
    body += L" has a Disk ][ controller in slot 6 but no disk in drive 1, "
            L"and will spin forever waiting for one. A system master disk "
            L"is available from the Asimov archive "
            L"(https://www.apple.asimov.net).\n\n"
            L"Alternatives:\n"
            L"    ";
    body += s_kchBullet;
    body += L" Skip and use Disk > Insert Drive 1... (Ctrl+1) to "
            L"mount your own .dsk.\n"
            L"    ";
    body += s_kchBullet;
    body += L" Skip and press Ctrl+Reset once the drive starts "
            L"spinning to drop to BASIC.\n\n"
            L"Which disk would you like to download? ";

    title  = L"Casso ";
    title += s_kchEmDash;
    title += L" Boot Disk";

    buttons[0].nButtonID     = s_kIdDos33;
    buttons[0].pszButtonText = L"DOS 3.3 System Master\n"
                               L"Boots Applesoft BASIC; type CATALOG to list files.";
    buttons[1].nButtonID     = s_kIdProDOS;
    buttons[1].pszButtonText = L"ProDOS Users Disk\n"
                               L"Boots ProDOS 8 with the BASIC.SYSTEM shell.";

    cfg.hwndParent      = hwndParent;
    cfg.dwFlags         = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    cfg.pszWindowTitle  = title.c_str();
    cfg.pszMainIcon     = TD_INFORMATION_ICON;
    cfg.pszMainInstruction = L"No boot disk mounted";
    cfg.pszContent      = body.c_str();
    cfg.cButtons        = ARRAYSIZE (buttons);
    cfg.pButtons        = buttons;
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    cfg.nDefaultButton  = s_kIdDos33;

    hr = TaskDialogIndirect (&cfg, &chosen, nullptr, nullptr);

    if (FAILED (hr))
    {
        // TaskDialog unavailable for some reason — fall back to a
        // simpler MessageBox prompt: Yes=DOS3.3, No=ProDOS, Cancel=Skip.
        chosen = MessageBoxW (hwndParent,
            (body + L"\n\nYes = DOS 3.3, No = ProDOS, Cancel = Skip").c_str(),
            title.c_str(),
            MB_YESNOCANCEL | MB_ICONQUESTION);

        if      (chosen == IDYES) chosen = s_kIdDos33;
        else if (chosen == IDNO)  chosen = s_kIdProDOS;
        else                      chosen = s_kIdSkip;
    }

    if (chosen == s_kIdDos33)
    {
        result = &s_kDos33Disk;
    }
    else if (chosen == s_kIdProDOS)
    {
        result = &s_kProDOSDisk;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OfferBootDiskDownload
//
//  When invoked for a machine with a Disk ][ controller and no disk
//  has been resolved yet, prompts the user to download a stock Apple
//  master disk (DOS 3.3 / ProDOS) from the Asimov mirror, drops it
//  into `diskDir`, and returns its absolute path in `outDiskPath`.
//  Returns S_FALSE (and `outDiskPath` empty) when:
//    * the machine config has no Disk ][ controller, or
//    * the user declined the download.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::OfferBootDiskDownload (
    HINSTANCE                hInstance,
    const wstring          & machineName,
    HWND                     hwndParent,
    const fs::path         & diskDir,
    wstring                & outDiskPath,
    string                 & outError)
{
    HRESULT               hr           = S_OK;
    bool                  hasDisk      = false;
    const BootDiskSpec  * choice       = nullptr;
    HINTERNET             hSession     = nullptr;
    fs::path              destPath;
    vector<Byte>          payload;
    error_code            ec;


    outDiskPath.clear();

    hr = HasDiskController (hInstance, machineName, hasDisk, outError);
    CHR (hr);

    BAIL_OUT_IF (!hasDisk, S_FALSE);

    choice = PromptBootDisk (hwndParent, GetEmbeddedDisplayName (hInstance, machineName));
    BAIL_OUT_IF (choice == nullptr, S_FALSE);

    fs::create_directories (diskDir, ec);
    destPath = diskDir / choice->cassoName;

    // If the user already has the disk on disk (e.g. left over from a
    // prior session), skip the download.
    if (fs::exists (destPath, ec))
    {
        outDiskPath = destPath.wstring();
        BAIL_OUT_IF (true, S_OK);
    }

    hSession = WinHttpOpen (s_kpszUserAgent,
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
    CBRF (hSession != nullptr,
          outError = "Cannot initialize WinHTTP session");

    hr = DownloadHttp (hSession,
                       s_kpszAsimovHost,
                       choice->asimovUrlPath,
                       choice->expectedSize,
                       choice->shortLabel,
                       payload,
                       outError);
    CHR (hr);

    hr = WriteFileBytes (destPath, payload);
    CHRF (hr,
          outError = format ("Cannot write {}", destPath.string()));

    outDiskPath = destPath.wstring();

Error:
    if (hSession != nullptr)
    {
        WinHttpCloseHandle (hSession);
    }

    return hr;
}
