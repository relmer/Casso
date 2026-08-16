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
damaged ones. Boot-level gates reuse the existing real-CPU DOS-boot harness with
mandatory skip-if-missing.

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

- **I. Code Quality (NON-NEGOTIABLE)** — PASS. EHM single-exit, declarations at
  top, banners, no anonymous namespaces, no magic numbers. Gated by CheckStyle
  pre-push and CI. New classes get their own `.h`/`.cpp` pairs rather than being
  nested in a skeleton header — the existing readers/writers are declared inside
  `ProDosSkeleton.h`, which is precisely why a filename survey missed them
  (R-001). That is debt to avoid repeating, not a template.
- **II. Testing Discipline (NON-NEGOTIABLE)** — PASS. Every volume operation is
  data-in/data-out over byte buffers, so no test touches a real file, registry,
  or process. The boot-level acceptance gates reuse the sanctioned existing
  exception (cached master image, graceful skip when absent).
- **III. User Experience Consistency** — PASS. Subcommand style with long
  options; errors to stderr; every capability in `--help`; exit statuses reuse
  the meanings `as65` and `run` already assign rather than minting new ones.
- **IV. Performance** — PASS. Negligible data volumes; nothing on the emulation
  thread.
- **V. Simplicity** — PASS with one deliberate generalization: the integrity pass
  is built once as a first-class mechanism rather than three or four times inside
  its callers (R-005). Justified below.
- **VI. Thin Executable, Testable Core (NON-NEGOTIABLE)** — **initially FAILED;
  now PASS after re-siting.** The first draft of this plan put `FileCommit`, the
  exit-status mapping, the staleness comparison, and `DoDisk`'s dispatch and
  message construction in `CassoCli` and recorded the principle as satisfied. It
  is not: **`UnitTest.vcxproj` references CassoCore, CassoEmuCore, Casso, and
  Dxui — not CassoCli** (verified). Nothing in `CassoCli` can be linked by a
  test, so every one of those was unreachable by the litmus this plan quotes.
  Temp-name derivation, collision policy, status mapping, and metadata
  comparison are *decisions*, not syscalls, and the existing `CommandLine.cpp`
  is exactly what the principle names as divergence not to imitate.

  Resolved by moving all of it into `CassoEmuCore` — which `UnitTest` links and
  `CassoCli` references — behind an `IDiskFileIo` seam: core owns `IVolume`,
  both volumes, `VolumeIntegrityReport`, `SectorDecodeReport`, `TrackWritability`,
  `CommitPlan`, `DiskCommandRunner`, and the extended `NibblizationLayer`; the
  `disk` grammar stays in `CassoCore` beside the parser. `CassoCli` keeps only
  `Win32DiskFileIo` (the `ifstream` / `ofstream` / `ReplaceFileW` calls) and a
  `DoDisk` that constructs it, calls the runner, prints, and returns the status.
- **II. Testing Discipline / Test Isolation (NON-NEGOTIABLE)** — PASS **only
  because of the seam above**. The commit path's verification (SC-005: image
  unchanged after every documented failure, no temporary left behind) would
  otherwise require reading and writing real files, which Test Isolation forbids.
  Those checks run against a fake `IDiskFileIo` whose file table is inspectable.
  Crash safety genuinely cannot be unit-tested, so the interrupted-write check is
  the one sanctioned manual pass, called out as such in quickstart.md.

**Post-design re-check**: PASS, after a cross-artifact analysis pass caught the
VI violation above. Complexity Tracking carries one justified entry.

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
└── tasks.md             # Phase 2 (/speckit-tasks — T001..T051)
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
└── DirectBootBuilder.h/.cpp     # NEW — no-OS boot image (US5, P3)

CassoCore/
├── CommandLineOptions.h         # EXTEND — Disk subcommand + DiskVerb + operands
├── CommandLineParser.h/.cpp     # EXTEND — ONE table row, ONE arm, ParseDiskOptions
├── AppleTextCodec.h/.cpp        # NEW — host text <-> high ASCII, line endings
└── ApplesoftTokenizer.h/.cpp    # NEW — listing <-> tokenized form (US6, P3)

CassoCli/                        # syscalls and printing ONLY — UnitTest cannot link this
├── CommandLine.cpp              # + DoDisk: construct IO, call runner, print, return status
└── Win32DiskFileIo.h/.cpp       # NEW — ifstream / ofstream / ReplaceFileW / stat

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
