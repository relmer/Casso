#include "Pch.h"

#include "AssetBootstrap.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Ehm.h"
#include "External/StbVorbisWrapper.h"
#include "RegistrySettings.h"
#include "resource.h"
#include "UnicodeSymbols.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")


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
//  DiskAudioSpec
//
//  Per spec 005-disk-ii-audio Phase 13 (T131 / FR-017 / FR-018):
//  fully-specified map of OpenEmulator OGG sample basenames to Casso
//  WAV filenames, keyed by mechanism. Files live under
//  `raw.githubusercontent.com/openemulator/libemulation/master/res/sounds/<Mechanism>/`.
//
//  The Alps 2124A drive doesn't have a door (the user just yanks the
//  disk out), so DoorOpen / DoorClose entries are Shugart-only.
//
//  *** Licensing note ***: the upstream OpenEmulator samples are
//  GPL-3. We only fetch them on explicit user consent through the
//  bootstrap dialog (T133); the consent body discloses GPL-3 and
//  links to OpenEmulator's `COPYING` file. The fetched bytes are
//  *not* redistributed by Casso (we re-encode to WAV under the
//  user's local install only).
//
////////////////////////////////////////////////////////////////////////////////

struct DiskAudioSpec
{
    string_view  mechanism;          // "Shugart" or "Alps"
    string_view  oggBasename;        // upstream filename inside the OGG sounds folder
    string_view  wavBasename;        // Casso target filename (matches DiskIIAudioSource s_kpszSampleFiles)
};

static constexpr LPCWSTR  s_kpszOpenEmulatorHost      = L"raw.githubusercontent.com";
static constexpr LPCWSTR  s_kpszOpenEmulatorPathFmt   = L"/openemulator/libemulation/master/res/sounds/";

static constexpr DiskAudioSpec s_kDiskAudioCatalog[] =
{
    { "Shugart", "Shugart SA400 Drive.ogg", "MotorLoop.wav" },
    { "Shugart", "Shugart SA400 Head.ogg",  "HeadStep.wav"  },
    { "Shugart", "Shugart SA400 Stop.ogg",  "HeadStop.wav"  },
    { "Shugart", "Shugart SA400 Open.ogg",  "DoorOpen.wav"  },
    { "Shugart", "Shugart SA400 Close.ogg", "DoorClose.wav" },
    { "Alps",    "Alps 2124A Drive.ogg",    "MotorLoop.wav" },
    { "Alps",    "Alps 2124A Head.ogg",     "HeadStep.wav"  },
    { "Alps",    "Alps 2124A Stop.ogg",     "HeadStop.wav"  },
};

static constexpr string_view s_kDiskAudioMechanisms[] = { "Shugart", "Alps" };





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

    // A non-zero `expectedSize` is treated as an integrity check
    // (used for ROM downloads where we know the exact byte count
    // ahead of time). Pass 0 to skip the size check -- used for the
    // OpenEmulator OGG fetch where the upstream file size is not
    // pinned (T134).
    if (expectedSize > 0)
    {
        CBRF (outBytes.size() == expectedSize,
              outError = format ("Downloaded {} has wrong size: got {}, expected {}",
                                 displayName, outBytes.size(), expectedSize));
    }
    else
    {
        CBRF (!outBytes.empty (),
              outError = format ("Downloaded {} was empty", displayName));
    }

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




////////////////////////////////////////////////////////////////////////////////
//
//  FetchAndDecodeOgg
//
//  WinHTTP GET -> in-memory vector<uint8_t> -> stb_vorbis decode ->
//  int16 mono PCM at the source rate -> float32 -> linear-interp
//  resample to `targetSampleRate`. Discards the raw OGG bytes before
//  returning (NFR-006). Stereo input is downmixed to mono via simple
//  per-sample average; drive noise is broadband so we lose no
//  perceptual information.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::FetchAndDecodeOgg (
    HINTERNET         hSession,
    LPCWSTR           urlPath,
    uint32_t          targetSampleRate,
    vector<float>   & outPcm,
    string          & outError)
{
    HRESULT          hr             = S_OK;
    vector<Byte>     oggBytes;
    vector<int16_t>  shortPcm;
    uint32_t         srcRate        = 0;
    uint32_t         channels       = 0;
    size_t           srcFrames      = 0;
    size_t           dstFrames      = 0;
    size_t           i              = 0;
    string           narrowName;



    outPcm.clear ();

    CBRF (urlPath != nullptr,
          outError = "FetchAndDecodeOgg: null URL path");

    CBRF (targetSampleRate > 0,
          outError = "FetchAndDecodeOgg: target sample rate must be > 0");

    // Build a printable short name from the URL's last path component
    // so download errors are searchable in logs.
    {
        wstring  wUrl (urlPath);
        size_t   slash = wUrl.find_last_of (L'/');
        wstring  tail  = (slash == wstring::npos) ? wUrl : wUrl.substr (slash + 1);

        narrowName.reserve (tail.size ());

        for (wchar_t wch : tail)
        {
            narrowName.push_back (static_cast<char> (wch & 0x7F));
        }
    }

    hr = DownloadHttp (hSession,
                       s_kpszOpenEmulatorHost,
                       urlPath,
                       0,                       // 0 == any size acceptable
                       narrowName,
                       oggBytes,
                       outError);
    CHR (hr);

    hr = StbVorbisWrapper::DecodeOggToInterleavedShort (
        oggBytes.data (),
        oggBytes.size (),
        shortPcm,
        srcRate,
        channels,
        outError);
    CHR (hr);

    // The raw OGG bytes are not needed past this point; release them
    // before allocating the float buffer (NFR-006 -- no `.ogg` files
    // on disk, no in-memory hold past decode).
    oggBytes.clear ();
    oggBytes.shrink_to_fit ();

    CBRF (channels >= 1 && channels <= 2,
          outError = format ("Unsupported channel count {} (only mono and stereo handled)",
                             channels));

    srcFrames = shortPcm.size () / channels;

    CBRF (srcFrames > 0,
          outError = "OGG decoded to zero frames");

    // Downmix to mono float32 in a scratch buffer.
    {
        vector<float>  monoSrc;

        monoSrc.resize (srcFrames);

        for (i = 0; i < srcFrames; i++)
        {
            int32_t  sum = 0;
            uint32_t c   = 0;

            for (c = 0; c < channels; c++)
            {
                sum += static_cast<int32_t> (shortPcm[i * channels + c]);
            }

            // PCM 16-bit max swing is +/-32768; divide first to keep
            // the average inside a single int32 step, then normalize.
            monoSrc[i] = (float) sum / (32768.0f * (float) channels);
        }

        shortPcm.clear ();
        shortPcm.shrink_to_fit ();

        // Linear-interp resample (A-001: drive noise is broadband,
        // not pitch-critical, so we accept the cheap algorithm).
        if (srcRate == targetSampleRate)
        {
            outPcm = std::move (monoSrc);
        }
        else
        {
            double  ratio = (double) targetSampleRate / (double) srcRate;

            dstFrames = static_cast<size_t> ((double) srcFrames * ratio);

            outPcm.resize (dstFrames);

            for (i = 0; i < dstFrames; i++)
            {
                double  srcPos = (double) i / ratio;
                size_t  i0     = static_cast<size_t> (srcPos);
                size_t  i1     = (i0 + 1 < srcFrames) ? (i0 + 1) : i0;
                double  frac   = srcPos - (double) i0;

                outPcm[i] = (float) ((1.0 - frac) * (double) monoSrc[i0]
                                     +        frac * (double) monoSrc[i1]);
            }
        }
    }

Error:
    if (FAILED (hr))
    {
        outPcm.clear ();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WritePcmAsWav
//
//  16-bit PCM mono WAV. Samples outside [-1, +1] are clamped to int16
//  bounds; well-behaved drive samples won't hit the clamp but we lock
//  it in so a misbehaving source doesn't smear the entire WAV.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::WritePcmAsWav (
    const fs::path        & outPath,
    const vector<float>   & pcm,
    uint32_t                sampleRate,
    string                & outError)
{
    HRESULT     hr            = S_OK;
    ofstream    out;
    error_code  ec;
    size_t      sampleCount   = pcm.size ();
    uint32_t    dataBytes     = static_cast<uint32_t> (sampleCount * sizeof (int16_t));
    uint32_t    fileSize      = 36 + dataBytes;     // RIFF chunk-content size: 4 + (8 + 16) + (8 + dataBytes)
    uint16_t    numChannels   = 1;
    uint16_t    bitsPerSample = 16;
    uint32_t    byteRate      = sampleRate * numChannels * (bitsPerSample / 8);
    uint16_t    blockAlign    = numChannels * (bitsPerSample / 8);
    uint16_t    formatPcm     = 1;
    uint16_t    fmtSizeWord   = 0;
    uint32_t    fmtSize       = 16;
    size_t      i             = 0;
    int16_t     sampleI16     = 0;
    float       sampleF       = 0.0f;



    CBRF (sampleRate > 0,
          outError = "WritePcmAsWav: sample rate must be > 0");

    fs::create_directories (outPath.parent_path (), ec);

    out.open (outPath, ios::binary | ios::trunc);
    CBRF (out.good (),
          outError = format ("Cannot open {} for writing", outPath.string ()));

    out.write ("RIFF", 4);
    out.write (reinterpret_cast<const char *> (&fileSize), sizeof (fileSize));
    out.write ("WAVE", 4);

    out.write ("fmt ", 4);
    out.write (reinterpret_cast<const char *> (&fmtSize),       sizeof (fmtSize));
    out.write (reinterpret_cast<const char *> (&formatPcm),     sizeof (formatPcm));
    out.write (reinterpret_cast<const char *> (&numChannels),   sizeof (numChannels));
    out.write (reinterpret_cast<const char *> (&sampleRate),    sizeof (sampleRate));
    out.write (reinterpret_cast<const char *> (&byteRate),      sizeof (byteRate));
    out.write (reinterpret_cast<const char *> (&blockAlign),    sizeof (blockAlign));
    out.write (reinterpret_cast<const char *> (&bitsPerSample), sizeof (bitsPerSample));

    out.write ("data", 4);
    out.write (reinterpret_cast<const char *> (&dataBytes), sizeof (dataBytes));

    for (i = 0; i < sampleCount; i++)
    {
        sampleF = pcm[i] * 32767.0f;

        if (sampleF >  32767.0f) { sampleF =  32767.0f; }
        if (sampleF < -32768.0f) { sampleF = -32768.0f; }

        sampleI16 = static_cast<int16_t> (sampleF);
        out.write (reinterpret_cast<const char *> (&sampleI16), sizeof (sampleI16));
    }

    CBRF (out.good (),
          outError = format ("Write error on {}", outPath.string ()));

    out.close ();

    // `fmtSizeWord` is unused on Windows builds; silence the analyzer
    // by referencing it.
    (void) fmtSizeWord;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptDiskAudioConsent
//
//  Three-button TaskDialog: Download / Skip / Don't ask again this
//  session. Discloses the GPL-3 license of the OpenEmulator source
//  samples and the recipient obligation that entails. Mirrors
//  PromptBootDisk's runtime-template approach so we don't accrete a
//  second resource-script-based dialog.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int       s_kIdDiskAudioDownload       = 2001;
static constexpr int       s_kIdDiskAudioSkip           = IDCANCEL;
static constexpr LPCWSTR   s_kpszPromptForAudioDownload = L"PromptForAudioDownload";


////////////////////////////////////////////////////////////////////////////////
//
//  PromptDiskAudioConsent
//
//  Two-button TaskDialog: Download or Skip. Caller persists the
//  user's choice via the per-machine PromptForAudioDownload registry
//  DWORD so the prompt does not reappear on subsequent launches.
//
////////////////////////////////////////////////////////////////////////////////

static int PromptDiskAudioConsent (HWND hwndParent)
{
    int                  chosen     = s_kIdDiskAudioSkip;
    TASKDIALOGCONFIG     tdc        = {};
    TASKDIALOG_BUTTON    buttons[2] = {};
    HRESULT              hr         = S_OK;
    int                  result     = 0;

    LPCWSTR  content =
        L"Casso can download a small set of Disk II drive-noise samples "
        L"(\x2248 100 KB) from the OpenEmulator project to power the in-emulator "
        L"drive-audio feature. The samples will be cached on this machine.\n\n"
        L"The samples are licensed under GPL-3; please review their license "
        L"before redistributing them.";

    buttons[0].nButtonID     = s_kIdDiskAudioDownload;
    buttons[0].pszButtonText = L"Download\nFetch the samples and cache them locally.";
    buttons[1].nButtonID     = s_kIdDiskAudioSkip;
    buttons[1].pszButtonText = L"Skip\nLaunch without drive audio.";

    tdc.cbSize             = sizeof (tdc);
    tdc.hwndParent         = hwndParent;
    tdc.hInstance          = nullptr;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = L"Casso \x2014 Drive Audio Samples";
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = L"Download Disk II audio samples?";
    tdc.pszContent         = content;
    tdc.cButtons           = ARRAYSIZE (buttons);
    tdc.pButtons           = buttons;
    tdc.nDefaultButton     = s_kIdDiskAudioDownload;

    hr = TaskDialogIndirect (&tdc, &result, nullptr, nullptr);

    if (SUCCEEDED (hr))
    {
        chosen = result;
    }

    return chosen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectoryHasAnyWav
//
////////////////////////////////////////////////////////////////////////////////

static bool DirectoryHasAnyWav (const fs::path & dir)
{
    error_code  ec;
    bool        any = false;



    if (!fs::is_directory (dir, ec))
    {
        return false;
    }

    for (const auto & entry : fs::directory_iterator (dir, ec))
    {
        if (!entry.is_regular_file (ec))
        {
            continue;
        }

        if (entry.path ().extension () == L".wav")
        {
            any = true;
            break;
        }
    }

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckAndFetchDiskAudio
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::CheckAndFetchDiskAudio (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    HWND              hwndParent,
    const fs::path  & devicesDir,
    string          & outError)
{
    HRESULT      hr             = S_OK;
    HINTERNET    hSession       = nullptr;
    bool         anyMissing     = false;
    int          consent        = 0;
    DWORD        promptFlag     = 1;
    wstring      subkey;
    HRESULT      hrReg          = S_OK;
    error_code   ec;



    UNREFERENCED_PARAMETER (hInstance);

    // Per-machine "ask me about disk audio?" flag. Absent or non-zero
    // means "ask"; zero means the user has already answered (either
    // accepted the download or skipped it) and we never prompt again
    // for this machine on subsequent launches. Manual reset path:
    // delete the value (or set it to 1) via regedit.
    subkey = wstring (L"Machines\\") + machineName;
    hrReg  = RegistrySettings::ReadDword (subkey.c_str (),
                                          s_kpszPromptForAudioDownload,
                                          promptFlag);
    IGNORE_RETURN_VALUE (hrReg, S_OK);

    if (promptFlag == 0)
    {
        return S_FALSE;
    }

    for (string_view mech : s_kDiskAudioMechanisms)
    {
        fs::path  mechDir = devicesDir / string (mech);

        if (!DirectoryHasAnyWav (mechDir))
        {
            anyMissing = true;
            break;
        }
    }

    BAIL_OUT_IF (!anyMissing, S_OK);

    consent = PromptDiskAudioConsent (hwndParent);

    // The question is resolved either way -- never prompt again for
    // this machine. Persist the answer before acting on it so a
    // crash mid-download still suppresses the next launch's prompt.
    hrReg = RegistrySettings::WriteDword (subkey.c_str (),
                                          s_kpszPromptForAudioDownload,
                                          0);
    IGNORE_RETURN_VALUE (hrReg, S_OK);

    if (consent != s_kIdDiskAudioDownload)
    {
        return S_FALSE;
    }

    hSession = WinHttpOpen (s_kpszUserAgent,
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
    CBRF (hSession != nullptr,
          outError = "Cannot initialize WinHTTP session");

    for (const DiskAudioSpec & spec : s_kDiskAudioCatalog)
    {
        fs::path        mechDir   = devicesDir / string (spec.mechanism);
        fs::path        wavPath   = mechDir / string (spec.wavBasename);
        wstring         urlPath;
        vector<float>   pcm;
        HRESULT         hrItem    = S_OK;

        if (fs::exists (wavPath, ec))
        {
            continue;
        }

        fs::create_directories (mechDir, ec);

        // Build the URL path: prefix + mechanism + "/" + URL-escaped basename.
        urlPath  = s_kpszOpenEmulatorPathFmt;
        urlPath += wstring (spec.mechanism.begin (), spec.mechanism.end ());
        urlPath += L"/";

        for (char ch : spec.oggBasename)
        {
            if (ch == ' ')
            {
                urlPath += L"%20";
            }
            else
            {
                urlPath += static_cast<wchar_t> (static_cast<unsigned char> (ch));
            }
        }

        // Decode at the typical 44.1 kHz fallback; downstream
        // DiskIIAudioSource::LoadSamples re-resamples to the WASAPI
        // device rate via IMFSourceReader when it reads the WAV back
        // in. Per A-001, broadband drive noise tolerates the double
        // resample.
        hrItem = FetchAndDecodeOgg (hSession, urlPath.c_str (), 44100, pcm, outError);

        if (FAILED (hrItem) || pcm.empty ())
        {
            // FR-009: don't block startup on individual asset failure.
            DEBUGMSG (L"CheckAndFetchDiskAudio: %s\n",
                      wstring (outError.begin (), outError.end ()).c_str ());
            outError.clear ();
            continue;
        }

        hrItem = WritePcmAsWav (wavPath, pcm, 44100, outError);

        if (FAILED (hrItem))
        {
            DEBUGMSG (L"CheckAndFetchDiskAudio: write failed: %s\n",
                      wstring (outError.begin (), outError.end ()).c_str ());
            outError.clear ();
            continue;
        }
    }

Error:
    if (hSession != nullptr)
    {
        WinHttpCloseHandle (hSession);
    }

    return hr;
}
