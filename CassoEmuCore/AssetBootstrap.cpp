#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "AssetBootstrap.h"
#include "Shell/RepoCheckout.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "Core/MachineConfigUpgrade.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/StockBootDisks.h"
#include "EmbeddedMachineConfigs.h"
#include "External/StbVorbisWrapper.h"
#include "resource.h"
#include "Ui/ThemeManager.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/Dialogs/StartupDownloadDialog.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Core/UnicodeSymbols.h"
#include "Net/WinHttpClient.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "bcrypt.lib")


static constexpr LPCWSTR       s_kpszAppleWinHost = L"raw.githubusercontent.com";
static constexpr LPCWSTR       s_kpszUserAgent    = WinHttpClient::kpszUserAgent;
static constexpr LPCWSTR       s_kpszUrlPrefix    = L"/AppleWin/AppleWin/master/resource/";

static constexpr LPCWSTR       s_kpszAsimovHost   = L"www.apple.asimov.net";





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

    // Optional alternate source. Most ROMs come from the AppleWin GitHub
    // resource dir (host below empty -> s_kpszAppleWinHost + s_kpszUrlPrefix +
    // appleWinName). ROMs AppleWin does not carry (e.g. the Apple //c, which
    // AppleWin does not emulate) set an explicit host + fully-formed,
    // percent-encoded urlPath and leave appleWinName empty.
    string_view  altHost     = {};
    string_view  altUrlPath  = {};
    string_view  sourceLabel = {};   // shown in the download dialog (defaults to AppleWin)

    // SHA-256 of a file Casso used to install under this name and has since
    // updated. A ROM already on disk is otherwise taken as satisfied whatever
    // it contains -- the size is checked on download, not on what is already
    // there -- so a machine provisioned with the earlier file would keep it
    // forever. An on-disk file matching this hash is offered the updated ROM
    // in its place; anything else, including a regional variant the user
    // chose, is left alone.
    string_view  supersededSha256 = {};

    // SHA-256 the download must have; empty checks the size only. A download
    // that does not match is not written.
    string_view  expectedSha256   = {};
};




static constexpr RomSpec s_kRomCatalog[] =
{
    { "Apple2",           "Apple2.rom",            "Apple2.rom",                 "Machines/Apple2",           12288, "Apple ][ ROM (Integer BASIC)"              },
    { "Apple2",           "Apple2_Video.rom",      "Apple2_Video.rom",           "Machines/Apple2",            2048, "Apple ][/][+ Character Generator"          },
    { "Apple2Plus",       "Apple2Plus.rom",        "Apple2_Plus.rom",            "Machines/Apple2Plus",       12288, "Apple ][+ ROM (Applesoft BASIC)"           },
    { "Apple2Plus",       "Apple2_Video.rom",      "Apple2_Video.rom",           "Machines/Apple2Plus",        2048, "Apple ][/][+ Character Generator"          },
    { "Apple2e",          "Apple2e.rom",           "Apple2e.rom",                "Machines/Apple2e",          16384, "Apple //e ROM"                             },
    // The unenhanced //e's character generator is 342-0133-A, which has
    // ordinary glyphs at $40-$5F where the enhanced part has MouseText.
    // AppleWin ships only the enhanced one and uses it for both of its //e
    // models; Casso did the same, which put MouseText on a machine that
    // never had it. The real part comes from the same preservation mirror
    // the //c's ROM 4 does.
    { "Apple2e",          "Apple2e_Video.rom",     "",                           "Machines/Apple2e",           4096, "Apple //e Character Generator (342-0133-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIe/ROM%20Images/Apple%20IIe%20Video%20ROM%20-%20342-0133-A%20-%20US%201982.bin",
      "apple2.org.za",
      // Every Casso before this one installed the ENHANCED generator here,
      // so an existing //e has MouseText where it should have ordinary
      // characters. Re-fetch that exact file and nothing else.
      "52c3b87900ac939f6525402cab1ccfd8f8259290fc6df54da48fb4c98ae3ed0f" },
    { "Apple2eEnhanced",  "Apple2eEnhanced.rom",   "Apple2e_Enhanced.rom",       "Machines/Apple2eEnhanced",  16384, "Apple //e Enhanced ROM"                    },
    { "Apple2eEnhanced",  "Apple2eEnhanced_Video.rom", "Apple2e_Enhanced_Video.rom", "Machines/Apple2eEnhanced", 4096, "Apple //e Enhanced Character Generator + MouseText (342-0265-A)" },
    // AppleWin does not emulate the //c, so its 32K ROM 4 (memory-expansion
    // //c, chip 341-0445-B) comes from the apple2.org.za preservation mirror.
    { "Apple2c",          "Apple2c.rom",           "",                           "Machines/Apple2c",          32768, "Apple //c ROM 4 (341-0445-B, memory expansion)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIc/ROM%20Images/Apple%20IIc%20ROM%2004%20-%20341-0445-B.bin",
      "apple2.org.za" },
    // The //c character generator is the enhanced (MouseText) 342-0265, the
    // same part AppleWin ships as Apple2e_Enhanced_Video.rom -- reuse it.
    // It shares the part with the Enhanced //e but not the file name: each
    // machine's assets are self-contained, and one name for two different
    // parts is what hid the //e's wrong character ROM.
    { "Apple2c",          "Apple2c_Video.rom",     "Apple2e_Enhanced_Video.rom", "Machines/Apple2c",           4096, "Apple //c Character Generator + MouseText" },
    { "",                 "Disk2.rom",             "DISK2.rom",                  "Devices/DiskII",              256, "Disk ][ Boot ROM (slot 6)"                 },
    { "",                 "Disk2_13Sector.rom",    "DISK2-13sector.rom",         "Devices/DiskII",              256, "Disk ][ Boot ROM (13-sector)"              },
    // Apple's Super Serial Card firmware, 341-0065-A, from the same mirror as
    // the //c ROM. No machine config lists it under `rom`, so the startup
    // dialog never offers it; Settings and the missing-firmware infobar
    // download it on request through DownloadOptionalRom.
    { "",                 "SSC.rom",               "",                           "Devices/SuperSerialCard",    2048, "Super Serial Card firmware (341-0065-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/Serial/Apple%20II%20Super%20Serial%20Card/ROM%20Images/Apple%20II%20Super%20Serial%20Card%20ROM%20-%20341-0065-A.bin",
      "apple2.org.za" },
    // The SmartPort and hard disk cards' firmware and the //c Plus ROMs, from
    // the same mirror, each with the SHA-256 the download must match: MAME's
    // files for the SCSI card, the 3.5 controller card and the //c Plus, the
    // a2-liron disassembly's for the Liron card and the UniDisk 3.5, and the
    // first fetch's for the ProFile cards.
    { "",                 "341-0437-A.rom",        "",                           "Devices/AppleScsiCard",     16384, "Apple II SCSI Card ROM (341-0437-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/SCSI%20Controllers/Apple%20II%20SCSI%20Card/ROM%20Images/Apple%20II%20SCSI%20Card%20ROM%20Rev.%20C%20-%20341-0437-A.bin",
      "apple2.org.za", {}, "5e1efe51b2c8be901ab4283c671bd84364a574845b7269f92bdddc4dae9ae5d5" },
    { "",                 "341-0438-A.rom",        "",                           "Devices/Apple35Card",       32768, "Apple II 3.5 Disk Controller Card ROM (341-0438-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/Disk%20Drive%20Controllers/Apple%20II%203.5%20Disk%20Controller%20Card/ROM%20Images/Apple%20II%203.5%20Disk%20Controller%20Card%20ROM%20-%20341-0438-A.bin",
      "apple2.org.za", {}, "084587835c1d2c92f5708a9745fdd43ba32136fcdb861a12ee7d690de22dc172" },
    { "",                 "Liron.rom",             "",                           "Devices/LironCard",          4096, "UniDisk 3.5 interface card ROM",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/Disk%20Drive%20Controllers/Apple%203.5%20Floppy%20Disk%20Drive%20Interface%20Card/ROM%20Images/Apple%203.5%20Floppy%20Disk%20Drive%20Interface%20Card%20ROM.bin",
      "apple2.org.za", {}, "42c1ae66c6bec932669239599eb8989a5364752d56fa5c8997bb23d8dfc0b657" },
    { "",                 "UniDisk35.rom",         "",                           "Devices/UniDisk35",          8192, "UniDisk 3.5 drive controller ROM",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Peripherals/Disk%20Drives/Apple%20UniDisk%203.5/ROM%20Images/Apple%20UniDisk%203.5%20ROM.bin",
      "apple2.org.za", {}, "9f9f9160938f0b69c6f532f9acf194e3b48de8b195b6b47faad8423d6c1e3cce" },
    { "",                 "341-0271-A.rom",        "",                           "Devices/ProFileCard",        2048, "ProFile interface card ROM, 5 MB (341-0271-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/Hard%20Disk%20Drive%20Controllers/Apple%20II%20ProFile%20Interface%20Card/ROM%20Images/Apple%20II%20Interface%20341-0271-A%20ROM%202716%20for%205MB%20Profile.bin",
      "apple2.org.za", {}, "6cfb995ff58ac3414fdd9f905563e1c9d57bf8fc4066c08c2480a24538897ae3" },
    { "",                 "341-0299-B.rom",        "",                           "Devices/ProFileCard",        2048, "ProFile interface card ROM, 10 MB (341-0299-B)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Interface%20Cards/Hard%20Disk%20Drive%20Controllers/Apple%20II%20ProFile%20Interface%20Card/ROM%20Images/Apple%20II%20Interface%20341-0299-B%20ROM%202716%20for%2010MB%20Profile.bin",
      "apple2.org.za", {}, "b920145fb5999e35d73c1b861d770c7f02d96eb773ccb8de2b8491c6ca3272e2" },
    { "Apple2cPlus",      "Apple2cPlus.rom",       "",                           "Machines/Apple2cPlus",      32768, "Apple //c Plus ROM 05 (342-0625-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIc%20plus/ROM%20Images/Apple%20IIc%20plus%20ROM%2005%20-%20342-0625-A%20-%201988.bin",
      "apple2.org.za", {}, "5a62070f6a0b07784681d4df4bf2ce88b2809bec0cbaa65fcb963e804ed60374" },
    { "Apple2cPlus",      "Apple2cPlus_Video.rom", "",                           "Machines/Apple2cPlus",       4096, "Apple //c Plus Character Generator + MouseText (342-0265-A)",
      "mirrors.apple2.org.za",
      "/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIc%20plus/ROM%20Images/Apple%20IIc%20plus%20Video%20ROM%20-%20342-0265-A.bin",
      "apple2.org.za", {}, "52c3b87900ac939f6525402cab1ccfd8f8259290fc6df54da48fb4c98ae3ed0f" },
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskSpec
//
//  Apple master disk images downloaded from the Asimov mirror when the
//  user picks one in the boot or insert disk picker, or clicks Download
//  in Create new disk. Nothing fetches them on its own. The on-disk
//  filename is shorter than the Asimov one; a copy dropped into Disks/
//  under that name is picked up without a download.
//
//  THE FILENAME COMES FROM StockBootDisks, which is where the command line
//  looks the same file up. This table held its own copy until 2026-09-02,
//  agreeing with that one by hand, so a rename in either place would have
//  left `disk create --bootable` hunting for a file the emulator no longer
//  writes -- and nothing could have caught it, because each half was
//  internally consistent.
//
////////////////////////////////////////////////////////////////////////////////

struct BootDiskSpec
{
    LPCWSTR      cassoName;          // local filename under Disks/, from StockBootDisks
    LPCWSTR      asimovUrlPath;      // path component on the Asimov mirror
    size_t       expectedSize;       // exact byte count for integrity check
    string_view  shortLabel;         // text on the choice button
    string_view  description;        // longer text in the dialog body
    LPCWSTR      host = nullptr;     // the Asimov mirror when none is given
};


// SHA1 27EA2EE7114EBFA91DA0A16B7B8EBFF24EB8EE88; verified against
// `Disks/Apple/dos33-master.dsk` already used by the developer's local
// CatalogReproductionTest.
static constexpr BootDiskSpec s_kDos33Disk =
{
    StockBootDisks::kpszDos33MasterFile,
    L"/images/masters/DOS%203.3%20System%20Master%20-%20680-0210-A%20%281982%29.dsk",
    143360,
    "DOS 3.3",
    "Apple DOS 3.3 System Master (680-0210-A, 1982). Boots Applesoft BASIC; "
    "type CATALOG to list files."
};

// SHA1 40DC1A16E3F234857A29B49CA0B996E1B14D38B9.
static constexpr BootDiskSpec s_kProDOSDisk =
{
    StockBootDisks::kpszProDosUsersDiskFile,
    L"/images/masters/prodos/ProDOS%20Users%20Disk%20-%20680-0224-C.dsk",
    143360,
    "ProDOS",
    "Apple ProDOS Users Disk (680-0224-C). Boots ProDOS 8 with the BASIC.SYSTEM "
    "shell."
};

static constexpr LPCWSTR  s_kpszBootDiskMirrorHost = L"mirrors.apple2.org.za";

//  ProDOS 2.4.3 (John Brooks, 2023), from the project's release repository,
//  the files releases.prodos8.com serves. SHA-256
//  398D333CB2AB92DF9F8BB2CF64B946F2567116910EB8359CF4BDEE5D4194F0FA, as the
//  repository's SHA256SUM lists it.
static constexpr LPCWSTR  s_kpszProDosReleasesHost = L"raw.githubusercontent.com";

static constexpr BootDiskSpec s_kProDos243Disk =
{
    StockBootDisks::kpszProDos243File,
    L"/ProDOS-8/ProDOS8-Releases/master/ProDOS_2_4_3.po",
    143360,
    "ProDOS 2.4.3",
    "ProDOS 2.4.3 (2023), for every Apple II. Boots ProDOS 8 with the BASIC.SYSTEM "
    "shell and the Bitsy Bye program launcher.",
    s_kpszProDosReleasesHost
};

static constexpr BootDiskSpec s_kPascalDisk =
{
    StockBootDisks::kpszPascalBootDiskFile,
    L"/Apple%20II%20Diskware/Pascal/Apple%20II%20Pascal%201.3%20APPLE1%3A%20680-0283-A.dsk",
    143360,
    "Apple Pascal",
    "Apple Pascal 1.3, APPLE1: boot disk (680-0283-A).",
    s_kpszBootDiskMirrorHost
};

//  The default for CP/M: it runs on Applied Engineering's Z-RAM.
static constexpr BootDiskSpec s_kCpamDisk =
{
    StockBootDisks::kpszCpamBootDiskFile,
    L"/ftp.apple.asimov.net/images/cpm/os/CPAM51A.dsk",
    143360,
    "CP/AM 5.1",
    "Applied Engineering CP/AM 5.1, for the Z-RAM.",
    s_kpszBootDiskMirrorHost
};

//  SOS 1.1 for the Apple ///: the System Utilities disk, whose SOS loader and
//  SOS.KERNEL, SOS.DRIVER and SOS.INTERP make a disk start an Apple ///.
static constexpr BootDiskSpec s_kSosDisk =
{
    StockBootDisks::kpszSosSystemDiskFile,
    L"/Apple%20II%20Documentation%20Project/Computers/Apple%20III/Apple%20III/Disk%20Images/Apple%20III%20System%20Utilities.dsk",
    143360,
    "SOS",
    "Apple /// System Utilities with SOS 1.1.",
    s_kpszBootDiskMirrorHost
};

static constexpr BootDiskSpec s_kSoftCardDisk =
{
    StockBootDisks::kpszSoftCardBootDiskFile,
    L"/ftp.apple.asimov.net/images/cpm/os/CPM2.23%2860k%29.dsk",
    143360,
    "CP/M 2.23",
    "CP/M 2.23, for a Microsoft SoftCard.",
    s_kpszBootDiskMirrorHost
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDisplayName
//
//  Maps a machine id to the name a user would recognize.
//
//  The two are separate because the id is a filesystem-safe identifier used in
//  paths and prefs, while the display name carries the punctuation the real
//  machines were sold under -- "Apple ][+" and "Apple //e" cannot be directory
//  names.
//
//  An unrecognized id WIDENS AS-IS rather than falling back to a placeholder,
//  so a machine added to the catalog before it is listed here still shows
//  something recognizable instead of "(unknown)".
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring MachineDisplayName (std::string_view machineId)
{
    // An id with no pretty name widens as-is, so a machine added to the
    // catalog still shows something recognizable before it is listed here.
    std::wstring  name = TextEncoding::Utf8ToWide (std::string (machineId));



    if      (machineId == "Apple2")          { name = L"Apple ]["; }
    else if (machineId == "Apple2Plus")      { name = L"Apple ][+"; }
    else if (machineId == "Apple2e")         { name = L"Apple //e"; }
    else if (machineId == "Apple2eEnhanced") { name = L"Apple //e Enhanced"; }
    else if (machineId == "Apple2c")         { name = L"Apple //c"; }
    else if (machineId == "Apple2cPlus")     { name = L"Apple //c Plus"; }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedConfig / s_kEmbeddedConfigs
//
//  Moved to EmbeddedMachineConfigs.h (included above) so AssetBootstrapTests
//  reads the same stamp table and can assert in CI that each stamp matches
//  its embedded JSON's $cassoMachineVersion.
//
////////////////////////////////////////////////////////////////////////////////





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

    // v6 Apple2e.json (before the slot-4 Mockingboard / slot-1 printer).
    { "Apple2e",    "8593a47d87db9090ce001e439fea318854bdc6b255fa328f8ac2dabee1eb9f63" },

    // v7 Apple2Plus.json (master, before the slot-4 Mockingboard).
    { "Apple2Plus", "99824c2f34e40c9411d46d17c5a34d78b1bbd2a29e4487fc0326457b18986236" },

    // v7 Apple2e.json (master Mockingboard slot 4; superseded by v8, which
    // carries both the Mockingboard and the slot-1 parallel printer).
    { "Apple2e",    "294312b9acc832c022b14c9d2e38946b13f7baab9d149760f79a8ee75ec3178a" },

    // v8 Apple2Plus.json (master Mockingboard slot 4; superseded by v9, which
    // carries both the Mockingboard and the slot-1 parallel printer).
    { "Apple2Plus", "c3b3222ddfd2c08b65afb80ece15fa5fee87fa487e5e3f4ed661931ba323a9c4" },

    // The five configs as they stood before `ports` -- the drive attached to
    // a connector became the config's business rather than a hard-coded pair.
    //
    // ALL FIVE machines appear here, including the two that have never been
    // in this table before: Apple2eEnhanced (v2) and Apple2c (v1) shipped
    // after the last bump, so this is the first time their on-disk extracts
    // have needed recognizing. Leaving them out would present every existing
    // extract of those two as user-edited and rename it aside.
    { "Apple2",          "acbc87edad795b8f5d0e6f144f81e6953e10a2b8e65ab8086098d79938f32c35" },
    { "Apple2Plus",      "f8b8a241e5ada0575ffc47bf073c92eb1e68c4cf73333d9827458d754cd35df1" },
    { "Apple2e",         "1c355cafb7a8f301b9d34e9f718697c6208b2e93b3c21a109387a9f7c1fa6b89" },
    { "Apple2eEnhanced", "89355968f0683869a22fe7d2a564cb62b4783b7a675886420df9371a5bc4ca92" },
    { "Apple2c",         "ad85c6350233e5d5312c6ba9b4dc97e225583ad0de0c2b113f68d99b0be8bfd0" },

    // The four slotted machines before either card: Apple2 v10,
    // Apple2Plus v13, Apple2e v12, Apple2eEnhanced v7.
    { "Apple2",          "32b2d21a67494e91d9b232bc9d4e1777831fa2d0a99749f15c38842a99f6d49d" },
    { "Apple2Plus",      "9dda6064ecd6cd9af46091da47ada410bcc08a57fb49e8600e60f0c9620152a7" },
    { "Apple2e",         "08ef86bc0a3ecd79ea6475331a2a05d94873eafd72621ee3f646f6dee048d761" },
    { "Apple2eEnhanced", "0ee887a4b0b42660de16caf3b329cc4aea4effa5efcf7ced9d63cdf22132eec4" },

    // The same four with the Super Serial Card in slot 2 and before
    // the Uthernet II entry in slot 3: Apple2 v11, Apple2Plus v14,
    // Apple2e v13, Apple2eEnhanced v8.
    { "Apple2",          "8857e6dfd805a32967a8c7b5396014b3e78b7fa0f66f3980a6304c0a42c0ef3a" },
    { "Apple2Plus",      "5fd41292fb955256277ba6a4864b43e12c60ff7a8561974fc05fc74a8c0a8737" },
    { "Apple2e",         "3b5caa6968292ef27d89fa078781e7a477f9d842d95d31f27bf8f5144c921f9f" },
    { "Apple2eEnhanced", "30e5f9b9bf8352d9495e40f08241724d8eeacbd09feefc711c77eee42629de2f" },

    // All five at the crystal divided by 14 (1,022,727 Hz), before the
    // clock became the scanline average: Apple2 v12, Apple2Plus v15,
    // Apple2e v14, Apple2eEnhanced v9, Apple2c v5.
    { "Apple2",          "3c2b7e071afcf540a4f7bbe1ae7b7c33840131c78240246a22a6306c205c4d68" },
    { "Apple2Plus",      "b2995b59e6d5826dd620e0f5930a98f892eb29fd7eadde2100632c44583091b0" },
    { "Apple2e",         "3c7b492ad67ad8fd325a82a8f3c6080fdb07b9716368ce20922e941a30be0983" },
    { "Apple2eEnhanced", "bf6b8311abb1354116cd44e461f5a19a6eb14ab092637e466a0c3122d0df62a9" },
    { "Apple2c",         "001b5e57036ad0a34ff50418be0e794052c27b2746934eb6222736246319d93b" },
    // The //e and the enhanced //e before the SCSI card in slot 7: Apple2e
    // v15, Apple2eEnhanced v10.
    { "Apple2e",         "ba7106c242b21875c6430ddcdafdc512cb5d4ed7e4b8352ae9f6ffc06cbdb548" },
    { "Apple2eEnhanced", "5fddd5c15c4ae77358724182713209c301576e867f21d656505f548fecfb0ca6" },
    // The same two with the SCSI card, before the UniDisk 3.5 interface card
    // in slot 5: Apple2e v16, Apple2eEnhanced v11.
    { "Apple2e",         "b0f9dc454a3e6d3bf1a9e5461618ea79e6097b8c0a4c8d5e82f4e29cb0397bc2" },
    { "Apple2eEnhanced", "4638ca69a0eaef913b88b794884369627f92b832122a12d724998bedc584ed48" },
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
//  Widening for the catalog's string literals (descriptions, filenames).
//  They are ASCII today; decoding them as UTF-8 keeps a non-ASCII one intact
//  rather than turning each byte into its own character.
//
////////////////////////////////////////////////////////////////////////////////

static wstring AsciiToWide (string_view s)
{
    return TextEncoding::Utf8ToWide (std::string (s));
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
//  CheckRomSha256
//
//  A downloaded ROM against the SHA-256 its catalog row lists. A row with
//  none passes; a mismatch fails with the file's name, and the caller then
//  writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT CheckRomSha256 (const RomSpec & spec, const vector<Byte> & payload, string & outError)
{
    HRESULT  hr      = S_OK;
    string   bytes   (reinterpret_cast<const char *> (payload.data()), payload.size());
    string   hex;
    bool     matches = false;



    BAIL_OUT_IF (spec.expectedSha256.empty(), S_OK);

    hex     = MachineConfigUpgrade::BytesToHex (ComputeSha256 (bytes));
    matches = hex == spec.expectedSha256;

    CBRF (matches, outError = format ("The download of {} doesn't match its checksum, so it was not saved.", spec.cassoName));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMatchesSha256
//
//  Whether the file at `path` hashes to `expectedHex`. An empty
//  expectation, an unreadable file, or a BCrypt failure all answer no,
//  which leaves the file alone -- the callers use this to decide whether
//  to REPLACE something, so uncertainty has to mean "keep what is there".
//
////////////////////////////////////////////////////////////////////////////////

static bool FileMatchesSha256 (const fs::path & path, string_view expectedHex)
{
    ifstream    file;
    string      bytes;
    string      actualHex;
    error_code  ec;
    bool        matches = false;



    if (expectedHex.empty())
    {
        return (false);
    }

    file.open (path, std::ios::binary);

    if (!file.is_open())
    {
        return (false);
    }

    bytes.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char> ());

    actualHex = MachineConfigUpgrade::BytesToHex (ComputeSha256 (bytes));
    matches   = (actualHex == expectedHex);

    return (matches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFileBytes
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteFileBytes (const fs::path & path, span<const Byte> bytes)
{
    HRESULT     hr        = S_OK;
    ofstream    out;
    bool        wroteWell = false;



    out.open (path, ios::binary | ios::trunc);

    wroteWell = out.good();
    CBRA (wroteWell);

    out.write (reinterpret_cast<const char *> (bytes.data()), static_cast<streamsize> (bytes.size()));

    wroteWell = out.good();
    CBRA (wroteWell);

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



#if defined(_DEBUG)
    // Self-check: each embedded config's currentVersion stamp must match the
    // $cassoMachineVersion baked into its JSON. If they drift, Plan() below
    // treats an unchanged on-disk extract as already current and silently
    // skips a real change -- the failure that once hid the slot-4 Mockingboard.
    for (const EmbeddedConfig & selfCheck : s_kEmbeddedConfigs)
    {
        span<const Byte>  scBytes   = ExtractResource (hInstance, selfCheck.resourceId);
        string            scJson    = string (reinterpret_cast<const char *> (scBytes.data()), scBytes.size());
        JsonValue         scRoot;
        JsonParseError    scErr;
        int               scVersion = 0;
        HRESULT           scParseHr = S_OK;
        HRESULT           scVerHr   = S_OK;



        scParseHr = JsonParser::Parse (scJson, scRoot, scErr);

        if (SUCCEEDED (scParseHr))
        {
            scVerHr = scRoot.GetInt ("$cassoMachineVersion", scVersion);

            _ASSERTE (SUCCEEDED (scVerHr)
                      && scVersion == selfCheck.currentVersion
                      && "s_kEmbeddedConfigs currentVersion out of sync with embedded $cassoMachineVersion");
        }
    }
#endif

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
        string                        embeddedHashHex;
        MachineConfigUpgradeAction    action          = MachineConfigUpgradeAction::Skip;



        fs::create_directories (machineSubdir, ec);

        diskExists = fs::exists (target, ec);

        if (diskExists)
        {
            stringstream        ss;
            array<uint8_t, 32>  diskHash;

            ifstream            diskFile (target);
            diskHash = {};

            if (!diskFile.good())
            {
                continue;
            }

            ss << diskFile.rdbuf();
            diskContent = ss.str();
            diskHash    = ComputeSha256 (MachineConfigUpgrade::NormalizeBytes (diskContent));
            diskHashHex = MachineConfigUpgrade::BytesToHex (diskHash);
        }

        // Digest of THIS build's embedded default: version stamps can
        // collide across feature branches, so Plan treats content
        // equality (not the stamp) as the up-to-date test.
        bytes = ExtractResource (hInstance, cfg.resourceId);

        if (bytes.empty())
        {
            hr = E_FAIL;
            continue;
        }

        {
            string              embeddedContent (reinterpret_cast<const char *> (bytes.data()), bytes.size());
            array<uint8_t, 32>  embeddedHash =
                ComputeSha256 (MachineConfigUpgrade::NormalizeBytes (embeddedContent));

            embeddedHashHex = MachineConfigUpgrade::BytesToHex (embeddedHash);
        }

        action = MachineConfigUpgrade::Plan (
            cfg.machineName,
            cfg.currentVersion,
            embeddedHashHex,
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
};




static constexpr EmbeddedThemeFile s_kDarkModernFiles[] =
{
    { IDR_THEME_DARK_THEME_JSON,         "theme.json"          },
};




static constexpr EmbeddedThemeFile s_kRetroTerminalFiles[] =
{
    { IDR_THEME_RETRO_THEME_JSON,         "theme.json"          },
};




static constexpr EmbeddedThemeFile s_kPhosphorFiles[] =
{
    { IDR_THEME_PHOSPHOR_THEME_JSON,      "theme.json"          },
    { IDR_THEME_PHOSPHOR_FONT_TTF,        "fonts/VT323-Regular.ttf" },
    { IDR_THEME_PHOSPHOR_FONT_OFL,        "fonts/OFL.txt"       },
};




static const EmbeddedTheme s_kEmbeddedThemes[] =
{
    { "Skeuomorphic",  1, span<const EmbeddedThemeFile> (s_kSkeuomorphicFiles)  },
    { "DarkModern",    1, span<const EmbeddedThemeFile> (s_kDarkModernFiles)    },
    { "RetroTerminal", 1, span<const EmbeddedThemeFile> (s_kRetroTerminalFiles) },
    { "Phosphor",      1, span<const EmbeddedThemeFile> (s_kPhosphorFiles)      },
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
        fs::path              themeSubdir      = themesDir / theme.dirName;
        bool                  diskExists       = false;
        string                diskJsonText;
        string                embeddedJsonText;
        int                   themeJsonResId   = 0;
        ThemeBootstrapAction  action           = {};
        fs::path              themeJsonPath     = themeSubdir / "theme.json";
        action = ThemeBootstrapAction::Skip;



        diskExists = fs::exists (themeJsonPath, ec);

        if (diskExists)
        {
            stringstream  ss;

            ifstream      file (themeJsonPath);

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
//  EnsureImageWriterSounds
//
//  Extract the embedded ImageWriter II mechanical sound grains to
//  %LOCALAPPDATA%\Casso\ImageWriter II Sounds\ so PrinterAudioSource can decode
//  them by path (Media Foundation needs a file). Unlike machine configs /
//  themes there is no user-edit or versioning story: the grains are read-only
//  assets, so a missing file is (re)written and an existing one is left alone.
//  Best-effort throughout -- a failed write leaves that grain silent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::EnsureImageWriterSounds (HINSTANCE hInstance)
{
    struct SoundAsset { int resourceId; const wchar_t * fileName; };

    static const SoundAsset  s_kSounds[] =
    {
        { IDR_SOUND_PRINT_DRAFT,       L"print_draft_loop.mp3"  },
        { IDR_SOUND_PRINT_MEDIUM,      L"print_medium_loop.mp3" },
        { IDR_SOUND_PRINT_NLQ,         L"print_nlq_loop.mp3"    },
        { IDR_SOUND_LINE_FEED_1,       L"line_feed_01.mp3"      },
        { IDR_SOUND_LINE_FEED_2,       L"line_feed_02.mp3"      },
        { IDR_SOUND_LINE_FEED_3,       L"line_feed_03.mp3"      },
        { IDR_SOUND_PAGE_FEED_SHORT,   L"page_feed_short.mp3"   },
        { IDR_SOUND_PAGE_FEED_MEDIUM,  L"page_feed_medium.mp3"  },
        { IDR_SOUND_PAGE_FEED_LONG,    L"page_feed_long.mp3"    },
        { IDR_SOUND_PAPER_TEAR_1,      L"paper_tear_01.mp3"     },
        { IDR_SOUND_PAPER_TEAR_2,      L"paper_tear_02.mp3"     },
        { IDR_SOUND_PAPER_TEAR_3,      L"paper_tear_03.mp3"     },
        { IDR_SOUND_PAPER_TEAR_4,      L"paper_tear_04.mp3"     },
        { IDR_SOUND_PAPER_TEAR_5,      L"paper_tear_05.mp3"     },
    };

    HRESULT     hr        = S_OK;
    fs::path    soundsDir;
    error_code  ec;

    soundsDir = GetAssetBaseDirectory() / L"ImageWriter II Sounds";
    fs::create_directories (soundsDir, ec);

    for (const SoundAsset & asset : s_kSounds)
    {
        fs::path          target = soundsDir / asset.fileName;
        span<const Byte>  bytes;
        HRESULT           hrItem;

        if (fs::exists (target, ec))
        {
            continue;   // already extracted; grains are immutable
        }

        bytes = ExtractResource (hInstance, asset.resourceId);

        if (bytes.empty())
        {
            hr = E_FAIL;   // resource missing from this build
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
//  RegisterSymbolFont
//
//  Points DirectWrite at the embedded symbol font for the Apple keycap
//  codepoints, so `s_kpszOpenApple` and `s_kpszClosedApple` render as glyphs
//  wherever chrome puts them in a string.
//
//  Registered, not extracted: the theme fonts above go to disk because a theme
//  is a directory a user can edit, while this one is chrome that has to work
//  whatever became of the asset directory. The bytes come straight from the
//  module image and DirectWrite copies them, so nothing here outlives the call.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::RegisterSymbolFont (HINSTANCE hInstance)
{
    HRESULT           hr       = S_OK;
    span<const Byte>  bytes;
    bool              embedded = false;



    bytes    = ExtractResource (hInstance, IDR_FONT_SYMBOLS);
    embedded = !bytes.empty();
    CBRA (embedded);

    hr = DxuiTextRenderer::AddSymbolFont (bytes.data(),
                                          bytes.size(),
                                          s_kSymbolFontFirst,
                                          s_kSymbolFontLast);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyThemeTypography
//
//  Sets the process-wide body face and text glow from a theme. The face is
//  read from the module image rather than from the extracted theme folder,
//  for the same reason as RegisterSymbolFont: chrome text has to draw
//  whatever became of the asset directory.
//
//  Non-asserting when the resource is missing: a test host links this code
//  without Casso's resources. The body face is cleared, and the chrome is
//  drawn in the system face.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::ApplyThemeTypography (HINSTANCE hInstance, const CassoTheme & theme)
{
    HRESULT           hr           = S_OK;
    span<const Byte>  bytes;
    bool              isSystemFace = (theme.bodyFace == CassoTheme::BodyFace::System);
    bool              embedded     = false;



    DxuiTextRenderer::SetTextGlow               (theme.textGlowAlpha, theme.textGlowRadiusEm);
    DxuiTextRenderer::SetMonochromeGlyphsForced (theme.monochromeGlyphs);

    if (isSystemFace)
    {
        DxuiTextRenderer::ClearBodyFace();
    }

    BAIL_OUT_IF (isSystemFace, S_OK);

    bytes    = ExtractResource (hInstance, IDR_THEME_PHOSPHOR_FONT_TTF);
    embedded = !bytes.empty();
    CBR (embedded);

    hr = DxuiTextRenderer::SetBodyFace (bytes.data(), bytes.size(), theme.bodyFaceScale);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        DxuiTextRenderer::ClearBodyFace();
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
    error_code   ec;
    fs::path     disks  = base / L"Disks";



    fs::create_directories (disks, ec);
    return disks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsForeignCheckoutDisk
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::IsForeignCheckoutDisk (const fs::path & p)
{
    // The %LOCALAPPDATA% recent-disks MRU is shared across every checkout of
    // the repo (the main tree plus each .claude/worktrees/<name> copy), so it
    // accumulates absolute paths from all of them -- the same disk then shows
    // once per checkout. Hide any MRU disk that belongs to a checkout OTHER
    // than the one this build runs from (a sibling worktree, or the main tree
    // when we run from a worktree); disks outside the repo entirely (the
    // user's own folders, %LOCALAPPDATA%) always pass. The classification is
    // pure/lexical and unit-tested in RepoCheckout.h.
    static const std::wstring  runningKey =
        RepoCheckout::GetWorktreeKey (PathResolver::GetExecutableDirectory());



    return RepoCheckout::IsForeignCheckoutDisk (p, runningKey);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendBundledDemoDisks
//
//  Offers the Apple2/Demos images in the disk picker, so a build always has
//  something to boot. A repo build finds the source tree's copy up the walk;
//  a release zip carries its own next to the exe, which the i=0 pass finds
//  first. Both arrive here as the same directory, which is why the release
//  packaging needed no code and must keep staging the disks under that name.
//
//  Two different cache lifetimes are at work here, and the split is the point.
//
//  The DIRECTORY is located once per process and remembered, including its
//  absence. Its location is purely a function of the exe path and what was
//  shipped or checked out around it, and neither changes while the process
//  runs, so a miss is never re-attempted. std::nullopt records that absence
//  distinctly from "not yet looked".
//
//  The CONTENTS are enumerated fresh on every open, because a user can drop a
//  new image into that directory while Casso is running and the picker should
//  pick it up. The directory is small, so the listing is cheap.
//
//  The walk-up tries a few levels because the exe lives at
//  <repo>/<platform>/<config>/Casso.exe, and also tries the working directory
//  for a run launched from the repo root.
//
//  De-duplication uses fs::equivalent rather than string comparison, so the
//  same file reached through a different spelling -- a symlink, a mapped
//  drive, differing case -- is not listed twice.
//
////////////////////////////////////////////////////////////////////////////////

void AssetBootstrap::AppendBundledDemoDisks (std::vector<DiskMru::Entry> & mountable, MediaFilter isWanted)
{
    std::vector<fs::path>  demos;
    error_code             ec;



    // Locate Apple2/Demos ONCE per process. Its location is purely a function
    // of the exe path and what sits around it -- neither changes while we run
    // -- so a miss can't change at runtime either and we record the absence
    // rather than re-walking. `std::nullopt` means "no demos dir"; a value is
    // the resolved directory.
    static const std::optional<fs::path>  demosDir = [] () -> std::optional<fs::path>
    {
        fs::path     cursor = PathResolver::GetExecutableDirectory();
        error_code   ecDir;

        // A release zip puts Apple2/Demos beside the exe, which is the first
        // candidate tried. A repo build runs from
        // <repo>/<platform>/<config>/Casso.exe, so keep walking up a few
        // levels (and also try the working directory).
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

    // Neither layout has one -- nothing to offer, and that will never change.
    if (!demosDir.has_value())
    {
        return;
    }

    // Enumerate the directory fresh on every open: the user can drop a new
    // disk image into Apple2/Demos while Casso is running, and the picker
    // should pick it up. Listing this small directory is cheap.
    for (const fs::directory_entry & entry : fs::directory_iterator (*demosDir, ec))
    {
        error_code  ecFile;

        if (entry.is_regular_file (ecFile) &&
            isWanted (entry.path().wstring()))
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
//  AppendSiblingDisksFromMruFolders
//
//  Surfaces disk images sitting NEXT TO recently-mounted ones, so a personal
//  disk stash appears in the picker without every image having been mounted
//  once first.
//
//  The folder set comes from the MRU rather than from a configured search
//  path: where the user keeps disks is revealed by where they have opened
//  them, and no setting has to be maintained.
//
//  Each folder is enumerated fresh on every open, since a user can drop a new
//  image beside a recent one -- or a tool can hand one back -- while Casso is
//  running. These are small directories (a disk stash or the Demos folder), so
//  the walk is cheap.
//
//  Foreign checkout disks are filtered out, which keeps a developer's picker
//  from filling with test images that happen to live under a build tree.
//
//  De-duplication matches AppendBundledDemoDisks: fs::equivalent against both
//  the existing entries and anything appended earlier in this same pass, so
//  two MRU folders that resolve to the same directory cannot double-list.
//
////////////////////////////////////////////////////////////////////////////////

void AssetBootstrap::AppendSiblingDisksFromMruFolders (
    std::vector<DiskMru::Entry>       & mountable,
    const std::vector<DiskMru::Entry> & scanFrom,
    MediaFilter                         isWanted)
{
    std::vector<fs::path>  folders = DiskMru::DistinctFolders (scanFrom);



    AppendSiblingDisksFromFolders (folders, mountable, isWanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendSiblingDisksFromFolders
//
//  The scan AppendSiblingDisksFromMruFolders describes, over a folder list the
//  caller supplies.
//
////////////////////////////////////////////////////////////////////////////////

void AssetBootstrap::AppendSiblingDisksFromFolders (
    const std::vector<fs::path>    & folders,
    std::vector<DiskMru::Entry>    & mountable,
    MediaFilter                      isWanted)
{
    std::vector<fs::path>  discovered;



    // Enumerate each recent-disk folder fresh on every open: the user can
    // drop a new image next to a recent one (or hand one back from a tool)
    // while Casso runs, and the picker should surface it without a mount.
    // These folders are small (a personal disk stash or the Demos dir), so
    // the walk is cheap.
    for (const fs::path & dir : folders)
    {
        error_code  ecDir;

        for (const fs::directory_entry & entry : fs::directory_iterator (dir, ecDir))
        {
            error_code  ecFile;

            if (entry.is_regular_file (ecFile) &&
                isWanted (entry.path().wstring()) &&
                !IsForeignCheckoutDisk (entry.path()))
            {
                discovered.push_back (entry.path().lexically_normal());
            }
        }
    }

    std::sort (discovered.begin(), discovered.end());

    // Append the disks we don't already list, de-duplicating by filesystem
    // identity against the existing entries (and anything appended earlier
    // in this same pass), matching AppendBundledDemoDisks.
    for (const fs::path & disk : discovered)
    {
        bool        already = false;
        error_code  ecDup;

        for (const DiskMru::Entry & existing : mountable)
        {
            if (fs::equivalent (existing.path, disk, ecDup))
            {
                already = true;
                break;
            }
        }

        if (!already)
        {
            mountable.push_back (DiskMru::Entry { disk, 0 });
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
            if (spec.cassoName == cassoName && spec.machineName.empty())
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
    HRESULT       hr           = S_OK;
    HttpRequest   request;
    HttpResponse  response;
    size_t        receivedSize = 0;
    bool          hasBytes     = false;



    outBytes.clear();

    request.host            = host;
    request.path            = urlPath;
    request.displayName     = displayName;
    request.progressBytes   = progressBytes;
    request.cancelRequested = cancelRequested;

    hr = WinHttpClient::GetWithSession (hSession, request, response, outError);
    CHR (hr);

    CBRF (response.statusCode == HTTP_STATUS_OK,
          outError = format ("HTTP {} fetching {}", response.statusCode, displayName));

    outBytes = std::move (response.body);

    // A non-zero `expectedSize` is treated as an integrity check
    // (used for ROM downloads where we know the exact byte count
    // ahead of time). Pass 0 to skip the size check -- used for the
    // OpenEmulator OGG fetch where the upstream file size is not
    // pinned (T134).
    receivedSize = outBytes.size();
    hasBytes     = !outBytes.empty();

    if (expectedSize > 0)
    {
        CBRF (receivedSize == expectedSize,
              outError = format ("Downloaded {} has wrong size: got {}, expected {}",
                                 displayName, receivedSize, expectedSize));
    }
    else
    {
        CBRF (hasBytes,
              outError = format ("Downloaded {} was empty", displayName));
    }

Error:
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
    HRESULT                 hr       = S_OK;
    const EmbeddedConfig  * cfg      = nullptr;
    bool                    hasBytes = false;
    span<const Byte>        bytes;



    outJsonText.clear();
    outNarrowName.clear();
    outNarrowName = TextEncoding::WideToUtf8 (machineName);

    cfg = FindEmbeddedConfig (machineName);
    CBRF (cfg != nullptr,
          outError = format ("No embedded config for machine '{}'", outNarrowName));

    bytes    = ExtractResource (hInstance, cfg->resourceId);
    hasBytes = !bytes.empty();

    CBRF (hasBytes,
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
//  AssetBootstrap::LoadMergedConfig
//
//  The machine `machineName` as the user currently has it configured: the
//  embedded default config with the user's per-machine delta folded in, so
//  a slot the user disabled in Settings > Machine ("enabled": false) reads
//  as it does when the machine is built (MachineConfig::Parse). Falls back
//  to the embedded config if the merge fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::LoadMergedConfig (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    JsonValue       & outMerged,
    string          & outError)
{
    HRESULT          hr         = S_OK;
    HRESULT          hrMerge    = S_OK;
    string           jsonText;
    string           narrowName;
    JsonValue        root;
    JsonValue        mergedTmp;
    JsonParseError   parseError;
    Win32FileSystem  fsMerge;
    UserConfigStore  store      (GetAssetBaseDirectory().wstring());



    hr = LoadEmbeddedJson (hInstance, machineName, jsonText, narrowName, outError);
    CHR (hr);

    hr = JsonParser::Parse (jsonText, root, parseError);
    CHRF (hr,
          outError = format ("Embedded config for '{}' is malformed: {} at line {}",
                             narrowName, parseError.message, parseError.line));

    hrMerge = store.Load (narrowName, root, fsMerge, mergedTmp);

    if (SUCCEEDED (hrMerge) && mergedTmp.GetType() == JsonType::Object)
    {
        outMerged = std::move (mergedTmp);
    }
    else
    {
        outMerged = std::move (root);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasDiskController
//
//  True iff the machine `machineName`, as the user currently has it
//  configured, has an ENABLED Disk ][ controller slot. A slot the user
//  disabled in Settings > Machine is reported as absent -- otherwise we'd
//  offer a boot-disk download / pop the boot-disk picker for a machine the
//  user deliberately stripped of its controller. Used to gate the boot-disk
//  flow.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::HasDiskController (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    bool            & outHasDiskController,
    string          & outError)
{
    HRESULT             hr           = S_OK;
    JsonValue           merged;
    const JsonValue   * pSlots       = nullptr;
    HRESULT             hrOpt        = S_OK;
    size_t              idx          = 0;
    string              device;



    outHasDiskController = false;

    hr = LoadMergedConfig (hInstance, machineName, merged, outError);
    CHR (hr);

    // `slots` is optional (][/][+ omit it); a missing slots array
    // simply means there is no Disk ][ controller for this machine.
    hrOpt = merged.GetArray ("slots", pSlots);
    BAIL_OUT_IF (FAILED (hrOpt), S_OK);

    for (idx = 0; idx < pSlots->GetArraySize(); idx++)
    {
        const JsonValue  & entry   = pSlots->GetArrayElement (idx);
        bool               enabled = false;
        HRESULT            hrDev   = entry.GetString ("device", device);
        enabled = true; // optional key; defaults enabled

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
//  AssetBootstrap::HasSlot6Drives
//
//  Whether the machine `machineName`, as the user currently has it
//  configured, has slot 6's drives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::HasSlot6Drives (
    HINSTANCE         hInstance,
    const wstring   & machineName,
    bool            & outHasDrives,
    string          & outError)
{
    HRESULT    hr = S_OK;
    JsonValue  merged;



    outHasDrives = false;

    hr = LoadMergedConfig (hInstance, machineName, merged, outError);
    CHR (hr);

    outHasDrives = HasSlot6Drives (merged);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssetBootstrap::HasSlot6Drives
//
//  Whether a machine config has slot 6's drives: an enabled Disk II card in
//  slot 6, or the //c's built-in drive. The //c is the only machine with a
//  banked system ROM, the same discriminator ShellDisks uses.
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::HasSlot6Drives (const JsonValue & config)
{
    const JsonValue  * pRom      = nullptr;
    const JsonValue  * pSlots    = nullptr;
    string             bankSize;
    string             device;
    int                slot      = 0;
    bool               isEnabled = true;
    bool               hasDrives = false;
    HRESULT            hrRom     = config.GetObject ("systemRom", pRom);
    HRESULT            hrSlots   = config.GetArray ("slots", pSlots);
    HRESULT            hrBank    = S_OK;
    HRESULT            hrDevice  = S_OK;
    HRESULT            hrSlot    = S_OK;
    HRESULT            hrEnabled = S_OK;
    size_t             idx       = 0;



    if (SUCCEEDED (hrRom) && pRom != nullptr)
    {
        hrBank    = pRom->GetString ("romBankSize", bankSize);
        hasDrives = SUCCEEDED (hrBank);
    }

    for (idx = 0; !hasDrives && SUCCEEDED (hrSlots) && pSlots != nullptr && idx < pSlots->GetArraySize(); idx++)
    {
        const JsonValue  & entry = pSlots->GetArrayElement (idx);

        isEnabled = true;
        hrDevice  = entry.GetString ("device", device);
        hrSlot    = entry.GetInt ("slot", slot);
        hrEnabled = entry.GetBool ("enabled", isEnabled);
        IGNORE_RETURN_VALUE (hrEnabled, S_OK);

        hasDrives = SUCCEEDED (hrDevice) && SUCCEEDED (hrSlot) && device == kpszDiskIiDevice && slot == 6 && isEnabled;
    }

    return hasDrives;
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

wstring AssetBootstrap::GetEmbeddedDisplayName (HINSTANCE hInstance, const wstring & machineName)
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

    if (SUCCEEDED (hr) && root.HasString ("name", name) && !name.empty())
    {
        result = TextEncoding::Utf8ToWide (std::string (name));
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DownloadStockBootDisk
//
//  Pure download helper behind the DOS 3.3 / ProDOS rows of the boot and
//  insert disk pickers and, through EnsureStockBootDisk, the Download
//  button in Create new disk. Returns at once when the file is already
//  under `diskDir`; otherwise fetches `spec` from the Asimov mirror,
//  writes it there, and returns the absolute path in `outDiskPath`. No
//  UI; the caller owns the prompt and any error reporting.
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
                       (spec.host != nullptr) ? spec.host : s_kpszAsimovHost,
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

    //  A write that failed partway leaves a truncated file, and the check at
    //  the top would hand that back as the download on the next attempt.
    if (FAILED (hr))
    {
        fs::remove (destPath, ec);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportStockDiskDownloadFailure
//
//  The one place a failed stock download is shown. Both pickers call it and
//  then come back up, so the user reads why and still has every alternative
//  in front of them. The reason is DownloadStockBootDisk's own text, which
//  states what went wrong (no network, an HTTP status, a wrong size, a file
//  that could not be written) rather than only that something did.
//
//  The reason goes through NarrowToWide rather than the file's AsciiToWide,
//  because a write failure interpolates the cache path, and that path carries
//  the user's account name in the process's narrow code page.
//
////////////////////////////////////////////////////////////////////////////////

static void ReportStockDiskDownloadFailure (
    HWND                     hwndParent,
    const IDxuiTheme       * theme,
    const wstring          & caption,
    const BootDiskSpec     & spec,
    string_view              error,
    const wchar_t          * choices)
{
    wstring  text;



    text  = L"Download failed\n\n";
    text += format (L"Casso could not download the {} master from the Asimov archive.\n",
                    AsciiToWide (spec.shortLabel));

    if (!error.empty())
    {
        text += TextEncoding::NarrowToWide (string (error));
        text += L".\n";
    }

    text += choices;

    DxuiMessageBox (hwndParent, theme, text.c_str(), caption.c_str(), MB_OK | MB_ICONWARNING);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStockBootDiskSpec
//
////////////////////////////////////////////////////////////////////////////////

static const BootDiskSpec & GetStockBootDiskSpec (AssetBootstrap::StockBootDisk disk)
{
    switch (disk)
    {
        case AssetBootstrap::StockBootDisk::ProDosUsersDisk:  return s_kProDOSDisk;
        case AssetBootstrap::StockBootDisk::ProDos243Disk:    return s_kProDos243Disk;
        case AssetBootstrap::StockBootDisk::PascalBootDisk:   return s_kPascalDisk;
        case AssetBootstrap::StockBootDisk::CpamBootDisk:     return s_kCpamDisk;
        case AssetBootstrap::StockBootDisk::SoftCardBootDisk: return s_kSoftCardDisk;
        case AssetBootstrap::StockBootDisk::SosSystemDisk:    return s_kSosDisk;
        default:                                              return s_kDos33Disk;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStockBootDiskPath
//
////////////////////////////////////////////////////////////////////////////////

fs::path AssetBootstrap::GetStockBootDiskPath (StockBootDisk disk)
{
    return GetDiskDirectory() / GetStockBootDiskSpec (disk).cassoName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsStockBootDiskCached
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::IsStockBootDiskCached (StockBootDisk disk)
{
    error_code  ec;



    return fs::exists (GetStockBootDiskPath (disk), ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureStockBootDisk
//
//  Downloads the stock master into the cache when absent (already-cached
//  returns immediately with the path). Blocking, no UI of its own -- the
//  caller owns the consenting click and any progress or error reporting.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::EnsureStockBootDisk (
    StockBootDisk   disk,
    wstring       & outDiskPath,
    string        & outError)
{
    return DownloadStockBootDisk (GetStockBootDiskSpec (disk), GetDiskDirectory(), outDiskPath, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DownloadOptionalRom
//
//  Downloads a catalog ROM that no machine config requires (the Super Serial
//  Card's firmware) into its device folder under the asset base, replacing
//  any file already there. Blocking, with no UI of its own: the caller owns
//  the user's consent and the reporting.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::DownloadOptionalRom (
    string_view   cassoName,
    fs::path    & outPath,
    string      & outError)
{
    HRESULT          hr        = S_OK;
    const RomSpec  * spec      = FindRomSpec ("", cassoName);
    HINTERNET        hSession  = nullptr;
    bool             hasSource = (spec != nullptr) && !spec->altHost.empty();
    vector<Byte>     payload;
    error_code       ec;
    wstring          host;
    wstring          urlPath;



    outPath.clear();

    CBRF (hasSource,
          outError = format ("Casso has no download source for {}.", cassoName));

    host    = AsciiToWide (spec->altHost);
    urlPath = AsciiToWide (spec->altUrlPath);

    hSession = WinHttpOpen (s_kpszUserAgent,
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
    CBRF (hSession != nullptr,
          outError = "Cannot initialize WinHTTP session");

    hr = DownloadHttp (hSession,
                       host.c_str(),
                       urlPath.c_str(),
                       spec->expectedSize,
                       spec->cassoName,
                       payload,
                       outError);
    CHR (hr);

    hr = CheckRomSha256 (*spec, payload, outError);
    CHR (hr);

    outPath = GetAssetBaseDirectory() / string (spec->localRelDir) / string (spec->cassoName);
    fs::create_directories (outPath.parent_path(), ec);

    hr = WriteFileBytes (outPath, payload);
    CHRF (hr,
          outError = format ("Cannot write {}", outPath.string()));

Error:
    if (hSession != nullptr)
    {
        WinHttpCloseHandle (hSession);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMissingCardRom
//
////////////////////////////////////////////////////////////////////////////////

string AssetBootstrap::GetMissingCardRom (const string & device, const vector<fs::path> & searchPaths)
{
    const BlockCardSpec  * card = BlockDeviceCatalog::FindCard (device);
    const RomSpec        * spec = nullptr;
    string                 missing;



    if (card != nullptr && card->defaultFirmware != nullptr)
    {
        spec = FindRomSpec ("", card->defaultFirmware);
    }

    if (spec != nullptr && PathResolver::FindFile (searchPaths, fs::path (string (spec->localRelDir)) / spec->cassoName).empty())
    {
        missing = string (spec->cassoName);
    }

    return missing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOptionalRomInstalled
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::IsOptionalRomInstalled (string_view cassoName)
{
    const RomSpec  * spec      = FindRomSpec ("", cassoName);
    error_code       ec;
    fs::path         path;
    uintmax_t        size      = 0;
    bool             installed = false;



    if (spec != nullptr)
    {
        path      = GetAssetBaseDirectory() / string (spec->localRelDir) / string (spec->cassoName);
        size      = fs::file_size (path, ec);
        installed = !ec && size == spec->expectedSize;
    }

    return installed;
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



    outPcm.clear();

    CBRF (urlPath != nullptr,
          outError = "FetchAndDecodeOgg: null URL path");

    CBRF (targetSampleRate > 0,
          outError = "FetchAndDecodeOgg: target sample rate must be > 0");

    // Build a printable short name from the URL's last path component
    // so download errors are searchable in logs.
    {
        size_t   slash = 0;
        wstring  tail;

        wstring  wUrl (urlPath);
        slash = wUrl.find_last_of (L'/');
        tail = (slash == wstring::npos) ? wUrl : wUrl.substr (slash + 1);

        narrowName = TextEncoding::WideToNarrow (tail);
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
        oggBytes.data(),
        oggBytes.size(),
        shortPcm,
        srcRate,
        channels,
        outError);
    CHR (hr);

    // The raw OGG bytes are not needed past this point; release them
    // before allocating the float buffer (NFR-006 -- no `.ogg` files
    // on disk, no in-memory hold past decode).
    oggBytes.clear();
    oggBytes.shrink_to_fit();

    CBRF (channels >= 1 && channels <= 2,
          outError = format ("Unsupported channel count {} (only mono and stereo handled)",
                             channels));

    srcFrames = shortPcm.size() / channels;

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

        shortPcm.clear();
        shortPcm.shrink_to_fit();

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
        outPcm.clear();
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
    size_t      sampleCount   = pcm.size();
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
    bool        wroteWell     = false;



    CBRF (sampleRate > 0,
          outError = "WritePcmAsWav: sample rate must be > 0");

    fs::create_directories (outPath.parent_path(), ec);

    out.open (outPath, ios::binary | ios::trunc);

    wroteWell = out.good();
    CBRF (wroteWell,
          outError = format ("Cannot open {} for writing", outPath.string()));

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

    wroteWell = out.good();
    CBRF (wroteWell,
          outError = format ("Write error on {}", outPath.string()));

    out.close();

    // `fmtSizeWord` is unused on Windows builds; silence the analyzer
    // by referencing it.
    (void) fmtSizeWord;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRomDownloadFn
//
//  The fetch a ROM row of the download dialog runs: from the ROM's own host
//  when the catalog lists one, from AppleWin's resource folder otherwise,
//  with the dialog's progress and cancel flags wired into the transfer.
//
////////////////////////////////////////////////////////////////////////////////

static StartupAssetEntry::DownloadFn MakeRomDownloadFn (const RomSpec * spec, fs::path destPath)
{
    return [spec, destPath] (
        std::atomic<std::uint64_t> & bytesDone,
        std::atomic<bool>          & cancel,
        std::string                & err) -> HRESULT
    {
        HRESULT       hr      = S_OK;
        HINTERNET     hSes    = nullptr;
        vector<Byte>  payload;
        error_code    ecLocal;

        // Default AppleWin source, or the ROM's explicit alternate host
        // (e.g. the //c ROM on the apple2.org.za mirror).
        bool          useAlt  = !spec->altHost.empty();
        wstring       wHost   = useAlt ? AsciiToWide (spec->altHost) : wstring (s_kpszAppleWinHost);
        wstring       wPath   = useAlt
                                ? AsciiToWide (spec->altUrlPath)
                                : (wstring (s_kpszUrlPrefix) + AsciiToWide (spec->appleWinName));

        hSes = WinHttpOpen (s_kpszUserAgent,
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
        CBRF (hSes != nullptr, err = "Cannot initialize WinHTTP session");

        hr = DownloadHttp (hSes,
                           wHost.c_str(),
                           wPath.c_str(),
                           spec->expectedSize,
                           spec->cassoName,
                           payload,
                           err,
                           &bytesDone,
                           &cancel);
        CHR (hr);

        hr = CheckRomSha256 (*spec, payload, err);
        CHR (hr);

        fs::create_directories (destPath.parent_path(), ecLocal);
        hr = WriteFileBytes (destPath, payload);
        CHRF (hr, err = format ("Cannot write {}", destPath.string()));

    Error:
        if (hSes != nullptr)
        {
            WinHttpCloseHandle (hSes);
        }

        return hr;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  OfferOptionalRomDownload
//
//  The startup download dialog, opened for one catalog ROM that no machine
//  config requires: `title` and `intro` in place of the startup wording,
//  Cancel in place of Exit, and centered on `owner`. The row is locked on,
//  so the only choices are Download and Cancel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::OfferOptionalRomDownload (
    HWND                 owner,
    const IDxuiTheme   * theme,
    string_view          cassoName,
    const wstring      & title,
    const wstring      & intro,
    bool               & outDownloaded,
    string             & outError)
{
    HRESULT                  hr        = S_OK;
    const RomSpec          * spec      = FindRomSpec ("", cassoName);
    bool                           hasSource = (spec != nullptr) && !spec->altHost.empty();
    auto                           failure   = std::make_shared<string>();
    HINSTANCE                      hInst     = GetModuleHandleW (nullptr);
    StartupDownloadSet             set;
    StartupAssetEntry              entry;
    StartupDownloadPrompt          prompt;
    StartupDownloadResult          result    = StartupDownloadResult::Exit;
    StartupAssetEntry::DownloadFn  fetch;



    outDownloaded = false;
    outError.clear();

    CBRF (hasSource,
          outError = format ("Casso has no download source for {}.", cassoName));

    entry.kind          = StartupAssetKind::Rom;
    entry.groupLabel    = L"Firmware";
    entry.displayName   = AsciiToWide (spec->description);
    entry.kindLabel     = L"ROM";
    entry.source        = AsciiToWide (spec->sourceLabel);
    entry.selectable    = false;
    entry.selected      = true;
    entry.expectedBytes = (std::uint64_t) spec->expectedSize;
    entry.destPaths.push_back (GetAssetBaseDirectory() / string (spec->localRelDir) / string (spec->cassoName));

    // The dialog reports a failed row only as "Failed", so the reason is
    // copied out for the caller.
    fetch            = MakeRomDownloadFn (spec, entry.destPaths.front());
    entry.downloadFn = [fetch, failure] (
        std::atomic<std::uint64_t> & bytesDone,
        std::atomic<bool>          & cancel,
        std::string                & err) -> HRESULT
    {
        HRESULT  hrFetch = fetch (bytesDone, cancel, err);



        if (FAILED (hrFetch))
        {
            *failure = err;
        }

        return hrFetch;
    };

    set.entries.push_back (std::move (entry));

    prompt.title = title;
    prompt.intro = intro;
    prompt.theme = theme;

    result        = StartupDownloadDialog::Show (hInst, owner, "", L"", set, &prompt);
    outDownloaded = result == StartupDownloadResult::AllDone;

    if (result == StartupDownloadResult::PartialDone)
    {
        outError = failure->empty() ? string ("The download failed.") : *failure;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssetBootstrap::OfferMachineRoms
//
//  A ROM two of the machines have is listed once, under the first of them.
//  A machine with no configuration, or a ROM with no download source, is
//  passed over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssetBootstrap::OfferMachineRoms (
    HWND                    owner,
    const IDxuiTheme      * theme,
    const vector<wstring> & machineNames,
    const wstring         & title,
    const wstring         & intro,
    bool                  & outDownloaded,
    string                & outError)
{
    HRESULT                hr          = S_OK;
    HINSTANCE              hInst       = GetModuleHandleW (nullptr);
    vector<fs::path>       searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
    fs::path               baseDir     = GetAssetBaseDirectory();
    StartupDownloadSet     set;
    StartupDownloadPrompt  prompt;
    StartupDownloadResult  result      = StartupDownloadResult::NothingToDo;
    std::set<fs::path>     listed;



    outDownloaded = false;
    outError.clear();

    for (const wstring & machineName : machineNames)
    {
        string          narrowMachine = TextEncoding::WideToUtf8 (machineName);
        fs::path        configPath    = PathResolver::FindFile (searchPaths, fs::path ("Machines") / narrowMachine / (narrowMachine + ".json"));
        vector<string>  romFiles;
        string          jsonText;
        string          ignored;
        HRESULT         hrRoms        = S_OK;

        //  The machine's installed configuration, as this is also called
        //  from a program with no machine configurations among its resources.
        if (configPath.empty())
        {
            continue;
        }

        {
            std::ifstream  in (configPath, std::ios::binary);

            jsonText.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        hrRoms = MachineConfigLoader::CollectRomFiles (jsonText, romFiles, ignored);

        if (FAILED (hrRoms))
        {
            continue;
        }

        for (const string & romFile : romFiles)
        {
            const RomSpec    * spec    = FindRomSpec (narrowMachine, romFile);
            fs::path           relPath = (spec != nullptr) ? fs::path (string (spec->localRelDir)) / spec->cassoName : fs::path();
            StartupAssetEntry  entry;

            if (spec == nullptr || !PathResolver::FindFile (searchPaths, relPath).empty() || !listed.insert (relPath).second)
            {
                continue;
            }

            entry.kind          = StartupAssetKind::Rom;
            entry.groupLabel    = MachineDisplayName (narrowMachine) + L" ROMs";
            entry.displayName   = AsciiToWide (spec->description);
            entry.kindLabel     = L"ROM";
            entry.source        = spec->sourceLabel.empty() ? L"AppleWin (GitHub)" : AsciiToWide (spec->sourceLabel);
            entry.selectable    = false;
            entry.selected      = true;
            entry.expectedBytes = (std::uint64_t) spec->expectedSize;
            entry.destPaths.push_back (baseDir / relPath);
            entry.downloadFn    = MakeRomDownloadFn (spec, entry.destPaths.front());

            set.entries.push_back (std::move (entry));
        }
    }

    BAIL_OUT_IF (set.entries.empty(), S_OK);

    prompt.title = title;
    prompt.intro = intro;
    prompt.theme = theme;

    result        = StartupDownloadDialog::Show (hInst, owner, "", L"", set, &prompt);
    outDownloaded = result == StartupDownloadResult::AllDone;

    if (result == StartupDownloadResult::PartialDone)
    {
        outError = "A download failed.";
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetCatalogRom
//
////////////////////////////////////////////////////////////////////////////////

bool AssetBootstrap::TryGetCatalogRom (string_view cassoName, size_t & outSize, bool & outHasSha256)
{
    const RomSpec  * spec = FindRomSpec ("", cassoName);



    outSize      = spec != nullptr ? spec->expectedSize : 0;
    outHasSha256 = spec != nullptr && !spec->expectedSha256.empty();

    return spec != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddCardRomEntries
//
//  A row for each SmartPort or hard disk card ROM the machine's enabled
//  cards list and the asset folders don't have. Each can be unchecked: the
//  machine runs without the card, whose slot scan then passes over it.
//
////////////////////////////////////////////////////////////////////////////////

void AssetBootstrap::AddCardRomEntries (
    HINSTANCE                hInstance,
    const wstring          & machineName,
    const vector<fs::path> & searchPaths,
    const fs::path         & assetBaseDir,
    StartupDownloadSet     & set)
{
    HRESULT            hr     = S_OK;
    JsonValue          merged;
    string             error;
    const JsonValue  * slots  = nullptr;
    bool               hasArr = false;
    bool               loaded = false;



    hr     = LoadMergedConfig (hInstance, machineName, merged, error);
    loaded = SUCCEEDED (hr);
    BAIL_OUT_IF (!loaded, S_OK);

    hasArr = merged.HasArray ("slots", slots);
    BAIL_OUT_IF (!hasArr, S_OK);

    for (size_t i = 0; i < slots->GetArraySize(); i++)
    {
        const JsonValue      & slot    = slots->GetArrayElement (i);
        const BlockCardSpec  * card    = nullptr;
        const RomSpec        * spec    = nullptr;
        string                 device;
        string                 rom;
        bool                   enabled = true;
        StartupAssetEntry      entry;



        if (slot.GetType() != JsonType::Object || !slot.HasString ("device", device))
        {
            continue;
        }

        card = BlockDeviceCatalog::FindCard (device);

        if (card == nullptr || (slot.HasBool ("enabled", enabled) && !enabled))
        {
            continue;
        }

        rom  = slot.HasString ("rom", rom) ? rom : string (card->defaultFirmware);
        spec = FindRomSpec ("", rom);

        if (spec == nullptr || !PathResolver::FindFile (searchPaths, fs::path (string (spec->localRelDir)) / spec->cassoName).empty())
        {
            continue;
        }

        entry.kind          = StartupAssetKind::Rom;
        entry.groupLabel    = L"Card ROMs";
        entry.displayName   = AsciiToWide (spec->description);
        entry.kindLabel     = L"ROM";
        entry.source        = AsciiToWide (spec->sourceLabel);
        entry.selectable    = true;
        entry.selected      = true;
        entry.destPaths.push_back (assetBaseDir / string (spec->localRelDir) / spec->cassoName);
        entry.expectedBytes = (std::uint64_t) spec->expectedSize;
        entry.downloadFn    = MakeRomDownloadFn (spec, entry.destPaths.front());

        set.entries.push_back (std::move (entry));
    }

Error:
    return;
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
    bool                   & outUserExited,
    string                 & outError)
{
    HRESULT                hr              = S_OK;
    StartupDownloadSet     set;
    StartupDownloadResult  result          = StartupDownloadResult::NothingToDo;
    vector<string>         romFiles;
    string                 narrowMachine;
    bool                   audioIncluded   = false;
    bool                   refreshIncluded = false;
    error_code             ec;
    fs::path               devicesDir      = assetBaseDir / "Devices" / "DiskII";



    UNREFERENCED_PARAMETER (hInstance);

    outUserExited = false;

    narrowMachine = TextEncoding::WideToUtf8 (machineName);

    hr = GetRequiredRoms (hInstance, machineName, romFiles, outError);
    CHR (hr);

    for (const string & romFile : romFiles)
    {
        const RomSpec    * spec      = FindRomSpec (narrowMachine, romFile);
        fs::path           relPath;
        fs::path           found;
        StartupAssetEntry  entry;
        bool               isRefresh = false;

        CBRF (spec != nullptr,
              outError = format ("ROM '{}' is missing and Casso has no download "
                                 "URL for it. Place the file under {} and try again.",
                                 romFile, assetBaseDir.string()));

        relPath = fs::path (string (spec->localRelDir)) / spec->cassoName;
        found   = PathResolver::FindFile (searchPaths, relPath);

        // A ROM already on disk is satisfied -- unless it is a file Casso
        // itself installed here and has since updated, in which case a
        // replacement is offered. The machine boots on the old file, so
        // the offer can be skipped, and once skipped it is not repeated.
        if (!found.empty())
        {
            if (!FileMatchesSha256 (found, spec->supersededSha256) ||
                prefs.romRefreshConsent == "decline")
            {
                continue;
            }

            isRefresh       = true;
            refreshIncluded = true;
        }

        entry.kind          = StartupAssetKind::Rom;
        entry.groupLabel    = MachineDisplayName (narrowMachine) + L" ROMs";
        entry.displayName   = AsciiToWide (spec->description);
        entry.kindLabel     = L"ROM";
        entry.source        = spec->sourceLabel.empty()
                              ? L"AppleWin (GitHub)"
                              : AsciiToWide (spec->sourceLabel);
        entry.selectable    = isRefresh;
        entry.selected      = true;
        entry.destPaths.push_back (assetBaseDir / string (spec->localRelDir) / spec->cassoName);
        entry.expectedBytes = (std::uint64_t) spec->expectedSize;
        entry.downloadFn    = MakeRomDownloadFn (spec, entry.destPaths.front());

        set.entries.push_back (std::move (entry));
    }

    AddCardRomEntries (hInstance, machineName, searchPaths, assetBaseDir, set);

    if (considerDiskAudio && prefs.audioDownloadConsent != "decline")
    {
        for (string_view mechanism : s_kDiskAudioMechanisms)
        {
            StartupAssetEntry  entry;
            size_t             missingCount = 0;
            string             mechStr   (mechanism);
            wstring            mechW     = TextEncoding::NarrowToWide (mechStr);

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
                    fs::path                    mechDir      = devicesDir / string (spec.mechanism);
                    fs::path                    wavPath      = mechDir / string (spec.wavBasename);
                    vector<float>               pcm;
                    wstring                     urlPath;
                    bool                        fAborted     = false;
                    std::atomic<std::uint64_t>  perFileBytes {0};

                    if (spec.mechanism != mechStr)
                    {
                        continue;
                    }

                    if (fs::exists (wavPath, ecLocal))
                    {
                        continue;
                    }

                    BAIL_OUT_IF (cancel.load (std::memory_order_relaxed), E_ABORT);

                    urlPath  = s_kpszOpenEmulatorPathFmt;
                    urlPath += TextEncoding::NarrowToWide (string (spec.mechanism));
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
                                                            urlPath.c_str(),
                                                            44100,
                                                            pcm,
                                                            err,
                                                            &perFileBytes,
                                                            &cancel);

                    // Either the fetch reported an abort or the dialog's
                    // cancel flag went up mid-download; both end this entry.
                    fAborted = (hr == E_ABORT) ||
                               cancel.load (std::memory_order_relaxed);
                    BAIL_OUT_IF (fAborted, E_ABORT);

                    if (FAILED (hr))
                    {
                        DEBUGMSG (L"Drive audio: skipping %S (%s)\n",
                                  spec.oggBasename.data(),
                                  TextEncoding::NarrowToWide (err).c_str());
                        err.clear();
                        hr = S_OK;
                        continue;
                    }

                    fs::create_directories (mechDir, ecLocal);

                    hr = AssetBootstrap::WritePcmAsWav (wavPath, pcm, 44100, err);

                    if (FAILED (hr))
                    {
                        DEBUGMSG (L"Drive audio: write failed for %S (%s)\n",
                                  spec.wavBasename.data(),
                                  TextEncoding::NarrowToWide (err).c_str());
                        err.clear();
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

    BAIL_OUT_IF (set.entries.empty(), S_OK);

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

        if (refreshIncluded)
        {
            prefs.romRefreshConsent = "decline";
        }

        hr = S_OK;
        break;

    case StartupDownloadResult::Exit:
        // The user asked to quit rather than download. Nothing failed,
        // so the caller shuts down without an error box.
        outUserExited = true;
        hr            = S_OK;
        break;
    }

Error:
    return hr;
}
