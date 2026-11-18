#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/StockBootDisks.h"
#include "Shell/DiskMru.h"

class JsonValue;





////////////////////////////////////////////////////////////////////////////////
//
//  MediaFilter
//
//  Which files of a folder a scan lists: disks, tapes, or both.
//
////////////////////////////////////////////////////////////////////////////////

using MediaFilter = bool (*) (const std::wstring & path);

class IDxuiTheme;
struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  AssetBootstrap
//
//  First-run asset wrangling for the Casso shell:
//   * Ensure a Machines/ directory exists (extract the embedded
//     defaults if the user has none).
//   * Detect missing ROM files for a given machine config and offer
//     to download them from the AppleWin project.
//
//  Runtime asset roots live under the user-writable
//  %LOCALAPPDATA%\Casso\ directory.
//
////////////////////////////////////////////////////////////////////////////////

class AssetBootstrap
{
public:

    static HRESULT  EnsureMachineConfigs  (HINSTANCE hInstance);

    // Overhaul : extract the three built-in
    // themes into `<assetBase>/Themes/<Name>/` on first launch
    // (and on subsequent launches whose embedded theme has bumped
    // `$cassoThemeVersion`). User-authored theme directories — i.e.
    // any whose `theme.json` does NOT carry `$cassoBuiltIn: true` —
    // are NEVER overwritten. The per-theme decision is delegated
    // to `ThemeBootstrapPlanner::Plan`.
    static HRESULT  EnsureThemes          (HINSTANCE hInstance);

    // Extract the embedded ImageWriter II sound set into
    // `<assetBase>/ImageWriter II Sounds/` on first launch. Each grain is
    // written only if missing (the files are read-only assets, never
    // user-edited), so subsequent launches are a cheap existence check.
    // Best-effort: a failed write leaves that grain absent and the printer
    // audio simply plays it as silence. CC BY 4.0 (Scott Lawrence / BleuLlama).
    static HRESULT  EnsureImageWriterSounds (HINSTANCE hInstance);

    // Hand the embedded symbol font to DirectWrite, so the open and closed
    // Apple codepoints render from it anywhere chrome names those keys.
    //
    // Unlike the assets above, nothing is written to disk: the font is
    // registered straight out of the module image. Chrome that has to say
    // which key a machine has cannot depend on an asset directory that a user
    // may have moved, and the file is under two kilobytes.
    //
    // Best-effort, like its neighbors. Chrome that draws the symbol instead
    // of the words asks DxuiTextRenderer::HasSymbolFont first and writes the
    // words when this failed, so a machine without the font still names the
    // key rather than showing a missing-glyph box that says nothing.
    static HRESULT  RegisterSymbolFont    (HINSTANCE hInstance);

    // Apply a theme's body face and text glow to the process default
    // typography, or to a window's. Best-effort: when the face's font is
    // missing from the module image, the system face is used.
    static HRESULT  ApplyThemeTypography  (HINSTANCE hInstance, const CassoTheme & theme);
    static HRESULT  ApplyThemeTypography  (HINSTANCE hInstance, const CassoTheme & theme, DxuiTextRenderer::Typography & typography);

    // Returns the install root that contains (or should contain) the
    // per-machine `Machines/` and per-device `Devices/` subtrees. The
    // downloader places freshly fetched ROMs at
    // `<base>/Machines/<MachineName>/<RomName>` for machine-specific
    // ROMs and `<base>/Devices/DiskII/<RomName>` for shared Disk II
    // controller ROMs.
    static fs::path GetAssetBaseDirectory();

    static fs::path  GetDiskDirectory  ();
    static fs::path  GetStateDirectory ();

    // Append disk images bundled in an Apple2/Demos directory to
    // `mountable`, sorted and de-duplicated against existing entries. A repo
    // build finds the source tree's; a release zip ships its own beside the
    // exe. No-op where neither is present.
    static void     AppendBundledDemoDisks (std::vector<DiskMru::Entry> & mountable,
                                            MediaFilter                   isWanted);

    // Append every supported disk image found in the folders that already
    // contain a `mountable` entry (i.e. the recent-disk folders), so a disk
    // sitting next to a recent one is offered in the picker without having
    // to be mounted first. Each MRU folder is scanned fresh; results are
    // sorted and de-duplicated against existing entries by filesystem
    // identity, and foreign-worktree disks are excluded. Call this BEFORE
    // AppendBundledDemoDisks so the scan set is exactly the MRU folders.
    //  The folders come from scanFrom -- every recent entry, disk or tape --
    //  so disks and tapes share one set of folders; isWanted picks which kind
    //  of file is listed.
    static void     AppendSiblingDisksFromMruFolders (std::vector<DiskMru::Entry>       & mountable,
                                                      const std::vector<DiskMru::Entry> & scanFrom,
                                                      MediaFilter                         isWanted);

    // The same scan over folders the caller chooses -- the shared known-folder
    // list -- with the same filtering and de-duplication.
    static void     AppendSiblingDisksFromFolders (const std::vector<fs::path>    & folders,
                                                   std::vector<DiskMru::Entry>    & mountable,
                                                   MediaFilter                      isWanted);

    // True if `p` is a disk belonging to a checkout of this repo OTHER than
    // the one this build runs from (a sibling .claude/worktrees/<name> copy,
    // or the main tree when the running exe lives in a worktree). Used to keep
    // the shared %LOCALAPPDATA% recent-disks MRU from listing disks that live
    // in a different checkout; disks outside the repo entirely (the user's own
    // folders, %LOCALAPPDATA%) always pass. See RepoCheckout.h for the pure,
    // unit-tested classification.
    static bool     IsForeignCheckoutDisk (const fs::path & p);

    static HRESULT  GetRequiredRoms       (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           vector<string>         & outRomFiles,
                                           string                 & outError);

    // The machine's name from its embedded config, such as "Apple //e";
    // the id when the config can't be read.
    static wstring  GetEmbeddedDisplayName (HINSTANCE hInstance, const wstring & machineName);

    static HRESULT  HasDiskController     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           bool                   & outHasDiskController,
                                           string                 & outError);

    // Whether the machine has slot 6's drives: an enabled Disk II card in
    // slot 6, or the //c's built-in drive.
    static HRESULT  HasSlot6Drives        (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           bool                   & outHasDrives,
                                           string                 & outError);
    static bool     HasSlot6Drives        (const JsonValue        & config);

    // In-memory OGG Vorbis fetch + decode + resample to mono float32
    // at `targetSampleRate`. Stereo input is downmixed to mono via
    // arithmetic average so the resulting buffer feeds Casso's
    // mono-per-source DriveAudioMixer without further channel work.
    // Linear interpolation is acceptable for drive noise (broadband,
    // not pitch-critical; see plan.md A-001). The compressed OGG
    // bytes are discarded before this function returns.
    static HRESULT  FetchAndDecodeOgg      (HINTERNET                hSession,
                                            LPCWSTR                  urlPath,
                                            uint32_t                 targetSampleRate,
                                            vector<float>          & outPcm,
                                            string                 & outError,
                                            std::atomic<std::uint64_t> * progressBytes  = nullptr,
                                            std::atomic<bool>          * cancelRequested = nullptr);

    // Write `pcm` as a 16-bit PCM mono WAV file to `outPath`. Clips
    // out-of-range samples to int16 (drive noise stays well inside
    // the range after the wrapper's downmix so clipping is for
    // safety, not for level). Used by the bootstrap path to bake
    // freshly decoded OGGs into WAVs that the existing IMFSourceReader
    // loader (Disk2AudioSource::LoadSamples) can pick up unchanged.
    static HRESULT  WritePcmAsWav          (const fs::path         & outPath,
                                            const vector<float>    & pcm,
                                            uint32_t                 sampleRate,
                                            string                 & outError);

    // Unified startup downloader. Inspects the current install for
    // every required-or-optional asset that's missing (ROMs from the
    // catalog, Disk II drive audio per mechanism) and presents a
    // SINGLE themed dialog letting the user accept or decline the
    // download in one decision. Downloads run on a worker thread with
    // live per-asset progress; the user can Exit at any point and
    // partial files are removed before this returns.
    //
    // Returns S_OK once everything required is present (some optional
    // items may have failed or been skipped), or a failure HRESULT.
    //
    // `outUserExited` is set when the user chose Exit in the dialog.
    // That is not a failure -- the caller is expected to shut down
    // quietly rather than report an error -- so it travels as an
    // out-param instead of riding on the result code.
    //
    // `prefs.audioDownloadConsent` is read AND updated to reflect the
    // user's choice (allow / decline). The caller is responsible for
    // flushing prefs to disk after this returns. Boot-disk selection is
    // StartupDiskPicker's.
    static HRESULT  RunStartupDownloader  (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const vector<fs::path> & romSearchPaths,
                                           const fs::path         & assetBaseDir,
                                           bool                     considerDiskAudio,
                                           struct GlobalUserPrefs & prefs,
                                           bool                   & outUserExited,
                                           string                 & outError);

    // The stock OS masters the create-disk flow installs from. Availability
    // is the download cache under GetDiskDirectory(); Ensure downloads on
    // demand (blocking, no UI -- the caller owns consent and reporting).
    using StockBootDisk = StockBootDisks::Which;

    static fs::path  GetStockBootDiskPath  (StockBootDisk disk);
    static bool      IsStockBootDiskCached (StockBootDisk disk);
    static HRESULT   EnsureStockBootDisk   (StockBootDisk disk,
                                            wstring     & outDiskPath,
                                            string      & outError);

    // A catalog ROM no machine config requires (the Super Serial Card's
    // firmware), downloaded on request into its device folder. Blocking, no
    // UI; `outPath` is the file written.
    static HRESULT   DownloadOptionalRom   (string_view   cassoName,
                                            fs::path    & outPath,
                                            string      & outError);

    // The startup download dialog for that ROM, with `title` and `intro` in
    // place of the startup wording and Cancel in place of Exit. Modal on
    // `owner`. `outDownloaded` is true when the ROM was written; a failed
    // download sets `outError` and returns S_OK, as Cancel does without it.
    static HRESULT   OfferOptionalRomDownload (HWND                 owner,
                                               const IDxuiTheme   * theme,
                                               string_view          cassoName,
                                               const wstring      & title,
                                               const wstring      & intro,
                                               bool               & outDownloaded,
                                               string             & outError);

    // The startup download dialog for every ROM the machines lack, each
    // machine a group, with `title` and `intro` in place of the startup
    // wording and Cancel in place of Exit. Modal on `owner`. `outDownloaded`
    // is true when every ROM was written, and false with nothing missing.
    static HRESULT   OfferMachineRoms (HWND                    owner,
                                       const IDxuiTheme      * theme,
                                       const vector<wstring> & machineNames,
                                       const wstring         & title,
                                       const wstring         & intro,
                                       bool                  & outDownloaded,
                                       string                & outError);

    // Whether that ROM is in its device folder under the asset base, at the
    // size the catalog lists.
    static bool      IsOptionalRomInstalled (string_view cassoName);

    // A device ROM's row in the download catalog: its size, and whether a
    // download is checked against a SHA-256. False when there is no row.
    static bool      TryGetCatalogRom      (string_view cassoName, size_t & outSize, bool & outHasSha256);

    // The catalog ROM a SmartPort or hard disk card runs by default, when
    // none of `searchPaths` has it. Empty when one does, or for a device
    // that isn't such a card.
    static string    GetMissingCardRom     (const string & device, const vector<fs::path> & searchPaths);

private:
    static HRESULT  LoadMergedConfig      (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           JsonValue              & outMerged,
                                           string                 & outError);

    // The startup download rows for the machine's missing card ROMs.
    static void     AddCardRomEntries     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           const vector<fs::path> & searchPaths,
                                           const fs::path         & assetBaseDir,
                                           struct StartupDownloadSet & set);
};
