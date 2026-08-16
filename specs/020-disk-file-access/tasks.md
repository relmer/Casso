# Tasks: Disk File Access for the Build Loop

**Input**: Design documents from `/specs/020-disk-file-access/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — Constitution Testing Discipline is non-negotiable; every
pure component ships its unit suite in the same phase. Guest-visible gates reuse
the real-CPU DOS-boot harness and **FAIL when its cached asset is absent** — a
test that cannot reach its data must not pass, or "N passed" stops meaning N
things were checked.

**Organization**: Phases follow plan.md's **dependency order**, not story order.
User Story 1 is **DELIVERED** (shipped `--raw` / `--dos-bin`) and has no tasks.
US3 (read) precedes US2 (write) because extraction is what a migrating developer
needs first and because a write path must not be built before the decode report
exists. Constitution commit discipline: commit + push after each completed phase.

## Format: `[ID] [P?] [Story] Description`

---

## Phase 1: Setup

- [ ] T001 Add EHM-conformant stubs for every new file and wire them into `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `CassoCore/CassoCore.vcxproj(.filters)`, `CassoCli/CassoCli.vcxproj(.filters)`, `UnitTest/UnitTest.vcxproj(.filters)`: `CassoEmuCore/Devices/Disk/{IVolume.h, VolumeTypes.h, SectorDecodeReport.h, Dos33Volume.h/.cpp, ProDosVolume.h/.cpp, VolumeIntegrityReport.h/.cpp, TrackWritability.h/.cpp, IDiskFileIo.h, DiskCommandRunner.h/.cpp, CommitPlan.h/.cpp, DirectBootBuilder.h/.cpp}`, `CassoCore/{AppleTextCodec.h/.cpp, ApplesoftTokenizer.h/.cpp}`, `CassoCli/{Win32DiskFileIo.h/.cpp, DiskCommand.h/.cpp}`, `UnitTest/{AppleTextCodecTests.cpp, ApplesoftTokenizerTests.cpp}`, `UnitTest/EmuTests/{Dos33VolumeTests.cpp, VolumeIntegrityTests.cpp, DiskCommandRunnerTests.cpp, CommitPlanTests.cpp, DirectBootTests.cpp, FakeDiskFileIo.h}` — x64 Debug compiles clean

---

## Phase 2: Foundational (blocking prerequisites — plan.md Phase A)

**Nothing in any later phase may consume denibblized output until this phase is
complete.** T003–T006 are the fix for a defect that is live on the emulator's
flush path today (GH #115); building a write path on top of the current decoder
would ship it a second time.

**Scope boundary on T005 — deliberate, not unfinished.** T005 is
freeze-and-sidecar: write the recovery file, name it in the notification, leave
the mount alone. **Promotion** — switching the running mount over to the recovery
image — is explicitly NOT in this feature. It ripples into six surfaces (drive
widget label, write-protect menu text, tooltip, recent-disks MRU, the
`DiskImageStore` entry, and the persisted `disk1Path` / `disk2Path`), and getting
the last of those wrong loses the user's work on next launch. That is an
emulator-behavior change, and it belongs to #115 as a follow-up with spec 021's
disk-manager owner in the loop. There are two features here and only one of them
is this one — record that next to the implementation so a later reader does not
mistake the stopping point for an oversight.

- [x] T002 [P] Define `SectorDecodeReport` and `TrackDecodeOutcome` (`Complete` / `Unformatted` / `Partial`) with per-track 16-bit `coverage`, `duplicated`, `hasDataLoss`, `unrecoveredCount` in `CassoEmuCore/Devices/Disk/SectorDecodeReport.h` per data-model.md. **Track layer, not `VolumeTypes.h`** — these describe nibble decoding, and putting them in the filesystem-layer header would make `NibblizationLayer` include a header from the layer above it
- [x] T003 Rewrite the per-track decode loop in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp` (currently lines 762–780): continue past a failed sector and resynchronize on the next address prologue instead of `break`; maintain the coverage mask; classify the track by **coverage** (`Complete` iff all sixteen logical sectors filled, each exactly once), using address-fields-found to separate `Unformatted` from `Partial`. Add the four-argument `Denibblize` overload carrying `SectorDecodeReport`
- [x] T004 Rewire the existing three-argument `Denibblize` in `CassoEmuCore/Devices/Disk/NibblizationLayer.h/.cpp` to **forward to the reporting form and fail when the report shows data loss**, succeeding only when every track is `Complete` or `Unformatted`. It must not remain a reportless passthrough — `DiskImage::Serialize` (`DiskImage.cpp:434`) is the sole production caller and the one place the defect matters. Verify all twelve existing call sites still compile unchanged. **This is a user-visible behavior change on a constantly-running path**: `DiskImage::Serialize` today returns `S_OK` over a partly-zeroed buffer and the flush completes silently; afterwards it fails on a partially-decodable track, and `DiskImageStore::FlushEntry` already routes persist failures to the user through the shared EHM notifier. Users will now see an error where they previously got a quietly corrupted image. Requires the CHANGELOG entry in T049 and the recovery affordance in T005
- [x] T005 Give the refusal from T004 a recovery path, because a correct refusal that leaves the user stuck is only half the fix. At flush time the in-memory image holds the session's guest writes while the file on disk holds the last good copy: refusing preserves the original but strands the session, and writing anyway is the defect. In `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, on a flush refused for data loss, write a recovery file beside the target — `<name>.recovered.woz` — via `WozLoader::Serialize (image, bytes)`, leave the original untouched, and name that path in the notification. **Serialize to WOZ, not to the rejected sector buffer.** The sector buffer is the denibblized output holding zeros exactly where the damaged track should be — it discards the very content that caused the refusal. `WozLoader::Serialize` takes the image's live per-track bit streams verbatim and is format-agnostic, so a `.dsk`-sourced mount round-trips **losslessly**, damaged track included; the file is genuinely useful (mount it and the odd track is still there) and the notification needs no lossiness caveat. Same cost — one call on data already in memory. Handle name collisions; never overwrite an existing recovery file
- [x] T006 In `UnitTest/EmuTests/NibblizationTests.cpp`: narrow the comment on `Denibblize_UnformattedTrack_ZeroFillsThatTrackAndKeepsOthers` (line 308) so it claims correctness **only** for the wholly-unformatted case — as written it generalizes to "missing sectors read back as zeros … not silent corruption", which is the standing license that let the defect survive. Add the three siblings that pin what it did not cover: `Denibblize_PartiallyDecodableTrack_ReportsDataLossAndDoesNotZeroTail`, `Denibblize_OutOfRangeSectorNumber_ReportsIncompleteCoverage`, `Denibblize_DuplicateSectorNumbers_ReportsIncompleteCoverage` — each built by nibblizing a valid image then patching one address field's 4-and-4 sector value. The existing test must keep passing unchanged
- [x] T007 [P] Implement `TrackWritability` in `CassoEmuCore/Devices/Disk/TrackWritability.h/.cpp`: whole-image refusal first (quarter-track map resolving any position off `qt / 4` via `DiskImage::ResolveQuarterTrack`; image metadata declaring timing-sensitive capture), then per-track writable iff `Complete` or `Unformatted`. Positive proof only — never protection-scheme recognition. Tests in `UnitTest/EmuTests/NibblizationTests.cpp`
- [x] T008 [P] Implement `NibblizationLayer::RenibblizeTracks` (re-encode only the listed tracks into `DiskImage::GetTrackBitsForWrite`, leaving every other track's packed bits byte-identical) in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp`; test that an unrelated track's bits are bit-for-bit unchanged after a write elsewhere
- [x] T009 [P] Define `FilePath` (path-based from the outset per FR-009), `FileEntry`, `FilePayload`, and `VolumeListing` in `CassoEmuCore/Devices/Disk/VolumeTypes.h` per data-model.md — fields a filesystem does not store are absent, not zero, so "no load address" is distinguishable from "loads at $0000". **Delivered with `FilePath` in its own `FilePath.h/.cpp` pair rather than in `VolumeTypes.h`**: it has methods, and the style rule that a class with behavior gets its own pair named for it outranks this task's wording. `VolumeTypes.h` holds only the plain-data types. Recorded so the split reads as a decision rather than a discrepancy someone "corrects" by moving the file back
- [x] T010 Define the `IVolume` seam in `CassoEmuCore/Devices/Disk/IVolume.h` per contracts/volume-api.md: `Enumerate`, `Read`, `Write`, `Delete`, `BuildIntegrityReport`, `SetStartupProgram`. Every mutating call takes the current sector buffer and yields a **new** one — nothing mutates in place
- [x] T011 Implement `VolumeIntegrityReport` in `CassoEmuCore/Devices/Disk/VolumeIntegrityReport.h/.cpp`: `claimedBy`, `crossLinked`, `allocatedButUnclaimed`, `claimedButFree`, `unfollowableChains`, `catalogFullyParsed`, `isClean`. **Traversal MUST terminate on any input** (FR-038) — visited set plus a ceiling derived from volume capacity; a chain hitting the bound is recorded unfollowable, never followed. Tests in `UnitTest/EmuTests/VolumeIntegrityTests.cpp` including cyclic and self-referential chains (SC-010)

**Checkpoint**: the decoder no longer loses data silently, and the integrity pass
exists for all four of its consumers.

---

## Phase 3: User Story 3 — Read a disk image's contents (P1)

**Goal**: list a volume and extract files from it, across every mountable format.

**Independent test**: quickstart §US3 — list a known disk and confirm name, type,
size, and lock state for every file plus free space; extract one and compare
byte-for-byte; repeat against `.dsk`, `.do`, `.po`, and `.woz` of the same content.

**Why first**: a migrating developer's source is on Apple II disks and cannot be
edited until it comes off. Nothing else in the feature is reachable for them
until this exists.

- [x] T012 [US3] Implement `Dos33Volume` read side in `CassoEmuCore/Devices/Disk/Dos33Volume.h/.cpp` — **no DOS 3.3 reader exists today**: walk the VTOC (T17 S0) and catalog chain (T17 S15→S1), decode each entry's name (30 bytes high ASCII), type byte with the `$80` lock bit masked off, sector count, and track/sector list; `Enumerate` and `Read`. Reuse `Dos33Skeleton`'s geometry constants rather than restating offsets. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`
- [x] T013 [P] [US3] Implement `Dos33Volume::BuildIntegrityReport` (walk every catalog entry's T/S chain into the claim map; compare against the VTOC free bitmap) in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`; damaged-volume cases in `UnitTest/EmuTests/VolumeIntegrityTests.cpp`
- [x] T014 [US3] Implement `ProDosVolume` read side in `CassoEmuCore/Devices/Disk/ProDosVolume.h/.cpp`: wrap the existing `ProDosReader::ExtractFile` behind `IVolume`, add `Enumerate` over the volume directory, and add **path-based traversal** so a file inside a subdirectory is reachable (FR-009). Where traversal is not yet supported, refuse a multi-component path with a clear reason — never silently truncate. Extend `UnitTest/EmuTests/ProDosVolumeTests.cpp`
- [x] T015 [P] [US3] Implement `ProDosVolume::BuildIntegrityReport` (walk seedling / sapling / tree block chains into the claim map; compare against the volume bitmap) in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`
- [x] T016 [P] [US3] Implement `AppleTextCodec` in `CassoCore/AppleTextCodec.h/.cpp`: high-ASCII ↔ host text with line-ending normalization both directions (FR-021); round-trip tests in `UnitTest/AppleTextCodecTests.cpp`
- [ ] T017 [US3] Extend the command-line surface **additively**: add `Disk` to `CommandLineOptions::Subcommand`, a `DiskVerb` enum and operand fields in `CassoCore/CommandLineOptions.h`; **one row** `{ "disk", …Subcommand::Disk }` in `s_kSubcommands` and **one arm** calling a new `ParseDiskOptions` in `CassoCore/CommandLineParser.cpp`. Do not reshape the dispatcher. Accept `ls`→`list` and `rm`→`delete` aliases. **`UnitTest/CommandLineTests.cpp` must stay green — spec 019 is being developed concurrently against these files**; add disk-grammar cases beside the existing ones
- [ ] T018 [US3] Define the file seam `IDiskFileIo` in `CassoEmuCore/Devices/Disk/IDiskFileIo.h` — read all bytes, write all bytes, stat (size + modification time), exists, delete, atomic replace — plus a `FakeDiskFileIo` in `UnitTest/EmuTests/` that serves synthetic images from memory and can be told to fail, to report a changed stat, or to already hold a colliding name. **`UnitTest` does not link `CassoCli`** (verified: `UnitTest.vcxproj` references CassoCore, CassoEmuCore, Casso, Dxui only), so anything placed in `CassoCli` is unreachable by tests and violates Principle VI's litmus
- [ ] T019 [US3] Implement `DiskCommandRunner` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.h/.cpp`: takes `CommandLineOptions` and an `IDiskFileIo &`, and returns a result carrying the exit status, the stdout payload, and the diagnostic text. It owns every **decision** — verb dispatch, exit-status mapping (`0` clean, `1` succeeded-with-complaints, `2` produced no output, matching `as65` and `run`), and failure-message construction naming image, file, and reason (FR-031, FR-033). Covers spec US3 acceptance 4. Tests in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp` against the fake seam
- [ ] T020 [US3] Implement `list --long` and `get --out <file>` from the grammar in contracts/disk-subcommand.md — both are in the contract and neither had a task. Then reduce `CassoCli` to the irreducible edge: `Win32DiskFileIo` implementing `IDiskFileIo` over `ifstream`/`ofstream`/`ReplaceFileW`/`GetFileAttributesExW`, and a `DoDisk` that constructs it, calls `DiskCommandRunner`, writes the returned payload to stdout and diagnostics to stderr, and returns the status. No branching on outcomes and no message building in the exe — if a decision appears here, it belongs in T019. **Both go in a NEW `CassoCli/DiskCommand.h/.cpp`, not in `CommandLine.cpp`.** Two reasons: `CommandLine.cpp` is 1,222 lines and is the specific file GH #85 names, so appending to it worsens the condition this task exists to fix; and spec 019 is editing that same file (diagnostic positions, the help-text block), so a new file shrinks the overlap to the single `PrintUsage` registration line
- [ ] T021 [US3] Cross-format extraction gate (SC-004): the same content as `.dsk`, `.do`, `.po`, and `.woz`; every file extracts byte-identically from each. Plus the damaged-volume cases from quickstart §US3.5–7 — partial track reports unrecovered sectors as unrecovered (never zeros), wholly unformatted track reports blank not damage
- [ ] T022 [US3] Runtime validation pass over quickstart §US3; fix what it finds

**Checkpoint**: US3 is complete and independently shippable — a developer can get
their source off an Apple II disk, which is the first thing they need.

---

## Phase 4: User Story 2 — Put a file onto a disk image (P1)

**Goal**: place a binary on a disk, replacing any file of the same name, with
every failure leaving the image byte-for-byte unchanged.

**Independent test**: quickstart §US2 — place a 512-byte binary as `PROG` at
`$6000`, boot, confirm `CATALOG` lists `B 002 PROG` and `BLOAD PROG` lands the
bytes; then run every failure mode and confirm the image hash is unchanged.

**Note**: delete lands here, not later. Replace is delete + write, and neither
filesystem can delete today.

- [ ] T023 [US2] Generalize DOS 3.3 writing into `Dos33Volume::Write` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: allocate from the VTOC free bitmap, build the track/sector list, write data sectors, create the catalog entry, leave the bitmap consistent. `Dos33FileWriter::WriteHello` is a zero-parameter hardcoded emitter — treat it as a worked example of the structures, not a base to extend. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`
- [ ] T024 [US2] Implement `Dos33Volume::Delete` with free-space return in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: free **only** sectors the integrity report shows this file uniquely owns, report the rest as leaked, and remain available for a file whose T/S chain is damaged so a bad file cannot strand the volume (FR-011). Warn distinctly when `catalogFullyParsed` is false (FR-040)
- [ ] T025 [US2] Add **tree growth** to the ProDOS writer (master index block of index blocks) in `CassoEmuCore/Devices/Disk/ProDosSkeleton.cpp` / `ProDosVolume.cpp` — the existing writer handles seedling and sapling only; extend `ProDosVolume::Write` to grow storage type as size requires (FR-008). Block-accounting asserted against `BuildIntegrityReport`, not by inspection
- [ ] T026 [US2] Implement `ProDosVolume::Delete` with volume-bitmap free-space return in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`, under the same unique-ownership rule as T024
- [ ] T027 [US2] Implement replace in both volumes (FR-012): compute the complete post-replacement buffer as a **whole** — never a delete applied to the target followed by a write, which would free the old file and lose it outright if the write then failed. Test that a write failing after the delete step leaves the original file intact
- [ ] T028 [US2] Wire the pre-commit self-check (FR-039): every `Write` and `Delete` runs `BuildIntegrityReport` over its **computed result** and refuses to return a result that fails it — sector claimed twice, free map disagreeing with the catalog, chain broken by the edit. Feed the path a deliberately corrupted result and confirm refusal (SC-009)
- [ ] T029 [US2] Implement the bit-stream write path in `CassoEmuCore/Devices/Disk/`: diff the pre- and post-edit sector buffers, call `RenibblizeTracks` for **only** the changed tracks, and refuse via `TrackWritability` when the write needs a track that is not writable (FR-016, FR-017). Test that writing to a WOZ leaves untouched tracks bit-identical, and that a `Partial` track refuses the write with the image unchanged (SC-008). **Write and re-read across all four formats** — `.dsk`, `.do`, `.po`, `.woz` (FR-015): extraction is covered across formats by T021, but sector ordering differs between DOS and ProDOS order and a mistake there is silent, so the write side needs its own matrix
- [ ] T030 [US2] Implement the **commit policy** in `CassoEmuCore/Devices/Disk/CommitPlan.h/.cpp` as pure functions over data: temporary-name derivation from the target path and an attempt counter, the staleness comparison (`IsStale (recordedSize, recordedTime, observedSize, observedTime)`), and the ordering rule that the temporary is removed on any failure. Names must not collide between concurrent invocations (FR-013). Tests in `UnitTest/EmuTests/CommitPlanTests.cpp` — these are decisions, not syscalls, and they must be unit-testable
- [ ] T031 [US2] Wire the staleness re-verify and the best-effort probe into `DiskCommandRunner` (T019): record size and modification time at read via `IDiskFileIo`, re-verify immediately before commit and refuse if either changed (FR-036); best-effort exclusive-open probe refusing when **another** holder has the file open, with help text that does not imply it detects Casso (FR-035). Tested against the fake `IDiskFileIo`, which can report a changed size or time on demand
- [ ] T032 [US2] Add `put` and `delete` verbs to `ParseDiskOptions` and `DiskCommandRunner` (`--as`, `--type`, `--addr`, `--text`/`--basic`/`--verbatim`), with write-protect and locked-file refusals reported in intelligible terms rather than raw platform codes (FR-014). Use `--verbatim`, **not** `--raw` or `--binary`: both already name assembler output shapes (`OutputFormat::Raw`, `OutputFormat::Binary`) in this same parser, and `--verbatim` says what it does — the other two selectors transform the bytes, this one does not. **`--verbatim` is the lossless path and must work on BOTH sides.** Text decoding is deliberately many-to-one — mixed high and low bytes decode alike, which is what lets a real vendor file be read at all — so `get` then `put` through the converting path rewrites bytes nobody edited (37 of them in a real macro library). Gate it: `get --verbatim` followed by `put --verbatim` with no edit leaves the image **byte-for-byte unchanged**, which is a stronger statement than any conversion test. Extract-edit-replace is the workflow US3 exists for, and it must not perturb the bytes the user did not touch
- [ ] T033 [US2] Guest-visible gate (SC-003): real-CPU tests — place a binary on a DOS 3.3 image, boot, `CATALOG` shows `B 002 PROG`, `BLOAD PROG` lands bytes at `$6000`; same payload on ProDOS shows type `BIN` aux `$6000`. Attempt to overwrite the stock master's locked `HELLO` (type `$82`) and confirm refusal
- [ ] T034 [US2] Failure-mode gate (SC-005) as **unit tests against the fake `IDiskFileIo`**, not against real files: for **every** documented failure — volume full, locked file, write-protected image, illegal name, unwritable track, stale target — assert the target's bytes are unchanged, the resulting image still parses as a mountable volume, and no temporary remains in the fake's file table. Test Isolation is NON-NEGOTIABLE, so the seam is what makes this suite legal; the real-file version of the same checks is the single manual pass in T035
- [ ] T035 [US2] Runtime validation pass over quickstart §US2 plus the **manual** interrupted-write check — the one sanctioned real-file exercise: kill the process mid-commit, confirm the original is intact and bootable and no temporary remains. Crash safety cannot be unit-tested, which is exactly why it is the part done by hand; everything else in T034 runs against the seam

**Checkpoint**: the minimum viable loop is closed — assemble, place, boot, run.

---

## Phase 5: User Story 4 — Make the disk boot the program (P2)

**Goal**: boot straight into the developer's program with no typing.

**Independent test**: quickstart §US4 — set a boot program, boot the image, the
program runs unattended.

- [ ] T036 [US4] Implement `Dos33Volume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: patch the greeting filename **in place at T01 S09 `+$75`**, 30 bytes, high ASCII, `$A0`-padded (verified against the stock master in research R-003). No catalog change, no chaining file
- [ ] T037 [US4] Implement `ProDosVolume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`: reorder the volume directory so the chosen `SYS` file is the first the boot path finds. **Deliberately not shared with T036** — the two mechanisms differ in kind, and a unified "write the boot name" helper would be wrong for both
- [ ] T038 [US4] Add the `boot` verb to `ParseDiskOptions` and the runner (FR-024); refuse a program not present on the volume, naming the missing file (FR-025)
- [ ] T039 [US4] Boot gate: real-CPU tests — a DOS 3.3 image with a set boot program runs it unattended after DOS loads; a ProDOS image launches the chosen system program

---

## Phase 6: User Story 5 — Boot with no operating system (P3)

**Goal**: an image that boots directly into a binary, with no DOS or ProDOS.

**Independent test**: quickstart §US5 — the payload runs measurably sooner than
the equivalent OS boot.

- [ ] T040 [US5] Implement direct-boot image generation in `CassoEmuCore/Devices/Disk/DirectBootBuilder.h/.cpp`: a boot-sector loader that pulls the payload's sectors and jumps to it (FR-026). **Resolve the loader's sector capacity before starting** — R-010 deferred it, and FR-027's reported number is undefined until it is settled. Unit tests in `UnitTest/EmuTests/DirectBootTests.cpp` asserting the generated image's structure and the capacity boundary
- [ ] T041 [US5] Refuse a payload exceeding what the boot path can load, reporting the available capacity (FR-027); support an entry address different from the load address (FR-028), in `CassoEmuCore/Devices/Disk/DirectBootBuilder.cpp`
- [ ] T042 [US5] Gate (SC-007): real-CPU test — the direct-boot image reaches the payload in **under 25% of the emulated CPU cycles** the equivalent DOS 3.3 boot of the same program takes. Emulated cycles, not wall clock, so the result is deterministic across hosts

---

## Phase 7: User Story 6 — BASIC source as a runnable program (P3)

**Goal**: place an Applesoft listing written as host text as a program the guest
can `RUN`.

**Independent test**: quickstart §US6 — place a listing, boot, `LIST` reproduces
the source.

- [ ] T043 [US6] Implement Applesoft tokenization in `CassoCore/ApplesoftTokenizer.h/.cpp`: host-text listing → tokenized on-disk form with line-link fixups (FR-022). Settle the coverage boundary against a real listing — token spellings inside strings, `DATA` payloads, `REM` text
- [ ] T044 [US6] Implement detokenization (the reverse direction, FR-022) in `CassoCore/ApplesoftTokenizer.cpp`; round-trip tests
- [ ] T045 [US6] Refuse an untokenizable listing with the offending line number and text quoted (FR-023); wire `--basic` through `put` and `get`
- [ ] T046 [US6] Gate: place a known listing, boot, `LIST` in the guest, confirm it matches the source

---

## Phase 8: Polish & Cross-Cutting

- [ ] T047 Close the loop end to end (SC-001, SC-006): a gate that runs quickstart's five steps — assemble, put, list, boot, launch — as one scripted sequence, asserting each is a single invocation with no third-party tool and recording elapsed time against the 10-second budget. Neither criterion had a task; both were asserted in prose only
- [ ] T048 Help output (FR-034, SC-002): every capability documented, with a **worked example of the whole loop** — assemble, put, boot — not just a flag list. Assert mechanically that the help text contains that example and that every verb and option it uses also appears in the help output; whether a newcomer succeeds is a review gate, not a test. Document the exit statuses `disk` returns, including that it defines **none above 2** (FR-032) — the requirement is to document the subcommand's scoped codes, and "there are none" is the documentation. Say that `put`/`get` are named from the disk's perspective
- [ ] T049 Update `CHANGELOG.md` and `README.md` (user-visible feature, test-count change); document the deliberate asymmetry that command-line writes are crash-safe while emulator flushes are not, and that in-use detection is out of scope. **Include a CHANGELOG entry for the T004/T005 flush change**, phrased as what it prevents rather than what it refuses — "a damaged track no longer silently truncates your disk image on eject", not "flush now fails". Read the README's current test count at the time of writing rather than adjusting a remembered figure: the suite baseline is in flux independently of this work (the Dormann data was missing from some worktrees, so recent figures measured a suite doing less work, and a fix is in flight elsewhere)
- [ ] T050 Pre-merge gates: `scripts/RunTests.ps1 -Build` for x64 Debug **and** Release (different test sets — Release is not a substitute), `scripts/Build.ps1 -RunCodeAnalysis` clean, `scripts/CheckStyle.ps1` clean; merge to master with `--no-ff`. The gate is **all tests passing**, never a particular total — the suite baseline is moving for unrelated reasons, so a changed count is not by itself evidence of anything. The boot-gated tests (T033, T039, T042, T046) fail rather than skip when the cached master image is absent, so a green suite already proves they ran — no separate confirmation needed
- [ ] T051 Reference GH **#115** — already filed, OPEN, `bug` / `priority: high` / `impact: user` — from the Phase 2 commits: `Refs #115` on T002/T003, `Closes #115` on whichever commit lands T004 + T005 together, since the fix is not complete until the refusal has a recovery path. **Do not file a duplicate**; research R-002's evidence is already on the issue, and a second one splits the discussion

---

## Dependencies

```text
Phase 1 (Setup)
    ↓
Phase 2 (Foundational) ──── blocks EVERYTHING; T003–T006 are a data-loss fix
    ↓
Phase 3 (US3, P1 read) ──── independently shippable
    ↓
Phase 4 (US2, P1 write) ─── needs US3's readers + delete; closes the MVP loop
    ↓
    ├── Phase 5 (US4, P2)
    ├── Phase 6 (US5, P3) ── independent of 4/5; needs only Phase 2
    └── Phase 7 (US6, P3)
    ↓
Phase 8 (Polish)
```

**Within Phase 2**: T002 ∥ T007 ∥ T008 ∥ T009 are parallel; T003 → T004 → T005 →
T006 is a chain; T010 needs T009; T011 needs T009. **T004 and T005 ship
together** — T004 alone converts silent corruption into a refusal with no
recourse, which is half a fix.

**Within Phase 3**: T012 ∥ T014 ∥ T016 (different files); T013 needs T012; T015
needs T014; T017 → T018 → T019 → T020.

**Within Phase 4**: T023 → T024 and T025 → T026 are two independent chains that
can run in parallel; T027 needs both; T028 needs T027; T030 → T031.

## Implementation Strategy

**MVP = Phases 1–4.** US1 is already shipped; US3 then US2 closes the loop the
feature exists to provide. Stop there and the feature is genuinely useful:
extract source off old disks, assemble on the host, place the result back,
boot it.

Phases 5–7 are refinements, each independently valuable, none gating the
migration. Phase 6 (US5) depends only on Phase 2 and can be pulled forward if
direct-boot turns out to matter more than boot configuration.

**The one ordering that is not negotiable** is Phase 2 before any write path.
The decoder currently returns success over data it silently zeroed; building a
write path on top of that ships the same defect from a second direction.
