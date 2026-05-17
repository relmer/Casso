# Implementation Plan: Disk II Audio

**Branch**: `feature/005-disk-ii-audio` | **Date**: 2026-03-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/005-disk-ii-audio/spec.md`
**Research basis**: [`research.md`](./research.md) — source-verified survey of OpenEmulator, MAME, AppleWin, and the Casso codebase
**Constitution**: `.specify/memory/constitution.md`

## Summary

Add realistic Disk II mechanical audio (motor hum, head-step clicks,
track-0 bumps, disk insert/eject) to Casso's //e emulator and mix it
into the existing WASAPI pipeline alongside the speaker, in stereo with
per-drive panning. The implementation is structured around a new
`DriveAudioMixer` that owns a collection of `IDriveAudioSource` instances
(one per attached drive). v1 ships one concrete source — `DiskIIAudioSource`
— driven by an `IDriveAudioSink` event interface that the Disk II
controller fires at the precise points where head, motor, and disk-mount
state changes occur. The mixer is consumed once per audio frame by
`WasapiAudio::SubmitFrame()`, which mixes drive PCM additively (stereo,
per-channel) into the same float buffer that the speaker writes (speaker
is centered, drives are panned). A "Drive Audio" check toggle in a new
**View → Options...** dialog (default on) gates the mixer.

The abstraction is intentionally generic: future drive types (//c internal
5.25, DuoDisk, Apple 5.25 Drive, Apple /// drive, ProFile) implement
`IDriveAudioSource` and register with the same mixer; the mixer, sink
interface, and WASAPI plumbing remain untouched.

The design is grounded in `research.md` §4–5 (Casso-specific implementation
sketch), which fingerprints exact line ranges in
`DiskIIController.{h,cpp}`, `WasapiAudio.{h,cpp}`, and `EmulatorShell.cpp`
where touch points are required.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK + STL only — Windows MediaFoundation
(`IMFSourceReader`) for WAV decoding and resampling, WinHTTP for the
first-run bootstrap fetch (already in use by `AssetBootstrap`), and a
single in-tree third-party header: **stb_vorbis.c** (public domain / MIT,
~5500 lines, single file) for in-memory OGG Vorbis decoding of OpenEmulator's
upstream sample assets.
**Storage**: Bootstrap-downloaded WAVs land at
`Devices/DiskII/Shugart/{MotorLoop,HeadStep,HeadStop,DoorOpen,DoorClose}.wav`
and `Devices/DiskII/Alps/{MotorLoop,HeadStep,HeadStop}.wav`. Optional
per-file user override at `Devices/DiskII/<filename>.wav` (top-level,
FR-019). No `.ogg` files retained on disk (NFR-006). This feature also
performs a one-time directory restructure: top-level `ROMs/` migrates to
`Machines/<MachineName>/<MachineName>.rom` (each machine config and its
ROMs share a directory), and `Devices/DiskII/` gains the Disk II
controller ROMs (`Disk2.rom`, `Disk2_13Sector.rom`) alongside the audio
subdirs. `Assets/` stays as-is for README screenshots. `.gitignore` flips
to a whitelist-JSON-only rule under `Machines/**` and `Devices/**`,
automatically covering every bootstrap-downloaded binary regardless of
extension.
**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New
`DriveAudioMixerTests.cpp`, `DiskIIAudioSourceTests.cpp`, plus extensions
to existing `DiskIIControllerTests.cpp` to cover the new event firing
(including disk insert/eject).
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Existing 5-project solution; new code in `CassoEmuCore`
(library) and `Casso` (GUI shell). No new project.
**Performance Goals**: Adding drive-audio mixing MUST NOT measurably
increase per-frame WASAPI submit cost; the inner loop is a small fixed
number of float multiply-adds per active sample stream per channel (≤ 6
simultaneous streams in v1: motor + head + door per drive, up to 2 drives).
**Constraints**: No regression in speaker pipeline (FR-011, SC-006). No
buffer underruns (NFR-001). Same-thread state model — no locks introduced
in v1 (NFR-002, with explicit revisit point if expensive DSP arrives).
Asset footprint is advisory (NFR-003, revisit at >1 MB compressed).
GPL-3 drive-audio samples MUST NOT be committed (NFR-004). Real-hardware
recordings preferred over synthesis when permissively licensed (NFR-005).
**Scale/Scope**: ~5 new source files (~700–900 LOC), ~6 touched files,
~25–40 new test cases. New Options dialog scaffold (small).

## Constitution Check

### Principle I — Code Quality (NON-NEGOTIABLE)

| Rule                                              | How this plan complies                                                                                                                  |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| Pch.h-first include                               | Every new `.cpp` includes `"Pch.h"` as the first include. No angle-bracket includes outside `Pch.h`. All STL/Windows headers go via `Pch.h`. |
| Formatting Preservation                           | All edits to existing files are surgical insertions at the line ranges named in research §5.3; no existing aligned blocks are touched.  |
| EHM on fallible functions                         | `DiskIIAudioSource::LoadSamples`, WAV-decode helpers, and any MediaFoundation calls use the standard `HRESULT hr; … Error: … return hr;` pattern. |
| No calls inside macro arguments                   | Store results first; reviewed per PR.                                                                                                   |
| Single exit via `Error:` label                    | Standard pattern in every new fallible function.                                                                                        |
| Avoid Nesting (≤ 2-3 indent beyond EHM)           | `GeneratePCM()` factored into `MixMotor()`, `MixHead()`, `MixDoor()` helpers, each ≤ 2 levels.                                          |
| Variable Declarations at Top of Scope             | All new functions follow this; column-aligned per project rules.                                                                        |
| Function Comments in `.cpp` Only                  | Headers carry only declaration comments; doc blocks live in `.cpp` with 80-`/` delimiters.                                              |
| Function Spacing — `func()` vs `func (a, b)`      | Verified via `rg -n '\w \(\)' CassoEmuCore/Audio/ Casso/` before commit on every PR phase.                                              |
| Smart Pointers                                    | `DriveAudioMixer` is owned by `EmulatorShell` directly (composition); no smart pointer needed. Sample buffers are `std::vector<float>`. |
| PascalCase file names                             | All new source files (`DriveAudioMixer.{h,cpp}`, `DiskIIAudioSource.{h,cpp}`, `IDriveAudioSink.h`, `IDriveAudioSource.h`, `OptionsDialog.{h,cpp}`) and asset files (`MotorLoop.wav`, `HeadStep.wav`, `HeadStop.wav`, `DoorOpen.wav`, `DoorClose.wav`) use PascalCase with no underscores. |
| No magic numbers                                  | `kMotorVolume = 0.25f`, `kHeadVolume = 0.30f`, `kDoorVolume = 0.30f`, `kSpeakerCenter = 0.7071f` (= √0.5), `kDrivePanOffset = 0.3927f` (= π/8 radians), `kSeekThresholdCycles = 16368`, `kHeadIdleCycles = 51150` are all named constants. |

### Principle II — Test Isolation (NON-NEGOTIABLE)

Drive-audio tests run on the same headless harness as existing
`CassoEmuCore` tests. `DriveAudioMixer` and `DiskIIAudioSource` are
constructed with caller-owned in-memory sample buffers; tests never read
host filesystem. The `IDriveAudioSink` interface lets `DiskIIController`
tests substitute a recording mock with no audio device involved.

### Principle V — Function Size & Structure

`GeneratePCM()` is the only function with appreciable logic. `DriveAudioMixer::GeneratePCM` factors as:

```
GeneratePCM (stereoOut, n)
  Clear (stereoOut, 2*n)
  if !m_enabled: return
  for each registered source:
      source.GeneratePCM (sourceMono, n)           // ≤ 25 LOC inside source
      ApplyPanAndMix (stereoOut, sourceMono, n,    // ≤ 10 LOC
                      source.PanLeft, source.PanRight)
  // No motor-hum dedup — see FR-008 (independent per-drive sum,
  // bounded by equal-power panning + phase incoherence).
```

`DiskIIAudioSource::GeneratePCM` (mono output):
```
GeneratePCM (out, n)
  MixMotor (out, n)                                // ≤ 15 LOC
  MixHead  (out, n)                                // ≤ 20 LOC
  MixDoor  (out, n)                                // ≤ 15 LOC
  UpdateSeekState (currentCycle)                   // ≤ 10 LOC
```

All helpers stay under ~25 LOC.

## Architecture

### Components

```
                 ┌─────────────────────┐
                 │  DiskIIController   │  (CassoEmuCore/Devices)
                 │   ─ HandleSwitch    │──┐
                 │   ─ HandlePhase     │  │ IDriveAudioSink*
                 │   ─ Tick (spindown) │  │
                 │   ─ NotifyInsert    │  │ OnMotorStart()
                 │   ─ NotifyEject     │  │ OnMotorStop()
                 └─────────────────────┘  │ OnHeadStep(qt)
                                          │ OnHeadBump()
                                          │ OnDiskInserted()
                                          │ OnDiskEjected()
                                          ▼
                 ┌──────────────────────────────────────────┐
                 │ DiskIIAudioSource : IDriveAudioSource    │
                 │                     : IDriveAudioSink    │ (CassoEmuCore/Audio)
                 │   ─ LoadSamples / SynthesizeSamples      │
                 │   ─ panLeft, panRight (per-drive)        │
                 │   ─ GeneratePCM (out, n)  → mono floats  │
                 │      ├── MixMotor                        │
                 │      ├── MixHead (step + bump + seek)    │
                 │      └── MixDoor (open/close one-shots)  │
                 └──────────────────────────────────────────┘
                            ▲                       │
                            │ registers             │ mono PCM
                            │                       ▼
                 ┌──────────┴─────────────────────────────────┐
                 │ DriveAudioMixer                            │  (CassoEmuCore/Audio)
                 │   ─ RegisterSource (IDriveAudioSource*)    │
                 │   ─ SetEnabled                             │
                 │   ─ GeneratePCM (stereoOut, n)             │◄── called from WasapiAudio
                 │      └── for each source: pan+sum→stereo   │
                 │          (no dedup; FR-008, FR-012)        │
                 └────────────────────────────────────────────┘
                            ▲                       │
                            │ owns                  │ stereo float* diskBuf
                            │                       ▼
                 ┌──────────┴───────────┐   ┌──────────────────────────┐
                 │   EmulatorShell      │──►│   WasapiAudio            │
                 │   ─ owns mixer       │   │   ─ SubmitFrame (..., mix)│
                 │   ─ owns Options dlg │   │     ├ GeneratePCM(speaker)│
                 │   ─ owns Disk II     │   │     │   → center stereo  │
                 │     audio sources    │   │     ├ GeneratePCM(mix)   │
                 │   ─ SetAudioSink     │   │     │   → stereo         │
                 │     (controller →    │   │     └ additive mix +     │
                 │      source)         │   │       clamp per channel  │
                 │   ─ NotifyInsert/    │   └──────────────────────────┘
                 │     Eject from UI    │
                 └──────────────────────┘
                            ▲
                            │ View menu  → Options... → "Drive Audio" check
                            │             → mixer.SetEnabled (checked)
                            ▼
                 ┌──────────────────────┐
                 │   OptionsDialog      │  (Casso/OptionsDialog.{h,cpp})
                 │   ─ Drive Audio chk  │
                 │   ─ (future toggles) │
                 └──────────────────────┘
```

### Data Flow Per Audio Frame

1. CPU runs an emulation slice (`ExecuteCpuSlices()` in `EmulatorShell.cpp`).
2. Within the slice, `DiskIIController::HandleSwitch()`, `HandlePhase()`, and
   `Tick()` may fire `OnMotorStart` / `OnMotorStop` / `OnHeadStep` /
   `OnHeadBump` into the drive's `DiskIIAudioSource`. UI mount/eject paths
   fire `OnDiskInserted` / `OnDiskEjected`. These calls mutate
   `m_motorRunning`, `m_headBuf`/`m_headPos`, `m_doorBuf`/`m_doorPos`,
   `m_lastStepCycle`, `m_seekMode`.
3. After the slice, `WasapiAudio::SubmitFrame()` is called with the mixer
   pointer. It generates `numSamples` of speaker PCM into a scratch buffer
   (mono → duplicated to L=R for stereo), then asks the mixer to generate
   `numSamples * 2` of stereo drive PCM into a parallel stereo scratch
   buffer, then sums per-channel (clamping to `[-1, +1]`) into
   `m_pendingSamples`.
4. WASAPI drains `m_pendingSamples` to the render client (downmixing to mono
   if the device is mono).

No new threads; no new locks. All mutation and consumption happen on the
single CPU-emulation thread that owns `ExecuteCpuSlices()`.

### Event Trigger Specification

Reproduced from `research.md` §4.1, normative for this feature:

| Event                | Trigger                                                                         | Call site                                    |
|----------------------|----------------------------------------------------------------------------------|----------------------------------------------|
| `OnMotorStart`       | `m_motorOn` transitions from false → true                                        | `HandleSwitch()` case 0x9                    |
| `OnMotorStop`        | `m_motorOn` transitions from true → false (post-spindown)                        | `Tick()` after spindown counter expires      |
| `OnHeadStep(newQt)`  | `qtDelta != 0` AND head NOT clamped against a stop                               | `HandlePhase()` after `m_quarterTrack += qtDelta` |
| `OnHeadBump()`       | `qtDelta != 0` AND post-clamp `m_quarterTrack == 0` (from < 0) OR `== kMaxQuarterTrack` (from > kMaxQuarterTrack) | Same `HandlePhase()` site |
| `OnDiskInserted()`   | Disk image newly mounted (or swapped onto a previously-mounted drive after eject) | `DiskIIController::MountImage` (or shell-level mount path) |
| `OnDiskEjected()`    | Disk image unmounted                                                             | `DiskIIController::EjectImage` (or shell-level eject path) |

`OnHeadStep` and `OnHeadBump` are mutually exclusive for any single
`HandlePhase()` invocation. Door events are independent of motor state.

### Step-vs-Seek Discrimination (FR-005)

The mixer adopts the **OpenEmulator cycle-gap approach** (per `research.md`
§4.3) translated to Casso's 1.023 MHz cycle counter:

- `kSeekThresholdCycles = 16368` (≈ 16 ms × 1.023 MHz)
- On `OnHeadStep`: if `(currentCycle - m_lastStepCycle) < kSeekThresholdCycles`,
  enter seek mode: do **not** restart the `HeadStep` one-shot; instead, if
  not already in seek mode, start (or keep playing) a continuous seek-rate
  audio. Otherwise (gap ≥ threshold), restart the `HeadStep` one-shot.
- `m_lastStepCycle` is updated on every `OnHeadStep`.
- Seek mode auto-clears when no step arrives within `kHeadIdleCycles = 51150`
  (≈ 50 ms). Checked at the top of `GeneratePCM()`.

The "continuous seek" sound in v1 may be implemented either as a separate
loop sample or simply by holding the `HeadStep` sample's tail / not
restarting it; the spec (FR-005) requires that the audible result not
fragment into N discrete clicks.The exact synthesis is an implementer's
choice within FR-005.

### Sample Sourcing (Bootstrap Download from OpenEmulator)

Casso's source repository ships with **zero** Disk II audio assets. On
first launch with a Disk II-equipped machine profile, `AssetBootstrap`
detects the absence of `Devices/DiskII/<Mechanism>/*.wav` files and
surfaces a single consent dialog (FR-017) offering to download both Alps
and Shugart sample sets from `openemulator/libemulation`'s GitHub
repository. The dialog discloses GPL-3, the user's recipient obligations,
and the silent-fallback alternative.

Acquisition pipeline per OGG file (FR-018):

1. WinHTTP GET against
   `raw.githubusercontent.com/openemulator/libemulation/master/res/sounds/<Mechanism>/<Mechanism>%20<SampleName>.ogg`
   into a `std::vector<uint8_t>` in memory.
2. `stb_vorbis_decode_memory()` → interleaved int16 PCM + native sample
   rate.
3. Convert to mono float32 (downmix if necessary) and resample to the
   WASAPI device rate. Reuse the existing `IMFSourceReader`-based
   resampler used for WAV loading (or a simple linear resampler — drive
   noise is broadband mechanical content and is not pitch-critical).
4. Write a PCM WAV file (16-bit or float32, whichever the WAV loader
   already prefers) to `Devices/DiskII/<Mechanism>/<CanonicalName>.wav`.
5. Discard the OGG bytes. No `.ogg` is written to disk (NFR-006).

Filename mapping from upstream OGG → canonical PascalCase WAV:

| Upstream (Shugart) | Canonical WAV | Upstream (Alps) | Canonical WAV |
|---|---|---|---|
| `Shugart SA400 Drive.ogg` | `MotorLoop.wav` | `Alps 2124A Drive.ogg` | `MotorLoop.wav` |
| `Shugart SA400 Head.ogg`  | `HeadStep.wav`  | `Alps 2124A Head.ogg`  | `HeadStep.wav` |
| `Shugart SA400 Stop.ogg`  | `HeadStop.wav`  | `Alps 2124A Stop.ogg`  | `HeadStop.wav` |
| `Shugart SA400 Open.ogg`  | `DoorOpen.wav`  | *(not shipped upstream)* | — |
| `Shugart SA400 Close.ogg` | `DoorClose.wav` | *(not shipped upstream)* | — |

The Shugart `Align.ogg` (continuous seek-buzz) is not part of v1: FR-005's
cycle-gap discriminator produces a serviceable buzz by retriggering
`HeadStep.wav` rapidly, which matches OpenEmulator's Alps approach. A
future feature MAY add `HeadSeek.wav` support.

### Loader Precedence (FR-019)

`DiskIIAudioSource::LoadSamples` looks for each canonical filename in this
order:

1. `Devices/DiskII/<filename>.wav` — top-level user override (self-recorded
   or otherwise permissively-licensed; may be committed if license permits).
2. `Devices/DiskII/<SelectedMechanism>/<filename>.wav` — bootstrap-downloaded
   (always gitignored).
3. *(none)* — leave buffer empty; FR-009 graceful silence at runtime.

The precedence is per-file. A user can override only `MotorLoop.wav` with
their own recording and leave the rest pulling from the selected mechanism
subdir.

### Options Dialog Wire-Up

`Casso/OptionsDialog.{h,cpp}` (new) hosts two controls:

- **Drive Audio** check toggle (FR-006), initial state `checked`.
- **Disk II mechanism** dropdown (FR-006), choices: *Shugart SA400*
  (default), *Alps 2124A*. Default rationale: Shugart's sample set is
  complete (5 sounds, including door + bump variants); Alps ships only
  3, leaving eject/insert silent.

`Casso/MenuSystem.{h,cpp}` gains:

- A new menu-item id `IDM_VIEW_OPTIONS`.
- Under the existing `View` popup: a new "Options..." item.
- Handler: opens `OptionsDialog`. On OK, applies the toggle via
  `m_driveAudioMixer.SetEnabled(checked)` and reloads the active sample
  set if the mechanism dropdown changed (calls
  `m_diskAudioSources[i].LoadSamples(...)` with the new mechanism
  subdir).

No persistence required for v1; defaults reset every launch.

### Cold-Boot Mount Suppression (FR-013)

Per FR-013, disk insert sounds fire only for **user-initiated, mid-session**
mounts. Cold-boot mounts (command-line arguments, last-session restoration,
autoload) MUST NOT fire `OnDiskInserted()`. `EmulatorShell` tracks a
"startup phase" flag that begins `true` and transitions to `false` after
machine initialization completes and the main message loop begins
delivering user input; any mount performed while the flag is `true`
suppresses the insert event. Eject events always fire (no cold-boot eject
case in practice). Implementation lands in T082b; paired tests in T082c.

### Mixing Math (FR-010, FR-011, FR-012, NFR-001)

Stereo, equal-power pan (constant-loudness across positions), per-channel
clamp:

```
// Speaker is centered using equal-power center: L = R = sqrt(0.5) ≈ 0.707
for i in [0, n):
    speakerL[i] = speaker[i] * kSpeakerCenter   // kSpeakerCenter = 0.7071
    speakerR[i] = speaker[i] * kSpeakerCenter

// Each drive source produces mono, panned per-drive (equal-power):
for each source s with precomputed panL, panR (panL² + panR² = 1):
    s.GeneratePCM (srcMono, n)
    for i in [0, n):
        diskL[i] += srcMono[i] * s.panLeft
        diskR[i] += srcMono[i] * s.panRight

// Sum and clamp per channel:
for i in [0, n):
    outL[i] = clamp (speakerL[i] + diskL[i], -1.0f, +1.0f)
    outR[i] = clamp (speakerR[i] + diskR[i], -1.0f, +1.0f)
```

Per-source pan (FR-012, equal-power, computed once in `SetPan`):

Pan position is parameterized by `kDrivePanOffset = π/8 ≈ 0.3927` (radians).
Drive angles are measured from the right speaker, so `π/4` is center, `π/2`
is hard left, and `0` is hard right. The constant controls the magnitude
of the offset from center.

- Drive 1 (left bias): `θ = π/4 + kDrivePanOffset = 3π/8`
  - `panL = cos(θ) ≈ 0.924`, `panR = sin(θ) ≈ 0.383`
- Drive 2 (right bias): `θ = π/4 - kDrivePanOffset = π/8`
  - `panL = cos(θ) ≈ 0.383`, `panR = sin(θ) ≈ 0.924`
- Centered (lone drive, or future explicitly-centered source): `θ = π/4`
  - `panL = panR = √0.5 ≈ 0.707`

`SetPan(float panLeft, float panRight)` stores the precomputed coefficients
directly; callers that want angle-based placement use a helper like
`SetPanAngle(float thetaRadians)` that does the `sin`/`cos` once.

Per-sound attenuation (source-internal):

- Motor loop: ×0.25
- Head one-shot: ×0.30
- Door one-shot: ×0.30
- Speaker (unchanged): ×0.50 (existing `AudioGenerator` behavior)

Worst-case per-channel sum: single-drive full speaker + full motor + full
head, drive centered (`0.707` pan):
`0.50 × 0.707 + (0.25 + 0.30) × 0.707 ≈ 0.74` per channel — safely under
1.0. Two-drive case is bounded by per-drive panning concentrating each
drive's energy in one channel; see FR-008 rationale.

When the WASAPI device is mono, the stereo output is downmixed by
`(L + R) × 0.5` after clamp, preserving overall amplitude.

## Constitution Check — Post-Design Re-Validation

- ✅ All new functions stay within size and indent budgets.
- ✅ EHM applied to `LoadSamples` and WAV-decode path; `GeneratePCM`,
      `MixMotor`, `MixHead`, and event hooks are `void` and infallible
      (they only mutate POD state and float buffers).
- ✅ No new threads, no locks (NFR-002).
- ✅ No GPL-3 sample assets committed (NFR-004) — the asset directory is
      either populated by the implementer with self-recorded or synthesized
      content, or empty (graceful-degradation path takes over).
- ✅ Test isolation: in-memory sample buffers, mock sink for controller
      tests, no host filesystem reads.

## Phasing (cross-reference to `tasks.md`)

The implementation follows `research.md` §6.3's recommended order, expanded
into atomic tasks in [`tasks.md`](./tasks.md). Phase summary:

1. **Phase 0 — Contracts**: `IDriveAudioSink`, `IDriveAudioSource`,
   `DriveAudioMixer.h`, `DiskIIAudioSource.h` declarations, named constants.
2. **Phase 1 — Source skeleton**: `DiskIIAudioSource.cpp` with silent
   `GeneratePCM`, infallible event hooks (incl. insert/eject), unit tests
   against a mock that exercises every event combination.
3. **Phase 2 — Mixer skeleton**: `DriveAudioMixer.cpp` with source
   registration, stereo equal-power pan+sum (no motor-hum dedup per
   revised FR-008), silent-source tests.
4. **Phase 3 — Controller wiring**: Add `IDriveAudioSink* m_audioSink` to
   `DiskIIController.h`; add 4 emulation call sites plus mount/eject calls.
   Tests using a recording sink confirm event ordering.
5. **Phase 4 — Sample loading (mechanism-aware)**: Implement
   `DiskIIAudioSource::LoadSamples(mechanismSubdir, deviceSampleRate)`
   with per-file precedence (FR-019): user-override top-level → mechanism
   subdir → silent. Reuse the existing `IMFSourceReader` WAV decoder.
6. **Phase 5 — Mixer playback**: `MixMotor`, `MixHead`, `MixDoor`. Tests
   verify per-sample output for canonical sequences.
7. **Phase 6 — Step/seek discrimination**: `m_lastStepCycle`, `m_seekMode`,
   idle timeout; tests verify rapid-step bursts collapse per FR-005.
8. **Phase 7 — WASAPI stereo integration**: Negotiate stereo from WASAPI,
   extend `SubmitFrame` to produce stereo, additive per-channel mix,
   per-channel clamp, mono device downmix. Speaker regression test (SC-006).
9. **Phase 8 — Shell wiring**: `EmulatorShell` owns mixer + per-drive
   sources; sink hookup; mount/eject hookup with cold-boot suppression
   flag. Boot-and-listen sanity in DOS 3.3 fixture (manual).
10. **Phase 9 — Options dialog**: Create `OptionsDialog.{h,cpp}`; add
    View → Options... menu entry; Drive Audio checkbox; Disk II mechanism
    dropdown (Shugart default); runtime toggle test (SC-003, SC-010).
11. **Phase 10 — Directory restructure + .gitignore**: Migrate top-level
    `ROMs/` into per-machine `Machines/<Name>/` and per-device
    `Devices/DiskII/` directories. Move existing screenshots out of
    `Assets/` is intentionally NOT done (Assets/ stays for screenshots).
    Update `.gitignore` to whitelist-JSON-only under `Machines/**` and
    `Devices/**`. Update `AssetBootstrap`'s ROM catalog paths. Verify
    backward-compat search in `PathResolver` for existing user installs.
12. **Phase 11 — Bootstrap fetch (stb_vorbis + OGG decode + consent)**:
    Add `stb_vorbis.c` to `CassoEmuCore`. Add `s_kDiskAudioCatalog` and
    `AssetBootstrap::CheckAndFetchDiskAudio` mirroring the existing
    `CheckAndFetchRoms` pattern. Implement the in-memory WinHTTP-to-WAV
    pipeline (no on-disk OGG, per NFR-006). Consent dialog (FR-017) with
    explicit GPL-3 disclosure and link to OpenEmulator's `COPYING` file.
13. **Phase 12 — Asset graceful-degradation verification**: Confirm
    declined-consent and per-mechanism missing-file paths produce silent
    audio with at most one log warning per file (FR-009, SC-004).
14. **Phase 13 — Polish**: CHANGELOG, README, manual A/B listening, final
    constitution sweep.

## Risks & Mitigations

| Risk                                                                                  | Mitigation                                                                                                                            |
|---------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| Step-vs-seek tuning sounds wrong for non-DOS-3.3 software (ProDOS, copy-protected)    | Threshold is a single named constant (`kSeekThresholdCycles`); tune via listening test against the existing fixture disks in Phase 6. |
| Disk-audio amplitude causes audible clipping when speaker is at full deflection        | Per-source attenuation + post-sum clamp; tune `kMotorVolume`/`kHeadVolume` in Phase 5 with the speaker test suite as regression guard.|
| Missing or broken WAV files at user installs                                          | FR-009 graceful degradation; the mixer treats any empty buffer as "muted." Verified in Phase 12.                                      |
| `IMFSourceReader` initialization failure on locked-down systems                       | WAV-decode path returns HRESULT; on failure, the affected sample buffer stays empty and FR-009 kicks in. No popup, single log line.   |
| GPL-3 sample contamination — accidental commit of bootstrap-downloaded WAVs            | `.gitignore` whitelist-JSON-only rule under `Machines/**` and `Devices/**` covers every `.wav`/`.rom`/`.ogg`/`.txt` automatically; git won't even offer them for staging. |
| User redistributes their Casso install (zipping the directory) containing GPL-3 WAVs   | User's responsibility per the FR-017 consent dialog disclosure; NFR-006 minimizes the surface by not retaining `.ogg` files. We cannot prevent users from redistributing data they downloaded. |
| OpenEmulator repository moves or the OGG files are renamed/removed upstream            | Single point of update in `s_kDiskAudioCatalog`; FR-009 keeps Casso functional with silent disk audio in the meantime.                |
| stb_vorbis bug / decode failure                                                       | Library is widely used and battle-tested (Unity, Unreal, etc.). On failure, FR-009 keeps the affected sound silent.                  |
| Mixing introduces buffer-underrun (NFR-001)                                           | Disk mix is pure CPU float math inside the same `SubmitFrame` call; no I/O, no syscalls. Measure WASAPI underrun count in soak test.  |
