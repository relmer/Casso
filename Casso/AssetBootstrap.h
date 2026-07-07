#pragma once

#include "Pch.h"

#include "Shell/DiskMru.h"





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

    // Returns the install root that contains (or should contain) the
    // per-machine `Machines/` and per-device `Devices/` subtrees. The
    // downloader places freshly fetched ROMs at
    // `<base>/Machines/<MachineName>/<RomName>` for machine-specific
    // ROMs and `<base>/Devices/DiskII/<RomName>` for shared Disk II
    // controller ROMs.
    static fs::path GetAssetBaseDirectory();

    static fs::path GetDiskDirectory();

    // Append disk images bundled in the source tree's Apple2/Demos
    // directory (present only in a repo build) to `mountable`, sorted and
    // de-duplicated against existing entries. No-op in an installed layout.
    static void     AppendBundledDemoDisks (std::vector<DiskMru::Entry> & mountable);

    // True if `p` is a disk under a git worktree checkout OTHER than the one
    // this build runs from. Used to keep the shared %LOCALAPPDATA% recent-disks
    // MRU from listing the same disk once per sibling worktree copy of the repo
    // -- an MRU entry under a worktree is shown only when the running exe lives
    // in that same worktree; entries outside any worktree always pass.
    static bool     IsForeignWorktreeDisk (const fs::path & p);

    static HRESULT  GetRequiredRoms       (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           vector<string>         & outRomFiles,
                                           string                 & outError);

    static HRESULT  HasDiskController     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           bool                   & outHasDiskController,
                                           string                 & outError);

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

    // Themed boot-disk picker. Lists the user's recent disk images
    // plus "Download" rows for the DOS 3.3 and ProDOS stock masters
    // (sourced from the Asimov archive). Always shown when the
    // machine has a Disk ][ controller and no boot disk has been
    // resolved yet, even when the MRU is empty -- the download rows
    // give a fresh install somewhere to go. Picking a row mounts it
    // (downloading on demand for the stock rows); Skip leaves the
    // slot empty.
    //
    // On return:
    //   outDiskPath = path to mount, or empty if the user skipped /
    //                 the machine has no Disk ][ controller.
    static HRESULT  PromptBootDiskMru     (HINSTANCE                hInstance,
                                           HWND                     hwndParent,
                                           const wstring          & machineName,
                                           const vector<DiskMru::Entry> & mruEntries,
                                           const fs::path         & diskDir,
                                           std::string_view         themeName,
                                           wstring                & outDiskPath,
                                           bool                   & outUserClosed,
                                           string                 & outError);

    // Themed runtime disk-insert picker. Mirrors PromptBootDiskMru
    // but is used when the user clicks a drive widget (or invokes
    // Disk -> Insert in drive N) on a running machine. Lists the
    // user's recent disk images plus "Download" rows for the DOS 3.3
    // and ProDOS stock masters so the picker is never empty even on
    // a fresh install. The "Browse..." footer button falls through to
    // the Win32 IFileOpenDialog for ad-hoc images. Cancel / close box
    // leaves the slot untouched.
    //
    // On return:
    //   outDiskPath  = path to mount, or empty if the user cancelled
    //                  or chose Browse (caller then runs IFileOpenDialog)
    //   outBrowse    = true if the user clicked Browse... (caller
    //                  should fall through to its file-picker path)
    static HRESULT  PromptInsertDiskMru   (HINSTANCE                hInstance,
                                           HWND                     hwndParent,
                                           int                      drive,
                                           const vector<DiskMru::Entry> & mruEntries,
                                           const fs::path         & diskDir,
                                           std::string_view         themeName,
                                           wstring                & outDiskPath,
                                           bool                   & outBrowse,
                                           string                 & outError);

    // Unified startup downloader. Inspects the current install for
    // every required-or-optional asset that's missing (ROMs from the
    // catalog, Disk II drive audio per mechanism) and presents a
    // SINGLE themed dialog letting the user accept or decline the
    // download in one decision. Downloads run on a worker thread with
    // live per-asset progress; the user can Exit at any point and
    // partial files are removed before this returns.
    //
    // Returns:
    //   S_OK       -> everything required is present (some optional
    //                 items may have failed or been skipped)
    //   S_FALSE    -> user chose Exit
    //   <0 HRESULT -> hard failure
    //
    // `prefs.audioDownloadConsent` is read AND updated to reflect the
    // user's choice (allow / decline). The caller is responsible for
    // flushing prefs to disk after this returns. Boot-disk selection is
    // not handled here -- it is owned solely by PromptBootDiskMru.
    static HRESULT  RunStartupDownloader  (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const vector<fs::path> & romSearchPaths,
                                           const fs::path         & assetBaseDir,
                                           bool                     considerDiskAudio,
                                           struct GlobalUserPrefs & prefs,
                                           string                 & outError);
};
