#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIAudioSource
//
//  Concrete IDriveAudioSource for the Disk II 5.25" drive. Owns the
//  per-drive sample buffers (MotorLoop, HeadStep, HeadStop, DoorOpen,
//  DoorClose), the looping motor playback position, the head and door
//  one-shot positions, and the step-vs-seek discriminator.
//
//  All event hooks (IDriveAudioSink methods) mutate state only; PCM
//  is produced lazily in GeneratePCM(). Same-thread state model per
//  NFR-002 -- no locks, no atomics.
//
//  Spec reference: FR-001..FR-005, FR-009, FR-012, FR-013, FR-014.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIAudioSource : public IDriveAudioSource
{
public:
    // Per-sound attenuation (sums fall safely under 1.0 even with
    // speaker at full deflection -- see plan.md "Mixing Math").
    static constexpr float    kMotorVolume        = 0.25f;
    static constexpr float    kHeadVolume         = 0.30f;
    static constexpr float    kDoorVolume         = 0.30f;

    // OpenEmulator-derived seek-vs-step threshold. 16,368 cycles
    // ~= 16 ms at the //e's 1.023 MHz CPU clock. If a step arrives
    // within this window of the previous step, the source enters
    // seek mode: the running head one-shot is not restarted, so
    // back-to-back steps audibly fuse into a buzz rather than
    // re-triggering N overlapping clicks (FR-005).
    static constexpr uint64_t kSeekThresholdCycles = 16368;

    // Auto-clear seek mode if no new step has arrived in ~50 ms
    // (51,150 cycles). Bounds the seek-state lifetime so a long idle
    // between disk operations resets cleanly (FR-005).
    static constexpr uint64_t kHeadIdleCycles      = 51150;

    DiskIIAudioSource();
    ~DiskIIAudioSource() override;

    // Asset loading. Decodes MotorLoop.wav, HeadStep.wav, HeadStop.wav,
    // DoorOpen.wav, DoorClose.wav at `targetSampleRate` mono float32
    // via IMFSourceReader. Per-file precedence (FR-019):
    //
    //   1. devicesDir/<filename>.wav                 (manual override)
    //   2. devicesDir/<mechanism>/<filename>.wav      (per-mechanism)
    //   3. silent                                    (FR-009)
    //
    // `devicesDir` is the absolute path to `Devices/DiskII/`,
    // `mechanism` is L"Shugart" / L"Alps" (no path separators).
    HRESULT  LoadSamples (const wchar_t * devicesDir,
                          const wchar_t * mechanism,
                          uint32_t        targetSampleRate);

    // IDriveAudioSource:
    void   GeneratePCM   (float * outMono, uint32_t numSamples) override;
    float  PanLeft       () const override { return m_panLeft;  }
    float  PanRight      () const override { return m_panRight; }
    void   SetPan        (float panLeft, float panRight) override;

    // IDriveAudioSink:
    void   OnMotorStart   () override;
    void   OnMotorStop    () override;
    void   OnHeadStep     (int newQt) override;
    void   OnHeadBump     () override;
    void   OnDiskInserted() override;
    void   OnDiskEjected  () override;

    // Called once per audio frame by DriveAudioMixer with the current
    // CPU cycle (FR-005 idle timeout).
    void   Tick           (uint64_t currentCycle);

    // Test-only seam: inject a sample buffer directly without touching
    // the host filesystem. Slot key matches the WAV filename without
    // ".wav" (e.g., "MotorLoop", "HeadStep", "HeadStop", "DoorOpen",
    // "DoorClose"). Used by UnitTest/Audio to avoid IMFSourceReader.
    void   SetSampleBufferForTest (
        const wchar_t *       slot,
        vector<float> &&      samples);

    // Test introspection.
    bool   IsMotorRunning() const { return m_motorRunning; }
    bool   IsSeekMode     () const { return m_seekMode; }
    uint64_t GetLastStepCycle() const { return m_lastStepCycle; }

private:
    void   MixMotor (float * out, uint32_t n);
    void   MixHead  (float * out, uint32_t n);
    void   MixDoor  (float * out, uint32_t n);

    // Pan (equal-power, precomputed by SetPan).
    float                 m_panLeft   = IDriveAudioSource::kCenterPan;
    float                 m_panRight  = IDriveAudioSource::kCenterPan;

    // Motor loop.
    vector<float>         m_motorBuf;
    uint32_t              m_motorPos     = 0;
    bool                  m_motorRunning = false;

    // Head one-shot (points at m_stepBuf during a normal step, at
    // m_stopBuf for a track-0 / max-track bump). nullptr means no
    // shot is currently playing.
    vector<float>         m_stepBuf;
    vector<float>         m_stopBuf;
    const vector<float> * m_headBuf = nullptr;
    uint32_t              m_headPos = 0;

    // Door one-shot. Points at m_doorCloseBuf on insert, m_doorOpenBuf
    // on eject. nullptr means no door sound playing.
    vector<float>         m_doorOpenBuf;
    vector<float>         m_doorCloseBuf;
    const vector<float> * m_doorBuf = nullptr;
    uint32_t              m_doorPos = 0;

    // Step-vs-seek discriminator (FR-005).
    uint64_t              m_lastStepCycle = 0;
    uint64_t              m_currentCycle  = 0;
    bool                  m_seekMode      = false;
};
