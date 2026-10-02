# Implementation Plan: Apple II Cassette Tape Support

**Branch**: `037-cassette-tape` | **Date**: 2026-10-02 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/037-cassette-tape/spec.md`

## Summary

Emulate the Apple ][, ][+ and //e cassette port as signal, not data. An
inserted WAV, AIFF or MP3 is decoded once into a list of level transitions
(DC removal, adaptive hysteresis). A new `CassettePort` bus device reports the
level at the exact bus cycle of each $C060 read, and records the cycle of each
$C020 toggle. Recording overwrites the inserted WAV from the current position,
as a real deck does. Fast loading is a hidden Maximum-speed override with
suppressed audio; it is on only while the deck moves and the guest has touched
the port within the last 100 ms of emulated time. A flat tape deck widget in
the drive band drives it. See [research.md](research.md) for the decisions and
the survey of the code they rest on.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145

**Primary Dependencies**: Windows SDK (Media Foundation for MP3 only), Dxui;
no new third-party code

**Storage**: tape files through `IDiskFileIo`; prefs through `UserConfigStore`
and `GlobalUserPrefs`

**Testing**: CppUnitTest in `UnitTest`, with real-ROM machines through
`TestMachine`

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: desktop emulator; all logic in `CassoEmuCore`

**Performance Goals**: a 16 KB load in under 10 s of host time with fast
loading (SC-004). The per-access cost of $C060 is a cursor step, O(1)
amortized.

**Constraints**: the load path holds no knowledge of the Apple tape byte
format (FR-004); no ROM trap; no non-Apple formats; no tape on the //c

**Scale/Scope**: tapes up to about 30 minutes. PCM is held as 16-bit mono for
audible playback and splicing: about 5 MB per minute at 44.1 kHz, and about
345 MB in the worst case (96 kHz, 30 min), which is acceptable.

## Constitution Check

| Principle | Status |
|---|---|
| I. Code quality | Pass: EHM throughout; new types follow one class per pair |
| II. Testing, isolation | Pass: codecs and decoder are pure over byte buffers; files go through `IDiskFileIo` (`FakeDiskFileIo` in tests); MP3 goes through `ITapeAudioDecoder` (fake in tests); dialogs through `IHostDialogs` |
| III. UX consistency | Pass: widget copies drive widget conventions; sentence-case labels; errors in the project format |
| IV. Performance | Pass: decode once at insert; cursor walk per access |
| V. Simplicity | Pass: reuses Maximum, `IDriveAudioSource`, `DiskMru`, `AutoMountResolver`; no new dependency |
| VI. Thin exe | Pass: nothing goes in `Casso`; everything is in `CassoEmuCore` |
| Security (no external binaries) | Pass: c2t is built from source locally to make fixtures only, and is never committed |

Post-design re-check: unchanged, no violations, so Complexity Tracking stays
empty.

## Project Structure

### Documentation (this feature)

```text
specs/037-cassette-tape/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── guest-io.md
│   ├── tape-files.md
│   └── tape-deck-ui.md
└── tasks.md            (next: /speckit-tasks)
```

### Source Code

```text
CassoEmuCore/
├── Devices/Tape/
│   ├── WavCodec.{h,cpp}            pure WAV read/write over bytes
│   ├── AiffCodec.{h,cpp}           pure AIFF/AIFF-C read
│   ├── ITapeAudioDecoder.h         MP3 seam (+ null impl)
│   ├── MfTapeAudioDecoder.{h,cpp}  Media Foundation MP3 decode
│   ├── TapeSignalDecoder.{h,cpp}   PCM -> transitions
│   ├── TapeImage.{h,cpp}           load by content type, isWritable
│   ├── TapeDeck.{h,cpp}            transport, position, ITapeDeckPort
│   ├── TapeRecorder.{h,cpp}        capture -> splice -> WAV
│   └── TapeTurboGovernor.{h,cpp}   pure override decision
├── Machines/Apple2/Common/
│   ├── CassettePort.{h,cpp}        $C020-$C02F device, $C060 forward target
│   ├── AppleGamePort.*             widen to $C060, forward $C060/$C068
│   └── Apple2eKeyboard.*           forward $C020-$C02F, $C060/$C068
├── Machines/IMachine.h, Apple2c.h  HasCassettePort()
├── Audio/TapeAudioSource.{h,cpp}   audible playback (IDriveAudioSource)
├── WasapiAudio.*                   SetSuppressed
├── Shell/CpuManager.*              m_maximumOverride, GetEffectiveSpeedMode
├── Shell/TapeManager.{h,cpp}       shell-side owner: insert/eject/new/persist
├── Ui/TapeDeckState.h              UI-visible snapshot
├── Ui/Chrome/TapeDeckWidget.{h,cpp} drive-band widget
├── Ui/Settings/HardwarePage.*      Fast tape loading checkbox
├── Config/DiskSettings.*, GlobalUserPrefs.*  tapePath, recentTapes
└── Core/UnicodeSymbols.h           transport glyphs

UnitTest/
├── EmuTests/TapeTestEncoder.{h,cpp}  test-only Apple tape signal generator
├── EmuTests/WavCodecTests.cpp, AiffCodecTests.cpp, TapeSignalDecoderTests.cpp
├── EmuTests/CassettePortTests.cpp     bus decode per model
├── EmuTests/TapeRomLoadTests.cpp      real-ROM matrix, robustness, round trip
├── EmuTests/TapeTurboGovernorTests.cpp, TapeDeckTests.cpp, TapeRecorderTests.cpp
├── UiTests/TapeDeckWidgetTests.cpp, TapeAutoMountTests.cpp
└── Fixtures/Tapes/                    c2t WAVs + LICENSE
```

**Structure Decision**: the tape model gets its own `Devices/Tape` folder,
next to `Devices/Disk`. Each new file is added to `CassoEmuCore.vcxproj` and
`UnitTest.vcxproj` by hand (there is no globbing and no filters file).

## Phasing

1. **Codecs and decoder** (pure): WAV/AIFF, signal decoder, test encoder. No
   machine wiring.
2. **Playback on the bus** (US1, the MVP): `TapeImage`, `TapeDeck`,
   `CassettePort`, model forwarding, `HasCassettePort`, and the real-ROM load
   matrix plus robustness.
3. **Deck UI and persistence** (US4): `TapeManager`, the widget, the menu, the
   picker, drag-and-drop, `tapePath` reinsert, `recentTapes`; MP3 decoder.
4. **Fast loading** (US2): governor, CPU override, audio suppression, the
   preference, `TapeAudioSource`.
5. **Recording** (US3): capture, splice, blank tape, round trip, protected
   tapes.
6. **Polish**: c2t fixtures, manual quickstart pass, CHANGELOG/README (last,
   after owner sign-off).

## Open items for review

- The 3D desk scene shows the flat widget in v1; a 3D recorder is a follow-on.
- If c2t's license can't be confirmed, the interoperability fixtures are
  dropped (research R9).

## Complexity Tracking

None.
