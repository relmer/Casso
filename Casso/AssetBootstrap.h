#pragma once

#include "Pch.h"





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

    static HRESULT  GetRequiredRoms       (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           vector<string>         & outRomFiles,
                                           string                 & outError);

    static HRESULT  HasDiskController     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           bool                   & outHasDiskController,
                                           string                 & outError);

    static HRESULT  CheckAndFetchRoms     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const vector<fs::path> & searchPaths,
                                           const fs::path         & assetBaseDir,
                                           std::string_view         themeName,
                                           string                 & outError);

    // Audio / FR-017 / FR-018. Inspects
    // `devicesDir`'s per-mechanism subdirectories (Alps/, Shugart/);
    // if either is missing any WAVs the user gets a GPL-3 disclosure
    // consent dialog. On accept, fetches the matching OGGs from
    // OpenEmulator's GitHub mirror, decodes them in memory with
    // stb_vorbis, and writes resampled 16-bit PCM WAVs to the
    // per-mechanism subdirs. Returns S_OK when at least one mechanism
    // is populated (or already was), S_FALSE if the user declined,
    // and an error HRESULT with `outError` set on hard failure.
    // Errors do not block startup (FR-009).
    //
    // `prefs.audioDownloadConsent` carries the persisted user choice
    // across launches ("ask" / "allow" / "decline"); the function
    // reads it to decide whether to prompt, and writes the user's new
    // choice back. The caller is responsible for flushing prefs to
    // disk after this returns.
    static HRESULT  CheckAndFetchDiskAudio (HINSTANCE                hInstance,
                                            const wstring          & machineName,
                                            HWND                     hwndParent,
                                            const fs::path         & devicesDir,
                                            struct GlobalUserPrefs & prefs,
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
    // loader (DiskIIAudioSource::LoadSamples) can pick up unchanged.
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
                                           const vector<fs::path> & mruEntries,
                                           const fs::path         & diskDir,
                                           std::string_view         themeName,
                                           wstring                & outDiskPath,
                                           bool                   & outUserClosed,
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
    // flushing prefs to disk after this returns.
    // `outBootDiskPath` (if non-empty on return) names the freshly
    // downloaded stock master disk; the caller should treat it as
    // disk1 for the impending machine boot. Empty if no boot-disk
    // entry was included or download did not finish.
    static HRESULT  RunStartupDownloader  (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const vector<fs::path> & romSearchPaths,
                                           const fs::path         & assetBaseDir,
                                           bool                     considerDiskAudio,
                                           bool                     offerBootDisk,
                                           const fs::path         & diskDir,
                                           struct GlobalUserPrefs & prefs,
                                           wstring                & outBootDiskPath,
                                           string                 & outError);
};
