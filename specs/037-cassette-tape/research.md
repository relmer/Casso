# Research: Apple II Cassette Tape Support

**Feature**: 037-cassette-tape | **Date**: 2026-10-02 | **Spec**: [spec.md](spec.md)

Paths are relative to the repository root unless they start with a project
folder (`CassoEmuCore\`, `CassoCore\`, `UnitTest\`).

## R1. How $C060 and $C020 decode today

**Findings**

- The bus (`CassoEmuCore\Core\MemoryBus.h:58-172`) routes $C000-$FFFF through
  a per-byte device map. Each device reports one contiguous range; on overlap
  the device with the lowest start address wins (`MemoryBus.h:142-158`). An
  unmapped $C0xx read returns the floating-bus byte (`MemoryBus.cpp:81-107`).
- Machine models are C++ classes (`Machines\MachineDefinitions.cpp`):
  Apple2 -> Apple2Plus -> Apple2e -> {Apple2c, Apple2eEnhanced}. Motherboard
  devices come from `IMachine::GetInternalDevices()`; `Apple2c` inherits the
  //e list.

| Address | ][ / ][+ today | //e today | //c today |
|---|---|---|---|
| $C020 read/write | unmapped: floating bus, write ignored | inside `Apple2eKeyboard` ($C000-$C063), returns 0 (`Apple2eKeyboard.cpp:96-98`) | same as //e |
| $C060 | unmapped: floating bus | `Apple2eKeyboard` returns 0 outside //c mode (`Apple2eKeyboard.cpp:84-99`) | RD80SW (case switch) in bit 7 |
| $C061-$C063 | `AppleGamePort` buttons PB0-PB2 | `Apple2eKeyboard::ReadButton`: Open Apple, Closed Apple, Shift (PB2) | same; $C063 is the mouse button |
| $C068 | inside `AppleGamePort` ($C061-$C070), returns 0 | `Apple2eSoftSwitchBank`, returns 0 | same |

- Nothing models PB3, cassette in, or cassette out on any model.
- On the ][, ][+ and //e the cassette port is part of the motherboard: two
  1/8-inch jacks on the back panel, with no card. The recorder is an external
  peripheral plugged into those jacks, so the machine config, the slot list
  and the machine JSON stay unchanged. The //c dropped the jacks.

**Decision**

- Add one new motherboard device, `CassettePort`, owning $C020-$C02F. That
  range is free on the ][ and ][+. On the //e, `Apple2eKeyboard` starts lower
  and so owns the byte. It already forwards $C030-$C03F and $C061-$C063 to
  sibling devices, and it gains the same forwarding for $C020-$C02F and
  $C060/$C068.
- On the ][ and ][+, `AppleGamePort` widens its range down to $C060 and
  forwards $C060 and $C068 to `CassettePort`. Starting at $C060 is safe,
  because nothing else claims that byte.
- Cassette in sets bit 7; bits 0-6 keep each model's current behavior
  (floating bus on the ][ and ][+, 0 on the //e).
- A $C020 read or write toggles the output flip-flop, as on hardware, where any
  access toggles it.
- The //c gets no `CassettePort`: a new `IMachine::HasCassettePort()`
  capability, false on `Apple2c`, follows the `HasAnnunciators` precedent and
  gates both the device and the tape deck UI. `Apple2c` keeps its RD80SW
  meaning of $C060.

**The //e PB3 overlap (FR-010).** On the //e, $C060 bit 7 is the cassette
input, and it is also how software reads a fourth game button wired to that
line. A game that polls $C060 for a button would look exactly like a tape
loader. The fast-loading trigger therefore never relies on access alone: it
requires the deck to be playing or recording AND an access within the window
(R3). Polling with the deck stopped or empty never starts it.

**Alternatives considered**

- A single device spanning $C020-$C06F: rejected, because lowest-start-wins
  would hand it the speaker ($C030) and every soft switch in between.
- Two cassette devices ($C020-$C02F and a one-byte $C060): rejected; $C068
  would still sit inside `AppleGamePort`, and the //e routes both through its
  keyboard device anyway.

## R2. Cycle timestamps

**Findings**: Devices timestamp from one of two CPU counters
(`CassoCore\Cpu6502.h:36-50`). `GetCycleCounterPtr()` advances only at
instruction boundaries; `GetBusCyclePtr()` is accurate to the access in flight
(issue #67) and is what the paddle timer and the Disk II use
(`MachineBuilder.cpp:1441,1491,1496`).

**Decision**: `CassettePort` takes `GetBusCyclePtr()` via the existing
`SetCpuCycleSource (const uint64_t *)` convention. Tape time is
`busCycle - playStartCycle`, which advances only while the CPU runs, so pausing
holds the position (FR-003; the CPU loop blocks while paused,
`CpuManager.cpp:425-447`).

## R3. Fast loading: Maximum-speed override

**Findings**

- `SpeedMode {Authentic, Double, Maximum}` (`Ui\UiCommandTypes.h:43`) lives in
  `std::atomic<SpeedMode> CpuManager::m_speedMode`.
- Maximum just drops the per-frame timer wait (`CpuManager.cpp:496-508`).
- Frames are published at a 60 Hz wall-clock cap
  (`EmulatorShellPresent.cpp:1164-1173`), which already provides frame skipping.
- Only three places read the speed at runtime: the pacing loop,
  `ExecuteCpuSlices` (`EmulatorShellCpuThread.cpp:802`), and
  `ShouldPublishFrame`. The menu and Settings never read it back; Settings
  shows its own saved `speedMode`.
- No temporary speed override exists anywhere in the tree (no disk fast-load).
  This is the first.

**Decision**

- `CpuManager` gains `std::atomic<bool> m_maximumOverride` and
  `GetEffectiveSpeedMode()`, which returns Maximum while the override is set and
  `m_speedMode` otherwise. The three runtime readers switch to it. Since
  `m_speedMode` is never written, the user's setting is restored automatically,
  and the menu and Settings keep showing it.
- A pure `TapeTurboGovernor` decides the override, on the CPU thread once per
  slice. The override is on when all of these hold:
  - the preference is on
  - the deck is playing or recording
  - the last cassette access (a $C060 read or $C020 toggle while the deck is
    moving) is within 100 ms of emulated time, computed from the machine clock
- It turns off as soon as any of those fails: end of tape, stop or eject, or no
  access for 100 ms.
- Reset and machine change stop the deck, so the override drops on the next
  slice.

**Audio during the override (FR-011)**: Maximum today only drops excess audio,
because `WasapiAudio::SubmitFrame` skips slices while more than three frames
are pending (`WasapiAudio.cpp:538-548`). That is choppy, not muted. Muting
through the user's master mute would change visible UI state. Instead
`WasapiAudio` gains a hidden `SetSuppressed (bool)` that submits silence, and
the shell sets it from the override.

**Alternatives considered**: calling `SetSpeedMode(Maximum)` and saving or
restoring the old value races with the user changing speed mid-load, and was
rejected by clarification. A separate turbo path was also rejected by
clarification.

## R4. Reading tape files (WAV, AIFF, MP3)

**Findings**

- No RIFF reader exists. `AssetBootstrap::WritePcmAsWav`
  (`AssetBootstrap.cpp:3310`) writes 16-bit mono WAV straight to disk with no
  seam.
- Media Foundation decode from a path is already used, in
  `Audio\PrinterAudioSource.cpp` (`MFCreateSourceReaderFromURL`,
  `MFStartup (MF_VERSION, MFSTARTUP_LITE)`) and `Disk2AudioSource.cpp`.
- MF headers are in `CassoEmuCore\Pch.h:17-28`.
- File seam: `IDiskFileIo` (`Devices\Disk\IDiskFileIo.h:62`), which has
  `ReadAllBytes`, `WriteAllBytes`, `ReplaceAtomically` and `Exists`, with
  `UnitTest\EmuTests\FakeDiskFileIo.h`.

**Decision**

- `WavCodec` and `AiffCodec` are pure byte parsers: they take a byte buffer and
  return mono float PCM plus the sample rate. They cover:
  - PCM 8/16/24/32-bit
  - IEEE float 32/64
  - `WAVE_FORMAT_EXTENSIBLE`
  - AIFF and AIFF-C (`NONE`/`sowt`)
  - 8 kHz-96 kHz
- Stereo is mixed to mono. Inverted channels would cancel, so if the mix is far
  quieter than either channel the codec falls back to the louder channel.
- They need no OS calls, so unit tests drive them with synthetic buffers.
- MP3 goes through `ITapeAudioDecoder` and `MfTapeAudioDecoder`, which decode
  to float PCM using the same MF source-reader pattern as `PrinterAudioSource`.
  This needs `MFCreateMFByteStreamOnStream` over the bytes read through
  `IDiskFileIo`, so the seam stays file-free. The tests use a fake decoder.
  Media Foundation is part of the Windows SDK baseline, so no new dependency.
- `WavCodec` also writes: 16-bit mono PCM to a byte buffer, saved via
  `IDiskFileIo::ReplaceAtomically`.

**Alternatives considered**

- MF for WAV too: rejected, because it is untestable without files and hides
  format failures.
- Reusing `WritePcmAsWav`: it writes to disk directly with no seam. It stays as
  is (never delete library code); the new codec is the testable path.

## R5. Turning audio into level transitions (FR-002, FR-005)

**Decision**: `TapeSignalDecoder` is a pure function from float PCM and sample
rate to a sorted list of transition times, as fractional sample indices, plus
the starting level. It runs at insert time, so playback is a cursor walk. Only
the transition list is kept: a 16 KB load is about 130,000 edges (about 1 MB),
so the decoded PCM is dropped once decoding finishes.

1. DC removal: a one-pole high-pass at about 20 Hz. The Apple tones are
   770 Hz-2.5 kHz, far above it.
2. Optional smoothing: a short low-pass at about 6 kHz, applied only when the
   sample rate is at least 22 kHz, to reject hiss without moving edges.
3. Adaptive hysteresis: a running peak envelope (fast attack, about 50 ms
   release). The level flips when the signal crosses +h or -h, where h is
   about 15% of the envelope, with an absolute floor so silence and hiss
   produce no transitions.
4. Linear interpolation of each crossing between samples, for sub-sample edge
   timing.

The guest's timing loop does all bit decoding; the decoder knows only "level
changed here". Speed drift needs no handling of its own: the ROM measures each
half-cycle independently, so a ±3% drift shifts every period by the same
proportion, well inside the ROM's 1-bit/0-bit threshold.

**Playback mapping**: `levelAt(cycle)` converts
`sample = (cycle - playStartCycle) * sampleRate / cpuClockHz + startSample`,
then advances a cursor through the transition list (amortized O(1); a binary
search re-seeks after rewind). `cpuClockHz` comes from the machine's timing
config, not a constant.

**Alternatives considered**: a plain zero crossing without hysteresis chatters
on noise and fails SC-003. Decoding Apple bits in Casso is forbidden by FR-004.

## R6. Recording (FR-006, FR-007, FR-007a)

**Decision**

- `CassettePort` appends the absolute bus cycle of every $C020 access to the
  deck's capture while record is armed. Without an armed deck, accesses toggle
  the flip-flop and are otherwise discarded, as with a disconnected recorder.
- On stop or eject, `TapeRecorder` renders the toggles as a square wave at the
  tape's own sample rate, at 80% full scale. It re-reads the WAV through
  `IDiskFileIo` and splices the square wave in from the record start position,
  overwriting and extending past the end if needed. It then writes the WAV
  back, re-runs the decoder, and drops the PCM again.
- A new blank tape is a zero-length 44.1 kHz 16-bit mono WAV. The user picks
  its file name through `IHostDialogs::PickFileToSave`, with the default folder
  `Documents\Casso Tapes`.
- Record is disabled for MP3, AIFF and read-only files (`IDiskFileIo::Stat`).
  This matches a tape whose tab has been broken out.

**Alternatives considered**: writing on every toggle means far too many file
writes. Recording at 8-bit was rejected because 16-bit is what other tools
expect most widely.

## R7. Tape deck UI

**Findings**

- Drive widgets: state in `Ui\DriveWidgetState.h:48`, drawing in
  `Ui\Chrome\DriveWidget.h:38`, both owned by the shell and laid out in the
  drive band (`Shell\EmulatorShellChrome.cpp:79-159`, with the centering math in
  `Shell\Layout\DriveRowLayout.h`).
- Per-model omission happens in the drive band (`EmulatorShell.cpp:1082-1110`
  hides drives with no controller or no //c external drive). The toolbar never
  reflows; missing devices show disabled.
- Insert goes through an MRU picker (`AssetBootstrap::PromptInsertDiskMru`) and
  `IHostDialogs`. Drag-and-drop goes through `DxuiDragDropTarget` with
  hit-rect tags.
- New blank disk is modeled by `CreateDiskDialog` and `FileBrowseModel`.
- There is no transport button or progress-bar widget anywhere. The nearest
  analog is the drive head bar (a rail with a lit core).
- Glyphs come from `Core\UnicodeSymbols.h` (`s_kpszMdl2Play` U+E768 exists);
  there are no stop, record, rewind or eject glyphs yet.

**Decision**

- A `TapeDeckState` model (UI-thread fields plus CPU-thread atomics, like
  `DriveWidgetState`) and a `TapeDeckWidget : IDxuiControl` drawn in the drive
  band, to the left of the drives, using the same caption column, name row and
  rail.
- Controls are a row of icon buttons: rewind, play, stop, record, eject. A
  progress rail shows position over length, with an `m:ss / m:ss` readout.
- The name row works like a drive's: clicking it opens the tape MRU picker,
  dropping a file inserts it, and it marquees when too long.
- The record button shows a disabled state for a protected tape. Pressing
  record arms it; recording starts and stops with play and stop, as on a real
  deck.
- On models where `HasCassettePort()` is false, the widget is hidden, following
  the drive-band precedent.
- Menu access: a Tape submenu (insert, new blank tape, eject, rewind) next to
  the disk items, using the same routing as the Disk menu
  (`WindowCommandManager`).
- Every glyph is picked from a rendered MDL2 sheet before use, never guessed,
  and added to `UnicodeSymbols.h`.

**3D desk scene**: Apple never sold a tape drive. The manuals called for any
portable cassette recorder, and the Panasonic RQ-309DS was the one Apple
recommended by name. The desk scene therefore gets an accurate model of the
Panasonic RQ-309DS, built from reference photos and measurements:
- a new `CassetteRecorder` `DeskDeviceKind` (the RQ-309DS)
- a hand-authored `Resources\Models\CassetteRecorder\CassetteRecorder.mesh`,
  like the Disk II and ImageWriter meshes
- a desk placement in `DeskSceneLayout`
- hit regions in `DeskSceneHitTester` for its piano keys (record, play,
  rewind, stop/eject), its cassette door (insert) and a tape counter

The flat widget is hidden in that theme, as the flat drives are.

## R8. Persistence (FR-016) and preference (FR-012)

**Decision**

- The tape path is stored per machine as `tapePath`, through the same
  `UserConfigStore` path as `disk1Path` (`Config\DiskSettings.cpp:173,240`).
  At launch and on machine switch, `AutoMountResolver` (Mount / LeaveEmpty /
  ClearStaleEntry) decides whether to reinsert it, always at position 0 and
  stopped. A missing file clears the entry.
- Recent tapes use the existing `DiskMru` type with a separate list in
  `GlobalUserPrefs` (`recentTapes`).
- The preference is a new bool, `fastTapeLoading` (default true). It follows
  the `floppySoundEnabled` template through `SettingsUiPrefs`,
  `ISettingsApplySink` and `UserConfigStore` defaults, and appears as a
  "Fast tape loading" checkbox on the Hardware page next to the speed combo.
- With the preference off, the tape is mixed into the audio output through a
  `TapeAudioSource` (an `IDriveAudioSource`, like the drive and printer
  sounds) that synthesizes a square wave from the transitions while playing, so the load is audible (FR-012). With it on, the tape is
  audible while it plays and the override is off, and silent during the
  override.

## R9. Test strategy

**Findings**: `UnitTest\EmuTests\TestMachine` builds real machines
("Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c") through the
real builder. `Slots::Empty` boots to the BASIC prompt. `KeystrokeInjector`,
`MachineIdle` and `TextScreenScraper` drive and inspect them;
`Pr3AuxClearTest.cpp:101-186` is the pattern. ROMs are fetched by
`scripts\FetchRoms.ps1 -Fixtures`.

**Decision**

- **Test tape encoder (test code only)**: `UnitTest\EmuTests\TapeTestEncoder`
  builds the Apple tape signal in memory from a byte array, with parameters for
  sample rate, bit depth, leader length, speed factor, DC offset, gain ramp,
  noise (seeded PRNG) and polarity. Knowing the byte format is fine in test
  code; FR-004 constrains the load path only.
- **c2t as the reference**: c2t (Egan Ford, BSD-3-Clause,
  https://github.com/datajerk/c2t) is the standard tool for making Apple II
  tape audio. `TapeTestEncoder` is this project's own implementation, written
  with c2t's source as the reference for leader, sync and bit timing. No c2t
  code is copied: it would not meet this project's style or EHM rules, so the
  encoder is written fresh. Its banner credits c2t; as test code it needs no dependency entry. No c2t binary
  is built or run. Interoperability with other tools (SC-002 scenario 2) is a
  manual check of a Casso-written WAV.
- **Real-ROM load tests** (the SC-001 matrix):
  - Monitor `800.9FFR` on the ][, ][+ and //e
  - Applesoft `LOAD` on the ][+ and //e
  - Integer BASIC `LOAD` on the ][
  - each compares memory byte-for-byte and checks that no `ERR` appears on
    screen
- **Robustness** (SC-003): drift of ±3% (must pass) and ±5% (expected to pass,
  recorded), DC offset, a level ramp, noise at about 15 dB SNR, inverted
  polarity, 8 kHz and 96 kHz, 8/24-bit and float, and stereo.
- **Round trip** (SC-002): Monitor `800.9FFW` with record armed onto a blank
  tape, captured through `FakeDiskFileIo`, then reinsert into a fresh machine,
  `800.9FFR`, and compare.
- **Overwrite-in-place**: record over the middle of a tape and check that the
  PCM before the record point is unchanged.
- **Turbo governor** (SC-005): pure tests for each start and stop condition,
  the 100 ms window computed in cycles, //e $C061 polling with no tape (no
  override), and $C060 polling with the deck stopped (no override).
- **Bus decode**: $C060/$C068 bit 7 per model; bits 0-6 unchanged from
  today; $C020 toggles on read and write; //c $C060 still RD80SW and no
  `CassettePort`.
- **Codecs**: WAV and AIFF format matrix, malformed headers rejected with
  `ERROR_INVALID_DATA`, and write-then-read identity.
- **SC-004 (host time)** is measured by hand per `quickstart.md`, not in a
  unit test, since unit tests do not pace against wall-clock time.
- **SC-006 (a real archive tape)** is manual, per `quickstart.md`.
- The scenario suite is not triggered: this feature touches no disk code.

**Emulated cost**: a 512-byte tape is about 4 s of leader plus about 3.5 s of
data, so roughly 8 M cycles per load test. Tests use short leaders, and the
robustness matrix runs on the ][+ only, keeping the Debug suite's added time
small.
