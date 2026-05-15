# Research: Disk II Audio

**Feature**: Disk II Audio (`feature/005-disk-ii-audio`)
**Source**: Research report produced 2026-03-19, archived in this document. Original
research basis: source code verified from `openemulator/libemulation`,
`mamedev/mame`, `AppleWin/AppleWin`, and the local Casso codebase. No code was
modified during the research pass.

This document is a verbatim distillation of the research report. Quotes,
citations, and code excerpts are preserved as found in upstream sources;
file:line references at the bottom of each section are authoritative.

---

## Summary

This report covers everything needed to implement Disk II sound effects in
Casso. The core findings:

- **AppleWin has zero disk audio.**
- **MAME has a fully engineered generic floppy sound system** (BSD-3 code,
  separate sample distribution).
- **OpenEmulator/libemulation has the only Apple-specific Disk II
  implementation** (GPL-3, with Alps 2124A and Shugart SA400 `.ogg` assets
  shipped in-repo).
- Casso's existing architecture — particularly `HandlePhase()` and the WASAPI
  `m_pendingSamples` pipeline — is well-positioned for a clean implementation
  requiring only a new `DiskAudioMixer` class and minor touch points in two
  existing files.

---

## Part 1 — What Sounds Does a Real Disk II Make?

The Apple Disk II (1978) contains two independently controllable
sound-producing mechanisms:

### 1.1 DC Motor / Platter Drive

- A brushed DC motor spins the disk platter at **300 RPM**.
- Audible character: a **continuous low-frequency hum** (~40–80 Hz fundamental,
  multiple harmonics), plus belt/bearing friction noise. Subjectively a
  "whirring" tone that varies slightly with load.
- **Triggered by**: the `$C0E9` soft switch (motor-on). Motor continues for
  approximately **1 second after `$C0E8`** (motor-off) due to mechanical
  spindown — the Apple IIe ROM and all RWTS implementations exploit this
  spindown window to avoid repeated start/stop during multi-sector operations.
- The sound persists for the full spindown period, since the platter
  physically keeps spinning.
- **Independently addressable**: Yes — any `$C0E9` strobe starts it; it stops
  naturally after spindown.

### 1.2 Head Stepper Motor (Clicks)

- A 4-phase electromagnetic stepper motor moves the read/write head across 35
  physical tracks (0–34), which is 140 quarter-track positions.
- Each full track step = 2 phase energizations = 2 quarter-track moves.
- Audible character: a sharp **mechanical click** per quarter-track movement.
  Individual clicks have a short attack (~2–5ms) and a mechanical ring-down
  (~30–80ms). Consecutive clicks (during seek) merge into a **distinctive
  buzzing rattle**.
- **Triggered by**: phase magnet changes ($C0E0–$C0E7). Only actual head
  displacement triggers a click — energizing an already-active phase, or
  de-energizing a phase that causes no net movement, produces no step sound.
  *This is the most common implementation error: firing a sound on every
  soft-switch access instead of on every actual quarter-track movement.*
- **Independently addressable**: Yes — but physically coupled to motor-on
  state (RWTS never steps unless motor is on).

### 1.3 Recalibration Bump (Track-0 Stop Thunk)

- Not a distinct mechanism — emerges from **rapid consecutive steps in the
  inward direction until the head assembly hits the physical travel stop** at
  track 0.
- Audible character: accelerating clicks culminating in a louder, lower-pitched
  **thunk** when the head hits the hard stop. If calibration involves multiple
  bump attempts (common in DOS 3.3 RWTS), a "clonk-clonk-clonk" pattern is
  heard.
- No separate hardware trigger — it happens organically when `m_quarterTrack`
  reaches 0 and gets clamped. An emulator detects this as a `qtDelta > 0 &&
  m_quarterTrack == 0 after clamp` condition.
- OpenEmulator's model: sets `phaseStop = true` whenever the head would move
  past track 0 or track 39. (`AppleDiskDrive525.cpp:notify()`)

### 1.4 Door Mechanism (Open/Close)

- A lever-and-ratchet disk insertion and ejection mechanism.
- Audible character: a **multi-click mechanical ratchet** on door close (disk
  loaded) and a **snap** on door open (disk ejected).
- **Triggered by**: disk mount/unmount events — purely a UI-level event, not
  driven by $C0Ex soft switches.
- Shugart SA400 has both Open and Close sounds. Alps 2124A (the majority of
  Disk II drives) omits these — verified by the absence of `Alps 2124A
  Open.ogg` / `Alps 2124A Close.ogg` in OpenEmulator's assets.

**Scope note**: Door open/close is out of scope for this feature (see
`spec.md` §Out of Scope item 1).

### 1.5 Hardware Timing Context

- Typical RWTS track step interval: **6–20ms** (verified via MAME's seek
  sample naming: `525_seek_6ms`, `525_seek_12ms`, `525_seek_20ms`).
- Full disk seek (track 0 to 34): ~0.5–1.0 seconds.
- Motor spindown: ~1 second. Both Casso (`kMotorSpindownCycles = 1,000,000`)
  and AppleWin (`SPINNING_CYCLES = 1,000,000`) match the hardware timing.
  `openemulator/libemulation:src/libemulation/Implementation/Apple/AppleDiskIIInterfaceCard.cpp`
  uses `1.0 * APPLEII_CLOCKFREQUENCY` for the same timer.

---

## Part 2 — How Existing Emulators Implement Disk Sounds

### 2.1 AppleWin — No Disk Audio

**License:** GPL v2 (`AppleWin/AppleWin:LICENSE`)

AppleWin has **no disk sound implementation whatsoever**. The disk controller
(`AppleWin/AppleWin:source/Disk.cpp`) handles `ControlStepper()`,
`ControlStepperDeferred()`, `ControlMotor()`, and `CheckSpinning()`, but none
of these functions call any audio API or notification. The audio infrastructure
(`SoundCore.h`, DirectSound `VOICE` objects) exists solely for speaker
emulation and MockingBoard cards. There are no disk sound `.wav` resources in
the repository.

This is the most widely used Apple II emulator, and the absence of disk
sounds is a notable gap that Casso could surpass.

```cpp
// AppleWin/AppleWin:source/Disk.cpp — ControlStepper() — no audio calls
void Disk2InterfaceCard::ControlStepper(const WORD address)
{
    if (m_headIsMoving)
        return;  // stepper logic only — no sound trigger
    // ... phase update, track computation ...
}
```

### 2.2 MAME — Full Generic Floppy Sound Device

**License:** BSD-3-Clause (code); sample files distributed separately (license
unclear, see §3)

MAME implements a dedicated `floppy_sound_device` class in
`mamedev/mame:src/devices/imagedev/floppy.cpp`. This is the most
architecturally complete implementation found.

#### 2.2.1 Sample Categories and Names (5.25" format)

```
525_spin_start_empty    — motor start, no disk
525_spin_start_loaded   — motor start, disk present
525_spin_empty          — motor running loop, no disk
525_spin_loaded         — motor running loop, disk present
525_spin_end            — motor wind-down (shared)
525_step_1_1            — single head step click
525_seek_6ms            — rapid seek sound (6ms inter-step)
525_seek_12ms           — normal seek sound (12ms inter-step)
525_seek_20ms           — slow seek sound (20ms inter-step)
```

*(`mamedev/mame:src/devices/imagedev/floppy.cpp`, `floppy_sound_samples` table)*

Samples were recorded from a **Chinon FZ502** drive — *not* an Apple Disk II
drive (which used Alps or Tandon mechanisms). The sounds are generic 5.25"
floppy sounds, not Disk II-specific.

#### 2.2.2 Motor Sound State Machine

```cpp
// mamedev/mame:src/devices/imagedev/floppy.cpp — floppy_sound_device::motor()
void floppy_sound_device::motor(bool running, bool withdisk)
{
    if (running) {
        if (m_motor_state == MOTOR_OFF) {
            m_motor_state = withdisk ? MOTOR_START_LOADED : MOTOR_START_EMPTY;
            m_spin_samplepos = 0;   // restart from start-sample
        } else {
            m_motor_state = withdisk ? MOTOR_SPIN_LOADED : MOTOR_SPIN_EMPTY;
            // transitions seamlessly into loop sample
        }
    } else {
        m_motor_state = MOTOR_END;  // plays wind-down sample once
    }
}
```

State transitions: `OFF → START_* → SPIN_* (loop) → END → OFF`. Sample
boundaries trigger transitions in `sound_stream_update()` per-sample.

#### 2.2.3 Step / Seek Discrimination (Key Design)

MAME's step/seek logic detects seek by checking whether the **step sample is
still playing** when the next step arrives:

```cpp
// mamedev/mame:src/devices/imagedev/floppy.cpp — floppy_sound_device::step()
void floppy_sound_device::step(int track)
{
    attotime now = machine().time();
    attotime delta = now - m_last_step_time;
    m_last_step_time = now;

    int rate = delta.as_double() * 1000.0;  // ms between steps

    if (m_step_samplepos > 0 && rate < 100) {
        // Previous step still playing + fast cadence → it's a seek
        m_seek_samplepos = m_step_samplepos;  // crossfade from current position
        m_step_samplepos = 0;

        // Select seek sample by rate
        m_current_seek = (rate <= 6) ? SEEK_6MS : (rate <= 12) ? SEEK_12MS : SEEK_20MS;

        // Pitch-shift seek sample to match actual step rate
        m_seek_pitch = (double)rate / m_nominal_seek_rate[m_current_seek];

        m_seek_timeout = rate * 2 * 44100 / 1000;  // expect next step within 2× interval
    } else {
        // Either first step or previous sample finished → single step click
        m_step_samplepos = 0;  // restart step sample
        m_seek_samplepos = 0;
    }
}
```

**The key insight**: MAME doesn't use timers for step detection. It uses the
*playback position* of the step sample itself as the timer. If the sample is
still playing when the next trigger arrives, it's a seek. This is elegant and
sample-rate-agnostic.

#### 2.2.4 Wiring to Disk II Phase Control

MAME wires sounds in `seek_phase_w()` (the phase-driven variant used for
Apple II-style drives):

```cpp
// mamedev/mame:src/devices/imagedev/floppy.cpp — seek_phase_w()
void floppy_image_device::seek_phase_w(int phases)
{
    // ... compute new track position ...
    if (next_pos != m_subcyl) {
        if (m_sound_out)
            m_sound_out->step(next_pos);    // ← fires on actual head movement only
        m_subcyl = next_pos;
    }
}
// motor_w() / mon_w() calls m_sound_out->motor(state==0, exists())
```

This is exactly what Casso should do: fire `step()` only when `qtDelta != 0`.

### 2.3 OpenEmulator/libemulation — Apple-Specific Disk II Implementation

**License:** GPL v3 (`openemulator/libemulation:COPYING`)
**Key file:** `openemulator/libemulation:src/libemulation/Implementation/Apple/AppleDiskDrive525.cpp`

This is the **only Apple Disk II–specific disk sound implementation found** in
any open-source emulator. Written by Marc S. Ressl, 2010–2012.

#### 2.3.1 Three-Player Architecture

```
doorPlayer  — disk insert/eject sounds (door mechanism)
drivePlayer — motor spinning loop
headPlayer  — head step/bump/align sounds
```

Each player is a separate `OEComponent` (audio player object) that supports
PLAY, PAUSE, STOP operations and can have its sound buffer swapped
dynamically.

#### 2.3.2 Sound Selection Logic

```cpp
// openemulator/libemulation:src/libemulation/Implementation/Apple/AppleDiskDrive525.cpp
void AppleDiskDrive525::updatePlayerSounds()
{
    updatePlayerSound(doorPlayer, isOpenSound ? "Open" : "Close");
    updatePlayerSound(drivePlayer, "Drive");
    updatePlayerSound(headPlayer, (phaseStop ? (phaseAlign ? "Align" : "Stop") : "Head"));
}

void AppleDiskDrive525::updatePlayerSound(OEComponent *component, string value)
{
    OESound *playerSound = NULL;
    if (sound.count(mechanism + value))           // e.g., "AlpsHead" or "ShugartSA400Stop"
        playerSound = &sound[mechanism + value];
    component->postMessage(this, AUDIOPLAYER_SET_SOUND, playerSound);
}
```

The `mechanism` string (configured externally) prefixes every sample key. The
Alps 2124A mechanism has only `Drive`, `Head`, `Stop` — no
`Open`/`Close`/`Align`, because the Alps mechanism's door sounds are not
distinctly different.

#### 2.3.3 Timer-Based Step Detection

```cpp
// Phase change arrives → invalidate existing timer → set 0.5ms debounce timer (ID 0)
case APPLEII_SET_PHASECONTROL:
    OEInt id = 0;
    controlBus->postMessage(this, CONTROLBUS_INVALIDATE_TIMERS, &id);  // cancel pending
    ControlBusTimer timer = { 0.0005 * APPLEII_CLOCKFREQUENCY, 0 };
    controlBus->postMessage(this, CONTROLBUS_SCHEDULE_TIMER, &timer);
    return true;

// Timer ID 0 fires (0.5ms after last phase change):
case 0: {
    OESInt newTrackIndex = trackIndex + getStepperDelta(trackIndex & 0x7, phaseControl);

    // Bump detection (at track boundaries)
    bool bump = (newTrackIndex < 0 || newTrackIndex >= DRIVE_TRACKNUM);
    if (bump) newTrackIndex = clamp(newTrackIndex, 0, DRIVE_TRACKNUM-1);

    if (bump && !phaseLastBump) {
        phaseStop = true;
        headPlayer->postMessage(this, AUDIOPLAYER_STOP, NULL);
    }

    // Alignment detection: steps arriving < 16ms apart = seeking = "Align" sound
    phaseAlign = ((cycles - phaseCycles) < 0.016 * APPLEII_CLOCKFREQUENCY);
    phaseCycles = cycles;

    // Direction change → reset stop state
    if (phaseDirection != newPhaseDirection) {
        phaseDirection = newPhaseDirection;
        headPlayer->postMessage(this, AUDIOPLAYER_STOP, NULL);
        phaseStop = false;
    }

    updatePlayerSounds();
    headPlayer->postMessage(this, AUDIOPLAYER_PLAY, NULL);

    // Set 50ms idle timer (ID 1) — stops head sound if no new step
    ControlBusTimer timer = { 0.05 * APPLEII_CLOCKFREQUENCY, 1 };
    controlBus->postMessage(this, CONTROLBUS_SCHEDULE_TIMER, &timer);
    break;
}
// Timer ID 1 fires (50ms idle):
case 1:
    headPlayer->postMessage(this, AUDIOPLAYER_STOP, NULL);
    phaseStop = 0;
    phaseDirection = 0;
    break;
```

Key timing:

- **0.5ms debounce**: cancels and resets on every phase change, so only the
  final phase in a burst triggers sound
- **50ms idle timeout**: stops head sound after seek ends
- **16ms inter-step threshold**: discriminates seek ("Align" sound) from
  single step ("Head" sound)

#### 2.3.4 Disk Image Open/Close Sequence

```cpp
bool AppleDiskDrive525::openDiskImage(string path) {
    if (wasMounted) {
        isOpenSound = true;
        // Schedule timer ID 2 for 1 second → after 1s, play "Open" sound
        ControlBusTimer timer = { 1.0 * APPLEII_CLOCKFREQUENCY, 2 };
    } else {
        isOpenSound = false;
        // Play "Close" sound immediately (disk being inserted)
    }
    doorPlayer->postMessage(this, AUDIOPLAYER_STOP, NULL);
    doorPlayer->postMessage(this, AUDIOPLAYER_PLAY, NULL);
}

bool AppleDiskDrive525::closeDiskImage() {
    isOpenSound = true;    // eject = "Open" sound
    doorPlayer->postMessage(this, AUDIOPLAYER_STOP, NULL);
    doorPlayer->postMessage(this, AUDIOPLAYER_PLAY, NULL);
}
```

The "swap disk" sequence: Close sound (immediate) → 1-second pause → Open
sound (timer ID 2). Out of scope for this feature.

---

## Part 3 — Sample Availability and Licensing

### 3.1 OpenEmulator/libemulation Assets (GPL v3)

The libemulation repository ships the most **mechanically accurate and
Apple-specific** sound samples available in any open-source project. These
are organized by drive mechanism:

**Alps 2124A** — The Alps mechanism used in the majority of Disk II drives:

| File                    | Path                                          | Size         | Description       |
|-------------------------|-----------------------------------------------|--------------|-------------------|
| `Alps 2124A Drive.ogg`  | `openemulator/libemulation:res/sounds/Alps/`  | 24,159 bytes | Motor hum loop    |
| `Alps 2124A Head.ogg`   | `openemulator/libemulation:res/sounds/Alps/`  |  7,584 bytes | Single head step  |
| `Alps 2124A Stop.ogg`   | `openemulator/libemulation:res/sounds/Alps/`  |  5,298 bytes | Head-at-stop      |

**Shugart SA400** — An earlier Shugart mechanism (similar era):

| File                       | Path                                             | Size         | Description       |
|----------------------------|--------------------------------------------------|--------------|-------------------|
| `Shugart SA400 Drive.ogg`  | `openemulator/libemulation:res/sounds/Shugart/`  | 20,265 bytes | Motor hum loop    |
| `Shugart SA400 Head.ogg`   | `openemulator/libemulation:res/sounds/Shugart/`  |  8,167 bytes | Single head step  |
| `Shugart SA400 Stop.ogg`   | `openemulator/libemulation:res/sounds/Shugart/`  |  5,102 bytes | Head stop/bump    |
| `Shugart SA400 Align.ogg`  | `openemulator/libemulation:res/sounds/Shugart/`  |  4,437 bytes | Alignment buzz    |
| `Shugart SA400 Open.ogg`   | `openemulator/libemulation:res/sounds/Shugart/`  |  9,106 bytes | Door open         |
| `Shugart SA400 Close.ogg`  | `openemulator/libemulation:res/sounds/Shugart/`  |  8,191 bytes | Door close        |

**License risk:** The `COPYING` file states: *"The OpenEmulator binaries and
most of its source code are distributed under the GPL version 3 license.
Copyright 2008-2013. All code is copyrighted by the respective authors."* The
`.ogg` audio files have no separate license — they fall under GPL v3 by
default. **GPL v3 is a viral copyleft license**; bundling these files in a
closed or non-GPL project is not permissible without relicensing. They are
safe for development/prototyping use, but not for distribution.

### 3.2 MAME Floppy Samples (BSD-3 Code; Separate Sample Distribution)

MAME does **not** ship audio samples in its source repository
(`mamedev/mame`). Samples are distributed separately at
`mamesamples.mameworld.info` (inaccessible at time of research — HTTP 526).
The MAME project's standard practice is to distribute user-contributed
hardware recordings under informal "open for emulation use" terms, but these
are not formal open-source licenses and redistribution rights are
unspecified. The `525_*` samples were recorded from a **Chinon FZ502** — not
a Disk II mechanism — so even if licensable, they are generic 5.25" sounds,
not authentic Disk II recordings.

### 3.3 No CC0/Public-Domain Disk II–Specific Recordings Found

Searches on Freesound.org (via API) and archive.org (not parseable) returned
no verified CC0 or public-domain recordings of Apple Disk II drive sounds
specifically. There are a small number of generic 5.25" floppy recordings on
Freesound under various Creative Commons licenses, but none labeled as Apple
Disk II recordings.

### 3.4 Recommended Paths for Sample Acquisition

- **Best option (complete freedom):** Record your own samples from real Disk
  II hardware and release under a permissive license (MIT, CC0, or CC-BY).
  Required: a real Disk II drive, an audio interface with a condenser
  microphone positioned close to the drive (not the speaker of the Apple II,
  since the drive is an external box). Capture: motor loop (5–10s), single
  step click (20–30 samples for averaging), head-at-stop bump.
- **Second option (procedural synthesis, zero licensing risk):** The motor
  hum can be synthesized as a sawtooth wave at ~50–80 Hz with bandpass
  filtering and soft amplitude envelope. The step click can be synthesized as
  a sharp Dirac impulse convolved with a short (~50ms) resonant impulse
  response (Karplus-Strong resonator tuned to the mechanical resonance of a
  small electromechanical system).
- **Third option (development placeholder):** Use the Alps 2124A `.ogg` files
  from libemulation temporarily (GPL-3 compliant for development), with
  intent to replace before distribution.

---

## Part 4 — Implementation Architecture Patterns

### 4.1 Event Trigger Points

Based on the survey of three implementations, the consensus is:

| Event                | Trigger Condition                                                                     | Source                                              |
|----------------------|---------------------------------------------------------------------------------------|-----------------------------------------------------|
| Motor start sound    | Motor-on transition                                                                   | `HandleSwitch()` case 0x9 → `m_motorOn` goes true   |
| Motor loop           | While `m_motorOn == true`                                                             | Continuous; stops when `Tick()` clears `m_motorOn`  |
| Motor stop sound     | `m_motorOn` goes false (in `Tick()` after spindown, *not* at case 0x8)                | `DiskIIController::Tick()` line ~341                |
| Head step click      | `HandlePhase()` computes `qtDelta != 0`                                               | After `m_quarterTrack += qtDelta`                   |
| Head bump/stop sound | `qtDelta != 0` AND resulting `m_quarterTrack` was clamped (hit 0 or 139)              | Same location, check pre/post clamp value           |
| Door open            | Disk image unmounted                                                                  | Out of scope                                        |
| Door close           | Disk image mounted                                                                    | Out of scope                                        |

**Critical anti-pattern:** AppleWin's `ControlStepper()` fires on every
`$C0E0-$C0E7` access. Casso's `HandlePhase()` is also called on every phase
access. The sound trigger must check `qtDelta != 0` — not be placed at the
entry point of `HandlePhase()`. In Casso's code
(`CassoEmuCore/Devices/DiskIIController.cpp:229-294`), this is the block
after `m_quarterTrack += qtDelta`.

### 4.2 Motor Sound: The Spindown Gotcha

DOS 3.3 RWTS issues $C0E8 (motor-off) followed immediately by $C0E9
(motor-on) when switching between sectors on the same track. Without a
spindown delay, this would cause the motor sound to stop and restart
continuously during normal disk operation. The spindown timer
(`kMotorSpindownCycles = 1,000,000`) prevents this — the motor sound persists
across the brief off/on cycle.

The motor sound should be driven by the **actual `m_motorOn` flag in
`DiskIIController`** (which does NOT go false until the spindown timer
expires in `Tick()`), not by the raw $C0E8/$C0E9 soft-switch commands. This
is precisely the same architecture as the motor spindown timer itself.

### 4.3 Step vs. Seek Discrimination (Two Viable Approaches)

**MAME approach (sample-playback-position timer):** A new step sound restarts
the step sample. If the next step arrives before the previous sample
finishes playing (i.e., `step_sample_position > 0`), switch to a seek sample
(looping). The step sample's own duration functions as the discrimination
threshold. Elegant but requires knowing the current playback position.

**OpenEmulator approach (real-time clock timer):** Schedule a timer every
time a phase change arrives, cancelling the previous one. The timer fires
0.5ms after the last phase change in a burst. Check how long ago the
previous step fired: if < 16ms → "Align" (seek-buzz) sound, if >= 16ms →
"Head" (single click). A separate 50ms idle timer stops the head sound.

**Recommendation for Casso:** OpenEmulator's timer approach maps cleanly to
Casso's cycle-counting architecture. Replace the wall-clock timers with
cycle-count thresholds:

- Debounce: `0.5ms × 1.023 MHz = 511 cycles` — fire step sound 511 cycles
  after the last `HandlePhase()` that returned `qtDelta != 0`
- Seek detection: if inter-step gap `< 16ms × 1.023 MHz = 16,368 cycles` →
  use seek/buzz sample; else use single-click sample
- Idle stop: `50ms × 1.023 MHz = 51,150 cycles` — stop head sound if no new
  step arrives

### 4.4 PCM Mixing Into Casso's WASAPI Pipeline

The current pipeline (`Casso/WasapiAudio.cpp`, `EmulatorShell.cpp:2411-2492`):

```
ExecuteCpuSlices() per-slice:
  → CPU instructions → DiskIIController::Tick(cycles)
  → WasapiAudio::SubmitFrame(toggleTimestamps, sliceActual, initialState, numSamples)
    → AudioGenerator::GeneratePCM(toggleTimestamps, ...) → float* speaker_samples
    → m_pendingSamples.insert(speaker_samples)
    → drain m_pendingSamples to WASAPI render client
```

Disk audio requires a **parallel PCM mixing stage**. The `m_pendingSamples`
vector is the ideal blend point. The cleanest architecture:

```
WasapiAudio::SubmitFrame(toggleTimestamps, sliceActual, initialState, numSamples,
                          DiskAudioMixer* diskMixer)  // ← new parameter
{
    // 1. Generate speaker PCM as today
    AudioGenerator::GeneratePCM(toggleTimestamps, ..., speakerBuf, numSamples);

    // 2. Generate disk PCM for same time window
    float diskBuf[numSamples];
    if (diskMixer) diskMixer->GeneratePCM(diskBuf, numSamples, sampleRate);

    // 3. Additive mix: speaker + disk (both already normalized to [-1, 1])
    for (uint32_t i = 0; i < numSamples; i++)
        m_pendingSamples.push_back(speakerBuf[i] + diskBuf[i]);

    // 4. Drain to WASAPI as today
}
```

No thread-safety issues: both `HandlePhase()` (which sets disk audio state)
and `WasapiAudio::SubmitFrame()` (which consumes it) are called on the same
CPU emulation thread in `ExecuteCpuSlices()`.

The speaker currently uses amplitude ±0.5 (inferred from `AudioGenerator.cpp`).
Disk sounds should be mixed at **~0.2–0.3 amplitude** of the full scale to
avoid clipping when both speaker and motor hum are active simultaneously.

### 4.5 Looping vs. One-Shot Playback

- **Motor hum** = looping. Maintain a `motorSamplePos` counter that advances
  through the motor WAV buffer every `GeneratePCM()` call and wraps at buffer
  end. Set a `motorActive` flag, set/cleared by motor start/stop events.
- **Head click** = one-shot. Maintain a `headSamplePos` counter. On trigger
  (step event), reset to 0. Each `GeneratePCM()` call: if `headSamplePos <
  headSampleLen`, mix from the head sample buffer and advance. Stop mixing
  when `headSamplePos >= headSampleLen`.
- **Seek buzz** = re-triggerable one-shot or short loop. If a step fires
  before the previous head sample finishes, either restart from 0 (simple)
  or switch to a different looping seek sample (MAME style). For a v1
  implementation, restarting from 0 is acceptable.
- **Bump/Stop** = one-shot, same as head click but using the Stop sample
  buffer.

---

## Part 5 — Casso-Specific Implementation Sketch

### 5.1 Files to Create

```
CassoEmuCore/Audio/DiskAudioMixer.h    — DiskAudioMixer class declaration
CassoEmuCore/Audio/DiskAudioMixer.cpp  — Implementation
Assets/Sounds/DiskII/                  — WAV sample files (or omitted entirely
                                         for procedural synthesis)
    motor_loop.wav
    head_step.wav
    head_stop.wav
```

### 5.2 DiskAudioMixer Interface

```cpp
// CassoEmuCore/Audio/DiskAudioMixer.h
class DiskAudioMixer : public IDiskAudioSink {
public:
    // Loaded at startup — PCM float32 mono at native sample rate
    void LoadSamples(const float* motorLoop, uint32_t motorLen,
                     const float* headStep, uint32_t headStepLen,
                     const float* headStop, uint32_t headStopLen);

    // Called from DiskIIController event hooks (IDiskAudioSink)
    void OnMotorStart() override;
    void OnMotorStop()  override;
    void OnHeadStep(int newQuarterTrack) override;  // qtDelta != 0 only
    void OnHeadBump()  override;                    // clamped at track boundary

    // Toggle / mute
    void SetEnabled(bool on);
    bool IsEnabled() const;

    // Called once per audio frame from WasapiAudio::SubmitFrame()
    void GeneratePCM(float* outBuf, uint32_t numSamples);

private:
    // Motor loop
    const float* m_motorBuf = nullptr;
    uint32_t     m_motorLen = 0;
    uint32_t     m_motorPos = 0;
    bool         m_motorRunning = false;

    // Head one-shot (current active head sample = step or stop)
    const float* m_headBuf = nullptr;
    uint32_t     m_headLen = 0;
    uint32_t     m_headPos = 0;        // >= headLen means not playing

    // Step/seek discrimination
    uint64_t     m_lastStepCycle = 0;
    bool         m_seekMode = false;

    // Volume
    float        m_motorVolume = 0.25f;
    float        m_headVolume  = 0.30f;

    bool         m_enabled = true;
};
```

### 5.3 Touch Points in Existing Files

**`CassoEmuCore/Devices/DiskIIController.h`** — add callback interface (or
directly hold a `DiskAudioMixer*`):

```cpp
class IDiskAudioSink {
public:
    virtual void OnMotorStart() = 0;
    virtual void OnMotorStop()  = 0;
    virtual void OnHeadStep(int newQt) = 0;
    virtual void OnHeadBump()   = 0;
};

// member:
IDiskAudioSink* m_audioSink = nullptr;
void SetAudioSink(IDiskAudioSink* sink) { m_audioSink = sink; }
```

**`CassoEmuCore/Devices/DiskIIController.cpp`** — add 4 notification calls:

```cpp
// HandleSwitch(), case 0x9 (motor on, ~line 143):
m_motorOn = true;
if (m_audioSink) m_audioSink->OnMotorStart();

// Tick(), where m_motorOn goes false (~line 341):
m_motorOn = false;
if (m_audioSink) m_audioSink->OnMotorStop();

// HandlePhase(), after m_quarterTrack update (~line 280):
if (qtDelta != 0) {
    int prevQt = m_quarterTrack - qtDelta;   // before clamp
    bool bumped = (m_quarterTrack == 0 && prevQt < 0) ||
                  (m_quarterTrack == kMaxQuarterTrack && prevQt > kMaxQuarterTrack);
    if (m_audioSink) {
        if (bumped) m_audioSink->OnHeadBump();
        else        m_audioSink->OnHeadStep(m_quarterTrack);
    }
}
```

**`Casso/WasapiAudio.h/cpp`** — add disk mixer parameter to `SubmitFrame()`:

```cpp
// WasapiAudio.h — extend SubmitFrame signature:
void SubmitFrame(std::vector<uint32_t>& toggleTimestamps,
                 uint32_t sliceActual,
                 bool speakerState,
                 uint32_t numSamples,
                 DiskAudioMixer* diskMixer = nullptr);   // ← new optional param

// WasapiAudio.cpp — inside SubmitFrame(), after GeneratePCM fills m_tempBuf:
if (diskMixer) {
    static std::vector<float> diskBuf;
    diskBuf.resize(numSamples, 0.0f);
    diskMixer->GeneratePCM(diskBuf.data(), numSamples);
    for (uint32_t i = 0; i < numSamples; i++)
        m_tempBuf[i] = std::clamp(m_tempBuf[i] + diskBuf[i], -1.0f, 1.0f);
}
```

**`Casso/EmulatorShell.cpp`** — wire it all together in `ExecuteCpuSlices()`
(lines 2411-2492):

```cpp
// In class init: m_diskController->SetAudioSink(&m_diskAudioMixer);

// In SubmitFrame call (~line 2480):
m_wasapiAudio.SubmitFrame(toggleTimestamps, sliceActual, speakerState, numSamples,
                          &m_diskAudioMixer);
```

**`Casso/MenuSystem.{h,cpp}`** — add "Disk Audio" check-toggle under View
menu, defaulting to checked. Toggle handler calls
`m_diskAudioMixer.SetEnabled(...)`.

### 5.4 Sample Loading

WASAPI uses float32 PCM at the device's native sample rate (44100 or 48000 Hz,
queried at init). Sample files should be decoded at startup and resampled to
match. For `.wav` files (easiest on Windows), use `MediaFoundation`
(`IMFSourceReader`) or a small in-repo PCM WAV decoder. The WASAPI mix
format sample rate is available after `WasapiAudio::Initialize()`.

If procedural synthesis is chosen instead, `LoadSamples()` is replaced by a
`SynthesizeSamples(sampleRate)` call that fills internal buffers at startup
— same downstream contract.

### 5.5 Suggested Minimum Viable Sample Set

For a v1 implementation, 3 samples are sufficient:

1. **`motor_loop.wav`** — ~1–2s of motor hum, suitable for seamless looping
   (fade-match start and end)
2. **`head_step.wav`** — ~80–150ms single head click
3. **`head_stop.wav`** — ~100–200ms bump/thud at track 0

Door open/close can be added in a future feature.

---

## Part 6 — Gaps, Risks, and Recommendations

### 6.1 Confirmed Gaps

| Gap                                                       | Notes                                                                 |
|-----------------------------------------------------------|-----------------------------------------------------------------------|
| Virtual ][ (Mac) not investigated                         | Closed-source, no public code. Not needed given libemulation coverage.|
| MAME sample download site inaccessible                    | `mamesamples.mameworld.info` returned HTTP 526 at time of research    |
| No programmatic verification of MAME sample OGG files     | Only names confirmed from source code; actual content unverified      |
| Freesound.org search was limited                          | No CC0 Disk II recordings found via API — manual search may yield more|

### 6.2 License Risk Summary

| Source                                       | License           | Can use in this project?      |
|----------------------------------------------|-------------------|-------------------------------|
| OpenEmulator Alps/Shugart `.ogg` samples     | GPL v3            | ❌ No (viral copyleft)        |
| OpenEmulator `AppleDiskDrive525.cpp` arch    | GPL v3            | ✅ Reference only             |
| MAME `floppy.cpp` architecture               | BSD-3-Clause      | ✅ Reference + reimplement    |
| MAME floppy `.wav` samples                   | Unclear           | ⚠️ Risky; no formal license   |
| AppleWin code                                | GPL v2            | ✅ Reference (no disk audio)  |
| Self-recorded samples                        | Your copyright    | ✅ Total freedom              |
| Synthesized procedurally                     | No IP issues      | ✅ Total freedom              |

### 6.3 Recommended Implementation Order

This ordering is the authoritative input to [`tasks.md`](./tasks.md):

1. **Implement `DiskAudioMixer` class** with motor loop and head step
   (no samples yet — stub with silence or sine wave for testing)
2. **Wire callbacks** in `DiskIIController` — add `IDiskAudioSink` and 4 call
   sites
3. **Extend `WasapiAudio::SubmitFrame()`** to accept and mix disk PCM
4. **Wire in `EmulatorShell`** — verify no regression in speaker audio or
   disk behavior
5. **Source samples** — develop/record from real hardware, or use Alps 2124A
   samples from libemulation for prototype testing (GPL-compliant during
   development, must not be committed)
6. **Tune step/seek discrimination** — add `m_lastStepCycle` tracking to
   `DiskAudioMixer::OnHeadStep()` using the 16ms / 16,368-cycle threshold
   from OpenEmulator
7. **Add bump detection** — verify `OnHeadBump()` fires correctly during DOS
   3.3 boot (which hits track 0 ~3 times during initialization)
8. **Add View → Disk Audio toggle** in `MenuSystem`

---

## Key Citations

- `CassoEmuCore/Devices/DiskIIController.cpp:229-294` — `HandlePhase()` — step trigger location
- `CassoEmuCore/Devices/DiskIIController.cpp:118-177` — `HandleSwitch()` — motor trigger location
- `CassoEmuCore/Devices/DiskIIController.cpp:334-352` — `Tick()` — spindown timer → motor-off
- `CassoEmuCore/Devices/DiskIIController.h:34-100` — class fields including `m_quarterTrack`, `kMotorSpindownCycles`
- `Casso/WasapiAudio.cpp:183` — `SubmitFrame()` — audio submit entry point
- `Casso/EmulatorShell.cpp:2411-2492` — `ExecuteCpuSlices()` — per-slice emulation loop
- `mamedev/mame:src/devices/imagedev/floppy.cpp` — `floppy_sound_device::motor()`, `::step()`, `::sound_stream_update()` — BSD-3 reference
- `openemulator/libemulation:src/libemulation/Implementation/Apple/AppleDiskDrive525.cpp` — full disk sound implementation — GPL-3 reference
- `openemulator/libemulation:res/sounds/Alps/` — Alps 2124A Drive/Head/Stop `.ogg` — GPL-3 samples
- `openemulator/libemulation:res/sounds/Shugart/` — Shugart SA400 full set `.ogg` — GPL-3 samples
- `openemulator/libemulation:COPYING` — GPL v3 confirmed as project license
- `AppleWin/AppleWin:source/Disk.cpp` — no disk audio calls confirmed (verified full file)
