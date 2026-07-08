#include "Pch.h"

#include "AssetBootstrap.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "Core/MachineConfigUpgrade.h"
#include "Core/PathResolver.h"
#include "External/StbVorbisWrapper.h"
#include "resource.h"
#include "Ui/ThemeManager.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/Dialogs/StartupDownloadDialog.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiPanel.h"
#include "Window/DxuiDialogWindow.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiSearchBox.h"
#include "Core/UnicodeSymbols.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "bcrypt.lib")


static constexpr LPCWSTR       s_kpszAppleWinHost = L"raw.githubusercontent.com";
static constexpr LPCWSTR       s_kpszUserAgent    = L"Casso/1.0";
static constexpr LPCWSTR       s_kpszUrlPrefix    = L"/AppleWin/AppleWin/master/resource/";

static constexpr LPCWSTR       s_kpszAsimovHost   = L"www.apple.asimov.net";

static constexpr int           s_kBootMruBodyWidthDp = 520;





////////////////////////////////////////////////////////////////////////////////
//
//  RomSpec
//
//  Static map: (machineName, Casso ROM filename) -> AppleWin source
//  basename + size + on-disk relative directory. Per spec
//  005-disk-ii-audio (T122 + Q1), machine-specific ROMs are
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





static std::wstring MachineDisplayName (std::string_view machineId)
{
    if (machineId == "Apple2")          return L"Apple ][";
    if (machineId == "Apple2Plus")      return L"Apple ][+";
    if (machineId == "Apple2e")         return L"Apple //e";
    if (machineId == "Apple2eEnhanced") return L"Apple //e Enhanced";
    return std::wstring (machineId.begin (), machineId.end ());
}





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
    int          currentVersion;     // must match "$cassoMachineVersion" in the embedded JSON
};





static constexpr EmbeddedConfig s_kEmbeddedConfigs[] =
{
    { IDR_MACHINE_APPLE2,     "Apple2",     "Apple2.json",     8 },
    { IDR_MACHINE_APPLE2PLUS, "Apple2Plus", "Apple2Plus.json", 8 },
    { IDR_MACHINE_APPLE2E,    "Apple2e",    "Apple2e.json",    7 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  PriorDefaultHashes
//
//  Normalized SHA-256 (BOM-stripped, CRLF→LF) of every embedded
//  machine config we've shipped in the past, keyed by machine name.
//  Used by EnsureMachineConfigs to detect unmodified on-disk extracts
//  from prior Casso releases so we can safely refresh them when the
//  embedded default changes shape (e.g. adding a Disk II to ][ /
//  ][+). On-disk files matching none of these hashes are presumed
//  user-edited and renamed aside before the current default is
//  installed.
//
//  When you bump an embedded config's currentVersion, hash the *old*
//  on-disk content (before the bump) and add an entry here so users
//  of the previous release get the upgrade automatically.
//
////////////////////////////////////////////////////////////////////////////////

static const MachineConfigPriorHash s_kPriorDefaultHashes[] =
{
    // v1 Apple2.json (no slots, no $cassoDefault stamp).
    { "Apple2",     "4045892d3410de8464430a2ce04386e4ca34a98a1a4d6163672fe22231daa942" },

    // v1 Apple2Plus.json (no slots, no $cassoDefault stamp).
    { "Apple2Plus", "ccb0046a31c57816d58634bdba2ce08f813806d2281fcbe6da94b4bf27242c8b" },

    // v1 Apple2e.json (had slot 6 already, but no $cassoDefault stamp).
    { "Apple2e",    "965f1f0fa55db9289000ddf9ff1e1416a9d85c0ebb5d35b8b8aa7f5ccf0da680" },

    // v3 Apple2.json (pre-releaseYear).
    { "Apple2",     "f2412b4d70c6847593af1aae5c310bb034dbfd38cc10594de402af75194cf671" },

    // v3 Apple2Plus.json (pre-releaseYear).
    { "Apple2Plus", "ebd7556e3cdfa3dd3fccc4993ec5d456e88063b0bd6d286372425f031b1c85df" },

    // v3 Apple2e.json (pre-releaseYear).
    { "Apple2e",    "9644241cca2e3220f520b8c13670f99a503a9dc3954dea514cf0a91aa9350040" },

    // v4 Apple2.json (added releaseYear; no cpuManufacturer).
    { "Apple2",     "15161b4c950923a667dd36fa00ebd56cfd983f1f43c44adbb84638443b29fab6" },

    // v4 Apple2Plus.json (added releaseYear; no cpuManufacturer).
    { "Apple2Plus", "832e16feb56939c03c9ac919d3159524577f9e033d846e69c008e945f92ba488" },

    // v4 Apple2e.json (added releaseYear; no cpuManufacturer).
    { "Apple2e",    "5be9af71ff3480c357e82512abae961966cc783767b1dbedc86b492a9cc72087" },

    // v5 Apple2.json (added cpuManufacturer = "MOS Technology").
    { "Apple2",     "c350e447e6c4e4bbaa307f9f5f11ee1cfe0eff2a8823fbb20d0a6eabcdd122ac" },

    // v5 Apple2Plus.json (added cpuManufacturer = "Synertek" -- wrong, fixed in v6).
    { "Apple2Plus", "32dbbce9904f47914f348f443e80df7b0b3b6c6dbf7f7193a943acc69b3f150d" },

    // v5 Apple2e.json (added cpuManufacturer = "Synertek" -- wrong, fixed in v6).
    { "Apple2e",    "51de42de280e38a1f0c9dfbe5f01e5cccb1498cdc20f14616e7821e99756891f" },

    // v6 Apple2.json (before adding apple2-gameport device).
    { "Apple2",     "4318ca7fe09b4f476216a26fbbeab96cdbc86a8ed74e70567e2f05ddfddb3ab2" },

    // v6 Apple2Plus.json (before adding apple2-gameport device).
    { "Apple2Plus", "a8796968d56185daf6a03bdebebdecc776dfc004003447a78f0bcc5dafaac3e1" },

    // v6 Apple2e.json (before adding the slot-1 parallel printer card).
    { "Apple2e",    "8593a47d87db9090ce001e439fea318854bdc6b255fa328f8ac2dabee1eb9f63" },

    // v8 ][ / ][+ add the slot-1 parallel printer card (matching //e). No
    // prior-hash entry is needed for the v7->v8 bump: both machines' on-disk
    // defaults are stamped ($cassoMachineVersion), so Plan refreshes them by
    // version comparison (diskVersion < embeddedVersion -> OverwriteSilent).
    // Prior-hash matching only applies to the unstamped v1-era extracts above.
    // (v7 tables mistakenly stayed at 6, so pre-v8 users -- v6 and v7 alike --
    // are all < 8 and refresh cleanly.)
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioSpec
//
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
    string_view  wavBasename;        // Casso target filename (matches Disk2AudioSource s_kpszSampleFiles)
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
    CWRA (hRes != nullptr);

    size = SizeofResource (hInstance, hRes);
    CBRA (size > 0);

    hMem = LoadResource (hInstance, hRes);
    CWRA (hMem != nullptr);

    data = LockResource (hMem);
    CWRA (data);

    result = span<const Byte> (static_cast<const Byte *> (data), static_cast<size_t> (size));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeConfigBytes — moved to MachineConfigUpgrade::NormalizeBytes.
//  Kept locally only for the BCrypt wrapper below; ComputeSha256 expects
//  already-normalized input from the caller.

////////////////////////////////////////////////////////////////////////////////
//
//  ComputeSha256
//
//  Thin BCrypt wrapper. Returns all-zeros on any BCrypt failure; the
//  caller treats that as "no match" since the hash list never
//  contains all-zeros.
//
////////////////////////////////////////////////////////////////////////////////

static array<uint8_t, 32> ComputeSha256 (const string & data)
{
    HRESULT             hr       = S_OK;
    array<uint8_t, 32>  result   = {};
    BCRYPT_ALG_HANDLE   hAlg     = nullptr;
    BCRYPT_HASH_HANDLE  hHash    = nullptr;
    NTSTATUS            status   = 0;



    status = BCryptOpenAlgorithmProvider (&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    CBR (BCRYPT_SUCCESS (status));

    status = BCryptCreateHash (hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    CBR (BCRYPT_SUCCESS (status));

    status = BCryptHashData (hHash,
                             reinterpret_cast<PUCHAR> (const_cast<char *> (data.data())),
                             static_cast<ULONG> (data.size()),
                             0);
    CBRF (BCRYPT_SUCCESS (status), result = {});

    status = BCryptFinishHash (hHash,
                               result.data(),
                               static_cast<ULONG> (result.size()),
                               0);
    CBRF (BCRYPT_SUCCESS (status), result = {});


Error:
    if (hHash != nullptr)
    {
        BCryptDestroyHash (hHash);
    }

    if (hAlg != nullptr)
    {
        BCryptCloseAlgorithmProvider (hAlg, 0);
    }

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
//  BackupUserEditedConfig
//
//  Rename `target` aside with a timestamped suffix so it's preserved
//  but out of the way before the current embedded default takes its
//  place. Timestamp is local-time YYYYMMDD-HHMMSS to make the backup
//  set chronologically obvious in a file listing.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT BackupUserEditedConfig (const fs::path & target)
{
    HRESULT          hr          = S_OK;
    SYSTEMTIME       now         = {};
    wchar_t          stamp[32]   = {};
    fs::path         backupPath;
    error_code       ec;



    GetLocalTime (&now);
    swprintf_s (stamp,
                L".user-backup-%04u%02u%02u-%02u%02u%02u",
                now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond);

    backupPath = target;
    backupPath += stamp;

    fs::rename (target, backupPath, ec);
    CBR (!ec);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureMachineConfigs
//
//  Make sure at least one Machines/ directory exists with the stock
//  JSON configs. Upgrade decision is delegated to the pure planner
//  in MachineConfigUpgrade::Plan — this wrapper does only the I/O
//  bits (read on-disk content, compute SHA-256 via BCrypt, perform
//  the backup rename, write the resource bytes).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::EnsureMachineConfigs (
    HINSTANCE hInstance)
{
    HRESULT     hr           = S_OK;
    fs::path    machinesDir;
    error_code  ec;



    // Embedded machine JSONs always extract under %LOCALAPPDATA%\Casso\,
    // not into whichever exe-adjacent Machines/ a dev happens to have
    // lying around.
    machinesDir = GetAssetBaseDirectory() / L"Machines";
    fs::create_directories (machinesDir, ec);

    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        // Embedded
        // defaults extract into the per-machine subdir,
        // Machines/<MachineName>/<MachineName>.json, so each machine's
        // assets (config + ROMs + future per-machine extras) live
        // together.
        fs::path                      machineSubdir   = machinesDir / cfg.machineName;
        fs::path                      target          = machineSubdir / cfg.fileName;
        span<const Byte>              bytes;
        HRESULT                       hrItem          = S_OK;
        bool                          diskExists      = false;
        string                        diskContent;
        string                        diskHashHex;
        MachineConfigUpgradeAction    action          = MachineConfigUpgradeAction::Skip;



        fs::create_directories (machineSubdir, ec);

        diskExists = fs::exists (target, ec);

        if (diskExists)
        {
            ifstream            diskFile (target);
            stringstream        ss;
            array<uint8_t, 32>  diskHash = {};

            if (!diskFile.good())
            {
                continue;
            }

            ss << diskFile.rdbuf();
            diskContent = ss.str();
            diskHash    = ComputeSha256 (MachineConfigUpgrade::NormalizeBytes (diskContent));
            diskHashHex = MachineConfigUpgrade::BytesToHex (diskHash);
        }

        action = MachineConfigUpgrade::Plan (
            cfg.machineName,
            cfg.currentVersion,
            diskExists ? &diskContent : nullptr,
            diskHashHex,
            span<const MachineConfigPriorHash> (s_kPriorDefaultHashes));

        if (action == MachineConfigUpgradeAction::Skip)
        {
            continue;
        }

        if (action == MachineConfigUpgradeAction::BackupAndReplace)
        {
            HRESULT hrBak = BackupUserEditedConfig (target);

            if (FAILED (hrBak))
            {
                // If we can't rename the user's file aside, do NOT
                // clobber it — bail on this machine and let the user
                // sort it out.
                hr = hrBak;
                continue;
            }
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
//  EmbeddedThemeFile / EmbeddedTheme
//
//  Static catalog of every file (per built-in theme) that ships
//  embedded in Casso.exe. Each theme lists its files via a leaf
//  relative to the theme directory; `EnsureThemes` extracts them
//  into `<assetBase>/Themes/<dirName>/<relPath>` after the per-theme
//  install/skip decision (`ThemeBootstrapPlanner::Plan`).
//
//  Bump `currentVersion` in lockstep with the `$cassoThemeVersion`
//  field of the on-disk `theme.json` for that theme. Older built-in
//  copies on disk (same `dirName` + `$cassoBuiltIn: true` + lower
//  version) are upgraded in place; user themes (no built-in marker)
//  are never touched.
//
////////////////////////////////////////////////////////////////////////////////

struct EmbeddedThemeFile
{
    int          resourceId;
    const char * relativePath;       // POSIX-style; ASCII only
};





struct EmbeddedTheme
{
    const char *                       dirName;        // matches "name" in theme.json
    int                                currentVersion; // mirrors theme.json $cassoThemeVersion
    span<const EmbeddedThemeFile>      files;
};





static constexpr EmbeddedThemeFile s_kSkeuomorphicFiles[] =
{
    { IDR_THEME_SKEUO_THEME_JSON,         "theme.json"          },
    { IDR_THEME_SKEUO_FONT_TTF,           "fonts/Inter-Regular.ttf" },
    { IDR_THEME_SKEUO_FONT_OFL,           "fonts/OFL.txt"       },
    { IDR_THEME_SKEUO_FONT_TODO,          "fonts/TODO_FONTS.md" },
};





static constexpr EmbeddedThemeFile s_kDarkModernFiles[] =
{
    { IDR_THEME_DARK_THEME_JSON,         "theme.json"          },
    { IDR_THEME_DARK_FONT_TTF,           "fonts/Inter-Regular.ttf" },
    { IDR_THEME_DARK_FONT_OFL,           "fonts/OFL.txt"       },
    { IDR_THEME_DARK_FONT_TODO,          "fonts/TODO_FONTS.md" },
};





static constexpr EmbeddedThemeFile s_kRetroTerminalFiles[] =
{
    { IDR_THEME_RETRO_THEME_JSON,         "theme.json"          },
    { IDR_THEME_RETRO_FONT_TTF,           "fonts/VT323-Regular.ttf" },
    { IDR_THEME_RETRO_FONT_OFL,           "fonts/OFL.txt"       },
    { IDR_THEME_RETRO_FONT_TODO,          "fonts/TODO_FONTS.md" },
};





static const EmbeddedTheme s_kEmbeddedThemes[] =
{
    { "Skeuomorphic",  1, span<const EmbeddedThemeFile> (s_kSkeuomorphicFiles)  },
    { "DarkModern",    1, span<const EmbeddedThemeFile> (s_kDarkModernFiles)    },
    { "RetroTerminal", 1, span<const EmbeddedThemeFile> (s_kRetroTerminalFiles) },
};





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureThemes
//
//  For each built-in theme, consult `ThemeBootstrapPlanner::Plan`
//  with the on-disk theme.json (if any) and either skip the
//  directory (user theme or up-to-date built-in) or re-extract
//  every embedded file. User-authored sibling themes are untouched
//  because their `theme.json` lacks the `$cassoBuiltIn: true`
//  marker — the planner only ever returns `InstallBuiltIn` for an
//  already-built-in directory whose version is stale, or for an
//  empty / corrupt directory belonging to one of *our* names.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::EnsureThemes (
    HINSTANCE hInstance)
{
    HRESULT     hr        = S_OK;
    fs::path    themesDir;
    error_code  ec;



    // Same logic as EnsureMachineConfigs: extract to %LOCALAPPDATA%\Casso\,
    // not into an exe-adjacent Themes/ dir.
    themesDir = GetAssetBaseDirectory() / L"Themes";
    fs::create_directories (themesDir, ec);

    for (const EmbeddedTheme & theme : s_kEmbeddedThemes)
    {
        fs::path              themeSubdir       = themesDir / theme.dirName;
        fs::path              themeJsonPath     = themeSubdir / "theme.json";
        bool                  diskExists        = false;
        string                diskJsonText;
        string                embeddedJsonText;
        ThemeBootstrapAction  action            = ThemeBootstrapAction::Skip;
        int                   themeJsonResId    = 0;



        diskExists = fs::exists (themeJsonPath, ec);

        if (diskExists)
        {
            ifstream      file (themeJsonPath);
            stringstream  ss;

            if (file.good())
            {
                ss << file.rdbuf();
                diskJsonText = ss.str();
            }
        }

        // Find the embedded theme.json resource id so the planner can
        // diff disk vs canonical bytes. Built-in themes always have a
        // theme.json file as their first entry by convention; cope if
        // ever that stops being true by scanning.
        for (const EmbeddedThemeFile & f : theme.files)
        {
            if (std::string (f.relativePath) == "theme.json")
            {
                themeJsonResId = f.resourceId;
                break;
            }
        }

        if (themeJsonResId != 0)
        {
            span<const Byte>  embeddedBytes = ExtractResource (hInstance, themeJsonResId);
            embeddedJsonText.assign (reinterpret_cast<const char *> (embeddedBytes.data()),
                                     embeddedBytes.size());
        }

        action = ThemeBootstrapPlanner::Plan (diskExists ? &diskJsonText : nullptr,
                                              embeddedJsonText,
                                              theme.currentVersion);

        if (action == ThemeBootstrapAction::Skip)
        {
            continue;
        }

        // InstallBuiltIn: (re)extract every file. Create the theme
        // directory + any sub-dirs (e.g. fonts/) as we go.
        fs::create_directories (themeSubdir, ec);

        for (const EmbeddedThemeFile & f : theme.files)
        {
            fs::path          target = themeSubdir / f.relativePath;
            span<const Byte>  bytes;
            HRESULT           hrItem;

            fs::create_directories (target.parent_path(), ec);

            bytes = ExtractResource (hInstance, f.resourceId);

            // 0-byte resources are legal (the font placeholders).
            // bytes.data() can still be non-null because RC always
            // produces a tracked allocation. WriteFileBytes happily
            // writes a 0-byte file.
            hrItem = WriteFileBytes (target, bytes);

            if (FAILED (hrItem))
            {
                hr = hrItem;
            }
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAssetBaseDirectory
//
//  Returns %LOCALAPPDATA%\Casso\ -- the single, user-writable root for
//  every file Casso reads or writes at runtime (Machines/, Themes/,
//  Disks/, UserPrefs.json, schema backups, downloaded ROMs,
//  captured audio, etc.). Created on first launch if missing.
//
//  The location is no longer a function of where the EXE lives. That
//  removes every Program Files write attempt and keeps user state out
//  of the install directory.
//
//  Read-only built-in defaults (machine JSONs, theme.json, fonts) are
//  embedded as RT_RCDATA in Casso.exe and extracted into this dir on
//  first launch via EnsureMachineConfigs / EnsureThemes.
//
////////////////////////////////////////////////////////////////////////////////

fs::path AssetBootstrap::GetAssetBaseDirectory()
{
    return PathResolver::GetLocalAppDataDir (L"Casso");
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetDiskDirectory
//
//  Returns <localAppData>\Casso\Disks\ -- downloaded disk images land
//  here regardless of where the EXE was invoked from. Created on
//  demand.
//
////////////////////////////////////////////////////////////////////////////////

fs::path AssetBootstrap::GetDiskDirectory()
{
    fs::path     base   = GetAssetBaseDirectory();
    fs::path     disks  = base / L"Disks";
    error_code   ec;

    fs::create_directories (disks, ec);
    return disks;
}




////////////////////////////////////////////////////////////////////////////////
//
//  WorktreeKeyOf  (file-local)
//
//  A Claude worktree checkout lives at <repo>/.claude/worktrees/<name>/...
//  Returns the ".../.claude/worktrees/<name>" prefix identifying that
//  worktree, or an empty string if `p` is not inside one.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring WorktreeKeyOf (const fs::path & p)
{
    std::vector<fs::path>  comps (p.begin(), p.end());

    for (size_t i = 0; i + 2 < comps.size(); ++i)
    {
        if (_wcsicmp (comps[i].c_str(),     L".claude")   == 0 &&
            _wcsicmp (comps[i + 1].c_str(), L"worktrees") == 0)
        {
            fs::path  key;

            for (size_t j = 0; j <= i + 2; ++j)
                key /= comps[j];

            return key.wstring();
        }
    }

    return std::wstring();
}



////////////////////////////////////////////////////////////////////////////////
//
//  IsForeignWorktreeDisk
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::IsForeignWorktreeDisk (const fs::path & p)
{
    // The %LOCALAPPDATA% recent-disks MRU is shared across every checkout of
    // the repo (the main tree plus each .claude/worktrees/<name> copy), so it
    // accumulates absolute paths from all of them -- the same demo then shows
    // once per checkout. Hide any MRU disk that lives under a worktree OTHER
    // than the one this build runs from; disks outside any worktree (the main
    // tree, or the user's own folders) always pass.
    static const std::wstring  runningKey = WorktreeKeyOf (PathResolver::GetExecutableDirectory());
    const std::wstring         entryKey   = WorktreeKeyOf (p);

    if (entryKey.empty())
        return false;                                        // not under a worktree

    return _wcsicmp (entryKey.c_str(), runningKey.c_str()) != 0;
}



////////////////////////////////////////////////////////////////////////////////
//
//  AppendBundledDemoDisks
//
////////////////////////////////////////////////////////////////////////////////

void AssetBootstrap::AppendBundledDemoDisks (std::vector<DiskMru::Entry> & mountable)
{
    std::vector<fs::path>  demos;
    error_code             ec;

    // Locate Apple2/Demos ONCE per process. It ships in the source tree, not
    // an installed layout, so its location is purely a function of the exe
    // path + repo layout -- neither changes while we run. A miss means this
    // is not a repo build, and that can't change at runtime either, so we
    // record the absence and never re-attempt the walk-up. `std::nullopt`
    // means "no demos dir"; a value is the resolved directory.
    static const std::optional<fs::path>  demosDir = [] () -> std::optional<fs::path>
    {
        fs::path     cursor = PathResolver::GetExecutableDirectory();
        error_code   ecDir;

        // The exe runs from <repo>/<platform>/<config>/Casso.exe, so walk a
        // few levels up from the exe (and also try the working directory).
        for (int i = 0; i < 4 && !cursor.empty(); ++i)
        {
            fs::path  candidate = cursor / L"Apple2" / L"Demos";

            if (fs::is_directory (candidate, ecDir))
            {
                return candidate;
            }
            cursor = cursor.parent_path();
        }

        fs::path  cwdCandidate = PathResolver::GetWorkingDirectory() / L"Apple2" / L"Demos";
        if (fs::is_directory (cwdCandidate, ecDir))
        {
            return cwdCandidate;
        }

        return std::nullopt;
    } ();

    // Not a repo build -- nothing to offer, and that will never change.
    if (!demosDir.has_value())
    {
        return;
    }

    // Enumerate the directory fresh on every open: in a repo build the user
    // can drop a new disk image into Apple2/Demos while Casso is running, and
    // the picker should pick it up. Listing this small directory is cheap.
    for (const fs::directory_entry & entry : fs::directory_iterator (*demosDir, ec))
    {
        error_code  ecFile;

        if (entry.is_regular_file (ecFile) &&
            IsSupportedDiskImageExtension (entry.path().wstring()))
        {
            demos.push_back (entry.path().lexically_normal());
        }
    }

    std::sort (demos.begin(), demos.end());

    for (const fs::path & demo : demos)
    {
        bool        already = false;
        error_code  ecDup;

        for (const DiskMru::Entry & existing : mountable)
        {
            if (fs::equivalent (existing.path, demo, ecDup))
            {
                already = true;
                break;
            }
        }

        if (!already)
        {
            mountable.push_back (DiskMru::Entry { demo, 0 });
        }
    }
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
    HINTERNET                  hSession,
    LPCWSTR                    host,
    LPCWSTR                    urlPath,
    size_t                     expectedSize,
    string_view                displayName,
    vector<Byte>             & outBytes,
    string                   & outError,
    std::atomic<std::uint64_t> * progressBytes  = nullptr,
    std::atomic<bool>          * cancelRequested = nullptr)
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

    if (progressBytes != nullptr)
    {
        progressBytes->store (0, std::memory_order_relaxed);
    }

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

        if (cancelRequested != nullptr && cancelRequested->load (std::memory_order_relaxed))
        {
            outError = format ("{} cancelled", displayName);
            hr = E_ABORT;
            goto Error;
        }

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

        if (progressBytes != nullptr)
        {
            progressBytes->store ((std::uint64_t) outBytes.size(), std::memory_order_relaxed);
        }
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
//  True iff the machine `machineName`, as the user currently has it
//  configured, has an ENABLED Disk ][ controller slot. Starts from the
//  embedded default config and folds in the user's per-machine delta so a
//  slot the user disabled in Settings > Machine ("enabled": false) is
//  reported as absent -- otherwise we'd offer a boot-disk download / pop the
//  boot-disk picker for a machine the user deliberately stripped of its
//  controller. Used to gate the boot-disk flow.
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
    JsonValue           merged;
    JsonParseError      parseError;
    const JsonValue   * pSlots       = nullptr;
    HRESULT             hrOpt        = S_OK;
    size_t              idx          = 0;
    string              device;
    Win32FileSystem     fsMerge;
    UserConfigStore     store        (GetAssetBaseDirectory().wstring());



    outHasDiskController = false;

    hr = LoadEmbeddedJson (hInstance, machineName, jsonText, narrowName, outError);
    CHR (hr);

    hr = JsonParser::Parse (jsonText, root, parseError);
    CHRF (hr,
          outError = format ("Embedded config for '{}' is malformed: {} at line {}",
                             narrowName, parseError.message, parseError.line));

    // Fold the user's saved per-machine delta over the embedded default so a
    // Settings-disabled slot ("enabled": false) is honored here exactly as it
    // is when the machine is built (MachineConfig::Parse). Fall back to the
    // base config if the merge fails.
    {
        JsonValue  mergedTmp;
        HRESULT    hrMerge = store.Load (narrowName, root, fsMerge, mergedTmp);

        if (SUCCEEDED (hrMerge) && mergedTmp.GetType() == JsonType::Object)
        {
            merged = std::move (mergedTmp);
        }
        else
        {
            merged = root;
        }
    }

    // `slots` is optional (][/][+ omit it); a missing slots array
    // simply means there is no Disk ][ controller for this machine.
    hrOpt = merged.GetArray ("slots", pSlots);
    BAIL_OUT_IF (FAILED (hrOpt), S_OK);

    for (idx = 0; idx < pSlots->ArraySize(); idx++)
    {
        const JsonValue &  entry   = pSlots->ArrayAt (idx);
        HRESULT            hrDev   = entry.GetString ("device", device);
        bool               enabled = true;   // optional key; defaults enabled

        if (SUCCEEDED (hrDev) && device == "disk-ii")
        {
            HRESULT  hrEnabled = entry.GetBool ("enabled", enabled);

            IGNORE_RETURN_VALUE (hrEnabled, S_OK);
            if (enabled)
            {
                outHasDiskController = true;
                break;
            }
        }
    }

Error:
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
//  DownloadStockBootDisk
//
//  Pure download helper used by PromptBootDiskMru's "Download..." rows.
//  Fetches `spec` from the Asimov mirror, writes it under `diskDir`,
//  and returns the absolute path in `outDiskPath`. No UI; the caller
//  owns the prompt and progress reporting.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DownloadStockBootDisk (
    const BootDiskSpec     & spec,
    const fs::path         & diskDir,
    wstring                & outDiskPath,
    string                 & outError)
{
    HRESULT       hr        = S_OK;
    HINTERNET     hSession  = nullptr;
    fs::path      destPath;
    vector<Byte>  payload;
    error_code    ec;


    outDiskPath.clear();

    fs::create_directories (diskDir, ec);
    destPath = diskDir / spec.cassoName;

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
                       spec.asimovUrlPath,
                       spec.expectedSize,
                       spec.shortLabel,
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
//  FilesHaveSameContent
//
//  Cheap byte-equality check for disk-image dedup heuristics in the
//  MRU pickers. Bails on size mismatch before reading any bytes, so
//  the common "not the same file" case costs one stat per side.
//
//  When the sizes DO match (common: 140 KB DOS 3.3 / ProDOS masters
//  share a size with many recent disks), the files are compared a
//  chunk at a time and the scan bails on the first differing block.
//  Distinct same-size disk images almost always diverge in the boot
//  sector / catalog near the front, so this avoids reading both images
//  in full on every picker open -- only a genuine match (rare, <= the
//  couple of stock rows) reads to the end.
//
////////////////////////////////////////////////////////////////////////////////

static bool FilesHaveSameContent (const fs::path & a, const fs::path & b)
{
    constexpr std::streamsize  kChunk = 64 * 1024;

    std::error_code  ec;
    uintmax_t        sizeA   = fs::file_size (a, ec);
    uintmax_t        sizeB   = 0;
    std::ifstream    fa;
    std::ifstream    fb;
    std::vector<char> bufA ((size_t) kChunk);
    std::vector<char> bufB ((size_t) kChunk);


    if (ec)
    {
        return false;
    }

    sizeB = fs::file_size (b, ec);
    if (ec || sizeA != sizeB || sizeA == 0)
    {
        return false;
    }

    fa.open (a, std::ios::binary);
    fb.open (b, std::ios::binary);
    if (!fa.is_open() || !fb.is_open())
    {
        return false;
    }

    for (uintmax_t remaining = sizeA; remaining > 0; )
    {
        std::streamsize  want = (remaining < (uintmax_t) kChunk) ? (std::streamsize) remaining : kChunk;

        fa.read (bufA.data(), want);
        fb.read (bufB.data(), want);
        if (fa.gcount() != want || fb.gcount() != want)
        {
            return false;       // short read on either side -> treat as mismatch
        }

        if (std::memcmp (bufA.data(), bufB.data(), (size_t) want) != 0)
        {
            return false;       // first differing block -> done, no full read
        }

        remaining -= (uintmax_t) want;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession
//
//  Owns the live UI state for the boot / insert disk pickers: the model
//  rows, the themed search box, the multi-column list (with clickable
//  header sort and a scrollbar), the filtered + sorted view mapping, and
//  the dialog hooks that drive paint / input / focus / tick. A single
//  instance is captured by the dialog lambdas so all state lives in one
//  place. The list is focus stop 0 (auto-focused on open) and the search
//  box is stop 1, so the magnifier affordance shows until the user tabs
//  or clicks into the field. Result codes flow straight from the chosen
//  row back to the caller, which maps them to a path or download.
//
////////////////////////////////////////////////////////////////////////////////

class DiskMruPickerSession
{
public:
    struct ModelRow
    {
        std::wstring  name;                 // col 0 "Disk image"
        std::wstring  location;             // col 1 "Location"
        std::int64_t  loadedUnix  = 0;      // col 2 "Last loaded" source; 0 = none/unknown
        int           resultCode  = 0;      // value returned to the caller when this row is chosen
        bool          dimLocation = false;
    };

    DiskMruPickerSession (HINSTANCE hInstance, HWND hwndParent, std::string_view themeName)
        : m_hInstance (hInstance), m_hwndParent (hwndParent), m_themeName (themeName) {}

    void  SetText           (const std::wstring & title, const std::wstring & intro) { m_title = title; m_intro = intro; }
    void  SetModelRows      (std::vector<ModelRow> rows)                             { m_model = std::move (rows); }
    void  AddButton         (const DialogButton & button)                           { m_buttons.push_back (button); }
    void  SetCloseBoxResult (int code)                                              { m_closeBoxResult = code; }
    int   Run               ();

    static std::int64_t  FileMtimeUnix (const fs::path & path);

private:
    void                ConfigureWidgets   ();
    void                RebuildView        ();
    void                ApplySort          (int column);
    int                 ChosenResultAt     (int visibleRow) const;
    static std::wstring FormatLastLoaded   (std::int64_t loadedUnix);

    static constexpr int    s_kColLastLoaded        = 0;
    static constexpr int    s_kColDiskImage         = 1;
    static constexpr int    s_kColLocation          = 2;
    static constexpr int    s_kColumnCount          = 3;
    static constexpr int    s_kListStop             = 0;
    static constexpr int    s_kSearchStop           = 1;
    static constexpr int    s_kFocusStopCount       = 2;
    static constexpr int    s_kSearchHeightDip      = 30;
    static constexpr int    s_kSearchListGapDip     = 8;
    static constexpr int    s_kChromeReserveDip     = 240;
    static constexpr int    s_kMinBodyHeightDip     = 160;
    static constexpr int    s_kResizeGrabDip        = 4;
    static constexpr int    s_kResizableMinWidthDip  = 480;
    static constexpr int    s_kResizableMinHeightDip = 320;
    static constexpr int    s_kResizableDefWidthDip  = 720;
    static constexpr int    s_kResizableDefHeightDip = 520;
    static constexpr int    s_kUnclampedBodyHeightPx = 100000;
    static constexpr int    s_kDateTimeBufChars     = 64;
    static constexpr int    s_kBaseDpi              = 96;
    static constexpr UINT   s_kTickIntervalMs       = 30;
    static constexpr float  s_kIconSizeDip          = 64.0f;

    static constexpr std::uint64_t  s_kFiletimeTicksPerSec = 10000000ULL;
    static constexpr std::uint64_t  s_kUnixEpochFiletime   = 116444736000000000ULL;

    HINSTANCE                  m_hInstance       = nullptr;
    HWND                       m_hwndParent      = nullptr;
    std::string                m_themeName;
    std::wstring               m_title;
    std::wstring               m_intro;
    std::vector<ModelRow>      m_model;
    std::vector<DialogButton>  m_buttons;
    std::optional<int>         m_closeBoxResult;
    DxuiListView               m_list;
    DxuiSearchBox              m_search;
    std::vector<int>           m_view;
    std::wstring               m_filter;
    int                        m_sortColumn      = s_kColLastLoaded;
    bool                       m_sortDescending  = true;
    int                        m_focusStop       = -1;
    UINT                       m_dpi             = s_kBaseDpi;
    RECT                       m_searchRectPx    = {};
    RECT                       m_listRectPx      = {};
    int                        m_searchHeightPx  = 0;
    int                        m_gapPx           = 0;
    int                        m_maxBodyHeightPx = 0;
    std::optional<int>         m_pendingChoice;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::FileMtimeUnix
//
//  Returns the file's last-write time as Unix seconds, or 0 when the
//  file is missing / unreadable. Used to backfill a "Last loaded" date
//  for MRU entries recorded before the list tracked load times.
//
////////////////////////////////////////////////////////////////////////////////

std::int64_t DiskMruPickerSession::FileMtimeUnix (const fs::path & path)
{
    HRESULT                                hr      = S_OK;
    std::int64_t                           result  = 0;
    std::error_code                        ec;
    fs::file_time_type                     ftime;
    std::chrono::system_clock::time_point  sysTime;



    ftime = fs::last_write_time (path, ec);
    BAIL_OUT_IF (static_cast<bool> (ec), S_OK);

    sysTime = std::chrono::clock_cast<std::chrono::system_clock> (ftime);
    result  = (std::int64_t) std::chrono::system_clock::to_time_t (sysTime);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::FormatLastLoaded
//
//  Formats a Unix-second timestamp as a localized "short-date hh:mm"
//  string. Returns empty for unknown (<= 0) times.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskMruPickerSession::FormatLastLoaded (std::int64_t loadedUnix)
{
    HRESULT         hr      = S_OK;
    std::wstring    result;
    ULARGE_INTEGER  uli     = {};
    FILETIME        ftUtc   = {};
    FILETIME        ftLocal = {};
    SYSTEMTIME      st      = {};
    BOOL            ok      = FALSE;
    int             dateLen = 0;
    int             timeLen = 0;
    wchar_t         dateBuf[s_kDateTimeBufChars] = {};
    wchar_t         timeBuf[s_kDateTimeBufChars] = {};



    BAIL_OUT_IF (loadedUnix <= 0, S_OK);

    uli.QuadPart         = (ULONGLONG) loadedUnix * s_kFiletimeTicksPerSec + s_kUnixEpochFiletime;
    ftUtc.dwLowDateTime  = uli.LowPart;
    ftUtc.dwHighDateTime = uli.HighPart;

    ok = FileTimeToLocalFileTime (&ftUtc, &ftLocal);
    CWRA (ok);
    ok = FileTimeToSystemTime (&ftLocal, &st);
    CWRA (ok);

    dateLen = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, dateBuf, s_kDateTimeBufChars, nullptr);
    CWRA (dateLen > 0);

    timeLen = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, timeBuf, s_kDateTimeBufChars);
    CWRA (timeLen > 0);

    result  = dateBuf;
    result += L' ';
    result += timeBuf;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::ConfigureWidgets
//
//  One-time configuration of the search box and list view (DPI, columns,
//  header, live-filter callback). Run calls this before the first
//  RebuildView.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMruPickerSession::ConfigureWidgets()
{
    std::vector<DxuiListView::Column>  cols;



    m_search.SetDpi         (m_dpi);
    m_search.SetPlaceholder (L"Search");
    m_search.SetOnChange    ([this] (const std::wstring & value) { m_filter = value; RebuildView(); });

    cols.push_back ({ L"Last loaded", 0, false, DxuiTextRenderer::HAlign::Left });
    cols.push_back ({ L"Disk image",  0, false, DxuiTextRenderer::HAlign::Left });
    cols.push_back ({ L"Location",    0, false, DxuiTextRenderer::HAlign::Left });

    m_list.SetDpi                    (m_dpi);
    m_list.SetShowHeader             (true);
    m_list.SetColumns                (std::move (cols));
    m_list.SetSortIndicator          (m_sortColumn, m_sortDescending);
    m_list.EnableStickyTail          (false);
    m_list.SetHorizontalScrollEnabled (true);
    m_list.SetKeyboardColumnNav       (true);   // File-Explorer model: body -> header (Left/Right cycle, Space/Enter sort)
    m_list.SetPreciseAutoFit          (true);   // columns fit max(header + sort glyph, widest cell), grown as rows filter
    m_list.SetOnSortColumn           ([this] (int col) { ApplySort (col); });
    m_list.SetOnActivateRow          ([this] (int row) { m_pendingChoice = ChosenResultAt (row); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::RebuildView
//
//  Rebuilds the filtered + sorted view of the model and pushes the
//  resulting cells into the list. Clamps the selection if the visible
//  row count shrank.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMruPickerSession::RebuildView()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    int                                           selected = -1;

    // Tokenize the filter: whitespace-separated, lowercased. A row passes
    // when EVERY token appears in some field (name / location / last-loaded);
    // each occurrence is highlighted in its field.
    std::vector<std::wstring>  tokens;
    {
        std::wstring  cur;

        for (wchar_t ch : m_filter)
        {
            if (iswspace (ch))
            {
                if (!cur.empty()) { tokens.push_back (cur); cur.clear(); }
            }
            else
            {
                cur += (wchar_t) towlower (ch);
            }
        }

        if (!cur.empty()) { tokens.push_back (cur); }
    }

    auto  lower = [] (std::wstring s) -> std::wstring
    {
        for (wchar_t & c : s) { c = (wchar_t) towlower (c); }
        return s;
    };

    // Merged, sorted char ranges of every token occurrence in `text`.
    auto  matchesIn = [&tokens, &lower] (const std::wstring & text) -> std::vector<std::pair<int, int>>
    {
        std::vector<std::pair<int, int>>  out;

        if (tokens.empty() || text.empty()) { return out; }

        std::wstring  hay = lower (text);

        for (const std::wstring & tok : tokens)
        {
            for (size_t at = hay.find (tok); at != std::wstring::npos; at = hay.find (tok, at + 1))
            {
                out.emplace_back ((int) at, (int) (at + tok.size()));
            }
        }

        std::sort (out.begin(), out.end());

        std::vector<std::pair<int, int>>  merged;
        for (const std::pair<int, int> & r : out)
        {
            if (!merged.empty() && r.first <= merged.back().second)
            {
                merged.back().second = std::max (merged.back().second, r.second);
            }
            else
            {
                merged.push_back (r);
            }
        }

        return merged;
    };

    auto  rowPasses = [&tokens, &lower] (const ModelRow & row) -> bool
    {
        if (tokens.empty()) { return true; }

        std::wstring  hay = lower (row.name);
        hay += L'\n';
        hay += lower (row.location);
        hay += L'\n';
        hay += lower (DiskMruPickerSession::FormatLastLoaded (row.loadedUnix));

        for (const std::wstring & tok : tokens)
        {
            if (hay.find (tok) == std::wstring::npos) { return false; }
        }

        return true;
    };

    auto  sortLess = [this] (int lhs, int rhs) -> bool
    {
        const ModelRow &  a   = m_model[(size_t) lhs];
        const ModelRow &  b   = m_model[(size_t) rhs];
        int               cmp = 0;

        if (m_sortColumn == s_kColLastLoaded)
        {
            cmp = (a.loadedUnix < b.loadedUnix) ? -1 : (a.loadedUnix > b.loadedUnix) ? 1 : 0;
        }
        else if (m_sortColumn == s_kColLocation)
        {
            cmp = _wcsicmp (a.location.c_str(), b.location.c_str());
        }
        else
        {
            cmp = _wcsicmp (a.name.c_str(), b.name.c_str());
        }

        return m_sortDescending ? (cmp > 0) : (cmp < 0);
    };



    m_view.clear();
    m_view.reserve (m_model.size());

    for (int i = 0; i < (int) m_model.size(); ++i)
    {
        if (rowPasses (m_model[(size_t) i]))
        {
            m_view.push_back (i);
        }
    }

    std::stable_sort (m_view.begin(), m_view.end(), sortLess);
    m_list.SetSortIndicator (m_sortColumn, m_sortDescending);

    rows.reserve (m_view.size());
    for (int idx : m_view)
    {
        const ModelRow &  row  = m_model[(size_t) idx];
        std::wstring      date = FormatLastLoaded (row.loadedUnix);

        rows.push_back ({ { date,         true,            matchesIn (date) },
                          { row.name,     false,           matchesIn (row.name) },
                          { row.location, row.dimLocation, matchesIn (row.location) } });
    }

    m_list.SetRows (std::move (rows));
    m_list.UpdateAutoFitFromRows();

    selected = m_list.GetSelectedRow();
    if (selected >= (int) m_view.size())
    {
        m_list.SetSelectedRow ((int) m_view.size() - 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::ApplySort
//
//  Header-click handler. Toggles direction when the active column is
//  re-clicked, otherwise switches to the new column with its default
//  direction (newest-first for the date column, A-Z for the text
//  columns), then rebuilds the view.
//
////////////////////////////////////////////////////////////////////////////////

void DiskMruPickerSession::ApplySort (int column)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (column < 0 || column >= s_kColumnCount, S_OK);

    if (m_sortColumn == column)
    {
        m_sortDescending = !m_sortDescending;
    }
    else
    {
        m_sortColumn     = column;
        m_sortDescending = (column == s_kColLastLoaded);
    }

    RebuildView();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::ChosenResultAt
//
//  Maps a visible-row index to its model row's result code, or -1 when
//  the index is out of range.
//
////////////////////////////////////////////////////////////////////////////////

int DiskMruPickerSession::ChosenResultAt (int visibleRow) const
{
    int  result = -1;



    if (visibleRow >= 0 && visibleRow < (int) m_view.size())
    {
        result = m_model[(size_t) m_view[(size_t) visibleRow]].resultCode;
    }

    return result;
}





namespace
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PickerBodyPanel
    //
    //  Dxui content panel for the boot-disk picker: a search box docked at
    //  the top, a list filling the rest. Lays out in physical pixels (the
    //  hosted dialog passes a px content rect) so the fixed search-strip
    //  height scales with DPI.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PickerBodyPanel : public DxuiPanel
    {
    public:
        void  Init (DxuiSearchBox * search, DxuiListView * list, int searchHeightDip, int gapDip)
        {
            m_search          = search;
            m_list            = list;
            m_searchHeightDip = searchHeightDip;
            m_gapDip          = gapDip;

            Adopt (*search);
            Adopt (*list);
        }

        void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
        {
            int  sh  = scaler.Px (m_searchHeightDip);
            int  gap = scaler.Px (m_gapDip);


            SetBounds (boundsPx);

            if (m_search != nullptr)
            {
                RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + sh };

                m_search->Layout (r, scaler);
            }

            if (m_list != nullptr)
            {
                RECT  r = { boundsPx.left, boundsPx.top + sh + gap, boundsPx.right, boundsPx.bottom };

                m_list->Layout (r, scaler);
            }
        }

        //
        //  DxuiListView::OnMouse expects widget-LOCAL (0-based) coordinates,
        //  but the panel fan-out delivers absolute client-px, so a plain
        //  DxuiPanel::OnMouse would hand the list mis-offset points (wrong
        //  row selected, column-resize divider never hit). Translate to
        //  list-local and dispatch to the list first (it owns scroll / drag
        //  / resize / select + consumes any press inside itself); anything
        //  the list declines falls through to the search box, which hit-
        //  tests against its own absolute bounds.
        //
        bool  OnMouse (const DxuiMouseEvent & ev) override
        {
            DxuiMouseEvent  listEv  = ev;
            bool            handled = false;


            if (m_list != nullptr)
            {
                RECT  lb = m_list->Bounds();

                listEv.positionDip = { ev.positionDip.x - lb.left, ev.positionDip.y - lb.top };
                handled            = m_list->OnMouse (listEv);
            }

            if (!handled && m_search != nullptr)
            {
                handled = m_search->OnMouse (ev);
            }

            return handled;
        }

        LPCWSTR  CursorForPoint (POINT clientPx) const override
        {
            LPCWSTR  cursor = nullptr;


            if (m_list != nullptr)
            {
                RECT   lb    = m_list->Bounds();
                POINT  local = { clientPx.x - lb.left, clientPx.y - lb.top };

                cursor = m_list->CursorForPoint (local);
            }

            return cursor;
        }


    private:
        DxuiSearchBox *  m_search          = nullptr;
        DxuiListView  *  m_list            = nullptr;
        int              m_searchHeightDip = 0;
        int              m_gapDip          = 0;
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PickerDialog
    //
    //  DxuiDialogWindow hosting a pre-built picker body (search + list) and
    //  its action buttons. Non-cancel buttons carry their real (negative)
    //  result codes as command ids (so a click ends the modal with that
    //  code directly); the cancel button maps to IDCANCEL so Escape / the
    //  close-box fire it. Row activation ends the modal with the row result
    //  offset past s_kRowResultBase; MapResult un-offsets it and translates
    //  IDCANCEL back to the cancel button's real code (or the close-box
    //  result when no cancel button exists).
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PickerDialog : public DxuiDialogWindow
    {
    public:
        static constexpr int  s_kRowResultBase = 100000;   // row results offset past button / IDCANCEL codes


        void  ConfigurePicker (std::unique_ptr<DxuiPanel>          content,
                               IDxuiControl *                     initialFocus,
                               const std::vector<DialogButton> &  buttons,
                               int                                closeBoxResult)
        {
            m_pendingContent = std::move (content);
            m_pendingFocus   = initialFocus;
            m_buttons        = buttons;
            m_closeBoxResult = closeBoxResult;
        }

        int  DefaultCommandId () const { return m_defaultCommandId; }

        int  MapResult (int dialogResult) const
        {
            int     result = m_closeBoxResult;
            size_t  idx    = 0;


            if (dialogResult >= s_kRowResultBase)
            {
                result = dialogResult - s_kRowResultBase;
            }
            else if (dialogResult == IDCANCEL)
            {
                for (idx = 0; idx < m_buttons.size(); ++idx)
                {
                    if (m_buttons[idx].isCancel)
                    {
                        result = m_buttons[idx].resultCode;
                        break;
                    }
                }
            }
            else
            {
                result = dialogResult;
            }

            return result;
        }


    protected:
        void  OnCreate () override
        {
            size_t  i = 0;


            if (m_pendingContent != nullptr)
            {
                SetDialogContentOwned (std::move (m_pendingContent));
            }

            for (i = 0; i < m_buttons.size(); ++i)
            {
                int                    commandId = m_buttons[i].isCancel ? IDCANCEL : m_buttons[i].resultCode;
                DxuiButtonRow::Anchor  anchor    = m_buttons[i].anchorLeft ? DxuiButtonRow::Anchor::Left
                                                                           : DxuiButtonRow::Anchor::Right;

                AddDialogButton (m_buttons[i].label, commandId, anchor);

                if (m_buttons[i].isDefault)
                {
                    m_defaultCommandId = commandId;
                }
            }

            SetInitialFocus (m_pendingFocus);
        }


    private:
        std::unique_ptr<DxuiPanel>  m_pendingContent;
        IDxuiControl *              m_pendingFocus     = nullptr;
        std::vector<DialogButton>   m_buttons;
        int                         m_closeBoxResult   = -1;
        int                         m_defaultCommandId = 0;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMruPickerSession::Run
//
//  Builds a PickerDialog whose content is a search box + list, wires row
//  activation to close the dialog with the row's result, and shows it
//  modally via ShowModalDialog. Returns the chosen result code (a model row's
//  resultCode, a button's resultCode, or the close-box code).
//
////////////////////////////////////////////////////////////////////////////////

int DiskMruPickerSession::Run()
{
    HRESULT                           hr      = S_OK;
    CassoTheme                        theme   = CassoTheme::ForName (m_themeName);
    std::unique_ptr<PickerBodyPanel>  content = std::make_unique<PickerBodyPanel>();
    PickerDialog                      dlg;
    DxuiWindow::CreateParams          params;
    int                               chosen  = -1;
    int                               raw     = 0;


    m_dpi = (m_hwndParent != nullptr) ? GetDpiForWindow (m_hwndParent) : GetDpiForSystem();

    ConfigureWidgets();
    m_search.SetTheme (&theme);
    m_list.SetTheme   (&theme);
    RebuildView();

    //  Row activation (double-click / Enter on a row) ends the modal with
    //  that row's result code, offset past the button / close range so it
    //  never collides; PickerDialog::MapResult un-offsets it.
    m_list.SetOnActivateRow ([&dlg, this] (int row)
    {
        int  code = ChosenResultAt (row);

        if (code >= 0)
        {
            dlg.EndDialog (PickerDialog::s_kRowResultBase + code);
        }
    });

    content->Init (&m_search, &m_list, s_kSearchHeightDip, s_kSearchListGapDip);

    dlg.ConfigurePicker (std::move (content), &m_search, m_buttons, m_closeBoxResult.value_or (-1));

    params.title                    = m_title;
    params.hInstance                = m_hInstance;
    params.ownerHwnd                = m_hwndParent;
    params.initialSizeDip           = { s_kResizableDefWidthDip, s_kResizableDefHeightDip };
    params.minSizeDip               = { s_kResizableMinWidthDip, s_kResizableMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (&theme);

    raw    = dlg.ShowModalDialog (dlg.DefaultCommandId());
    chosen = dlg.MapResult (raw);

Error:
    return chosen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptBootDiskMru
//
//  Themed dialog that lists the user's recent disk images plus stock
//  "Download" rows for DOS 3.3 / ProDOS. Always shown when the machine
//  has a Disk ][ controller and no boot disk has been resolved yet,
//  even when the MRU is empty (the download rows give a fresh install
//  somewhere to go). Picking a row mounts that image (downloading on
//  demand for the stock rows); the Skip button leaves the slot empty.
//
//  On return:
//    outDiskPath = path to mount, or empty if the user skipped / the
//                  machine has no Disk ][ controller.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::PromptBootDiskMru (
    HINSTANCE                      hInstance,
    HWND                           hwndParent,
    const wstring                & machineName,
    const vector<DiskMru::Entry> & mruEntries,
    const fs::path               & diskDir,
    std::string_view               themeName,
    wstring                      & outDiskPath,
    bool                         & outUserClosed,
    string                       & outError)
{
    struct DownloadRow { const BootDiskSpec * spec; wstring label; };

    static constexpr int s_kSkipResult     = -2002;
    static constexpr int s_kCloseBoxResult = -1000;

    HRESULT             hr            = S_OK;
    bool                hasDisk       = false;
    wstring             title;
    wstring             intro;
    wstring             displayName;
    DownloadRow         downloads[]   =
    {
        { &s_kDos33Disk,  L"DOS 3.3"  },
        { &s_kProDOSDisk, L"ProDOS"   }
    };
    std::vector<const DownloadRow *>            shownDownloads;
    std::vector<const DownloadRow *>            mruLabels;
    std::vector<DiskMruPickerSession::ModelRow> models;
    DiskMruPickerSession  session       (hInstance, hwndParent, themeName);
    int                   chosen        = s_kSkipResult;
    int                   mruCount      = (int) mruEntries.size();
    int                   downloadCount = 0;
    int                   rowCount      = 0;
    error_code            ec;



    outDiskPath.clear();

    hr = HasDiskController (hInstance, machineName, hasDisk, outError);
    CHR (hr);

    BAIL_OUT_IF (!hasDisk, S_OK);

    displayName = GetEmbeddedDisplayName (hInstance, machineName);

    mruLabels.assign ((size_t) mruCount, nullptr);

    for (const DownloadRow & dr : downloads)
    {
        fs::path           wantPath  = diskDir / string (dr.spec->cassoName);
        bool               foundAny  = false;
        std::error_code    ecCmp;

        for (int i = 0; i < mruCount; ++i)
        {
            bool match = fs::equivalent (mruEntries[(size_t) i].path, wantPath, ecCmp);

            if (!match)
            {
                match = FilesHaveSameContent (mruEntries[(size_t) i].path, wantPath);
            }

            if (match)
            {
                mruLabels[(size_t) i] = &dr;
                foundAny = true;
            }
        }

        if (!foundAny)
        {
            shownDownloads.push_back (&dr);
        }
    }

    downloadCount = (int) shownDownloads.size();
    rowCount      = mruCount + downloadCount;

    title  = L"Casso ";
    title += s_kchEmDash;
    title += L" Boot Disk";

    if (mruCount > 0)
    {
        intro  = L"Choose a recent disk for ";
        intro += displayName;
        intro += L", or download a stock master from the Asimov archive.";
    }
    else
    {
        intro  = displayName;
        intro += L" has a Disk ][ controller but no boot disk. Pick a "
                 L"stock master from the Asimov archive to get started.";
    }

    models.reserve ((size_t) rowCount);

    for (int i = 0; i < mruCount; ++i)
    {
        const DiskMru::Entry &       entry = mruEntries[(size_t) i];
        DiskMruPickerSession::ModelRow  row;

        row.name        = (mruLabels[(size_t) i] != nullptr) ? std::wstring (mruLabels[(size_t) i]->label)
                                                             : entry.path.filename().wstring();
        row.location    = entry.path.parent_path().wstring();
        row.loadedUnix  = (entry.lastLoadedUnix != 0) ? entry.lastLoadedUnix
                                                      : DiskMruPickerSession::FileMtimeUnix (entry.path);
        row.resultCode  = i;
        row.dimLocation = true;
        models.push_back (std::move (row));
    }

    for (int j = 0; j < downloadCount; ++j)
    {
        const DownloadRow *          dr       = shownDownloads[(size_t) j];
        fs::path                     wantPath = diskDir / string (dr->spec->cassoName);
        bool                         present  = fs::exists (wantPath, ec);
        DiskMruPickerSession::ModelRow  row;

        row.name        = dr->label;
        row.location    = present ? L"Installed" : L"Asimov archive (Download)";
        row.loadedUnix  = 0;
        row.resultCode  = mruCount + j;
        row.dimLocation = true;
        models.push_back (std::move (row));
    }

    session.SetText          (title, intro);
    session.SetModelRows     (std::move (models));
    session.AddButton        ({ L"Skip", s_kSkipResult, true, true });
    session.SetCloseBoxResult (s_kCloseBoxResult);

    chosen = session.Run();

    if (chosen == s_kCloseBoxResult)
    {
        outUserClosed = true;
    }
    else if (chosen == s_kSkipResult)
    {
        // user skipped; outDiskPath stays empty
    }
    else if (chosen >= 0 && chosen < mruCount)
    {
        outDiskPath = mruEntries[(size_t) chosen].path.wstring();
    }
    else if (chosen >= mruCount && chosen < rowCount)
    {
        const BootDiskSpec & spec = *shownDownloads[(size_t) (chosen - mruCount)]->spec;
        hr = DownloadStockBootDisk (spec, diskDir, outDiskPath, outError);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptInsertDiskMru
//
//  Runtime-insert sibling of PromptBootDiskMru. Same MRU + DOS 3.3 /
//  ProDOS "Download" rows, but the dialog footer offers Browse... /
//  Cancel instead of Skip. Browse pops back to the caller which then
//  fires the existing IFileOpenDialog path. Cancel / close box leaves
//  the drive untouched.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::PromptInsertDiskMru (
    HINSTANCE                      hInstance,
    HWND                           hwndParent,
    int                            drive,
    const vector<DiskMru::Entry> & mruEntries,
    const fs::path               & diskDir,
    std::string_view               themeName,
    wstring                      & outDiskPath,
    bool                         & outBrowse,
    string                       & outError)
{
    struct DownloadRow { const BootDiskSpec * spec; wstring label; };

    static constexpr int  s_kBrowseResult   = -2000;
    static constexpr int  s_kCancelResult   = -2001;
    static constexpr int  s_kCloseBoxResult = -1000;

    HRESULT             hr            = S_OK;
    wstring             title;
    wstring             intro;
    DownloadRow         downloads[]   =
    {
        { &s_kDos33Disk,  L"DOS 3.3"  },
        { &s_kProDOSDisk, L"ProDOS"   }
    };
    std::vector<const DownloadRow *>            shownDownloads;
    std::vector<const DownloadRow *>            mruLabels;
    std::vector<DiskMruPickerSession::ModelRow> models;
    DiskMruPickerSession  session       (hInstance, hwndParent, themeName);
    int                   chosen        = s_kCancelResult;
    int                   mruCount      = (int) mruEntries.size();
    int                   downloadCount = 0;
    int                   rowCount      = 0;
    error_code            ec;



    outDiskPath.clear();
    outBrowse = false;

    mruLabels.assign ((size_t) mruCount, nullptr);

    for (const DownloadRow & dr : downloads)
    {
        fs::path           wantPath  = diskDir / string (dr.spec->cassoName);
        bool               foundAny  = false;
        std::error_code    ecCmp;

        for (int i = 0; i < mruCount; ++i)
        {
            bool match = fs::equivalent (mruEntries[(size_t) i].path, wantPath, ecCmp);

            if (!match)
            {
                match = FilesHaveSameContent (mruEntries[(size_t) i].path, wantPath);
            }

            if (match)
            {
                mruLabels[(size_t) i] = &dr;
                foundAny = true;
            }
        }

        if (!foundAny)
        {
            shownDownloads.push_back (&dr);
        }
    }

    downloadCount = (int) shownDownloads.size();
    rowCount      = mruCount + downloadCount;

    title  = L"Casso ";
    title += s_kchEmDash;
    title += format (L" Insert Disk in Drive {}", drive);

    if (mruCount > 0)
    {
        intro  = format (L"Choose a disk image for Drive {}, browse for "
                         L"another, or download a stock master from the "
                         L"Asimov archive.", drive);
    }
    else
    {
        intro  = format (L"No recent disks for Drive {}. Browse for an "
                         L"image, or download a stock master from the "
                         L"Asimov archive.", drive);
    }

    models.reserve ((size_t) rowCount);

    for (int i = 0; i < mruCount; ++i)
    {
        const DiskMru::Entry &       entry = mruEntries[(size_t) i];
        DiskMruPickerSession::ModelRow  row;

        row.name        = (mruLabels[(size_t) i] != nullptr) ? std::wstring (mruLabels[(size_t) i]->label)
                                                             : entry.path.filename().wstring();
        row.location    = entry.path.parent_path().wstring();
        row.loadedUnix  = (entry.lastLoadedUnix != 0) ? entry.lastLoadedUnix
                                                      : DiskMruPickerSession::FileMtimeUnix (entry.path);
        row.resultCode  = i;
        row.dimLocation = true;
        models.push_back (std::move (row));
    }

    for (int j = 0; j < downloadCount; ++j)
    {
        const DownloadRow *          dr       = shownDownloads[(size_t) j];
        fs::path                     wantPath = diskDir / string (dr->spec->cassoName);
        bool                         present  = fs::exists (wantPath, ec);
        DiskMruPickerSession::ModelRow  row;

        row.name        = dr->label;
        row.location    = present ? L"Installed" : L"Asimov archive (Download)";
        row.loadedUnix  = 0;
        row.resultCode  = mruCount + j;
        row.dimLocation = true;
        models.push_back (std::move (row));
    }

    session.SetText          (title, intro);
    session.SetModelRows     (std::move (models));
    session.AddButton        ({ L"&Browse...", s_kBrowseResult, false, false, true });   // bottom-left
    session.AddButton        ({ L"Cancel",     s_kCancelResult, true,  true  });
    session.SetCloseBoxResult (s_kCloseBoxResult);

    chosen = session.Run();

    if (chosen == s_kBrowseResult)
    {
        outBrowse = true;
    }
    else if (chosen == s_kCloseBoxResult || chosen == s_kCancelResult)
    {
        // user cancelled; outDiskPath stays empty
    }
    else if (chosen >= 0 && chosen < mruCount)
    {
        outDiskPath = mruEntries[(size_t) chosen].path.wstring();
    }
    else if (chosen >= mruCount && chosen < rowCount)
    {
        const BootDiskSpec & spec = *shownDownloads[(size_t) (chosen - mruCount)]->spec;
        hr = DownloadStockBootDisk (spec, diskDir, outDiskPath, outError);
        CHR (hr);
    }

Error:
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
    HINTERNET                    hSession,
    LPCWSTR                      urlPath,
    uint32_t                     targetSampleRate,
    vector<float>              & outPcm,
    string                     & outError,
    std::atomic<std::uint64_t> * progressBytes,
    std::atomic<bool>          * cancelRequested)
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
                       outError,
                       progressBytes,
                       cancelRequested);
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
//  RunStartupDownloader
//
//  Unified entry point: scans for every missing ROM and every missing
//  Disk II drive-audio WAV, then presents a single themed dialog that
//  downloads them on a worker thread with live per-asset progress.
//  Replaces the legacy PromptUser (ROMs) + PromptDiskAudioConsent
//  (audio) flows with one transparent experience.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::RunStartupDownloader (
    HINSTANCE                hInstance,
    const wstring          & machineName,
    HWND                     hwndParent,
    const vector<fs::path> & searchPaths,
    const fs::path         & assetBaseDir,
    bool                     considerDiskAudio,
    GlobalUserPrefs        & prefs,
    string                 & outError)
{
    HRESULT                hr             = S_OK;
    StartupDownloadSet     set;
    StartupDownloadResult  result         = StartupDownloadResult::NothingToDo;
    vector<string>         romFiles;
    string                 narrowMachine;
    fs::path               devicesDir     = assetBaseDir / "Devices" / "DiskII";
    bool                   audioIncluded  = false;
    error_code             ec;

    UNREFERENCED_PARAMETER (hInstance);

    narrowMachine.reserve (machineName.size ());

    for (wchar_t wch : machineName)
    {
        narrowMachine.push_back (static_cast<char> (wch & 0x7F));
    }

    hr = GetRequiredRoms (hInstance, machineName, romFiles, outError);
    CHR (hr);

    for (const string & romFile : romFiles)
    {
        const RomSpec    * spec    = FindRomSpec (narrowMachine, romFile);
        fs::path           relPath;
        fs::path           found;
        StartupAssetEntry  entry;

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

        entry.kind          = StartupAssetKind::Rom;
        entry.groupLabel    = MachineDisplayName (narrowMachine) + L" ROMs";
        entry.displayName   = AsciiToWide (spec->description);
        entry.kindLabel     = L"ROM";
        entry.source        = L"AppleWin (GitHub)";
        entry.selectable    = false;
        entry.selected      = true;
        entry.destPaths.push_back (assetBaseDir / string (spec->localRelDir) / spec->cassoName);
        entry.expectedBytes = (std::uint64_t) spec->expectedSize;
        entry.downloadFn    = [spec, destPath = entry.destPaths.front()] (
            std::atomic<std::uint64_t> & bytesDone,
            std::atomic<bool>          & cancel,
            std::string                & err) -> HRESULT
        {
            HRESULT       hr = S_OK;
            HINTERNET     hSes    = nullptr;
            vector<Byte>  payload;
            error_code    ecLocal;
            wstring       wPath   = wstring (s_kpszUrlPrefix) + AsciiToWide (spec->appleWinName);

            hSes = WinHttpOpen (s_kpszUserAgent,
                                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0);
            CBRF (hSes != nullptr, err = "Cannot initialize WinHTTP session");

            hr = DownloadHttp (hSes,
                                    s_kpszAppleWinHost,
                                    wPath.c_str (),
                                    spec->expectedSize,
                                    spec->cassoName,
                                    payload,
                                    err,
                                    &bytesDone,
                                    &cancel);
            CHR (hr);

            fs::create_directories (destPath.parent_path (), ecLocal);
            hr = WriteFileBytes (destPath, payload);
            CHRF (hr, err = format ("Cannot write {}", destPath.string ()));

        Error:
            if (hSes != nullptr)
            {
                WinHttpCloseHandle (hSes);
            }
            return hr;
        };

        set.entries.push_back (std::move (entry));
    }

    if (considerDiskAudio && prefs.audioDownloadConsent != "decline")
    {
        for (string_view mechanism : s_kDiskAudioMechanisms)
        {
            StartupAssetEntry  entry;
            string             mechStr   (mechanism);
            wstring            mechW     (mechanism.begin (), mechanism.end ());
            size_t             missingCount = 0;

            for (const DiskAudioSpec & spec : s_kDiskAudioCatalog)
            {
                fs::path  mechDir = devicesDir / string (spec.mechanism);
                fs::path  wavPath = mechDir / string (spec.wavBasename);

                if (spec.mechanism != mechanism)
                {
                    continue;
                }

                if (fs::exists (wavPath, ec))
                {
                    continue;
                }

                entry.destPaths.push_back (wavPath);
                missingCount++;
            }

            if (missingCount == 0)
            {
                continue;
            }

            entry.kind          = StartupAssetKind::DriveAudio;
            entry.groupLabel    = L"Disk ][ audio";
            entry.kindLabel     = L"Drive audio";
            entry.source        = L"OpenEmulator (GitHub)";
            entry.expectedBytes = 0;
            entry.selectable    = true;
            entry.selected      = true;
            entry.displayName   = mechW + L" mechanism";
            entry.downloadFn    = [devicesDir, mechStr] (
                std::atomic<std::uint64_t> & bytesDone,
                std::atomic<bool>          & cancel,
                std::string                & err) -> HRESULT
            {
                HRESULT     hr            = S_OK;
                HINTERNET   hSes          = nullptr;
                error_code  ecLocal;
                uint64_t    cumulative    = 0;

                hSes = WinHttpOpen (s_kpszUserAgent,
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
                CBRF (hSes != nullptr, err = "Cannot initialize WinHTTP session");

                for (const DiskAudioSpec & spec : s_kDiskAudioCatalog)
                {
                    fs::path                     mechDir = devicesDir / string (spec.mechanism);
                    fs::path                     wavPath = mechDir / string (spec.wavBasename);
                    vector<float>                pcm;
                    wstring                      urlPath;
                    std::atomic<std::uint64_t>   perFileBytes{0};

                    if (spec.mechanism != mechStr)
                    {
                        continue;
                    }

                    if (fs::exists (wavPath, ecLocal))
                    {
                        continue;
                    }

                    if (cancel.load (std::memory_order_relaxed))
                    {
                        hr = E_ABORT;
                        goto Error;
                    }

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

                    hr = AssetBootstrap::FetchAndDecodeOgg (hSes,
                                                            urlPath.c_str (),
                                                            44100,
                                                            pcm,
                                                            err,
                                                            &perFileBytes,
                                                            &cancel);

                    if (hr == E_ABORT || cancel.load (std::memory_order_relaxed))
                    {
                        hr = E_ABORT;
                        goto Error;
                    }

                    if (FAILED (hr))
                    {
                        DEBUGMSG (L"Drive audio: skipping %S (%s)\n",
                                  spec.oggBasename.data (),
                                  wstring (err.begin (), err.end ()).c_str ());
                        err.clear ();
                        hr = S_OK;
                        continue;
                    }

                    fs::create_directories (mechDir, ecLocal);

                    hr = AssetBootstrap::WritePcmAsWav (wavPath, pcm, 44100, err);

                    if (FAILED (hr))
                    {
                        DEBUGMSG (L"Drive audio: write failed for %S (%s)\n",
                                  spec.wavBasename.data (),
                                  wstring (err.begin (), err.end ()).c_str ());
                        err.clear ();
                        hr = S_OK;
                        continue;
                    }

                    cumulative += perFileBytes.load (std::memory_order_relaxed);
                    bytesDone.store (cumulative, std::memory_order_relaxed);
                }

            Error:
                if (hSes != nullptr)
                {
                    WinHttpCloseHandle (hSes);
                }
                return hr;
            };

            set.entries.push_back (std::move (entry));
            audioIncluded = true;
        }
    }

    BAIL_OUT_IF (set.entries.empty (), S_OK);

    result = StartupDownloadDialog::Show (hInstance, hwndParent, prefs.activeTheme,
                                          MachineDisplayName (narrowMachine), set);

    switch (result)
    {
    case StartupDownloadResult::NothingToDo:
    case StartupDownloadResult::AllDone:
    case StartupDownloadResult::PartialDone:
        if (audioIncluded)
        {
            prefs.audioDownloadConsent = "allow";
        }
        hr = S_OK;
        break;

    case StartupDownloadResult::Skipped:
        if (audioIncluded)
        {
            prefs.audioDownloadConsent = "decline";
        }
        hr = S_OK;
        break;

    case StartupDownloadResult::Exit:
        hr = S_FALSE;
        break;
    }

Error:
    return hr;
}
