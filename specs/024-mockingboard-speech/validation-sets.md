# Validation Sets: T040 regression matrix & T060 speech acceptance set

**Feature**: `024-mockingboard-speech` | **Issue**: #123

Both sets are **local gates** — commercial images cannot be committed, so
acquired disks live in the **primary checkout** (the Print Shop precedent),
and these runs never gate CI. The mechanical instrument for both is the
`CASSO_AUDIO_DUMP` tap: it captures the exact generated mix as raw float32,
so two builds' output can be compared programmatically, no ears required.

---

## T040 — Sound-only regression matrix (SC-002)

The set is defined by the spec as **the titles GH #66 used to validate the
sound half**, so a pass here means "known-good prior behavior, unchanged."

| # | Title | Exercises | Status |
|---|---|---|---|
| 1 | **Ultima IV** (music intro) | Timer-IRQ-paced music, both PSGs | acquire |
| 2 | **Music Construction Set** | Sustained multi-voice music | acquire |
| 3 | **Skyfox** | Sound effects, envelope use | acquire |
| 4 | **Apple Cider Spider** | Music + SFX mixed | acquire |
| 5 | `mockingboard-test.dsk` | Pure sustained tone, host path | **in repo** |
| 6 | `mockingboard-irq-test.dsk` | T1 interrupt cadence | **in repo** |

Rows 5–6 are runnable today and already covered by the committed baseline
hash (`0x9563EB75`) at card scope; rows 1–4 are the acquisition gate.

### Runbook (per title)

1. Build `master` and this branch, both Release.
2. For each build:
   `CASSO_AUDIO_DUMP=<build>-<title>.f32`, launch with the title mounted,
   let it play a fixed scene (intro music / attract mode) ~30 s, quit.
3. Compare the two dumps programmatically. The dumps will not be
   sample-aligned (boot timing differs run to run), so compare **windowed
   spectra / RMS envelopes**, not raw bytes; the click-analysis script from
   the pop investigation is the starting point.
4. **Pass** = no audible-class difference attributable to the card: same
   spectral content, same event timing within slice tolerance, no new
   clicks. Master carries no dump tap — for it, use loopback capture, or
   cherry-pick the tap commit onto a scratch master build.

Where to obtain: the usual Apple II preservation archives (Asimov mirrors,
archive.org's Apple II library). Store under the primary checkout's
`Apple2/Demos/` alongside the existing commercial `.woz` images.

---

## T060 — Speech acceptance set (SC-001d)

Research finding that reshapes this set: **the period commercial speech
titles are SC-01-era** — Crypt of Medea and Berzap! spoke through the
first-generation boards' Votrax SC-01, and playing their phonemes on an
SSI-263 requires conversion (a live topic on comp.sys.apple2.programmer).
The software that targets **our actual chip** is largely *modern*: Vince
Weaver (deater) ships free, open, actively maintained Apple II software
with native SSI-263 speech.

### Tier 1 — SSI-263-native, free, obtainable today

| Title | Speech content | Source |
|---|---|---|
| **Mist demake** (v1.03+) | Intro voiceover via SSI-263 | deater.net/weave/vmwprod/mist/ — disk images + GitHub source |
| **WarGames demo** | "Joshua" computer voice, SSI-263 TTS | deater.net/weave/vmwprod/wargames/ |
| **Peasant's Quest demake** | Trogdor's speech via SSI-263 | deater.net/weave/vmwprod/peasant/ |
| `mockingboard-speech-test.dsk` | Authored "HELLO" loop | **in repo** |

These three carry a bonus: deater runs them on **real hardware** and
recordings exist (see T058 below), so the same software doubles as a
tuning reference — run the identical disk in Casso and against the
real-hardware recording, and the phoneme stream is known on both sides.

### Tier 2 — period titles, with the chip caveat stated

| Title | Caveat |
|---|---|
| Sweet Micro demo/utility disks | Board-generation dependent; verify which chip each disk drives before drawing conclusions |
| Crypt of Medea | SC-01-era speech; on an SSI-263 board it needs the community phoneme conversion — a faithful C is *not expected* to speak it natively |
| Berzap! | Same SC-01 caveat |

Tier 2 failures that trace to the SC-01/SSI-263 split are **not** Casso
bugs; they are the same behavior a real Mockingboard C exhibits. Document,
don't chase.

### Runbook (per title)

Per SC-001d, the gate is the software's *interaction* with the chip, not
voice quality:

1. Boot the title on the //e profile (Mockingboard C default).
2. Speech occurs at the moments the software intends (intro, events).
3. The phoneme-pacing loop runs to the end of each utterance — no stall,
   no hang, whether the title polls or takes the CA1 interrupt.
4. Sound-only portions remain indistinguishable from the A (spot-check
   against T040's instrument if in doubt).

---

## T058 — the tuning reference exists

Direct evidence that real-hardware SSI-263 audio is obtainable without a
bespoke volunteer:

- **"Apple II - Mockingboard Speech Demo - SSI-263 - Real Hardware"**,
  YouTube, October 2024 — a recording of exactly the class we need.
- deater's demos are demonstrated on real hardware; any such capture of a
  Tier-1 title is a usable reference, because running the *same disk* in
  Casso reproduces the *same phoneme stream* — the "known phonemes"
  requirement is satisfied by the software itself, no prescribed sequence
  needed. YouTube audio compression does not destroy formant centers.
- The ReActiveMicro community actively tests SSI-263 chips on current
  Mockingboard clones (comp.sys.apple2 "v1a mockingboard and SSI-263
  speech chip, how to test?"), so a bespoke all-64-phoneme recording
  remains a realistic *ask* if precision tuning wants it.

Practical T058/T059 path: download a real-hardware capture of the Mist or
WarGames speech, extract audio, run the same disk in Casso with
`CASSO_AUDIO_DUMP`, and compare formant tracks between the two — the same
Goertzel tooling from the click investigation, pointed at vowels instead
of clicks.
