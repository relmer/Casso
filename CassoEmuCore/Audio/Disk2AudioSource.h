#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"
#include "Audio/IDriveAudioEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AudioSource
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

class Disk2AudioSource : public IDriveAudioSource
{
public:
    // Per-sound default playback gains. The motor loop sits a touch below
    // unity; head and door one-shots play at full amplitude. The summed
    // drive bus can exceed 1.0 at peak, but the WASAPI integrator clamps
    // per channel after the speaker is mixed in, so no clipping math is
    // needed here. The live per-source gains (m_motorVolume / m_headVolume
    // / m_doorVolume) are user-adjustable via SetVolumes.
    static constexpr float    kMotorVolume        = 0.90f;
    static constexpr float    kHeadVolume         = 1.00f;
    static constexpr float    kDoorVolume         = 1.00f;

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

    Disk2AudioSource();
    ~Disk2AudioSource() override;

    // Asset loading. Decodes MotorLoop.wav, HeadStep.wav, HeadStop.wav,
    // DoorOpen.wav, DoorClose.wav at `targetSampleRate` mono float32
    // via IMFSourceReader. Per-file precedence (FR-019):
    //
    //   1. devicesDir/<filename>.wav (manual override)
    //   2. devicesDir/<mechanism>/<filename>.wav (per-mechanism)
    //   3. silent (FR-009)
    //
    // `devicesDir` is the absolute path to `Devices/DiskII/`,
    // `mechanism` is L"Shugart" / L"Alps" (no path separators).
    HRESULT  LoadSamples (const wchar_t * devicesDir,
                          const wchar_t * mechanism,
                          uint32_t        targetSampleRate);

    // IDriveAudioSource:
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  GetPanLeft() const override { return m_panLeft;  }
    float  GetPanRight() const override { return m_panRight; }
    void   SetPan (float panLeft, float panRight) override;

    // IDriveAudioSink:
    void   OnMotorEngaged() override;
    void   OnMotorDisengaged() override;
    void   OnHeadStep (int newQt) override;
    void   OnHeadBump() override;
    void   OnDiskInserted() override;
    void   OnDiskEjected() override;

    //  A disk was replaced under the running machine: the door open sound then
    //  the close sound, back to back, with the disk present throughout. Not on
    //  IDriveAudioSink because only the shared-image pick-up path reaches it.
    void   OnDiskSwapped();

    // Called once per audio frame by DriveAudioMixer with the current
    // CPU cycle (FR-005 idle timeout).
    void   Tick (uint64_t currentCycle);

    // Sets the per-sound playback gains (0..1). The motor loop, head
    // one-shots, and door one-shots are scaled by these on mix. Called
    // on the CPU thread (same thread that mixes), so no synchronization
    // is required. Values are clamped to [0, 1].
    void   SetVolumes (float motor, float head, float door);

    // On-demand audition. Identifies which sound a test/play button
    // should preview.
    enum class TestSoundKind
    {
        Motor,
        Head,
        Door,
    };

    // Plays a one-shot of the requested sound through a dedicated test
    // channel, independent of the live emulation audio state, at the
    // current per-sound volume and the source's current pan. Used by the
    // settings panel's play buttons to audition a sound / stereo position.
    // CPU-thread only (same thread that mixes).
    void   PlayTestSound (TestSoundKind kind);

    // Spec-006 (FR-022, FR-025): attach an audio-decision sink so
    // the debug window can show what the audio path actually did at
    // each controller-event delivery. A nullptr sink (the default)
    // leaves audio output byte-identical to the pre-feature path.
    void   SetAudioEventSink (IDriveAudioEventSink * sink) noexcept
    {
        m_audioEventSink = sink;
    }

    // Spec-006 bug fix. Stamp the 0-based drive index this source
    // represents so audio-decision events report the correct drive.
    void   SetDriveIndex (int driveIndex) noexcept
    {
        m_driveIndex = driveIndex;
    }

    // Test-only seam: inject a sample buffer directly without touching
    // the host filesystem. Slot key matches the WAV filename without
    // ".wav" (e.g., "MotorLoop", "HeadStep", "HeadStop", "DoorOpen",
    // "DoorClose"). Used by UnitTest/Audio to avoid IMFSourceReader.
    void   SetSampleBufferForTest (
        const wchar_t *       slot,
        vector<float> &&      samples);

    // Test introspection.
    bool   IsMotorRunning() const { return m_motorRunning; }
    bool   IsDiskPresent() const { return m_diskPresent; }
    bool   IsSeekMode() const { return m_seekMode; }
    uint64_t GetLastStepCycle() const { return m_lastStepCycle; }

    // Test introspection for the boot-recalibrate ratchet (see OnHeadBump).
    uint32_t GetRatchetSlot() const { return m_ratchetSlot; }

private:
    void   MixMotor (float * out, uint32_t n);
    void   MixHead (float * out, uint32_t n);
    void   MixDoor (float * out, uint32_t n);
    void   MixTest (float * out, uint32_t n);

    // Starts a head one-shot on `buf` and fires the matching audio-event
    // (Started / Restarted / Silent). Shared by the bump and ratchet paths.
    void   TriggerHeadShot (
        SoundKind             kind,
        const vector<float> * buf,
        bool                  previousStillPlaying);

    // Pan (equal-power, precomputed by SetPan).
    float                 m_panLeft   = IDriveAudioSource::kCenterPan;
    float                 m_panRight  = IDriveAudioSource::kCenterPan;

    // Live per-sound playback gains (0..1), defaulting to the documented
    // attenuation constants. Adjustable at runtime via SetVolumes.
    float                 m_motorVolume = kMotorVolume;
    float                 m_headVolume  = kHeadVolume;
    float                 m_doorVolume  = kDoorVolume;

    // Motor loop.
    vector<float>         m_motorBuf;
    uint32_t              m_motorPos     = 0;
    bool                  m_motorRunning = false;

    // Disk presence (spec-006). The motor loop sample includes media
    // noise (read-head whirring against the cookie), so it is only
    // audible while a disk is mounted. An empty drive with the motor
    // commanded on is silent on real hardware. EmulatorShell drives
    // this via OnDiskInserted / OnDiskEjected. Defaults to false so
    // a fresh source with no insert notification stays silent.
    bool                  m_diskPresent  = false;

    // Head one-shot (points at m_stepBuf during a normal step, at
    // m_stopBuf for a track-0 / max-track bump). nullptr means no
    // shot is currently playing.
    vector<float>         m_stepBuf;
    vector<float>         m_stopBuf;
    const vector<float> * m_headBuf = nullptr;
    uint32_t              m_headPos = 0;

    // How far into m_headBuf this gesture is allowed to play, in samples.
    //
    // HeadStep.wav is not one step. Measured, both shipped mechanisms are a
    // SUSTAINED RATTLE: Alps runs 322 ms and is nearly twice as loud in the
    // middle as at its start, Shugart runs 413 ms at a flat level end to end,
    // and neither has discrete impulses at 1 ms resolution. They are
    // recordings of a seek, not of a step.
    //
    // So a step does not play the clip. It buys a SLICE of it, sized by how
    // far the head actually moved, and the slice accumulates while the seek
    // continues. A one-track move gets a blip; crossing the disk gets the
    // whole recording. Because the texture is uniform, an arbitrary length
    // out of it still sounds like a seek that lasted that long.
    uint32_t              m_headLimit = 0;

    // Where the current gesture started. The release ramp is a fraction of
    // what this gesture PLAYS, so it needs the slice's length, not the
    // absolute end index -- with a lead-in those differ, and sizing the ramp
    // off the end index made it as long as the whole slice.
    uint32_t              m_headSliceStart = 0;

    // Quarter-track the last step reported, so the next one yields a
    // distance. -1 means no step has been seen, which is not the same as a
    // step at quarter-track 0.
    int                   m_lastStepQt = -1;

    // Full stroke of a 35-track disk in quarter-tracks. The clip is treated
    // as one full-stroke seek, so this is the divisor that turns a distance
    // into a fraction of the recording.
    static constexpr int  kFullStrokeQuarterTracks = 140;

    // Where a slice STARTS, as a percent of the clip.
    //
    // Both shipped recordings open with roughly 10 ms of quiet before the
    // rattle settles, and a one-track seek buys about 9 ms. Starting every
    // gesture at sample 0 therefore served short seeks almost nothing but the
    // lead-in: measured against each clip's own body, a one-track move came
    // out at 0.27 of level on Alps and 0.51 on Shugart. Short moves were not
    // just short, they were faint.
    //
    // Measured level of a one-track slice against each clip's own body, by
    // where the slice starts:
    //
    //              0%     4%     8%    12%
    //     Alps    0.24   0.65   0.73   1.20
    //     Shugart 0.58   1.33   0.95   1.34
    //
    // Alps ramps for about 40 ms, so four percent was not enough for it even
    // though it was plenty for Shugart. Twelve puts both at or above the
    // level of their own sustained middle, and costs a full-stroke seek only
    // the part of the recording that was ramping in anyway.
    static constexpr int  kHeadLeadInPercent = 12;

    // Fade applied at the end of a slice. The cut lands in the middle of a
    // rattle, and stopping a waveform mid-cycle is a click. Short enough not
    // to soften the stop, long enough to remove the discontinuity.
    static constexpr uint32_t  kHeadReleaseSamples = 128;

    // Door one-shot. Points at m_doorCloseBuf on insert, m_doorOpenBuf
    // on eject. nullptr means no door sound playing.
    vector<float>         m_doorOpenBuf;
    vector<float>         m_doorCloseBuf;
    const vector<float> * m_doorBuf = nullptr;
    uint32_t              m_doorPos = 0;

    // A swap plays the open one-shot and then the close one-shot back to back,
    // because a disk came out and another went in. Set by OnDiskSwapped and
    // cleared when MixDoor rolls the open sample into the close one -- the door
    // buffer is a single one-shot, so the second sound is queued by this flag
    // rather than by a second buffer.
    bool                  m_doorThenClose = false;

    // Dedicated audition channel for the settings-panel play buttons. A
    // one-shot that plays a chosen buffer once through at m_testVolume,
    // independent of the live motor/head/door emulation state so a
    // preview never disturbs (or is disturbed by) real disk activity.
    const vector<float> * m_testBuf    = nullptr;
    uint32_t              m_testPos    = 0;
    float                 m_testVolume = 0.0f;

    // Step-vs-seek discriminator (FR-005).
    uint64_t              m_lastStepCycle = 0;
    uint64_t              m_currentCycle  = 0;
    bool                  m_seekMode      = false;

    // Boot-recalibrate ratchet. When the head is pinned against the
    // track-0 stop, the controller fires a steady ~52 Hz stream of bumps
    // (one per phase-on, ~19,690 cycles apart). Rendering every one as a
    // HeadStop thunk smears into a continuous "fast buzz". A real Disk II
    // recalibrate instead sounds like a slow machine gun: the ratcheting
    // mechanism produces a grouped [thunk, pause, click, click] cadence.
    // We reproduce that by cycling rapid consecutive bumps through a
    // 4-slot pattern -- a thunk, a silent rhythmic pause, then two step
    // clicks -- yielding a 2:1 click-to-thunk ratio at ~38 Hz effective.
    static constexpr uint32_t kRatchetPeriod     = 4;
    static constexpr uint32_t kRatchetSlotThunk  = 0;
    static constexpr uint32_t kRatchetSlotSilent = 1;

    uint32_t              m_ratchetSlot      = 0;
    bool                  m_lastEventWasBump = false;

    // Spec-006 audio-decision sink (FR-022 / FR-025). Optional.
    IDriveAudioEventSink * m_audioEventSink = nullptr;

    // Spec-006 bug fix. The owning shell stamps the 0-based drive
    // index on construction so the audio source can report the
    // correct drive on every IDriveAudioEventSink fire. Default 0
    // keeps legacy tests / single-drive configs working.
    int                    m_driveIndex     = 0;
};
