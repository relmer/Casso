# Source Tilt Experiment: the voice is 11 dB short above 800 Hz

**Feature**: `024-mockingboard-speech` | **Issue**: #123 | **Date**: 2026-09-05

**Status: NOT STARTED. The owner has not given the go-ahead.** This document
exists because the experiment had no written record anywhere in the tree: it was
carried in a handoff and in session transcripts only, and both of those expire.
Nothing here has been implemented.

Read the provenance section at the end before acting on any number in this file.
The two headline measurements come from an earlier session and have not been
reproduced; everything else is derived from the code and the phoneme ROM table
and can be re-checked in a minute.

---

## The finding

Measured against a recording of a real SSI-263, the synthesized voice carries
about **11 dB less energy above 800 Hz** than the chip does. Two consequences
follow, and both are audible:

- About **99.5% of a spoken line's energy sits below 800 Hz**.
- Sweeping the filter frequency register barely changes the sound.

The second one is the tell. That register scales the whole tract
(`GetFilterFrequencyHz` in `CassoEmuCore/Devices/Mockingboard/Ssi263.cpp`,
against `kNominalFilterHz`, clamped to 0.5x-2.0x in `GenerateSample`), so
sweeping it moves all three resonators together. If moving them is close to
inaudible, little energy is reaching them.

## Why 800 Hz is the line that matters

800 Hz falls almost exactly on the F1/F2 boundary of the chip's own parameter
ROM. Over the 63 phonemes in `s_kPhonemes` that carry nonzero formants:

| Formant | Range | Above 800 Hz |
|---|---|---|
| F1 | 176-731 Hz | 0 of 63 |
| F2 | 722-2408 Hz | 61 of 63 |
| F3 | 1424-2832 Hz | 63 of 63 |

So "11 dB short above 800 Hz" reads as "F2 and F3 are underexcited", and F2/F3
is where nearly all phonetic distinction lives. The same fact explains why
sweeping the filter register accomplishes so little: there is not much energy up
where that sweep would be heard.

## Where the tilt is

The voiced path is a chain of slopes, all in `Ssi263.cpp`:

| Stage | Slope | Constant |
|---|---|---|
| Glottal source, two cascaded one-pole sections | -12 dB/oct | `s_kSourceBreakHz` = 50.0, converted to `m_sourcePole` in `SetSampleRate` |
| Three Klatt-style resonators at F1/F2/F3 | -- | `s_kBandwidthHz` = 60/90/120 |
| Lip radiation, a first difference | +6 dB/oct | normalized by the device rate |
| Output low-pass, two cascaded one-pole sections | -12 dB/oct above its knee | `s_kfOutputLpCoef` = 0.32 |

Net voiced slope is -12 plus 6, so -6 dB/oct. The reference recording, of the
chip speaking at 88.7 Hz, falls at about **-8 dB/oct** from the fundamental.
The slope is therefore close; the deficit is in how much reaches F2 and F3, not
in the far-field slope alone.

**The source tilt is the -12 dB/oct rolloff of the glottal source**, and its
break frequency is the single constant the experiment would move.

## The proposed change

Make the source tilt shallower, or move its break up, so more energy arrives at
F2 and F3. Then re-measure against the reference recording band by band and
compare the per-band mean error against the current figure.

## Why this is gated rather than simply tried

**This exact constant has already caused one regression.** Commit `8b4a2556`
("break the glottal source below the pitch range, not inside it") fixed a break
sitting at 805 Hz, which left the response flat underneath and let lip radiation
tilt the output upward across the whole F1 region: 21 dB short at 80-200 Hz,
9-10 dB heavy from 400 Hz up, heard as a thin and buzzy voice. Raising the break
again moves back toward the failure that was just repaired.

**The tilt is not independent of the constants around it.** All of these were
fitted together against the same recording, and moving the source tilt puts
several of them out of adjustment:

- the F1 compensation, `731.0 / max(F1, 170.0)` in `GenerateExcitation`, which
  exists to offset the radiation tilt for low-F1 vowels
- `s_kfOutputLpCoef`, whose two poles were tightened to match the chip's fall
  above its top resonance
- `s_kfVoicedGain` and `s_kfOutputGain`, which set absolute level
- `s_kfNoiseGain`, `s_kfNoiseLpCoef`, `s_kfFricLpCoef` and
  `s_kFricBandwidthHz` on the parallel fricative branch, since the
  vowel-to-sibilant balance is a ratio between the two paths

A change to the source tilt that does not refit at least the fricative balance
will move the sibilants as a side effect.

## Instruments, all already in the tree

No new tooling is needed. Spec 028 shipped everything this requires:

- `Apple2/Demos/Mockingboard/Speech/pulse-probe.a65` and `phoneme-probe.a65`,
  with committed `.dsk` images under `Apple2/Demos`
- `CASSO_AUDIO_DUMP`, which captures the generated mix as raw float32 stereo
- `CASSO_AUDIO_DUMP_DEVICE`, which captures the far side of the pending queue,
  so the two can be differenced

Derive the capture rate by differencing the lengths of two captures rather than
assuming 48 kHz.

## What would count as success

1. Per-band mean error against the reference recording improves, and no band
   gets worse by more than it gains elsewhere. The fricative work reached a mean
   of 4.9 dB across six bands; that is the number to beat.
2. The share of a line's energy below 800 Hz falls well short of 99.5%.
3. Sweeping the filter frequency register produces an audible change.
4. The voice does not become thin or buzzy again. Compare against `8b4a2556`'s
   before-and-after directly, since that is the specific regression at risk.
5. The sibilants hold. Z against L was 33 dB apart after the fricative work; a
   refit must not close that.

## Provenance

| Claim | Source | Verified |
|---|---|---|
| 11 dB deficit above 800 Hz | earlier session, via handoff 028 | NO |
| 99.5% of energy below 800 Hz | earlier session, via handoff 028 | NO |
| Filter register sweep is barely audible | earlier session, via handoff 028 | NO |
| Formant ranges and the counts above 800 Hz | computed from `s_kPhonemes` | yes, 2026-09-05 |
| Chain slopes, constants, clamps | read from `Ssi263.cpp` | yes, 2026-09-05 |
| -8 dB/oct reference, 805 Hz regression, 4.9 dB and 33 dB figures | comments in `Ssi263.cpp` and commit `8b4a2556` | quoted, not re-measured |

The first three are the ones the experiment rests on, and they are exactly the
ones nobody has reproduced. Re-measure them before spending time on a refit: if
the deficit is not 11 dB, the work changes.
