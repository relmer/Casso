# Testing

How the Casso test suite is put together, what each part is for, and the one
place where the default is deliberately weaker than what is available.

## Running it

```powershell
.\scripts\RunTests.ps1 -Build            # build, then run the unit suite
.\scripts\RunTests.ps1 -Build -Filter Merlin
.\scripts\RunTests.ps1 -Build -Scenario  # the scenario suite -- see below
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
| Scenario suite (`ScenarioTests.dll`) | What a real guest makes of what we wrote, against material we do not own |
| Dormann functional suite | That whole *programs* behave correctly on the CPU |
| Harte SingleStepTests | That each *instruction* is correct in isolation: what it computes and what it costs |

Dormann and Harte cover what neither does alone. Dormann runs real 6502 code
and catches errors that compound into wrong behavior. Harte catches a single
flag being wrong in a case no sensible program creates, which is exactly
where copy protection lives.

## The scenario suite

`ScenarioTests.dll` holds the cases that are **system tests, not unit tests**:
they need external inputs -- the stock DOS 3.3 System Master, which is
fetched by the emulator rather than committed -- and they boot real guests to
ask what DOS 3.3, ProDOS, and Applesoft themselves make of the disks and
programs this tool produced. A tokenizer checked against its own detokenizer,
or a disk read back through the writer's own understanding, agrees with
itself perfectly while being wrong; the guest is the oracle that cannot.

They live in a **separate binary** rather than behind a category or filter,
so they structurally cannot run in the unit suite by accident: CI names
`UnitTest.dll` only, and `RunTests.ps1 -Scenario` is the one deliberate way
to run them. A case that cannot reach the master FAILS rather than skipping,
because a guest-visible gate that never started a guest has checked nothing.

One scenario case doubles as a fixture generator: the Applesoft construct
corpus (`UnitTest/Fixtures/Basic/`) is regenerated only by typing the
committed listing into a booted master, never from the tokenizer's own
output. The circularity guard is spelled out in the inventory beside the
fixture.

## The Harte vectors

Upstream ([SingleStepTests/65x02](https://github.com/SingleStepTests/65x02))
publishes, per opcode, a set of randomly generated test vectors. Each is one
instruction executed from a completely randomized machine state:

```
name=a9 cc 21   pc=B36A a=43 x=91 y=96 s=AC p=ED   ram=B36A:A9 B36B:CC B36C:21
cycles=2
```

Set the CPU to that exact state, execute one instruction, compare every field
of the result (registers, flags, and touched memory) against the recorded
final state, and compare what the instruction cost against the recorded cycle
count.

The randomness is in the *input*, and the expected output comes from
somewhere other than this codebase. That is what makes these an independent
oracle rather than a restatement of this implementation's assumptions: a
hand-written CPU test encodes its author's belief about the chip and can only
catch disagreements with that belief, so a misreading of the datasheet gets
enshrined rather than caught.

Upstream states its provenance carefully, and so should we: the sets are
produced by "an implementation ... that conforms to all available
documentation, official and third-party; passes all other published test
sets; and has been verified by usage in an emulated machine." That is a very
good oracle for documented behavior and a merely *pretty good* one for
undefined opcodes, where the documentation it conformed to is the same
documentation everyone else is arguing about. See the disputed slots below.

Across a full 10,000-vector file the initial state covers all 256 values of
A, X, Y and S, all 64 reachable status-flag combinations, and ~9,280 distinct
PC values.

### Cycle counts

Upstream ships a per-cycle bus trace with every vector. Casso's packed
fixtures keep its **length** -- the instruction's total cycle cost -- and
discard the individual accesses. That is what makes the suite a timing oracle
as well as a behavioral one, and it is cheap: one byte per vector, 2.0% of the
checked-in set. Keeping the trace itself would cost about 30% more, since an
instruction averages 3.9 cycles and each needs an address, a value and a
direction.

The recorded length is what the instruction actually took *for that vector's
operands*, so every conditional cycle is already in it and none of it is
reconstructed on load: an indexed read that crossed a page, a branch that was
taken, a branch that was taken across a page, the 65C02's extra cycle for
decimal `ADC`/`SBC`. Casso's own count comes from `Cpu::StepOne`, the number
the emulator really bills, rather than from anything the harness computes.

**What they still do not cover.** Only the total is kept, so the suite says
nothing about *which* cycle a given bus access lands on. Sub-instruction
timing -- the ground where a disk read sees rotational position -- belongs to
the Dormann and disk-timing tests.

**Fixture format version.** The header carries one, and the loader refuses
anything else. Version 2 added the cycle byte, so a version 1 file read as
version 2 would put every field after each vector's name one byte out of
place -- which surfaces as several hundred nonsense CPU errors rather than as
the one real complaint. A stale set is now named, with its version, and
pointed at the generator.

### Disputed slots

Three undefined 65C02 opcodes where the upstream corpus and the published
per-opcode tables disagree. Casso follows the tables and the tests carry the
exemption explicitly, in `HarteTestRunner.cpp`:

| Opcode | Casso | Upstream | Why |
|---|---|---|---|
| `$DB` | 1-byte NOP | 2-byte NOP | Klaus Dormann's functional test asserts 1 byte; the whole opcode is skipped |
| `$5C` | 3 bytes, 8 cycles | 3 bytes, 4 cycles | Bruce Clark's "65C02 Opcodes" and the oxyron.de 65C02 matrix both say 8; only the cycle comparison is skipped |
| `$CB` | 1 byte, 1 cycle | 1 byte, 2 cycles | Both references put every one-byte NOP at 1 cycle; $CB is only special on WDC parts, where it is `WAI`; only the cycle comparison is skipped |

Everything else about `$5C` and `$CB` -- registers, flags, memory, and how
many bytes the opcode swallows -- is still compared.

### The undocumented tier

The `6502` set holds 230 opcodes: the 151 legal ones plus all 79 undocumented
opcodes `Cpu::InitializeUndocumented` installs. Every `SLO`, `RLA`, `SRE`,
`RRA`, `SAX`, `LAX`, `DCP` and `ISC` addressing mode, and the whole `NOP`
family, is checked against the corpus for final state and cycle count, at the
same depth as the documented set.

It used to hold 153: the legal opcodes plus `$04` and `$CF`. The other 77
`TEST_METHOD`s found no file, skipped, and passed. That is the correct
per-opcode behavior -- upstream publishes all 256 bytes and Casso models a
subset -- but its effect was that the one tier verified only by tests written
against the implementation's own understanding was also the one tier no
external oracle had ever seen. It came out clean: 2,300,000 vectors at full
depth, no disagreement in state or timing.

Which opcodes to fetch is **read out of `CassoCore/Cpu.cpp`** by
`GenerateHarteTests.py`, which parses the `s_kUndocumentedOpcodes` table
rather than keeping its own copy of it. A 79-entry list written down twice is
a second list to get wrong; the two were previously kept in step by a comment
asking the next person to update both. Anything that stops the table being
found is an error, not a warning, because a short list would silently generate
fewer opcodes than Casso implements and the missing ones would go back to
passing on an absent file.

Casso deliberately does not implement the unstable opcodes -- `ANE` (`$8B`),
`LXA` (`$AB`), `SHA`, `SHX`, `SHY`, `TAS` -- whose results on real silicon
depend on the part, the temperature and what was last on the bus. Upstream
publishes vectors for them, encoding one defensible model of a thing hardware
does not do consistently. They are not generated, and there is nothing here
to exempt.

## Vector depth: 200 by default, 10,000 available

**The checked-in set is 200 vectors per opcode.** 486 opcode files, ~4.9 MB,
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

### When the full set is required

**Run the full 10,000-vector set for any change to CPU behavior or the
instruction set.** That means new or changed opcodes, addressing modes, flag
handling, decimal mode, cycle counts, interrupt timing, or anything in
`Cpu`, `CpuOperations`, `Cpu65C02` and the tables they read. The reduced set
samples 2% of each opcode's vectors; the bugs that matter here are usually
one flag wrong in one corner, and that is exactly what a sample misses.

Everything else runs fine against the checked-in reduced set. Disk, video,
audio, UI and assembler work does not touch the CPU, so paying eight extra
minutes per run buys nothing.

**The full set can be switched off by renaming its directory**, which is
worth doing while working on anything else, because it takes the Debug suite
from about nine minutes to under four:

```powershell
#  Off: the runner falls back to the checked-in 200-vector set
Rename-Item "$env:LOCALAPPDATA\Casso\HarteTests" "HarteTests.off"

#  Back on, before touching the CPU
Rename-Item "$env:LOCALAPPDATA\Casso\HarteTests.off" "HarteTests"
```

Renaming rather than deleting keeps the 1.7 GB download from having to be
paid again. The runner prints which depth it used on every run, so a full
run cannot be mistaken for a reduced one.

### Getting the full set

```powershell
.\scripts\RunHarteTests.ps1              # download, generate, build, run
```

It lands in `%LOCALAPPDATA%\Casso\HarteTests\<cpu>\`, which is shared by every
checkout on the machine, so this is once per machine and not once per
worktree. Budget about four minutes and 1.7 GB.

A cached set generated before the cycle counts landed is format version 1 and
is refused: `HarteVectorDepth` names the first stale file and the version it
found. Regenerate it; there is no upgrade path, because the cycle counts were
never in those files to begin with.

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
Harte 6502: 230 opcodes, 200 vectors each (46000 total, final state and cycle count) -- REDUCED set.
Harte 6502: 230 opcodes, 10000 vectors each (2300000 total, final state and cycle count) -- FULL depth.
```

The numbers are read out of the file headers, never hardcoded, so the message
cannot drift out of step with the data. The same test **fails** when the
directory is empty; that is the guard against the silent-pass mode returning.

The report shows a range rather than a single number when depth is not uniform
across a set. Upstream has been uniform at 10,000 for both `6502` and
`rockwell65c02` since the August 2026 regeneration; it was not always, so the
range is kept.

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
