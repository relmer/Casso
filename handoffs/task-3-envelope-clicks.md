# Task: SSI-263 phoneme boundaries click, because the envelope is gated on a
# flag only observed at audio-block boundaries

Start a fresh session. Fetch first and branch from current `origin/master`.

Read `CLAUDE.md` and `.github/copilot-instructions.md` first for conventions.

## Symptom

Faint clicks at phoneme boundaries during connected speech. Audible on
`Apple2/Demos/mockingboard-speech-demo.dsk` (branch `028-speech-demo`) and
reported by the project owner as "little clicks, maybe at the end of each chunk
of sound".

## Mechanism

`Ssi263` releases its amplitude envelope from `m_sounding`, but `m_sounding` is
only ever *observed* when samples are rendered, and samples are rendered in
blocks of ~44 from the state at the END of a CPU slice.

- `EmulatorShell::ExecuteCpuSlices` runs a whole ~1023-cycle slice, ticking the
  chip per instruction, and only then calls `SubmitFrame`.
- In the polled speech path the countdown expires, `Tick` sets
  `m_sounding = false` and raises the request, and the guest answers ~35-45
  emulated cycles later (measured 36-204, mean 46). That is 35-200 microseconds
  of genuine silence, which is inaudible.
- But when a slice boundary lands inside that window -- measured at ~4.5% of
  boundaries -- the whole 44-sample block renders with the envelope target at 0.
  The 4 ms release drops the vowel ~2.2 dB over 1 ms before the next block
  re-attacks it. A 22% amplitude notch, 1 ms wide, in the middle of a steady
  vowel is what reads as a click.
- Simulating the demo's first 8.7 s against a faithful port of `GenerateSample`:
  90 phoneme boundaries, 6 clipped this way, each a 2.17-2.22 dB / 1 ms notch.
  Rendered sample-accurately the same gaps cost 0.08-0.44 dB, i.e. nothing. The
  block quantization inflates a non-event by ~25x in duration.

## Fix

In `Ssi263`, stop letting a phoneme's duration expiry mute the chip.

On real hardware the phoneme duration governs A/R and the articulation
transition; the chip does not silence itself when the timer runs out, it holds
the last phoneme until software loads a new one. That is *why* software has to
send PA to get silence, and why every stream in the tree ends with a PA. Our
`Tick` conflates "the duration is up, ask for the next phoneme" with "stop
making sound", and hangs the envelope off that flag.

Make `m_sounding` govern the countdown and the request only, and drive the
envelope target from the active phoneme's own level. PA's level is 0, so PA
still gives a clean release and silence still works. The handshake gap then
becomes acoustically invisible at any render granularity, and no demo needs to
change.

Bound the hold so an abandoned chip still goes quiet: accumulate cycles in
`Tick` once `m_phonemeCycles` has reached zero and force the target to 0 after
roughly one further frame duration. Without that, a title that stops feeding
phonemes would drone.

Conservative alternative if the above trips a test: make the envelope advance on
emulated time rather than on rendered samples -- add a `m_silentCycles`
accumulated in `Tick` while not sounding, reset in `BeginPhoneme`, and derive
the envelope from it in `GenerateSample`.

**Do NOT** reduce the slice size. At 256 cycles the notch is still 0.54 dB and
`SubmitFrame` runs four times as often.

## Two hygiene findings alongside it

- `GenerateExcitation` (Ssi263.cpp:713-739) returns 0 for an unvoiced phoneme
  without decaying `m_excLp1`/`m_excLp2`, so the resonator input can drop from
  mid-pulse to zero in a single sample while the envelope is still open.
- `Ssi263AudioSource::GeneratePCM` (Ssi263AudioSource.cpp:37-45) zeroes the
  block without running the DC blocker, freezing `m_dcPrevIn`/`m_dcPrevOut` and
  hard-stepping the output to 0. About -57 dBFS here, so not the click, but it
  is the same class of defect.

## Testing

Add a test that renders across a phoneme boundary and asserts the output
envelope does not dip more than a stated fraction while a phoneme is being
re-triggered promptly. The existing `ConnectedSpeechHasNoClicks` bounds the
sample-to-sample step but not a millisecond-scale amplitude notch, which is why
it passes today.

Run `scripts/RunTests.ps1 -Build` and `scripts/CheckStyle.ps1`. Verify by ear
and by capture: `CASSO_AUDIO_DUMP=<file>`, boot with `--disk1` (a positional
path is silently ignored), and confirm the notches are gone.

Note: the dump is headerless float32 stereo at the WASAPI device mix rate,
**48000 Hz** on this hardware, not 44100.
