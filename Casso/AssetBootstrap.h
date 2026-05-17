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
//  Both helpers honor the existing repo layout when present (a
//  Machines/ or ROMs/ directory found via PathResolver search paths
//  is reused). Otherwise assets are placed next to casso.exe.
//
////////////////////////////////////////////////////////////////////////////////

class AssetBootstrap
{
public:

    static HRESULT  EnsureMachineConfigs  (HINSTANCE                hInstance,
                                           const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);

    // Returns the install root that contains (or should contain) the
    // per-machine `Machines/` and per-device `Devices/` subtrees. The
    // downloader places freshly fetched ROMs at
    // `<base>/Machines/<MachineName>/<RomName>` for machine-specific
    // ROMs and `<base>/Devices/DiskII/<RomName>` for shared Disk II
    // controller ROMs.
    static fs::path GetAssetBaseDirectory (const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);

    static fs::path GetDiskDirectory      (const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);

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
                                           string                 & outError);

    // Per spec 005-disk-ii-audio Phase 13 / FR-017 / FR-018. Inspects
    // `devicesDir`'s per-mechanism subdirectories (Alps/, Shugart/);
    // if either is missing any WAVs the user gets a GPL-3 disclosure
    // consent dialog. On accept, fetches the matching OGGs from
    // OpenEmulator's GitHub mirror, decodes them in memory with
    // stb_vorbis, and writes resampled 16-bit PCM WAVs to the
    // per-mechanism subdirs. Returns S_OK when at least one mechanism
    // is populated (or already was), S_FALSE if the user declined,
    // and an error HRESULT with `outError` set on hard failure.
    // Errors do not block startup (FR-009).
    static HRESULT  CheckAndFetchDiskAudio (HINSTANCE                hInstance,
                                            const wstring          & machineName,
                                            HWND                     hwndParent,
                                            const fs::path         & devicesDir,
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
                                            string                 & outError);

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

    static HRESULT  OfferBootDiskDownload (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const fs::path         & diskDir,
                                           wstring                & outDiskPath,
                                           string                 & outError);
};
