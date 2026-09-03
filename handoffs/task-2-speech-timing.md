# Task: SSI-263 phoneme duration is ~1.75x too long (clock-domain mismatch)

Start a fresh session on relmer-desktop. Fetch first and branch from current
`origin/master` (it moved today; do not trust a local master or a remembered
SHA — as of this writing origin/master is `6fb4a833`).

Read `CLAUDE.md` and `.github/copilot-instructions.md` first for conventions
(EHM macros, single exit, style gate, test expectations).

## The defect

`CassoEmuCore/Devices/Mockingboard/Ssi263.cpp`, in `BeginPhoneme` (~line 872):

    m_phonemeCycles = seconds * m_clockHz;

`m_clockHz` is XCK, the chip's own clock: `kDefaultClockHz = 1789772.5`
(Ssi263.h ~line 116), and the card constructs the chip with that default. So the
countdown is expressed in XCK ticks.

But `Ssi263::Tick(cycles)` decrements it with EMULATED 6502 CYCLES:

    m_phonemeCycles -= static_cast<double>(cycles);

Call chain: `EmulatorShell.cpp:7834` (and again at 8188) ->
`m_refs.mockingboard->Tick(m_cpu->GetLastInstructionCycles())` ->
`MockingboardCard::Tick` -> `Ssi263::Tick`. Those are phi2 cycles at
`kAppleCpuClock` (CassoEmuCore/Core/MachineConfig.h, ~1.0205 MHz).

Ratio: 1789772.5 / 1020484 = ~1.754. A phoneme the datasheet says lasts 146.5 ms
(the maximum, at R=0/D=0) actually sounds for ~256 ms. Every phoneme, every
utterance, is ~75% too long.

## Why no test catches it

`Ssi263Tests.cpp` has `FrameDurationMatchesDatasheetFormula` and the
phoneme-duration tests, but they assert `GetFrameDurationSec()` /
`GetPhonemeDurationSec()`, which are pure arithmetic over the same constant and
correct in isolation. Nothing asserts how many `Tick()` cycles a phoneme
actually takes to finish. `TimingIsIndependentOfHostSampleRate` covers
audio-rate independence, a different axis.

## What to do

1. Decide the correct model and document it in the code. XCK is a real hardware
   clock on the Mockingboard, independent of the 6502; the datasheet formulas
   (frame = 4096*(16-R)/XCK) are in XCK terms and should stay. What must change
   is the conversion into whatever clock `Tick()` is actually fed — either
   convert at load time (`seconds * tickClockHz`) or scale in `Tick()`. Either
   way, make both clock domains explicit and named so this cannot silently
   regress.
2. The chip needs to know the tick-domain rate. There is a `SetClock()` seam --
   and **it has no callers anywhere in the tree** (`Ssi263.cpp:171`), so it is
   dead code you can redefine without breaking any caller. `Ay8910::SetClock`
   looks uncalled too, which may fold into item 5. Decide whether the card or
   the shell should now pass the machine CPU clock through it. If one variable
   has to represent two different clocks, split it rather than overloading it.
   (Verified independently by the relmer-desktop relay against the tree.)
3. **Put the conversion where a test can reach it.** `EmulatorShell.cpp` is NOT
   compiled into the UnitTest project in this tree, so any logic that lands there
   is unverifiable — which is how this bug survived. The conversion belongs in
   `Ssi263.cpp` or `MockingboardCard`. (Carry from the relmer-desktop session
   that reviewed this brief.)
4. Add the test that would have caught it: drive `Tick()` with a known number of
   cycles and assert the phoneme ends after the expected COUNT, not after the
   expected formula value. Cover a couple of (R, D) combinations and both timing
   modes (phoneme timing vs frame timing, where the DR multiplier does not apply).
5. Check whether anything else in the chip mixes the domains the same way — the
   A/R request timing and any other countdown.

## Expected fallout, which needs judgement

Speech gets ~1.75x faster, which is the point, but it changes audible output:

- `Apple2/Demos/mockingboard-speech-test.a65` sets `RATE = $60`, tuned BY EAR
  against the buggy timing; it will sound rushed after the fix. Re-tune it and
  rebuild the `.dsk`. Note `scripts/BuildBootSectorDisk.ps1` appears broken
  independently: it calls `as65 ... -q -z` without `--flat` and then reads 256
  bytes from offset $0800, which no longer matches the assembler's default
  output.
- `Apple2/Demos/mockingboard-speech-demo.a65` (the speaking/singing demo on
  branch `028-speech-demo`) was deliberately written to survive this fix: its
  note lengths come from its own cycle-counted delay loops and it re-writes each
  phoneme every ~41 ms, which is shorter than the phoneme lifetime under both the
  current and the corrected timing. It should need no changes — but re-run it and
  confirm, since it is the most timing-sensitive speech software in the tree.
- Baseline hashes in `MockingboardCardTests` may move; if so, update them
  deliberately with a comment saying why.
- Say plainly in the CHANGELOG that emulated speech timing changed and why.

## Validation

Capture real output before and after: set `CASSO_AUDIO_DUMP=<file>` and boot with
`--disk1 Apple2\Demos\mockingboard-speech-test.dsk`. **`--disk1` is required** —
a positional path is silently ignored and boots whatever was mounted last (that
is a separate bug, chipped separately). Compare utterance length; the whole
utterance should get about 1.75x shorter.

Note when analyzing a `CASSO_AUDIO_DUMP` file: it is headerless raw float32
stereo at the WASAPI device mix rate, which is **48000 Hz** on this hardware, not
44100. Assuming 44.1 makes every measured frequency read ~8% flat.

Run `scripts/RunTests.ps1 -Build` (full suite) and `scripts/CheckStyle.ps1`.
