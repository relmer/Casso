#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixer
//
//  Owns a registry of caller-owned IDriveAudioSource pointers and
//  produces interleaved stereo float PCM (L, R, L, R, ...) into a
//  host-supplied buffer per audio frame. The mixer:
//
//    * iterates registered sources
//    * asks each source for `numSamples` of mono PCM
//    * accumulates `mono * source->PanLeft()`  into the L lane
//      and    `mono * source->PanRight()` into the R lane
//
//  Per-channel clamping is the WASAPI integrator's job (after the
//  speaker has been summed in). FR-008 (no motor-hum dedup), FR-010
//  (additive sum), FR-012 (per-drive equal-power pan), FR-016
//  (generic over source type).
//
//  Named constants for per-drive panning live here so non-Disk-II
//  drive sources can reuse the same magnitudes.
//
////////////////////////////////////////////////////////////////////////////////

class DriveAudioMixer
{
public:
    // Equal-power "center" coefficient (= 1/sqrt(2) = sqrt(0.5)). When
    // applied to both L and R, a mono signal preserves total power
    // across the stereo image (panL^2 + panR^2 == 1.0). Used as the
    // centered-pan default for a lone drive source and as the //e
    // speaker's pan (FR-011 / FR-012).
    static constexpr float kSpeakerCenter  = 1.0f / (float) std::numbers::sqrt2;

    // Per-drive pan offset from center, in radians. Drive 1 sits at
    // theta = pi/4 + offset (left-biased); Drive 2 at pi/4 - offset
    // (right-biased). pi/8 places each drive halfway between its
    // corresponding speaker and the //e's centerline (FR-012).
    static constexpr float kDrivePanOffset = (float) std::numbers::pi / 8.0f;

    // Center pan angle (pi/4 radians). At this angle, equal-power
    // panning yields panL = panR = kSpeakerCenter. Used as the base
    // around which per-drive offsets are applied (FR-012).
    static constexpr float kCenterAngle    = (float) std::numbers::pi / 4.0f;

    DriveAudioMixer();
    ~DriveAudioMixer();

    // Register / unregister a caller-owned source. The mixer does NOT
    // take ownership.
    void  RegisterSource       (IDriveAudioSource * source);
    void  UnregisterSource     (IDriveAudioSource * source);
    void  UnregisterAllSources();

    // Toggle gating used by the View -> Options... -> Drive Audio
    // checkbox (FR-006). Default is enabled on construction.
    void  SetEnabled       (bool enabled);
    bool  IsEnabled        () const;

    // Disk II mechanism management (spec 005-disk-ii-audio Phase 14 /
    // FR-006 / SC-010). The mixer holds the asset-load context
    // (devices dir + sample rate) so SetMechanism can reload every
    // registered DiskIIAudioSource without the shell having to
    // re-iterate the source set. `SetSampleLoadContext` must be
    // called before any `SetMechanism` for the reload to take
    // effect. `GetMechanism` returns the currently-active mechanism
    // (default L"Shugart").
    void          SetSampleLoadContext (const wstring & devicesDir,
                                        uint32_t        sampleRate);
    HRESULT       SetMechanism         (const wstring & mechanism);
    const wstring & GetMechanism       () const { return m_mechanism; }
    bool          IsValidMechanism     (const wstring & mechanism) const;

    // Fill `stereoOut` (length 2 * numSamples) with interleaved L/R
    // float PCM. Always clears the buffer first; emits silence if
    // disabled or if no sources are registered (FR-015).
    void  GeneratePCM      (float * stereoOut, uint32_t numSamples);

    // Tick each registered source's per-frame state machine (used by
    // DiskIIAudioSource for step-vs-seek timing). Called from
    // WasapiAudio::SubmitFrame() just before GeneratePCM, with the
    // current CPU cycle count (FR-005).
    void  Tick             (uint64_t currentCycle);

    // Pure helper exposed for unit testing. Sums interleaved-stereo
    // drive PCM into interleaved-stereo speaker PCM in place,
    // clamping per channel to [-1, +1] (FR-010 / SC-006). Speaker
    // amplitude is the responsibility of the caller. driveStereo
    // == nullptr is a no-op (FR-011 speaker-only fast path).
    static void MixDriveIntoSpeakerStereo (
        float *         speakerStereo,
        const float *   driveStereo,
        uint32_t        numSamples);

    // Test introspection.
    size_t GetRegisteredSourceCount() const { return m_sources.size(); }

private:
    vector<IDriveAudioSource *> m_sources;
    vector<float>               m_scratchMono;
    bool                        m_enabled = true;

    // Phase 14 (FR-006 / SC-010): cached load context so
    // SetMechanism can reload every Disk II source without the host
    // having to re-iterate the source set. The mechanism defaults to
    // "Shugart" (the SA400 was the original Disk II drive shipped
    // by Apple), matching spec FR-006.
    wstring                     m_devicesDir;
    uint32_t                    m_loadSampleRate = 0;
    wstring                     m_mechanism      = L"Shugart";
};
