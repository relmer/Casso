# Implementation Plan: Mockingboard C — Sound/Speech

**Branch**: `024-mockingboard-speech` | **Date**: 2026-08-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/024-mockingboard-speech/spec.md` · Tracking issue #123

## Summary

Add the Mockingboard's voice chip and split the emulated card into two models —
the sound-only **Mockingboard A** that ships today and a sound+speech
**Mockingboard C**, which becomes the default for the Apple ][+, //e, and //e
Enhanced profiles.

The approach that fell out of Phase 0: register **two device types** rather than
adding a variant field, so no configuration schema changes and GH #124's future
slot dropdown gets two ordinary entries (D1). Keep **one card class** carrying a
variant, with the C's speech decode written as a *prefix* over the A's existing
path so the A executes the same code it executes today and compatibility is
auditable by inspection rather than only by test (D2, D3). Model the chip as the
formant synthesizer it is, so it responds correctly to the inflection and rate
controls (D5), and verify it by **spectral assertion against the datasheet**
rather than golden audio (D6).

Phase 0 turned up one piece of foundational work that was not visible from the
spec: **the VIA has no control-line input seam and its `PCR` register is inert**
(F1). The voice chip signals readiness through a VIA control line, so nothing can
work until that exists. It is reusable rather than speech-specific — control-line
handshaking is how the 6522 talks to any peripheral that needs it.

## Technical Context

**Language/Version**: C++ `stdcpplatest`, MSVC v145+

**Primary Dependencies**: Windows SDK and STL only. **No new third-party
dependency** — the synthesis is written from the datasheet, and adding an
allowlist entry would be a constitution amendment.

**Storage**: Existing machine profile JSON (`Resources/Machines/*/*.json`) plus
the existing user-override layer. No new persistence mechanism.

**Testing**: Microsoft C++ Unit Test Framework, in `UnitTest/EmuTests/`. The
chip is pure computation over in-memory buffers, so it is fully reachable from
the test project with no system state mocked.

**Target Platform**: Windows 10/11, x64. ARM64 is build-only — x64 Debug and
Release green is the bar.

**Project Type**: Desktop emulator over a linked core static library.

**Performance Goals**: An idle voice chip costs nothing measurable — SC-004
requires a machine with the C installed to idle no more expensively than the same
machine with the A, following the `Ay8910::IsSilent` fast-path precedent.

**Constraints**: Clean-room from datasheets (FR-002). The sound-only path must be
byte-for-byte unchanged (FR-007). No interrupt software did not arm (FR-016).

**Scale/Scope**: One new chip core, one VIA control-line seam, one audio adapter,
one variant split across an existing card, three machine profile edits, and one
6502 demo program.

## Constitution Check

*GATE: evaluated before Phase 0, re-evaluated after Phase 1 design.*

| Principle | Assessment | Status |
|---|---|---|
| **I. Code Quality** | New code follows EHM, single-exit, top-of-scope declarations, class-static helpers, `.cpp`-only function comments, and the spacing rules. The chip core is arithmetic-heavy, which is where the "extract helpers aggressively" rule bites hardest — resonator update, excitation, and phoneme advance are separate functions, not one long `GenerateSample`. | ✅ PASS |
| **II. Testing Discipline** | Every entity in the data model is exercised from `UnitTest`. No file, registry, network, or system-API dependency: the chip consumes register writes and emits floats. The datasheet-conformance tests (D6) need no fixtures at all. | ✅ PASS |
| **III. UX Consistency** | No CLI surface changes. The Hardware tab names the model (P1); a variant change reports reset-required through the existing mechanism (P2). Backward compatibility is explicit: profiles naming the existing type keep their meaning (C1). | ✅ PASS |
| **IV. Performance** | FR-020 and SC-004 are stated as gates. Rendering reuses the existing per-frame scratch buffers; the chip allocates nothing per sample. | ✅ PASS |
| **V. Simplicity** | A variant field on `DeviceConfig` was rejected as YAGNI (D1). One card class rather than a subclass hierarchy (D2). See Complexity Tracking for the one justified exception. | ✅ PASS |
| **VI. Thin Executable, Testable Core** | Everything lands in `CassoEmuCore` — chip, VIA seam, audio adapter, variant, registry entry. The only shell-side work is display naming in the Hardware tab, which is presentation over core-supplied data. | ✅ PASS |

**Post-Phase-1 re-evaluation**: No gate changed status. The design added no
system-state dependency, no third-party dependency, and no logic in the
executable. The VIA seam (F1) *improves* the Principle II position by making
control-line behavior testable where it previously did not exist at all.

## Project Structure

### Documentation (this feature)

```text
specs/024-mockingboard-speech/
├── spec.md                          # Feature specification
├── plan.md                          # This file
├── research.md                      # Phase 0: decisions D1-D8, findings F1-F2, PENDING-1..4
├── data-model.md                    # Phase 1: entities, rules R1-R13, chip state machine
├── quickstart.md                    # Phase 1: six-stage validation guide
├── contracts/
│   ├── guest-visible-card.md        # What 6502 code sees; guarantees G1-G5, I1-I4
│   └── machine-configuration.md     # Device types, profile defaults, presentation P1-P4
└── checklists/
    └── requirements.md              # Spec quality checklist (complete)
```

`tasks.md` is produced by `/speckit-tasks`, not by this command.

### Source Code (repository root)

```text
CassoEmuCore/
├── Devices/Mockingboard/
│   ├── Ssi263.h / .cpp              # NEW: the voice chip (D5)
│   ├── Ssi263AudioSource.h / .cpp    # NEW: IDriveAudioSource adapter, center pan (D4)
│   ├── MockingboardCard.h / .cpp     # MODIFIED: variant, speech decode prefix (D2, D3)
│   ├── MockingboardAudioSource.h/.cpp# MODIFIED: gain budget recomputed for 3 sources (F2)
│   ├── Via6522.h / .cpp              # MODIFIED: control-line input seam, PCR edge select (F1)
│   └── Ay8910.h / .cpp               # UNCHANGED
└── Core/
    └── ComponentRegistry.cpp         # MODIFIED: register the second card type (D1)

Resources/Machines/
├── Apple2Plus/Apple2Plus.json        # MODIFIED: slot 4 -> sound+speech
├── Apple2e/Apple2e.json              # MODIFIED: slot 4 -> sound+speech
├── Apple2eEnhanced/Apple2eEnhanced.json # MODIFIED: slot 4 -> sound+speech
└── Apple2/Apple2.json                # UNCHANGED (ships no Mockingboard)

Casso/
└── Ui/Settings/                      # MODIFIED: model naming in the slot entry (P1)

UnitTest/EmuTests/
├── Ssi263Tests.cpp                   # NEW: datasheet conformance, quiescence, determinism
├── Via6522Tests.cpp                  # MODIFIED: control-line edges, PCR, R13 no-op proof
├── MockingboardCardTests.cpp         # MODIFIED: variant decode, A-unchanged sweep
└── Ay8910Tests.cpp                   # UNCHANGED

Apple2/Demos/
└── mockingboard-speech-test.a65      # NEW: first-light smoke test (D8)
```

**Structure Decision**: The feature is an extension of an existing device
directory, not a new subsystem, so it stays in
`CassoEmuCore/Devices/Mockingboard/` beside the chips it joins. The one
executable-side change is display naming, consistent with Principle VI. The demo
program sits with its tone-test counterpart in `Apple2/Demos/`.

## Sequencing

Ordered so each step is verifiable when it lands, and so the compatibility
evidence exists before the risky change does.

| # | Work | Gate | Blocked by |
|---|---|---|---|
| 1 | **Baseline capture** — full slot-page sweep and audio render of the sound-only card, as it ships today | Quickstart Stage 1 green *before* any change | — |
| 2 | **VIA control-line seam** (F1) — input state, PCR edge selection, IFR latching, and the R13 no-op proof | Quickstart Stage 2 | — |
| 3 | **Chip core** (D5) — register file, phoneme advance, formant synthesis | Quickstart Stage 3 | PENDING-1 |
| 4 | **Card integration** (D2, D3) — variant, speech decode prefix, request line into the VIA seam | Stage 1 re-run green | 2, 3, PENDING-2 |
| 5 | **Audio path** (D4, F2) — third source, recomputed gain budget | Stage 4 audio checks | 3 |
| 6 | **Registration and profiles** (D1) — second device type, three profile edits, Hardware tab naming | Contract C1–C3, P1–P3 | 4 |
| 7 | **Smoke test program** (D8) | Quickstart Stage 4 | 4, 5, 6 |
| 8 | **Intelligibility and titles** | Stages 5–6 | PENDING-3, PENDING-4 |

**Step 1 is not ceremony.** SC-002 compares against previous-release behavior, so
the comparison basis has to be captured before the tree changes. It is also the
cheapest possible insurance against the feature's headline risk.

**Steps 2 and 3 are independent** and can proceed in parallel; step 3 is the one
gated on acquiring the datasheet.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| A formant synthesizer is substantially more complex than anything else in the device layer — an excitation source, a resonator bank, per-phoneme parameter tables, and transition interpolation, against the PSG's square waves and LFSR | It is the irreducible complexity of the thing being emulated. FR-003 requires the inflection and rate controls to actually vary the voice, and SC-001a requires the output to carry documented formant content — both are statements about a formant model | **Sampled phoneme playback** would be far simpler and would produce recognizable speech, but it cannot respond correctly to the control registers. It would pass a naive listening test while failing FR-003 and SC-001a, which is precisely the "sounds fine, isn't faithful" failure the restructured SC-001 exists to catch |

No other principle required justification. The complexity is confined to one
class whose interface is narrow — register writes in, samples out — so it does
not leak into the card, the VIA, or the audio path.

## Risks

| Risk | Mitigation |
|---|---|
| **The datasheet proves hard to obtain** (PENDING-1) — SC-001a and D5's tables depend on it entirely | Highest-value input in the feature; acquire before step 3. Steps 1, 2 proceed regardless. There is no honest fallback: guessed tables would look correct and be wrong |
| **The removed `$Cn40` mirror breaks a title** | The A variant remains available (User Story 3) and unchanged (D3), so the escape hatch ships with the risk |
| **A music player dispatches on "the VIA interrupted"** | Discharged structurally by R13 + I1/I2 rather than by testing: the A never drives a control line, and an unprogrammed C never asserts |
| **Gain budget regression** (F2) — a third source into a headroom figure sized for two | SC-008 gates it; the existing `kMasterGain` comment documents the original arithmetic to redo |
| **Speech titles never materialize** (PENDING-4) | Confined to Stage 6 sign-off. Stages 1–5 fully validate the chip without them |
