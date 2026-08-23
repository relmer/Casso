# Implementation Plan: Disk File Access for the Build Loop

**Branch**: `020-disk-file-access` | **Date**: 2026-08-15 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/020-disk-file-access/spec.md`

## Summary

Close the loop **edit → assemble → place on disk → boot** inside one toolchain.
User Story 1 (assembler binary output) is **already delivered**; this plan covers
US2 onward.

The keystone is a filesystem layer that does not exist: `Dos33Volume` and
`ProDosVolume` over the flat 143,360-byte sector buffer — enumerate, read, write,
delete, allocate, free — in `CassoEmuCore` where `UnitTest` links it, per
Constitution Principle VI. A `disk` subcommand exposes it, added additively as
one row in the existing subcommand table.

Three findings from Phase 0 shape the plan more than the story list does:

1. **The two filesystems are not at the same starting point** (R-001). ProDOS has
   a reader and two-thirds of a writer; DOS 3.3 has no reader at all and a
   zero-parameter hardcoded emitter. US3 is P1 and needs both readers.
2. **The sector decoder silently discards what it cannot read** (R-002), on the
   emulator's live flush path. Building a write path on top of it would be
   building on a data-loss bug. The fix is foundational, not incidental.
3. **Delete is P1**, because replace depends on it and neither filesystem can
   delete today.

## Technical Context

**Language/Version**: C++20 (`stdcpplatest`), MSVC v145 (VS 2026)

**Primary Dependencies**: None new. Reuses `NibblizationLayer` (extended),
`DiskImage`, `WozLoader`, `BlankDiskBuilder` and both skeletons,
`CommandLineParser`/`CommandLineOptions`.

**Storage**: Host image files (`.dsk`, `.do`, `.po`, `.woz`), read and written by
the CLI shell only. Core never touches the filesystem.

**Testing**: Microsoft C++ Unit Test Framework. Synthetic buffers throughout;
`BlankDiskBuilder` produces formatted volumes, deliberate corruption produces
damaged ones. Boot-level gates use the real-CPU DOS-boot harness and **fail**
when its cached asset is absent, rather than skipping.

**Target Platform**: Windows 10/11 x64 (ARM64 build-only — no device available,
so x64 Debug + Release green is the bar)

**Project Type**: Desktop app + CLI — `CassoCore` / `CassoEmuCore` libraries,
`CassoCli` console shell, `UnitTest`

**Performance Goals**: Whole loop under 10 s (SC-006); the integrity pass runs
over 560 sectors or 280 blocks, so it is negligible even run per-write.

**Constraints**: All-or-nothing writes, structurally (FR-013); crash-safe commit;
core/shell doctrine; the command-line files are shared with spec 019 developed
concurrently.

**Scale/Scope**: 140 KB 5.25" media, 35 tracks. DOS 3.3 and ProDOS only.

## Constitution Check

*Constitution v1.8.0. Evaluated pre-Phase-0, re-checked post-design.*

**Every row states what was checked, not just the verdict.** A bare "PASS" is
indistinguishable from a check that verified nothing — the same shape the
instructions now call out as degraded operation reading as healthy operation. A
row citing its evidence is falsifiable by a reader in ten seconds; that is what
caught the Principle VI failure below, and it was reading a project file, not
thinking harder. See GH **#85** for the standing thin-executable work and the
list of what legitimately stays in an executable; this plan does not re-argue it.

- **I. Code Quality (NON-NEGOTIABLE)** — PASS. *Checked*: CheckStyle CS0001–CS0020
  runs pre-push and as CI's `style` job in `-Mode Tree`, so EHM shape, banners,
  spacing, and alignment are mechanically gated rather than asserted here. Magic
  numbers and EHM single-exit are not gated and remain review's job. New classes
  get their own `.h`/`.cpp` pairs: `ProDosReader` and `ProDosFileWriter` are
  declared inside `ProDosSkeleton.h`, which is exactly why a filename survey
  missed them (R-001) — debt to avoid repeating, not a template.
- **II. Testing Discipline (NON-NEGOTIABLE)** — PASS. *Checked*: every volume
  operation is data-in/data-out over byte buffers; the commit path reaches the
  host only through `IDiskFileIo`, which `UnitTest` substitutes with a fake whose
  file table is inspectable — so "image unchanged, no temporary left" is
  assertable without touching a real file. **The boot-level gates FAIL when the
  cached master image is absent; they do not skip.** This reverses what spec 017
  did and what an earlier draft of this plan inherited: a test that cannot reach
  its data must not pass, because "N passed" has to mean N things were checked.
  That exact pattern is why the Dormann suite ran green while doing no work.
  Crash safety is the one thing genuinely not unit-testable, so the
  interrupted-write check is a single declared manual pass.
- **III. User Experience Consistency** — PASS. *Checked*: subcommand style with
  long options; diagnostics to stderr and payloads to stdout so output pipes;
  every capability in `--help`; exit statuses 0/1/2 reuse the meanings
  `CommandLine.cpp:1218`/`1159`/`986` already assign rather than minting new ones.
  `--verbatim` was chosen over `--raw` and `--binary` because both already mean
  something else on the assembler side of the same parser.
- **IV. Performance** — PASS. *Checked*: the integrity pass walks at most 560
  sectors or 280 blocks per volume, so running it per write is negligible;
  nothing added runs on the emulation thread.
- **V. Simplicity** — PASS with one deliberate generalization: the integrity pass
  is built once as a first-class mechanism rather than three or four times inside
  its callers (R-005). Justified in Complexity Tracking below.
- **VI. Thin Executable, Testable Core (NON-NEGOTIABLE)** — **initially FAILED;
  now PASS.** *Checked*: `UnitTest.vcxproj` lines 555–570 reference
  `CassoCore`, `CassoEmuCore`, `Casso`, and `Dxui` — **not `CassoCli`**.
  Therefore nothing in `CassoCli` is reachable by a test.

  An earlier draft placed `FileCommit`, the exit-status mapping, the staleness
  comparison, and `DoDisk`'s dispatch and message construction there and recorded
  this row as PASS. Temp-name derivation, collision policy, status mapping, and
  metadata comparison are *decisions*, not syscalls. Resolved by moving them to
  `CassoEmuCore` (which `UnitTest` links and `CassoCli` references) behind
  `IDiskFileIo`. `CassoCli` keeps `Win32DiskFileIo` and a `DoDisk` that
  constructs it, calls the runner, prints, and returns the status — in its own
  `CassoCli/DiskCommand.cpp`, not appended to the 1,222-line `CommandLine.cpp`
  that GH #85 names as the offender.

  Recorded rather than erased, because the failure mode is instructive: the
  natural home for CLI-adjacent logic *is* the CLI, so this condition reproduces
  the violation faster than review catches it. It is the second same-day instance
  after the parser extraction in `8b632268`. The fix that worked was not a more
  careful self-assessment — it was reading a project file. Hence the
  evidence-citing format above.

**Post-design re-check**: PASS. Complexity Tracking carries one justified entry.

## Project Structure

### Documentation (this feature)

```text
specs/020-disk-file-access/
├── plan.md              # This file
├── research.md          # Phase 0 (R-001..R-010)
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── volume-api.md
│   └── disk-subcommand.md
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — T001..T135)
```

### Source Code (repository root)

```text
CassoEmuCore/Devices/Disk/
├── IVolume.h                    # NEW — the filesystem seam
├── Dos33Volume.h/.cpp           # NEW — catalog walk, T/S lists, VTOC bitmap
├── ProDosVolume.h/.cpp          # NEW — wraps + extends the existing reader/writer
├── VolumeIntegrityReport.h/.cpp # NEW — the one pass, four consumers
├── VolumeTypes.h                # NEW — FilePath, FileEntry, FilePayload, listing
├── SectorDecodeReport.h         # NEW — track layer, NOT VolumeTypes.h: the
│                                #   decoder must not include a header from the
│                                #   filesystem layer above it
├── NibblizationLayer.h/.cpp     # EXTEND — coverage-based classification; decode
│                                #   continues past a failed sector; RenibblizeTracks
├── TrackWritability.h/.cpp      # NEW — positive proof of standard-ness
├── Dos33Skeleton.h/.cpp         # EXTEND — geometry constants shared with the volume
└── ProDosSkeleton.h/.cpp        # EXTEND — tree growth, delete, directory reorder

├── IDiskFileIo.h                # NEW — byte-level file seam (read/write/stat/replace)
├── DiskCommandRunner.h/.cpp     # NEW — every disk-command DECISION, testable
├── CommitPlan.h/.cpp            # NEW — temp-name policy, staleness comparison
├── DirectBootBuilder.h/.cpp     # NEW — no-OS boot image (US5, P3). BUILT AND
│                                #   GATED; no caller outside the tests yet (#122)
└── StockBootDisks.h/.cpp        # NEW — where the OS masters are, so a bare
                                 #   --bootable finds one. Lifted out of
                                 #   Casso.exe's AssetBootstrap, which the command
                                 #   line cannot link. Downloading stays in the GUI

CassoCore/
├── CommandLineOptions.h         # EXTEND — Disk subcommand + DiskVerb + operands
├── CommandLineParser.h/.cpp     # EXTEND — ONE table row, ONE arm, ParseDiskOptions
├── AppleTextCodec.h/.cpp        # NEW — host text <-> high ASCII, line endings
└── ApplesoftTokenizer.h/.cpp    # NEW — listing <-> tokenized form (US6, P3)

CassoEmuCore/Cli/                # THE WHOLE COMMAND-LINE APPLICATION, testable
├── CliMain.h/.cpp               # everything main used to do: parse, dispatch,
│                                #   and the exit code each arm earns
├── DiskCommand.h/.cpp           # construct IO, call runner, deliver to the streams
├── Win32DiskFileIo.h/.cpp       # ifstream / ofstream / ReplaceFileW / stat
├── CommandLine.h/.cpp           # every page of help, and the terminal edge
└── ArtifactWriter, SourceAssembler, HostFile, and the four mode runners

CassoCli/                        # THE ENTRY POINT AND NOTHING ELSE — 57 lines
└── CassoCli.cpp                 # int main { return CliMain (argc, argv); }

# The plan put the CLI layer under CassoCli and called it "syscalls and printing
# ONLY". That was the wrong criterion, and the line above it proves the cost: the
# file reserved for "one registration line" reached 778, and the dispatch that
# chose every exit code sat beside it where no test could reach it. Two defects
# lived there undisturbed — the documented exit statuses were never the ones
# returned, and a bare invocation exited 0 where its own comment said 1.
#
# The criterion is UT-reachability, not platform, so the Win32 file layer moved
# into the library as well. See GitHub issue #85.

UnitTest/
├── CommandLineTests.cpp         # EXTEND — disk grammar (existing tests MUST stay green)
└── EmuTests/
    ├── Dos33VolumeTests.cpp     # NEW
    ├── ProDosVolumeTests.cpp    # EXTEND — tree, delete, reorder, paths
    ├── VolumeIntegrityTests.cpp # NEW — cross-links, cycles, termination
    └── NibblizationTests.cpp    # EXTEND — decode report, partial-track recovery
```

**Structure Decision**: Existing four-project split, no new projects, no new
dependencies. The filesystem layer lands in `CassoEmuCore` beside the disk
devices; the grammar lands in `CassoCore` beside the parser it extends.

`AppleTextCodec` and `ApplesoftTokenizer` go in `CassoCore` because they are pure
text transforms with no device dependency, they sit naturally beside `Parser` and
the expression evaluator, and `CassoCore` is the lower layer. **Not** because it
saves `CassoCli` from linking `CassoEmuCore` — `CassoCli.vcxproj` already
references both projects, so that reasoning is simply false and must not be
repeated as though it were a constraint.

**Adjustments made during implementation**, recorded because each was a decision
rather than a detail:

- **`FilePath` is its own header/pair**, not a type inside `VolumeTypes.h`. The
  style rule is one type per pair, and it has behavior (parsing, rejoining) that
  the other three plain aggregates do not.
- **`ChainWalkGuard` and `VolumeImage` were added** and are not in the tree
  above. The guard is the bounded-traversal mechanism both readers share, split
  out so the termination guarantee has one implementation; `VolumeImage` is
  where sector order and filesystem detection are settled once, so no caller has
  to remember which container it holds.
- **`CassoCli` gained its own `Pch.h`.** The platform edge needs `<windows.h>`,
  `<io.h>` and `<filesystem>`; putting those into `CassoCore`'s Pch would drag
  the platform into the assembler, which is deliberately free of it. The new
  header adds them and then includes `CassoCore/Pch.h` whole, so a translation
  unit here sees exactly what one in `CassoCore` sees, plus the platform. This
  also forced `std::` qualification on the disk headers, since they are now
  consumed by a project without `using namespace std`.
- **`ProDosSkeleton` gained a public `IsBlockInRange`** and validation on every
  block pointer it follows. This was a live out-of-bounds vector read reachable
  through `InstallBoot`, not hardening: proved by stubbing the check to `true`,
  which aborts the test host.

## Phasing

Ordered by dependency, not by story number. Each phase is a commit.

**`tasks.md` supersedes the lettering below.** These letters group work by
*component*; tasks.md regrouped the same work by *story* (Phase 1 Setup,
2 Foundational, 3 US3, 4 US2, 5 US4, 6 US5, 7 US6, 8 Polish) so each phase is an
independently testable increment, which is what the task breakdown needs and
what this sketch was not. The dependency order is identical either way — only the
grouping differs. Mapping: A → tasks Phase 2; B and C → split across tasks
Phases 3 (readers) and 4 (writers + delete); D → split across the same two, since
the CLI verbs land with the story they serve; E → Phase 5; F → Phases 6 and 7;
G → Phase 8.

**Phase A — Foundational (blocks everything).**
`SectorDecodeReport` classifying each track by **coverage** — a 16-bit mask,
`Complete` iff all sixteen logical sectors were filled exactly once — which
subsumes the `break`, out-of-range, and duplicate zero-fill paths in one check
rather than patching each (FR-018); decode continues past a failed sector and
resynchronizes; the three-argument `Denibblize` rewired to forward and fail on
data loss, which fixes `DiskImage::Serialize` without touching it; the misleading
generalization in `NibblizationTests.cpp:308`'s comment narrowed to the
unformatted case, plus sibling tests for the partial, out-of-range, and duplicate
cases; `TrackWritability` (FR-016, FR-017, FR-019); `RenibblizeTracks`;
`VolumeIntegrityReport` with bounded traversal (FR-037, FR-038). No write path
may consume denibblized output before this lands.

**Phase B — DOS 3.3 volume.** Reader first (US3 is P1 and nothing exists);
then write, delete with free-space return, replace. Generalizes
`Dos33FileWriter::WriteHello` into a real writer.

**Phase C — ProDOS volume.** Wrap the existing reader/writer behind `IVolume`;
add tree growth, delete with free-space return, path-based traversal.

**Phase D — Commit path + CLI.** `CommitPlan` and `DiskCommandRunner` in core
behind `IDiskFileIo`; the one table row and one arm; `Win32DiskFileIo` and a thin
`DoDisk` in the exe; help text. Delivers US2 and US3 end to end.

**Phase E — Boot configuration (US4, P2).** DOS 3.3 greeting patch at the
verified offset; ProDOS directory reorder. Two mechanisms, not one helper.

**Phase F — P3 stories.** Direct-boot image (US5); Applesoft tokenizer (US6).

**Phase G — Polish.** CHANGELOG, README, help worked example, full gates.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Volume integrity built as a first-class pass rather than inline in each caller | Four consumers need the same reference map: delete's safety rule (FR-011), the listing's damage report (US3), allocation's trust in the free map, and the pre-commit self-check on every write (FR-039) | Computing it inline in each caller means four approximate versions that drift. The fourth consumer is the one that matters: a write path that never inspects its own output is exactly how the denibblization defect (R-002) shipped and survived. Building it once is both less code and the structural fix for that class of defect. |
