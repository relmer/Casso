# Scenario / unit test split — working notes

Branch `scenario-test-split`, based on master `eb136554` (green CI).

## Why this branch exists

Merging 020 put eleven tests into `UnitTest.dll` that require the DOS 3.3
System Master. CI has no copy and never will, so every build job failed. The
emergency fix took the four `GuestVisible*.cpp` files out of the UT project so
master could go green; it did not give them a home.

Owner's framing, which drives everything below:

- These are **system tests, not unit tests**. They rely on external inputs.
- They **must not be able to run by accident** in the UT suite — a category or
  filter is not enough, they need to be a separate binary.
- **A UT validates that the (mock) target was configured correctly and the
  bytes are accurate.** Whether the machine then boots it is the ST's job.
- **Do not commit Apple's `HELLO`.** Synthesize our own BASIC program instead.

## Task 1 — `ScenarioTests` project

Give the guest tests a home that builds again.

- New `ScenarioTests.vcxproj` → `ScenarioTests.dll`, added to `Casso.sln`.
- Contains: `GuestVisibleBasicTests.cpp`, `GuestVisibleBootTests.cpp`,
  `GuestVisibleDirectBootTests.cpp`, `GuestVisiblePlacementTests.cpp`, and
  `ScenarioDirectBootSkew.cpp.parked` (rename, drop `.parked`) if task 2 does
  not delete it first.
- Shared helpers it needs, compiled into both projects (separate DLLs, so no
  duplicate-symbol problem): `GuestSession`, `HeadlessHost`,
  `KeystrokeInjector`, `MachineIdle`, `TextScreenScraper`, `FixtureProvider`,
  `FakeDiskFileIo`, `EhmTestHelper`.
- **`GuestSession.cpp` must stay in `UnitTest.vcxproj` as well** —
  `BootDiskTests`, `CatalogReproductionTest` and `GameBootTests` use its drive
  helpers and need no master. Removing it broke the UT link once already.
- `RunTests.ps1` gains `-Scenario` as the one deliberate way to run them.
- CI keeps naming `UnitTest.dll` only. Do not add the scenario DLL to CI.
- Header comment on the project: these need external inputs and a booted
  guest, they are not unit tests, they never run in the UT suite.

## Task 2 — the skew test becomes a UT against a constant

Owner: *"no reason not to simply validate that our ls/ps map is correct
against a static copy of that mapping. That's all the dos master is anyway,
just a much more difficult and involved way to get the same persisted data."*

The 16 bytes, from a real master, file offset `$4D`–`$5C` = track 0 sector 0,
loaded at `$0800`, so `$084D`:

```
00 0D 0B 09 07 05 03 01 0E 0C 0A 08 06 04 02 0F
```

- This is the **inverse** of our `kDsk_LtoP`
  (`0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15`). The existing test
  composes them and asserts identity. Keep that claim.
- Provenance for the comment: consumed by **boot0**, not RWTS (RWTS is not in
  memory yet). The instruction is `$0824: BD 4D 08  LDA $084D,X`, indexed by
  the sector counter at `$08FF`, storing to `$3D` (the sector number for the
  drive ROM's read call) while boot0 pulls the rest of track 0 into descending
  pages. The skew exists so the next wanted sector arrives under the head
  about when the loader is ready, instead of a revolution later.
- **Scope it as a Disk II / 16-sector 5.25″ + DOS 3.3 test, and say so in the
  name.** It is NOT a universal: our own `NibblizationLayer` carries a
  different table (`kPo_DosLogicalToFile`) for `.po` on identical media, and
  the concept is meaningless for UniDisk 3.5, ProFile, or any block device.
  Duodisk and the //c internal drive are Disk II-compatible and share it.
- Getting the interleave wrong is **silent in the emulator** — every sector is
  still found, just later — and only slow on real hardware. That is why the
  check is worth keeping at all.

## Task 3 — fix the `DosFileIndexForPhysicalSector` naming inversion

Found while tracing task 2. `NibblizationLayer::DosFileIndexForPhysicalSector
(P)` returns `kDsk_LtoP[P]` — indexing the logical→physical table *by
physical*. That permutation is not an involution
(`kDsk_LtoP[kDsk_LtoP[1]] == 4`, not 1), so one of the two uses is reading the
array against its own name.

Behavior is provably correct today: the composition test passes and
`BuildDemoDisk.ps1 -Verify` reports `casso-rocks.dsk` byte-identical. **Do not
"fix" the apparent inversion without re-running both** — a wrong interleave
produces a disk that still boots everywhere we test and is wrong on hardware.
Untangle the naming, keep the behavior, prove it with the demo disk.

## Task 4 — `put` / `boot` UTs against the mock

Owner: neither relies on existing DOS/ProDOS files on the target, since we are
not booting it. Validate placement and byte accuracy only.

Facts established:

- `disk boot` on DOS 3.3 patches the greeting at **track 1, sector 9, offset
  `$75`**, high-ASCII, space-padded (`Dos33Volume.h` `kGreetingTrack` /
  `kGreetingSector` / `kGreetingOffset`). Catalog untouched.
- On ProDOS there is no such field: the chosen SYS file is moved to the front
  of the volume directory.
- `--bootable` is unrelated to either: a blind full-surface copy of all 35
  tracks × 16 sectors from the master. No manifest, no file selection.
- A Casso-created DOS 3.3 disk has **track 0 sector 0 all zero** — `init`
  reserves tracks 0–2 but writes none of DOS's code. So a boot UT needs a
  DOS-*bearing* image, but not Apple's: a synthetic 35×16 image with a
  recognizable pattern at the greeting field is enough, because the claim is
  "we wrote the right bytes at the right offset".

Convert these to mock-based UTs (they are already `FakeDiskFileIo` shaped):
`Dos33_ADiskToldToBootAProgram...`, `Dos33_APlacedBinary_IsCataloged...`,
`Dos33_TheStockMastersLockedGreeting...`, `Dos33_ABinaryForced...NeverRuns`
(the *disk we build* is the UT; that it never runs stays an ST).

## Task 5 — the BASIC construct program and its fixture

Replaces Apple's `HELLO` as the round-trip and tokenizer corpus.

- Write **our own** Applesoft program covering the full construct set, worked
  from the Applesoft reference rather than from what `HELLO` happens to use.
  Owner asked for "all possible BASIC constructs, and any interesting
  combinations of those with operands that are valuable to check". Produce the
  construct inventory alongside it so coverage is reviewable — every
  statement, every function, every operator, plus operand edges: line 0 and
  63999, `32767` / `-32768`, floats and scientific notation, empty strings,
  embedded quotes, `?` shorthand, multi-statement lines, `REM` swallowing
  tokens, CTRL-D command strings, negative and positive `CALL`, arrays,
  `DATA`/`READ`/`RESTORE`, `DEF FN`, `ON…GOTO`, `ON…GOSUB`.
- **The scenario suite is the fixture generator**: type the listing into a
  real Applesoft, dump the tokenized bytes, commit them. The oracle is still
  Apple's ROM; nothing of Apple's is committed.
- The UTs then compare our tokenizer's output to the fixture, and assert
  detokenize→retokenize is byte-exact.
- Embedding mechanism already exists: `UnitTest/EmuTests/DemoAssets.rc`
  embeds fixtures as RCDATA. Reuse that pattern.
- **Circularity risk to guard**: if someone changes the tokenizer and
  regenerates the fixture carelessly, the check goes circular. Regeneration
  requires the master and a booted guest, so it cannot happen by accident, and
  a fixture diff is visible in review. Say so where the fixture lives.

## Standing gates for this branch

- CI builds with `-p:EnableCppCoreCheck=true -p:RunCodeAnalysis=true
  -p:CodeAnalysisTreatWarningsAsErrors=true`. **`scripts/Build.ps1
  -RunCodeAnalysis` reproduces it; a plain build is a weaker gate and will let
  findings through.** That is exactly how the 020 merge went in green locally
  and red on the server.
- Debug x64, Release x64 and ARM64 all build in CI; analysis findings fail all
  three before any test runs.
- `scripts/CheckStyle.ps1 -Mode Tree` before pushing; the pre-push hook runs a
  diff-ranged version that is stricter than the tree run.
- `scripts/BuildDemoDisk.ps1 -Verify` must keep reporting byte-identical.
- After any push to master: watch CI (and Release if triggered) to completion,
  diagnose failures, push trivial fixes.
