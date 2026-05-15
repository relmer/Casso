# Feature Specification: Disk II Audio

**Feature Branch**: `feature/005-disk-ii-audio`
**Created**: 2026-03-19
**Status**: Draft
**Input**: User description: "Emit realistic mechanical sounds during Disk II activity in the Casso //e emulator: motor hum (continuous while motor is on), head-step click (per quarter-track phase change), and track-0 bump/thunk (head pinned at travel stop while stepper still energized). Mix into the existing WASAPI audio pipeline alongside the //e speaker output. Must not regress speaker audio. View menu toggle to enable/disable disk audio independently (defaults to on). Sample sourcing — bundled WAV samples or procedurally-synthesized — is an implementation choice."

**Reference research**: [`research.md`](./research.md) — source-verified survey of disk-audio implementations in OpenEmulator/libemulation, MAME, and AppleWin, plus the Casso-specific architectural sketch this spec is grounded in.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Hear the disk drive boot a DOS 3.3 image (Priority: P1)

A retro-computing user mounts a stock DOS 3.3 disk image in Drive 1 of an emulated //e and cold-boots. From the moment the disk controller spins the drive, the user hears (alongside the //e speaker beep) the unmistakable Disk II soundscape: a low-frequency motor hum that comes up shortly after PR#6, a flurry of mechanical clicks while the head re-calibrates to track 0 (including audible "thunk" sounds when the head hits the track-0 stop), and a settled motor hum that persists through the load and across brief inter-sector motor-off windows without stuttering.

**Why this priority**: This is the entire feature's reason to exist. If a DOS 3.3 boot doesn't *sound* like a Disk II, the feature has failed. It exercises every audio code path — motor on, motor spindown, single steps, seek bursts, and track-0 bumps — in a single 5-second sequence on real software.

**Independent Test**: Boot a DOS 3.3 fixture disk in the //e profile and listen / capture the host audio stream. Verify the speaker emits its boot beep AND the motor hum + head-click + bump cluster fires during the DOS RWTS recalibration. Verify the motor hum does **not** drop out during the brief $C0E8/$C0E9 motor-off/on toggles that DOS issues between inter-track sector reads.

**Acceptance Scenarios**:

1. **Given** a //e with a DOS 3.3 disk image in Drive 1 and disk audio enabled, **When** the user cold-boots, **Then** the motor hum begins audibly within ~50ms of the first `$C0E9` strobe, and persists continuously across DOS's mid-load $C0E8/$C0E9 cycles until ~1 second after the final motor-off.
2. **Given** the same boot, **When** DOS's RWTS recalibrates the head to track 0, **Then** the user hears a burst of head-step clicks ending in at least one distinct, lower-pitched bump sound (the head being pinned at the travel stop while the stepper is still energized).
3. **Given** the //e speaker is also active (e.g., the boot beep), **When** both speaker and disk audio play simultaneously, **Then** neither pipeline drops samples, glitches, or distorts beyond the sum of their independent amplitudes (no buffer underruns, no clicks introduced by mixing).

---

### User Story 2 - Toggle disk audio independently of speaker (Priority: P2)

A user finds the disk sounds distracting during long sessions and disables them via **View → Disk Audio**. The //e speaker continues to function normally. The user can re-enable disk audio at any time without restarting the emulator, and the setting reflects the current state in the menu (checked / unchecked). Disk audio is **on by default** for a first-launch user.

**Why this priority**: Without an off switch, the feature is unshippable — disk audio is character-rich but also unrelenting during heavy disk activity. Independent of speaker volume because the speaker is application-essential and disk audio is decorative.

**Independent Test**: Launch the emulator, verify **View → Disk Audio** is checked by default; mount a disk and confirm disk sounds. Uncheck the menu item; verify disk sounds stop immediately while a speaker tone (e.g., from BASIC `PRINT CHR$(7)`) remains audible. Re-check; verify disk sounds resume on the next disk activity. Restart the emulator; verify the last-set state persists if a settings-persistence mechanism exists, otherwise verify it returns to default-on.

**Acceptance Scenarios**:

1. **Given** a fresh launch with no saved settings, **When** the user opens the View menu, **Then** "Disk Audio" appears as a toggleable item and is checked.
2. **Given** disk audio is enabled and a disk is actively seeking, **When** the user unchecks **View → Disk Audio**, **Then** disk sounds cease within one audio frame, the menu item is unchecked on its next display, and the //e speaker remains audible.
3. **Given** disk audio is disabled, **When** the user re-checks **View → Disk Audio** mid-session, **Then** subsequent disk activity produces audio without requiring a reboot or disk remount.

---

### User Story 3 - Run without sample assets present (Priority: P3)

A developer pulls the repo with no audio asset files staged (or with the asset directory missing on disk). The emulator launches, boots, and runs disk-based software end-to-end. No crash, no error popup, no message-box interruption. Disk audio is silently inactive; everything else — including the //e speaker and the **View → Disk Audio** menu item — continues to work.

**Why this priority**: A retro emulator must never refuse to start over a decorative subsystem. This is the contract that lets sample sourcing be deferred without blocking the rest of the feature. Lower priority than the toggle because the developer's workaround (procedural synthesis at implementation time, or simply landing the wiring without samples) is cheap.

**Independent Test**: Delete the `Assets/Sounds/DiskII/` directory (or rename it). Launch the emulator, boot a disk fixture, and confirm: process does not crash, no modal popup appears, logs show at most a single non-fatal warning per missing sample, the **View → Disk Audio** menu still toggles (it just toggles a no-op), and the speaker pipeline is unaffected.

**Acceptance Scenarios**:

1. **Given** `Assets/Sounds/DiskII/` does not exist, **When** the emulator launches, **Then** the process reaches the main window without prompting the user and the //e cold-boots normally.
2. **Given** the asset directory exists but `head_step.wav` is missing, **When** the head steps, **Then** no audible step click is produced but the motor hum (whose sample is present) continues normally.
3. **Given** any missing-asset condition, **When** the user toggles **View → Disk Audio**, **Then** the toggle succeeds without error.

---

### Edge Cases

- **Spindown mid-toggle**: User disables disk audio while the motor is in the ~1-second spindown window. The motor hum must stop immediately, not finish out its spindown buffer.
- **Rapid step bursts (DOS 3.3 boot)**: Four-plus head-step events arrive within ~16ms of each other. Behavior must collapse into a single continuous seek-rate audio gesture, not four overlapping one-shots stacking on each other and clipping the mix.
- **Track-0 wall-banging**: DOS 3.3 RWTS recalibration steps "inward" past track 0 multiple times. Each attempt that re-energizes phase 0 while `m_quarterTrack == 0` must produce a bump sound, and consecutive bumps must not stack into one continuous bump tone.
- **Disk audio sample length vs. event cadence**: A head-step sample (~50–150ms) may still be playing when the next step arrives. The mixer must transition into continuous-seek mode rather than restarting the one-shot from sample-position zero on every step (otherwise rapid seeks become a perceived "click-click-click" loop instead of a buzz).
- **Concurrent speaker amplitude**: Speaker output at full deflection (~±0.5) plus motor hum (~0.25) plus a head-click peak (~0.30) can summed exceed 1.0. Mixer must either clamp to the float PCM range without introducing audible artifacts, or scale to leave headroom.
- **Drive 2 activity**: Casso supports two Disk II drives. Behavior when both drives spin simultaneously is in scope (the audio should not double-count motor hum but should produce step/bump sounds for whichever drive is actively seeking) — see FR-008.
- **Save/restore mid-spindown**: Out of scope for this feature — Casso has no save-state subsystem to break here.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001 (Motor hum looping)**: While the Disk II controller's `m_motorOn` flag is true, the disk audio subsystem MUST emit a seamlessly looping motor-hum signal. The signal MUST be driven by the post-spindown-timer `m_motorOn` flag, not by the raw `$C0E8`/`$C0E9` soft-switch events, so that brief inter-sector motor-off/on cycles do **not** cause the hum to drop out.
- **FR-002 (Motor fade-out on spindown)**: When `m_motorOn` transitions from true to false (i.e., after the ~1-second spindown timer expires inside `DiskIIController::Tick()`), the motor hum MUST fade out cleanly (either via the natural tail of a windrun sample or a short envelope), with no audible click at the cut point.
- **FR-003 (Head-step click — actual movement only)**: A head-step click MUST fire exactly once per `HandlePhase()` invocation where `qtDelta != 0`, i.e., where the head actually moved a quarter-track. Soft-switch accesses that do not displace the head (e.g., energizing an already-active phase) MUST NOT produce a step sound.
- **FR-004 (Track-0 bump)**: When a phase change would move the head past `m_quarterTrack == 0` (and the head is clamped to 0), the audio subsystem MUST emit a distinct bump/thunk sound (separate from the ordinary head-step click). The same MUST hold at the upper travel stop (`m_quarterTrack == kMaxQuarterTrack`). Bump events MUST NOT also fire a regular step click for the same event.
- **FR-005 (Step-vs-seek discrimination)**: When multiple step events fire within ~16ms of one another (consistent with DOS 3.3 RWTS seek cadence: ≈6–20ms inter-step), the audio subsystem MUST emit a continuous seek-rate audio gesture rather than re-triggering N overlapping one-shot clicks. The discriminator MUST use either MAME's playback-position heuristic ("is the previous step sample still playing?") or OpenEmulator's cycle-gap timer (≈16,368 CPU cycles at 1.023 MHz), at the implementer's discretion. The threshold MUST be at least 16 ms; the exact value MAY be tuned during implementation.
- **FR-006 (View menu toggle, default on)**: The View menu MUST contain a "Disk Audio" item that toggles the disk-audio subsystem on and off. The toggle MUST be checked (enabled) by default on first launch. Toggling MUST take effect within one audio frame — no restart, no disk remount required.
- **FR-007 (Independent of speaker)**: The "Disk Audio" toggle MUST act only on disk audio. Disabling disk audio MUST NOT affect speaker output (amplitude, latency, or otherwise), and disabling/muting the speaker (when such control exists) MUST NOT silence disk audio.
- **FR-008 (Multi-drive behavior)**: When both Disk II drives are present in the machine configuration, motor hum MUST be emitted whenever **any** drive's `m_motorOn` is true (without double-amplitude when both are on simultaneously). Head-step and bump sounds MUST fire for whichever drive issued the phase change. (Casso's current scheduler activates drives mutually exclusively in practice, so simple OR-of-flags suffices for v1.)
- **FR-009 (Graceful asset absence)**: If any sample asset file is missing, malformed, or fails to decode at startup, the emulator MUST log a single non-fatal warning per missing asset and continue running. The affected sound (and only the affected sound) MUST be silently omitted. No modal error dialog, no crash, no popup. Other disk audio sounds whose samples loaded successfully MUST continue to function.
- **FR-010 (Mixing into existing WASAPI pipeline)**: The disk PCM stream MUST be mixed additively into the same per-frame float-PCM buffer that the speaker pipeline already produces inside `WasapiAudio::SubmitFrame()`. The combined buffer MUST be clamped or scaled to the `[-1.0, +1.0]` range without introducing audible distortion in the speaker signal. Disk audio MUST NOT spawn a second WASAPI render client.
- **FR-011 (No speaker regression)**: The speaker audio pipeline — including `AudioGenerator::GeneratePCM()` output amplitude, latency, and the queued/pending sample queue's drain behavior — MUST be functionally unchanged by the introduction of disk audio. Speaker-only tests MUST pass identically before and after.

### Non-Functional Requirements

- **NFR-001 (No buffer underruns)**: Disk-audio mixing happens inside the same `SubmitFrame()` call that already drains the WASAPI render client. Mixing MUST add no measurable risk of buffer underruns or audio glitches.
- **NFR-002 (Same-thread state model)**: All disk-audio state (motor flags, sample positions, last-step cycle counter) is mutated and read from the CPU-emulation thread (`ExecuteCpuSlices()`), eliminating cross-thread synchronization. The design MUST NOT introduce locks, atomics, or message queues for disk audio in v1.
- **NFR-003 (Asset footprint)**: Sample assets (if bundled) for v1 (motor loop, head step, head stop) SHOULD total under ~64 KB on disk to keep the emulator binary distribution small. Long ambient recordings are discouraged in favor of short, well-looped clips.
- **NFR-004 (License hygiene)**: Bundled sample assets MUST be either self-recorded, procedurally synthesized, or sourced from a permissive (MIT/CC0/CC-BY) license. OpenEmulator's GPL-3 Alps/Shugart samples MAY be used during development but MUST NOT be committed to `master` (see `research.md` §3.1 — viral copyleft incompatibility).

### Key Entities

- **DiskAudioMixer** — the new subsystem that owns sample buffers, playback positions, and the four event hooks (`OnMotorStart`, `OnMotorStop`, `OnHeadStep`, `OnHeadBump`). Generates PCM into a host-supplied buffer per audio frame.
- **IDiskAudioSink** — the abstract sink interface that `DiskIIController` notifies on motor and head events. Decouples the controller from the concrete mixer (and from the audio subsystem altogether for unit-test purposes).
- **Disk Audio asset set** — for v1, three logical samples: `motor_loop`, `head_step`, `head_stop`. Stored either as bundled WAV files under `Assets/Sounds/DiskII/`, or generated procedurally inside `DiskAudioMixer` at startup. The choice is deferred to implementation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: When a DOS 3.3 disk image is cold-booted with disk audio enabled, a listener can identify (without being told) that they are hearing a 5.25" floppy drive, distinct from the speaker beep. (Validated by informal listening test against a known-good recording of real Disk II hardware.)
- **SC-002**: Mounting a disk and triggering the //e speaker (e.g., a BASIC `PRINT CHR$(7)`) simultaneously produces both sounds without underrun, glitch, or speaker-tone degradation. Measured by zero buffer-underrun events reported by WASAPI over a 60-second mixed-activity run.
- **SC-003**: Toggling **View → Disk Audio** off and on five consecutive times during active disk activity completes without crash, without orphaned audio (sound persisting after toggle-off), and without delay greater than ~50 ms perceived by the user.
- **SC-004**: Deleting `Assets/Sounds/DiskII/` and launching the emulator results in a successful boot to the //e BASIC prompt with at most one warning log line per missing sample and zero modal dialogs.
- **SC-005**: A DOS 3.3 boot with rapid recalibration sequences produces an audibly-continuous seek/buzz sound rather than a discrete click-click-click train (verified against the FR-005 ~16 ms discrimination threshold).
- **SC-006**: Existing speaker-only unit tests and integration tests pass identically (same outputs, same timings) before and after the disk-audio subsystem is introduced. Zero speaker regressions.

## Out of Scope *(mandatory)*

The following items were considered and explicitly deferred. They MAY become follow-up features but are not delivered by this spec:

1. **Door open/close sounds** — purely a UI-event-driven sound (disk mount / eject), with no $C0Ex soft-switch trigger. Out of scope here; deferred to a follow-up that pairs with disk-image swap UI work.
2. **Sample sourcing decision (recording, synthesis, third-party samples)** — explicitly an implementation-time choice. The spec accommodates any of: bundled self-recorded WAV samples, bundled CC0/CC-BY samples, or procedurally-synthesized waveforms generated inside `DiskAudioMixer` at startup.
3. **Recording real Disk II hardware** — its own off-codebase task. Not blocking for landing the audio plumbing.
4. **Procedural synthesis design** — if the implementer chooses procedural synthesis, the detailed DSP design (sawtooth fundamentals, resonator filters, envelope shapes) is not specified here; it is an implementation detail bounded only by FR-001..FR-005 and FR-009.
5. **Non-Disk II drives** — Smartport drives, ProDOS hard-disk emulation, RAM disks. These have no equivalent mechanical sounds and are outside the scope of "Disk II audio."
6. **Disk II Enhanced / Liron / UniDisk 3.5"** — different mechanical characteristics; not part of this feature.
7. **Per-drive volume controls** — only a single global on/off toggle is in scope.
8. **Save-state of audio state** — Casso has no save-state subsystem; not applicable.
9. **Recording or exporting disk audio to a file** — playback only.

## Assumptions

- **A-001**: The WASAPI mix format is float32 PCM at the device's native sample rate (44100 or 48000 Hz), discoverable after `WasapiAudio::Initialize()`. This is true today per `Casso/WasapiAudio.cpp`.
- **A-002**: `DiskIIController::HandlePhase()` correctly computes `qtDelta` and clamps `m_quarterTrack` to the `[0, kMaxQuarterTrack]` range. The bump-detection FR (FR-004) relies on this invariant, which is already enforced (see `CassoEmuCore/Devices/DiskIIController.cpp:229-294` per research §6).
- **A-003**: The disk audio subsystem is only active when at least one Disk II controller is present in the machine configuration. Profiles without disk controllers (e.g., a cassette-only Apple ][) bypass disk-audio wiring entirely.
- **A-004**: All disk-audio state runs on the CPU emulation thread (the same one that calls `ExecuteCpuSlices()`), per NFR-002. This eliminates lock/atomic requirements for v1.
- **A-005**: The View menu already exists (per `Casso/MenuSystem.{h,cpp}`) and accepts new check-toggle items without architectural change. Confirmed by the codebase pre-touch.

## Glossary

- **Quarter-track** — The Disk II head positioning unit. A full track is 4 quarter-tracks; the full 35-track surface is 140 quarter-track positions (0..139). The head can be parked between physical tracks (half-track or quarter-track copy protection schemes exploit this).
- **Phase (Disk II)** — One of four electromagnets ($C0E0..$C0E7 in pairs of on/off) that pull the head stepper. Energizing the next phase in sequence moves the head one quarter-track; energizing a phase 180° opposed pulls it the other way.
- **`qtDelta`** — The signed quarter-track displacement that `HandlePhase()` computes from the current and target phase. `qtDelta == 0` means no head movement; `qtDelta != 0` is the audio-trigger condition for FR-003.
- **Bump / track-0 stop** — Mechanical limit at quarter-track 0 (and 139). RWTS recalibration intentionally walks the head past the stop to force a known position; the head physically can't move, but the stepper still energizes, producing the audible "thunk."
- **Spindown** — The ~1-second timer (`kMotorSpindownCycles = 1,000,000` cycles at 1.023 MHz) between the last `$C0E8` motor-off and the actual `m_motorOn = false` transition. Exists to avoid stop/start churn during multi-sector reads.
- **Seek vs. step** — A "step" is a single quarter-track movement; a "seek" is a contiguous sequence of steps to relocate the head. Audibly, a step is a click; a seek is a buzz. The ~16 ms inter-step threshold (FR-005) discriminates them.
- **WASAPI** — Windows Audio Session API; the host audio path Casso submits float PCM to. The mixing point for this feature is `WasapiAudio::SubmitFrame()`.
- **`DiskAudioMixer`** — The new class (`CassoEmuCore/Audio/DiskAudioMixer.{h,cpp}`) introduced by this feature that owns disk-audio state and produces per-frame disk PCM.
- **`IDiskAudioSink`** — Abstract notification interface from `DiskIIController` to `DiskAudioMixer`. Four methods: `OnMotorStart`, `OnMotorStop`, `OnHeadStep(int newQt)`, `OnHeadBump()`.
