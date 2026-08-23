# Testing

How the Casso test suite is put together, what each part is for, and the one
place where the default is deliberately weaker than what is available.

## Running it

```powershell
.\scripts\RunTests.ps1 -Build            # build, then run everything
.\scripts\RunTests.ps1 -Build -Filter Merlin
```

`RunTests.ps1` does not build unless you pass `-Build`, and it refuses to run
when the test assembly is older than the newest source that compiles into it.
That guard exists because a bare run against a stale `UnitTest.dll` reports a
confident green, which has bitten this project before.

Debug and Release do **not** run the same tests, and Release is not a
substitute for Debug. `ExpectedEhmAssert` has its `RequireCount()` and its
destructor's did-an-assert-fire check compiled out in Release, so those tests
still execute the code under test but verify nothing about the assert; they
would pass with the guard deleted. Run Debug before merging.

## What the suite is made of

| Layer | What it proves |
|---|---|
| Unit tests | Encoding, addressing modes, arithmetic, flags, assembler, audio, video, disk formats, UI models |
| `HeadlessHost` integration | Cold boot, disk boot, framebuffer hashing, reset semantics, the emulator with no Win32 window |
| Dormann functional suite | That whole *programs* behave correctly on the CPU |
| Harte SingleStepTests | That each *instruction* is correct in isolation, against real hardware |

Dormann and Harte cover what neither does alone. Dormann runs real 6502 code
and catches errors that compound into wrong behavior. Harte catches a single
flag being wrong in a case no sensible program creates, which is exactly
where copy protection lives.

## The Harte vectors

Upstream ([SingleStepTests/65x02](https://github.com/SingleStepTests/65x02))
publishes, per opcode, a set of test vectors generated **against real
hardware**. Each vector is one instruction executed from a completely
randomized machine state:

```
name=a9 cc 21   pc=B36A a=43 x=91 y=96 s=AC p=ED   ram=B36A:A9 B36B:CC B36C:21
```

Set the CPU to that exact state, execute one instruction, compare every field
of the result (registers, flags, and touched memory) against the recorded
final state.

The randomness is in the *input*. Nobody computed the expected output; it was
measured. That is what makes these an independent oracle rather than a
restatement of this implementation's assumptions: a hand-written CPU test
encodes its author's belief about the chip and can only catch disagreements
with that belief, so a misreading of the datasheet gets enshrined rather than
caught. Harte's vectors contain no belief at all.

Across a full 10,000-vector file the initial state covers all 256 values of
A, X, Y and S, all 64 reachable status-flag combinations, and ~9,280 distinct
PC values.

**What they do not cover.** `GenerateHarteTests.py` keeps only the initial and
final states, so this suite proves each instruction's *effect*. It says
nothing about cycle-by-cycle bus behavior or timing; that ground belongs to
the Dormann tests and the disk-timing tests.

## Vector depth: 200 by default, 10,000 available

**The checked-in set is 200 vectors per opcode.** 409 opcode files, ~4 MB,
under `UnitTest/HarteVectors/`. Every clone, every worktree, and CI get real
opcode coverage with no setup.

**The full set is 10,000 per opcode**, about 186 MB, not checked in.

### Why the default is reduced

The full set costs a **1.7 GB download** to produce. `GenerateHarteTests.py`
fetches ~4.2 MB of JSON per opcode from GitHub and packs it down to the
compact `.bin` form; the 9× shrink is the point of the conversion, but the
download is paid in full every time. `--max-vectors` truncates during packing
and does **not** reduce it.

That cost is why the vectors were gitignored, and being gitignored is what
caused the real problem: they existed only where somebody had run the
generator. Combined with the data directory being resolved from compile-time
`__FILE__`, every worktree and **every CI run since the suite was written**
found an empty directory and passed 409 opcode tests in under a millisecond
each without loading a vector. A missing per-opcode file is a legitimate skip,
so nothing reported it.

Checking in a reduced set removes the whole failure mode. No download, no
cache, no provisioning step, no network; a fresh clone is correct
immediately.

### Why 200 is a defensible default

- **It is a fair sample.** The vectors arrive in random order, not sorted:
  initial PC is non-monotonic across the file, and the first 200 alone cover
  60 of the 64 reachable flag combinations and 135 of 256 accumulator values.
  Truncation is unbiased, and deterministic, the same input always produces
  the same output.
- **It is ~82,000 vectors across the suite**, not a token gesture.
- **10,000 is redundant for most opcodes.** `LDA #imm` derives only N and Z
  from the loaded byte; 256 operand values times a few flag states covers it
  exhaustively.

### When to run the full depth

The count earns its keep where the state space is genuinely large, and 200 is
a thin sample of it. Use the full set when:

- **You touched the CPU core**: `CassoCore/Cpu.cpp`, `Microcode.h`, the
  opcode tables, `CassoEmuCore/Core/Cpu65C02.cpp`, or anything that changes
  instruction dispatch, flag computation, or addressing.
- **You are chasing a copy-protection or "this game misbehaves" bug.** These
  are usually one flag wrong in one corner, which is the case a 200-vector
  sample is most likely to miss.
- **You are adding or changing undocumented-opcode behavior.**
- **Before a release**, as a backstop.

Concretely, the arithmetic and shift/rotate opcodes are where depth matters:
`ADC` alone spans A (256) × operand (256) × carry (2) × decimal (2) =
**262,144** meaningful combinations, so even 10,000 samples under 4% of it.
Decimal-mode `ADC`/`SBC` flag behavior is the classic hiding place.

For the load/store/transfer/branch bulk, 200 and 10,000 are both
comprehensively sufficient.

### Getting the full set

```powershell
.\scripts\RunHarteTests.ps1              # download, generate, build, run
```

It lands in `%LOCALAPPDATA%\Casso\HarteTests\<cpu>\`, which is shared by every
checkout on the machine, so this is once per machine and not once per
worktree. Budget about four minutes and 1.7 GB.

If you already have a full set and want to refresh the checked-in reduced one:

```powershell
python scripts\ReduceHarteVectors.py `
    --src $env:LOCALAPPDATA\Casso\HarteTests\6502 `
    --out UnitTest\HarteVectors\6502 --cpu 6502 --vectors 200
```

That reads the existing `.bin` files and truncates them. No network.

### How the runner decides which set it used

`GetHarteTestDataDir` resolves at runtime, richest set first:

1. `CASSO_HARTE_DIR`: explicit override, for CI or a scratch set
2. `%LOCALAPPDATA%\Casso\HarteTests\<cpu>\`: the full set, if generated
3. `UnitTest/HarteVectors/<cpu>/`: the checked-in reduced set

So generating the full set silently upgrades every subsequent run, and
deleting it falls back to the reduced set.

**`HarteVectorDepth` reports which one ran, on every run:**

```
Harte 6502: 153 opcodes, 200 vectors each (30600 total) -- REDUCED set.
Harte 6502: 153 opcodes, 10000 vectors each (1530000 total) -- FULL depth.
```

The numbers are read out of the file headers, never hardcoded, so the message
cannot drift out of step with the data. The same test **fails** when the
directory is empty; that is the guard against the silent-pass mode returning.

A note on `rockwell65c02`: upstream depth is not uniform there, ranging from
1,000 to 10,000 vectors per opcode, which is why the report shows a range
rather than a single number.

### Pinning

`GenerateHarteTests.py` currently fetches from the upstream `main` branch
rather than a pinned commit, so a regeneration could silently pick up changed
data. Checking in the reduced set mitigates this, the bytes are now in git
history and upstream drift shows up as a reviewable diff, but pinning
`BASE_URL` to a commit SHA is still worth doing.

## Suite performance

Debug is roughly 5× slower than Release, and the ratio is uniform across every
compute-bound class (5.6×–8.8×) rather than concentrated anywhere. That is the
signature of compiler flags, not a hotspot. Measured contributions:

| cause | share of the gap |
|---|---|
| `/Od` (no inlining) | ~49%, not recoverable; it is what makes Debug steppable |
| `_ITERATOR_DEBUG_LEVEL=2` | 34% |
| `/RTC1` | 22%: **deliberately kept.** It catches local-buffer overruns and uninitialized reads, and the emulator writes through raw memory buffers |

Two fixes already landed: `/JMC` is off in `Directory.Build.props`
(`__CheckForDebuggerJustMyCode` was 24.5% of all Debug samples, the largest
single leaf in the process), and the DOS/ProDOS integration tests now run
until the machine is idle rather than spending a fixed cycle budget that was
16–50× larger than the work required.

When measuring, note that **a worktree and the primary repo do not run the
same amount of work** unless both have the same vector depth available, the
resolver prefers a full set in the cache. Always state which you measured.
