# Speaker Coverage Audit (Phase 0 / T015a)

**Spec**: 004-apple-iie-fidelity / FR-015 / audit §5
**Source under audit**: `CassoEmuCore/Devices/AppleSpeaker.h` + `.cpp`
**Test surface under audit**: `UnitTest/EmuTests/SpeakerTests.cpp`
**Audit date**: Phase 0 implementation

## Pre-existing speaker tests (before T015a)

| # | Test name                                                 | What it asserts                                                       |
|---|-----------------------------------------------------------|-----------------------------------------------------------------------|
| 1 | `SpeakerTests::Read_TogglesSpeakerState`                  | A `Read($C030)` flips the analog speaker state                        |
| 2 | `SpeakerTests::Read_AccumulatesTimestamps`                | A `Read` records exactly one timestamp when a cycle counter is wired  |
| 3 | `SpeakerTests::ClearTimestamps_EmptiesVector`             | `ClearTimestamps()` empties the timestamp ring                        |
| 4 | `SpeakerTests::NoToggles_SilentState`                     | Power-on speaker state is `-0.25f` and the timestamp ring is empty    |
| 5 | `SpeakerTests::Reset_ClearsState`                         | `Reset()` reverts state to silent and clears timestamps               |
| 6 | `SpeakerTests::CycleCounter_RecordsCorrectTimestamp`      | The recorded timestamp is the cycle counter at toggle time            |
| 7 | `SpeakerTests::CycleCounter_AdvancingCyclesRecordsDifferentTimestamps` | Two toggles at different cycles get distinct timestamps   |
| 8 | `SpeakerTests::NoCycleCounter_NoTimestampsRecorded`       | With no cycle counter wired, no timestamps are recorded               |

## Behavior matrix vs FR-015 / audit §5 / clarify guidance

| # | Behavior                                                                        | Pre-existing coverage | Status        |
|---|---------------------------------------------------------------------------------|-----------------------|---------------|
| A | `$C030` toggle on **read**                                                      | (1), (2), (6), (7)    | covered       |
| B | `$C030` toggle on **write** (e.g. `STA $C030`)                                  | none                  | **gap → closed in T015a** |
| C | `$C030-$C03F` 16-byte mirror — every address in range toggles identically       | none                  | **gap → closed in T015a** |
| D | Headless `MockAudioSink::ToggleCount` correctly reflects CPU writes             | none                  | gap → deferred to Phase 6 (audio path) — see note 1 |
| E | Soft-vs-power-cycle behavior: toggle-counter zeroed by power cycle but speaker state preserved across soft reset | none | gap → deferred to Phase 4 (reset semantics split) — see note 2 |
| F | Cycle counter timestamp accuracy                                                | (6), (7), (8)         | covered       |
| G | Power-on silent state                                                           | (4)                   | covered       |
| H | `Reset()` clears state and timestamps                                           | (5)                   | covered       |

### Gaps closed in T015a

- **B**: added `SpeakerTests::Write_TogglesSpeakerState` —
  `Write($C030, value)` flips the analog speaker state identically to a
  `Read`.
- **C**: added `SpeakerTests::Mirror_C03X_AllAddressesToggle` — verifies
  every address in `$C030..$C03F` toggles the speaker exactly once per
  access (read OR write), confirming the 16-byte mirror per audit §5.

### Gaps deferred (with rationale)

1. **D — `MockAudioSink::ToggleCount` reflects CPU writes**: in Phase 0 the
   `AppleSpeaker` is *not* wired through the new `IAudioSink` path; the
   audio renderer still walks the timestamp ring directly. Closing this
   gap requires routing speaker activity through the host shell's audio
   sink, which is a Phase 6 (audio-path consolidation) concern, not an
   Phase 0 concern. T015a includes a comment in `MockAudioSink.h` noting that
   Phase 0 ships only the toggle-count *API*; the wiring lands in Phase 6.

2. **E — soft-vs-power-cycle behavior**: Phase 0 only models `Reset()` as a
   single power-on path. Phase 4 splits `Reset` into `SoftReset` and
   `PowerCycle`. Once that split exists, T0xx in Phase 4 will add
   `SpeakerTests::SoftResetPreservesState` and
   `SpeakerTests::PowerCycleZerosToggleCounter`. Adding them now would
   require modifying the speaker production class (gating two reset
   paths), which violates the Phase 0 constitution rule "no production
   behavior changes yet".

## Tests added in T015a

| Test name                                          | FR / audit ref | Behavior tested                                                |
|----------------------------------------------------|----------------|----------------------------------------------------------------|
| `SpeakerTests::Write_TogglesSpeakerState`          | FR-015, §5     | `STA $C030` toggles the speaker the same way `LDA $C030` does  |
| `SpeakerTests::Mirror_C03X_AllAddressesToggle`     | FR-015, §5     | All 16 addresses `$C030..$C03F` toggle identically             |
| `SpeakerTests::Mirror_C03X_WriteAlsoToggles`       | FR-015, §5     | The 16-byte mirror also toggles on writes (full surface check) |

## Acceptance

- All 8 pre-existing tests still pass.
- The 3 new T015a tests pass.
- Items D and E are documented in this audit; their tests will be added
  in Phase 6 and Phase 4 respectively. The test names are pre-committed above so
  later phase tasks reference them.
