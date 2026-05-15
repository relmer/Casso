# Implementation Plan: Disk II Audio

**Branch**: `feature/005-disk-ii-audio` | **Date**: 2026-03-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/005-disk-ii-audio/spec.md`
**Research basis**: [`research.md`](./research.md) — source-verified survey of OpenEmulator, MAME, AppleWin, and the Casso codebase
**Constitution**: `.specify/memory/constitution.md`

## Summary

Add realistic Disk II mechanical audio (motor hum, head-step clicks,
track-0 bumps) to Casso's //e emulator and mix it into the existing WASAPI
pipeline alongside the speaker. The implementation is structured around a
single new class — `DiskAudioMixer` — that owns sample buffers and playback
state, and is driven by four event hooks fired from `DiskIIController` at
the precise points where head and motor state changes occur. The mixer is
consumed once per audio frame by `WasapiAudio::SubmitFrame()`, which mixes
disk PCM additively into the same float buffer that the speaker already
writes. A `View → Disk Audio` menu toggle (default on) gates the mixer.

The design is grounded in `research.md` §4–5 (Casso-specific implementation
sketch), which fingerprints exact line ranges in
`DiskIIController.{h,cpp}`, `WasapiAudio.{h,cpp}`, and `EmulatorShell.cpp`
where touch points are required.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK + STL only — Windows MediaFoundation
(`IMFSourceReader`) for WAV decoding, or in-repo minimal PCM WAV parser.
No new third-party libraries.
**Storage**: `Assets/Sounds/DiskII/*.wav` if bundled samples are chosen;
otherwise procedural synthesis at startup.
**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New
`DiskAudioMixerTests.cpp` plus extensions to existing
`DiskIIControllerTests.cpp` to cover the new event firing.
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Existing 5-project solution; new code in `CassoEmuCore`
(library) and `Casso` (GUI shell). No new project.
**Performance Goals**: Adding disk-audio mixing MUST NOT measurably
increase per-frame WASAPI submit cost; the inner loop is one float multiply
+ add per sample per active sound (≤ 3 simultaneous sounds in v1).
**Constraints**: No regression in speaker pipeline (FR-011, SC-006). No
buffer underruns (NFR-001). Same-thread state model — no locks introduced
(NFR-002). GPL-3 disk-audio samples MUST NOT be committed (NFR-004).
**Scale/Scope**: ~3 new source files (~400–600 LOC), ~5 touched files,
~15–30 new test cases.

## Constitution Check

### Principle I — Code Quality (NON-NEGOTIABLE)

| Rule                                              | How this plan complies                                                                                                                  |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| Formatting Preservation                           | All edits to existing files are surgical insertions at the line ranges named in research §5.3; no existing aligned blocks are touched.  |
| EHM on fallible functions                         | `DiskAudioMixer::LoadSamples`, WAV-decode helpers, and any MediaFoundation calls use the standard `HRESULT hr; … Error: … return hr;` pattern. |
| No calls inside macro arguments                   | Store results first; reviewed per PR.                                                                                                   |
| Single exit via `Error:` label                    | Standard pattern in every new fallible function.                                                                                        |
| Avoid Nesting (≤ 2-3 indent beyond EHM)           | `GeneratePCM()` factored into `MixMotor()`, `MixHead()` helpers, each ≤ 2 levels.                                                       |
| Variable Declarations at Top of Scope             | All new functions follow this; column-aligned per project rules.                                                                        |
| Function Comments in `.cpp` Only                  | Mixer header has only declaration comments; doc blocks live in `.cpp` with 80-`/` delimiters.                                           |
| Function Spacing — `func()` vs `func (a, b)`      | Verified via `rg -n '\w \(\)' CassoEmuCore/Audio/ Casso/` before commit on every PR phase.                                              |
| Smart Pointers                                    | `DiskAudioMixer` is owned by `EmulatorShell` directly (composition); no smart pointer needed. Sample buffers are `std::vector<float>`.   |
| No magic numbers                                  | `kMotorVolume = 0.25f`, `kHeadVolume = 0.30f`, `kSeekThresholdCycles = 16368`, `kStepDebounceCycles = 511`, `kHeadIdleCycles = 51150`, `kMotorLoopHeadroom = 1.0f` are all named constants. |

### Principle II — Test Isolation (NON-NEGOTIABLE)

Disk-audio tests run on the same headless harness as existing
`CassoEmuCore` tests. The `DiskAudioMixer` is constructed with caller-owned
in-memory sample buffers; tests never read host filesystem. The
`IDiskAudioSink` interface lets `DiskIIController` tests substitute a
recording mock with no audio device involved.

### Principle V — Function Size & Structure

`GeneratePCM()` is the only function with appreciable logic; factored as:

```
GeneratePCM (buf, n)
  Clear (buf, n)                       // 1-line helper
  if m_enabled:
      MixMotor (buf, n)                // ≤ 15 LOC, loops over sample, wraps
      MixHead  (buf, n)                // ≤ 20 LOC, advances headPos, halts on end
      UpdateSeekState (currentCycle)   // ≤ 10 LOC
```

All helpers stay under ~25 LOC.

## Architecture

### Components

```
                 ┌─────────────────────┐
                 │  DiskIIController   │  (CassoEmuCore/Devices)
                 │   ─ HandleSwitch    │──┐
                 │   ─ HandlePhase     │  │ IDiskAudioSink*  (header-only interface,
                 │   ─ Tick (spindown) │  │                   declared with controller)
                 └─────────────────────┘  │
                                          │ OnMotorStart()
                                          │ OnMotorStop()
                                          │ OnHeadStep(qt)
                                          │ OnHeadBump()
                                          ▼
                 ┌─────────────────────────────────────┐
                 │ DiskAudioMixer : IDiskAudioSink     │  (CassoEmuCore/Audio)
                 │   ─ LoadSamples / SynthesizeSamples │
                 │   ─ SetEnabled                      │
                 │   ─ GeneratePCM (out, n)            │◄── called from WasapiAudio
                 │      ├── MixMotor                   │
                 │      └── MixHead (step + bump +     │
                 │                   seek/buzz state)  │
                 └─────────────────────────────────────┘
                            ▲                       │
                            │                       │ float* diskBuf
                            │                       ▼
                 ┌──────────┴───────────┐   ┌───────────────────────┐
                 │   EmulatorShell      │──►│   WasapiAudio         │
                 │   ─ owns mixer       │   │   ─ SubmitFrame (... , │
                 │   ─ owns menu wire   │   │       diskMixer*)     │
                 │   ─ SetAudioSink     │   │     ├ GeneratePCM     │
                 │     (controller →    │   │     │  (speaker)      │
                 │      mixer)          │   │     ├ GeneratePCM     │
                 └──────────────────────┘   │     │  (diskMixer)    │
                            ▲               │     └ additive mix +  │
                            │               │       clamp [-1,+1]   │
                            │ MenuSystem    └───────────────────────┘
                            │   "Disk Audio" check toggle (default on)
                            │   → mixer.SetEnabled (checked)
                            ▼
                 ┌──────────────────────┐
                 │   MenuSystem         │
                 │   ─ View → Disk Audio│
                 └──────────────────────┘
```

### Data Flow Per Audio Frame

1. CPU runs an emulation slice (`ExecuteCpuSlices()` in `EmulatorShell.cpp`).
2. Within the slice, `DiskIIController::HandleSwitch()`, `HandlePhase()`, and
   `Tick()` may fire `OnMotorStart` / `OnMotorStop` / `OnHeadStep` /
   `OnHeadBump` into the mixer. These calls mutate `m_motorRunning`,
   `m_headBuf`/`m_headPos`, `m_lastStepCycle`, `m_seekMode`.
3. After the slice, `WasapiAudio::SubmitFrame()` is called with the disk
   mixer pointer. It generates `numSamples` of speaker PCM into a scratch
   buffer, then asks the mixer to generate `numSamples` of disk PCM into a
   parallel scratch buffer, then sums them (clamping to `[-1, +1]`) into
   `m_pendingSamples`.
4. WASAPI drains `m_pendingSamples` to the render client.

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

`OnHeadStep` and `OnHeadBump` are mutually exclusive for any single
`HandlePhase()` invocation.

### Step-vs-Seek Discrimination (FR-005)

The mixer adopts the **OpenEmulator cycle-gap approach** (per `research.md`
§4.3) translated to Casso's 1.023 MHz cycle counter:

- `kSeekThresholdCycles = 16368` (≈ 16 ms × 1.023 MHz)
- On `OnHeadStep`: if `(currentCycle - m_lastStepCycle) < kSeekThresholdCycles`,
  enter seek mode: do **not** restart the `head_step` one-shot; instead, if
  not already in seek mode, start (or keep playing) a continuous seek-rate
  audio. Otherwise (gap ≥ threshold), restart the `head_step` one-shot.
- `m_lastStepCycle` is updated on every `OnHeadStep`.
- Seek mode auto-clears when no step arrives within `kHeadIdleCycles = 51150`
  (≈ 50 ms). Checked at the top of `GeneratePCM()`.

The "continuous seek" sound in v1 may be implemented either as a separate
loop sample or simply by holding the `head_step` sample's tail / not
restarting it; the spec (FR-005) requires that the audible result not
fragment into N discrete clicks. The exact synthesis is an implementer's
choice within FR-005.

### Sample Sourcing (Implementation Choice, Per Spec)

Two implementations satisfy the spec:

- **Bundled WAVs**: `Assets/Sounds/DiskII/{motor_loop,head_step,head_stop}.wav`.
  Decoded once at startup via `IMFSourceReader` to mono float32 at the
  WASAPI mix rate, owned by `DiskAudioMixer`.
- **Procedural synthesis**: At `DiskAudioMixer` startup, fill internal
  buffers with synthesized PCM (motor: low-frequency sawtooth + noise; head
  click: short attack + Karplus-Strong-style decay; bump: same but
  longer/lower).

Either way, the downstream contract (`GeneratePCM`, event hooks, mixer
state machine) is identical. `LoadSamples` returns `S_OK` if anything
loaded successfully, and the mixer simply mutes any sound whose buffer is
empty (FR-009, graceful degradation).

### Menu Wire-Up

`Casso/MenuSystem.{h,cpp}` gains:

- A new menu-item id (e.g., `IDM_VIEW_DISKAUDIO`)
- Under the existing `View` popup: a new check item "Disk Audio", initially
  checked
- Handler: toggles the check state and calls
  `m_diskAudioMixer.SetEnabled(checked)` via the shell's existing
  menu-command dispatch.

Default state on first launch: ON (FR-006). No persistence required for
v1.

### Mixing Math (FR-010, FR-011, NFR-001)

```
for each frame sample i:
  out[i] = speaker[i] + disk[i]    // both pre-attenuated to safe levels
  out[i] = clamp (out[i], -1.0f, +1.0f)
```

Per-sound attenuation (mixer-internal):

- Motor loop: ×0.25
- Head one-shot: ×0.30
- Speaker (unchanged): ×0.50 (existing `AudioGenerator` behavior)

Worst-case sum: `0.50 + 0.25 + 0.30 = 1.05` → clamped to 1.0 at peak.
Resulting clamp distortion affects only the disk component during the
extreme speaker-at-peak instants; not audibly objectionable. Mixer
coefficients are named constants and tunable later.

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

1. **Phase 0 — Contracts**: `IDiskAudioSink` interface, `DiskAudioMixer.h`
   declarations, named constants.
2. **Phase 1 — Mixer skeleton**: `DiskAudioMixer.cpp` with silent
   `GeneratePCM`, infallible event hooks, unit tests against a mock that
   exercises every event combination.
3. **Phase 2 — Controller wiring**: Add `IDiskAudioSink* m_audioSink` to
   `DiskIIController.h`; add the 4 call sites in
   `DiskIIController.cpp`; tests using a recording sink confirm
   step-vs-bump discrimination.
4. **Phase 3 — Mixer playback**: Implement `LoadSamples` (or
   `SynthesizeSamples`), `MixMotor`, `MixHead`. Tests verify per-sample
   output for canonical motor-on / step / bump sequences.
5. **Phase 4 — Step/seek discrimination**: Add `m_lastStepCycle`,
   `m_seekMode`, idle timeout; tests verify rapid-step bursts collapse
   per FR-005.
6. **Phase 5 — WASAPI integration**: Extend `SubmitFrame`, additive mix,
   clamp; speaker regression test (SC-006).
7. **Phase 6 — Shell wiring**: `EmulatorShell` ownership, sink hookup, mix
   pass-through. Boot-and-listen sanity in DOS 3.3 fixture (manual).
8. **Phase 7 — View menu toggle**: Add menu item; runtime toggle test.
9. **Phase 8 — Asset graceful-degradation**: Verify missing-file path; log
   one non-fatal warning per missing asset; no popup.
10. **Phase 9 — Polish**: CHANGELOG, README, manual A/B listening, final
    constitution sweep.

## Risks & Mitigations

| Risk                                                                                  | Mitigation                                                                                                                            |
|---------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| Step-vs-seek tuning sounds wrong for non-DOS-3.3 software (ProDOS, copy-protected)    | Threshold is a single named constant (`kSeekThresholdCycles`); tune via listening test against the existing fixture disks in Phase 4. |
| Disk-audio amplitude causes audible clipping when speaker is at full deflection        | Per-source attenuation + post-sum clamp; tune `kMotorVolume`/`kHeadVolume` in Phase 5 with the speaker test suite as regression guard.|
| Missing or broken WAV files at user installs                                          | FR-009 graceful degradation; the mixer treats any empty buffer as "muted." Verified in Phase 8.                                       |
| `IMFSourceReader` initialization failure on locked-down systems                       | WAV-decode path returns HRESULT; on failure, the affected sample buffer stays empty and FR-009 kicks in. No popup, single log line.   |
| GPL-3 sample contamination from development snapshots                                 | `.gitignore` does **not** silence `Assets/Sounds/DiskII/` (per project hygiene rules); stray files surface in `git status`. PR review responsibility. |
| Mixing introduces buffer-underrun (NFR-001)                                           | Disk mix is pure CPU float math inside the same `SubmitFrame` call; no I/O, no syscalls. Measure WASAPI underrun count in soak test.  |
