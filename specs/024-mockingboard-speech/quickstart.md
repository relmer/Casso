# Quickstart: Validating Mockingboard C — Sound/Speech

**Feature**: `024-mockingboard-speech` | **Issue**: #123

How to prove this feature works. Ordered so that each stage is runnable before
the next one's inputs exist — the compatibility evidence in particular does not
wait on speech software, or on the voice chip working at all.

See [data-model.md](./data-model.md) for the entities and
[contracts/](./contracts/) for the guarantees these scenarios exercise.

---

## Prerequisites

| Stage | Needs |
|---|---|
| 1–2 | Nothing beyond the repo |
| 3 | The SSI-263 datasheet (research.md PENDING-1) |
| 4 | The speech smoke-test program (research.md D8) |
| 5 | A reference rendering (PENDING-3) |
| 6 | Period speech titles (PENDING-4) — sign-off only |

Build and run the suite:

```bash
pwsh scripts/RunTests.ps1 -Configuration Debug -Build
```

Release, which is where `EhmAssert` absence is verified:

```bash
pwsh scripts/RunTests.ps1 -Configuration Release -Build
```

> Validate the binary's timestamp before trusting a run. `RunTests.ps1` guards
> staleness, but the guard keys on newest-source-anywhere, so it can false-positive
> on data-only edits — prove it before reaching for `-AllowStale`.

---

## Stage 1 — The sound-only card did not move

**Runnable immediately, before any speech code exists. Run it first.**

This is the FR-007 / FR-014 evidence, and the reason to run it first is that a
green result here is only meaningful if it was green *before* the chip landed
too. Establish the baseline, then re-run at every subsequent stage.

| Check | Expected |
|---|---|
| Full read/write sweep of the slot page on the sound-only variant | Byte-identical to the previous release, mirrors included |
| Existing Mockingboard, VIA, PSG, and interrupt tests | Pass unchanged, with no test edited to accommodate the new chip *(SC-003)* |
| A machine profile naming the existing device type | Gets the sound-only card *(contract C1)* |

**A test modified to make it pass is a failure of this stage**, not a passing
result. SC-003 is worded that way deliberately.

## Stage 2 — The A cannot acquire an interrupt it never had

**Also runnable before the chip exists**, because it is a property of the VIA
seam (data-model R13), not of the voice chip.

| Check | Expected |
|---|---|
| A VIA whose control lines are never driven | Behaves exactly as today *(R13)* |
| Timer interrupts enabled, control line driven hard | Software enabling only timer interrupts is never vectored *(contract I3)* |
| Sound-only card, extended run | No control-line interrupt is possible at all *(I1)* |

## Stage 3 — The chip matches its datasheet

The SC-001a gate. No fixtures, no listener.

| Check | Expected |
|---|---|
| Each phoneme rendered in isolation | Energy concentrated at that phoneme's documented formant frequencies; in-band well above out-of-band *(D6)* |
| Each phoneme's duration | Matches the documented span in emulated cycles |
| Rate, inflection, amplitude sweeps | Shift the output in the documented direction *(FR-003)* |
| Same utterance at two host sample rates, and at 1x / 2x / max | Identical emulated-cycle span *(G4, FR-005)* |
| Two identically-programmed chips | Bit-identical streams *(R5)* |
| Every reachable control combination, including absurd ones | Output stays in range; no click, screech, or crash *(R6)* |
| Unprogrammed chip | Reports itself silent; contributes exactly zero *(R2, FR-020)* |
| Reset mid-utterance | Silence immediately, not after the phoneme finishes *(R3, G5)* |

## Stage 4 — First light on a real machine

Uses the repo-original smoke test (D8), so it needs no acquired media.

```bash
pwsh scripts/BuildDemoDisk.ps1     # per the existing demo workflow
```

Boot it on a machine configured with the sound+speech card and listen.

| Outcome | Means |
|---|---|
| Recognizable speech | The whole path works: guest write → decode → chip → audio source → mixer → output |
| Silence | The fault is host-side. Its tone counterpart, `mockingboard-test.a65`, isolates which half |
| Speech on the sound-only card | Decode bug — the A must not have a voice chip |

Then exercise the audio integration (User Story 4):

| Check | Expected |
|---|---|
| Volume slider and mute during speech | Speech responds like every other source *(FR-018)* |
| Speech + music + speaker + drives, all at full | No clipping *(SC-008)* |
| Machine reset mid-utterance | Immediate silence *(FR-021)* |
| Idle machine, C installed vs. A installed | No measurable CPU difference *(SC-004)* |

## Stage 5 — Intelligibility, judged once

The SC-001b gate. Needs a reference rendering (PENDING-3), which may come from
real hardware or from another emulator — comparing *output* is behavioral
comparison and stays inside FR-002's clean-room rule.

Play the same authored phoneme sequence from both. Have listeners who have not
seen the script transcribe each. Casso's accuracy must land **within 10
percentage points** of the reference's.

> The margin is the point. A result that is dramatically *clearer* than the
> reference fails, and should be investigated as infidelity rather than
> celebrated — that is the whole reason SC-001 was restructured.

## Stage 6 — Real software

The SC-001c gate, and User Story 1 sign-off. Needs the acceptance set
(PENDING-4).

| Check | Expected |
|---|---|
| Speech occurs where the software intends | Timing matches on-screen events |
| The phoneme-pacing loop | Runs to the end of each utterance; no stall or hang |
| Titles driving speech by interrupt | Handler runs; utterance completes |
| Titles driving it by polling | Same, without interrupts enabled *(G3)* |

Then re-run Stage 1 against the sound-only regression set (SC-002): render audio
from existing Mockingboard titles on the previous release and on this build, and
compare programmatically rather than by ear.

---

## Definition of done

- [ ] Stages 1–4 green, Debug and Release, x64
- [ ] Stage 1 re-run green after every subsequent stage
- [ ] Code Analysis clean *(constitution quality gate)*
- [ ] `CheckStyle` clean
- [ ] Stage 5 within margin
- [ ] Stage 6 green across the acceptance set
- [ ] Release notes updated *(FR-022, contract P4)*

ARM64 is build-only — x64 Debug and Release green is the bar.
