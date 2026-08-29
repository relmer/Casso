# Implementation Plan: Assembler-to-Disk Output

**Branch**: `026-assembler-to-disk` | **Date**: 2026-08-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/026-assembler-to-disk/spec.md`

## Summary

The assembler gains a disk image as a destination for its object, so the build
loop stops assembling to a host file and then placing that file. The object's
load address comes from the source's own origin instead of being retyped on the
command line, which is the correctness half of the feature. Three Merlin
directives start meaning what Merlin meant: `TYP` sets the filesystem type,
`SAV` writes an output and carries on, and `DSK` names a file on a volume
rather than on the host.

**The technical approach is almost entirely composition of seams that already
exist**, which is the single most important fact about this plan. `ArtifactSink`
is already an abstraction over where an assembly's output goes, with
`FileArtifactSink` as its one implementation and `AssemblerMode::Run` already
taking a sink by pointer. `IVolume` already computes a complete new buffer per
write and never mutates in place. `DiskImageSession` is already documented as
"ONE IMAGE, OPENED AND COMMITTED AS A TRANSACTION". So the all-or-nothing
guarantee across several save points is not something this feature builds; it is
something it inherits by feeding each write the buffer the previous one
returned and committing once at the end.

Three things are genuinely new: a save-point list on `AssemblyResult`, handlers
for `TYP` and `SAV` once their boundary rows are deleted, and a sink that writes
into a volume. Everything else is wiring.

## Technical Context

**Language/Version**: C++ (`stdcpplatest`, MSVC v145 / VS 2026)

**Primary Dependencies**: None new. Windows SDK and the STL, as today. This
feature adds no entry to the constitution's third-party allowlist.

**Storage**: Apple II disk images in the container formats the tree already
reads and writes (DSK/DO/PO sector images and WOZ bit streams), holding DOS 3.3
or ProDOS volumes. No new container or filesystem.

**Testing**: Microsoft C++ Unit Test Framework, in `UnitTest/`. Every path in
this feature is reachable there, because the assembler side is in `CassoCore`,
the sink and disk layer are in `CassoEmuCore`, and the host edge is behind
`IDiskFileIo`. No test in this feature touches a real file.

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only; x64 Debug
and Release green is the bar).

**Project Type**: CLI assembler plus emulator, over two static core libraries.

**Performance Goals**: Not a factor. The work is one image read, some in-memory
buffer composition, and one atomic replace, all of it already the cost of a
`disk put`. No hot path is touched.

**Constraints**: An assembly that fails at any point must leave the target
byte-for-byte unchanged, including after an earlier save in the same assembly
already produced a file. This is the design's central constraint and it is what
forces every output to buffer until the whole assembly has succeeded.

**Scale/Scope**: Around 11 source files touched and 5 added, plus 3 project
files, 3 documents and the CHANGELOG. One boundary table loses two rows; two
directive handlers arrive; one new `ArtifactSink` implementation; the assembler
grammar gains four flags and Merlin's listing flag changes shape.

**Unknowns**: None outstanding. Five were raised, four settled with the owner in
`/speckit-clarify`, and the fifth (what a second `SAV` writes) was settled
against Merlin's own manual rather than by preference. See
[research.md](research.md).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Verdict | How this plan satisfies it |
|---|---|---|
| **I. Code Quality** | PASS | No new style exceptions. EHM throughout, single exit for `HRESULT` functions, declarations at function top, class statics rather than free functions, American spelling, no `<>` includes outside `Pch.h`. The new files follow the banner and blank-line structure `CheckStyle.ps1` enforces; a new file is checked with `-Mode Staged` before its first commit, since diff mode cannot see it. |
| **II. Testing Discipline** | PASS | Every unit added is reachable from `UnitTest`. Volume writes run against synthetic in-memory sector buffers, and the host edge is mocked through `IDiskFileIo`, which 020 built for exactly this. No test reads or writes a real file. |
| **III. UX Consistency** | PASS | The image target reuses the disk grammar's existing vocabulary rather than inventing a second one, and refusals are composed the way the disk runner already composes them. FR-022 is the sharp edge here and the plan honors it: the startup-program rules are *shared with* `RunBoot`, not restated, so the two routes cannot come to disagree. |
| **IV. Performance** | PASS | Not applicable; no hot path touched. |
| **V. Simplicity** | PASS | The feature adds one implementation of an existing interface and two directive handlers. It deliberately does **not** adopt Merlin's streaming `DSK`, because Casso has no memory ceiling to justify it and streaming would defeat the all-or-nothing guarantee. See research finding 3. |
| **VI. Thin Exe, Testable Core** | PASS | Nothing lands in an exe. The assembler side is `CassoCore`, the sink and disk work are `CassoEmuCore`, and both are linked by `UnitTest`. `CassoCli` gains nothing. |

**Post-Phase-1 re-check**: PASS, unchanged. The design added no exe code, no new
dependency, and no untestable surface. The one thing worth re-stating after
design is that `CassoCore` still knows nothing about disks: the assembler
*reports* save points and a file type, and `CassoEmuCore` decides what a volume
does with them. That is what keeps FR-003 true rather than aspirational.

**Complexity Tracking**: No violations to justify. The table at the end of this
document is intentionally empty.

## Project Structure

### Documentation (this feature)

```text
specs/026-assembler-to-disk/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   ├── cli.md           # The image-target flags and their refusals
│   └── merlin-directives.md  # TYP / SAV / DSK behavior and the type map
├── checklists/
│   └── requirements.md  # Spec quality checklist (from /speckit-specify)
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
CassoCore/                        # Dialect-neutral assembler. Knows nothing of disks.
├── AssemblerTypes.h              # + SavePoint, AssemblyResult::savePoints, ::fileType,
│                                 #   + per-symbol output scope
├── AssemblySession.h/.cpp        # + HandlePass1FileType, HandlePass2SaveObject,
│                                 #   span tracking, DSK closing the current save point,
│                                 #   recording each symbol's scope as it is defined
├── Assembler.cpp                 # FormatDebugInfo: indexes per output, not one flat pair
├── MerlinSubsetBoundary.cpp      # - the TYP and SAV rows (six rows become four)
├── CommandLineOptions.h          # + the image-target fields on the assembler options
└── CommandLineParser.cpp         # + the flags, in the existing table-driven grammar

CassoEmuCore/
├── Cli/
│   ├── ArtifactWriter.h/.cpp     # unchanged seam; FileArtifactSink gains save points
│   ├── ImageArtifactSink.h/.cpp  # NEW: the sink that writes into a volume
│   └── AssemblerMode.cpp         # + choose the sink from the options
└── Devices/Disk/
    ├── AssembledFilePlacement.h/.cpp  # NEW: save point -> FilePayload, and the type map
    └── DiskCommandRunner.h/.cpp  # startup-program rules extracted for sharing (FR-022)

UnitTest/
├── SavePointTests.cpp            # NEW: the save-point invariants, no disk involved
├── AssemblerToDiskTests.cpp      # NEW: the sink, the transaction, the refusals
├── MerlinSaveObjectTests.cpp     # NEW: SAV span semantics, load addresses, two DSKs
├── MerlinSubsetBoundaryTests.cpp # the sweep, now over four rows
└── Fixtures/Merlin/              # + a two-save source; CLOCK.S cannot cover this

# Every NEW file above also needs a ClCompile/ClInclude entry in its owning
# .vcxproj. MSBuild does not glob, so a file that compiles locally can still be
# absent from a clean build.

docs/
├── merlin-subset.md              # "Six constructs" becomes four; TYP/SAV/DSK move
├── Assembler.md                  # the build loop loses a step
└── README.md                     # the same three-command example
```

**Structure Decision**: The existing layout, with no new project and no file
moved. The split falls out of one question asked per unit: can `UnitTest` link
and exercise it? The assembler half is dialect-neutral and belongs in
`CassoCore`, which must not learn what a disk is, or FR-003 stops being
structurally true and becomes a thing to remember. The sink and the placement
logic belong in `CassoEmuCore` beside the disk layer they use, and beside
`ArtifactWriter`, whose seam they implement. `CassoCli` and `Casso` gain
nothing, per Principle VI.

## Implementation Phases

The phases below are the intended shape for `/speckit-tasks`, not tasks
themselves. They are ordered so each one is independently verifiable and so the
P1 story is deliverable before any of the directive work begins.

### Phase A — Foundational: save points in the assembler

Nothing user-visible; everything downstream depends on it.

`AssemblyResult` gains a `savePoints` vector and a reported file type, following
the "REPORTED rather than acted on" pattern `outputFileName` already documents.
`AssemblySession` tracks where the current span began so a span can be cut. An
assembly with no `SAV` produces exactly one save point covering the whole
object, which is what makes every later phase uniform: **there is no separate
"single output" path to keep in step.**

Verified by unit tests over `AssemblyResult` alone, with no disk anywhere.

### Phase B — User Story 1: assemble onto an existing image (P1)

The whole feature in one action, and independently shippable.

`ImageArtifactSink` implements `ArtifactSink` over a `DiskImageSession`: open,
compose each save point through `IVolume::Write` onto the buffer the previous
one returned, commit once. `AssemblerMode::Run` selects it when the options name
an image. The command-line grammar gains the image target and the on-volume
name, through the existing table-driven parser.

The load address comes from the save point, never from a flag — FR-005, and the
correctness win the feature exists for.

**Refusals land here, not later**: image missing (FR-018), no recognized
filesystem, volume full, no free directory entry, name illegal on the target
filesystem, image held by another program. Each leaves the image byte-for-byte
unchanged, which `DiskImageSession` already guarantees, and each says which
condition it hit (FR-015).

### Phase C — User Story 2: `TYP` and `DSK` (P2)

Delete the `TYP` row from `s_kMerlinBoundary` and add `HandlePass1FileType`.
Add the ProDOS-to-DOS-3.3 type map from research finding 4, with `SYS` refused
by name on DOS 3.3 (FR-010) and an unrecognized byte refused naming the byte
(FR-011). Point `DSK` at the on-volume name when an image target is given.

The type map is a small, pure, table-driven unit and belongs where a test can
sweep it in both directions, the way `DirectiveTokenTests` sweeps `DialectId`.

### Phase D — User Story 3: `SAV`, and a second `DSK` (P3)

Delete the `SAV` row and add the pass-2 handler. This is where the manual's
semantics land: a save cuts the span and empties the accumulation, the next save
starts fresh, and each save point's load address is the address of its own first
byte (FR-012, FR-024). A second `DSK` closes the current save point and opens
another (FR-025).

`SAV` with no image target writes host files (FR-020), which is why
`FileArtifactSink` must also iterate save points. Doing both is *cheaper* than
special-casing one, because the list is the same list.

**The host-side artifacts change here too, and this is easy to miss.** Once an
assembly can produce several outputs, `Assembler::FormatDebugInfo`'s flat
by-address index stops being able to answer the question its own comment says it
exists for — "what is at $0310" — because independent outputs may occupy
overlapping addresses and are never loaded together. The listing, symbol and
debug artifacts split into a set per output, named from each output's own name
(FR-028, FR-031, FR-032), with the shared equates repeated into each so every
file stands alone (FR-035, FR-036). Single-output assemblies keep their present
names and destinations (FR-033), and multi-output cannot happen today since
`SAV` is refused, so the only change anyone can currently observe is Merlin's
listing flag dropping its filename and its standard-output default (FR-034,
FR-037). This
is not disk work and
does not depend on Phase B; it depends on Phase A's save points, and it is
required for the same reason `SAV` is.

**This phase carries the feature's hardest test**: an assembly that fails after
a save has already been composed must leave the image untouched. It is
structurally guaranteed, so the test must be written to fail without the
guarantee — see the note below.

### Phase E — User Story 4: the startup program (P3)

Extract the runnability rules from `RunBoot` — including
`IsRunnableAsDos33Greeting`, whose comment records that a DOS 3.3 greeting is
`RUN` and not `BRUN`, so a binary named as the greeting leaves the disk booting
and the program never running — into something both routes call. Then add the
flag (FR-021), refused when no image target was given (FR-023).

**The extraction is the point, not the flag.** A second copy of those rules is
precisely how `run --merlin` once came to refuse `XC` while `merlin` accepted
it, which `AssemblerMode.h` records.

### Phase F — Polish: docs, help, and the published boundary

`docs/merlin-subset.md` ("Six constructs are recognized and refused by name"
becomes four; `TYP`, `SAV` and the corrected `DSK` move to the supported
table), `docs/Assembler.md` and `README.md` (the three-command build loop
becomes two), the assembler's own help (FR-017), and `CHANGELOG.md`.

`MerlinSubsetBoundary::GetHelpText` needs no edit: it composes from the rows, so
deleting two rows updates the published list by construction. That is the
property the boundary table exists to have, and this feature is its first real
exercise.

## Testing Notes

Three of this tree's recorded lessons bear directly on this feature, so they are
called out rather than left to be rediscovered.

**A degraded write must not read as a healthy one.** The tree has been bitten
five times by this, most relevantly by `NibblizationLayer::Denibblize` returning
`S_OK` over sectors it had zero-filled, on the flush path (GH #115). Every
failure path here must refuse rather than partially succeed, and the tests must
assert the image is byte-for-byte unchanged, not merely that a call returned
non-`S_OK`.

**Assert a non-zero count before asserting over a collection.** A save-point
loop over an empty list passes while checking nothing, and is indistinguishable
in the output from a full one. Every test that iterates save points asserts how
many there are first.

**Mutate what the test covers and confirm the test notices.** The all-or-nothing
guarantee is structural — `IVolume` computes a whole buffer or none — so a test
for it can pass without the feature being right. There is no prior behavior to
revert to, so the discrimination has to come from stubbing: make the composition
commit after each save point instead of at the end, and confirm the test goes
red. A green run under that mutation means the test is measuring nothing.

Beyond the unit suite, this feature changes assembler output paths, so the
pre-merge gate runs the Dormann and Harte suites. It does not change the CPU or
the instruction set, so Harte at the checked-in 200-vector depth is the right
depth; full depth is for CPU work.

## Open Risks

- **A two-save Merlin fixture has to be authored, not acquired.** `CLOCK.S` is
  the only committed source with two `SAV` lines and they are mutually exclusive
  (`DO`/`ELSE`/`FIN`), so the corpus cannot cover Phase D. An authored fixture
  is a weaker authority than a vendor source, and the plan should say so out
  loud rather than let a hand-written file look like period evidence.

  **This is now partly mitigated.** Real Merlin Pro 2.23 can be driven under
  Casso and its answers read back off the disk, so an authored fixture can be
  checked against the period assembler instead of standing on its own. The
  delta-save rule and the origin rule were settled this way; the procedure and
  its traps are in [research.md](research.md) finding 2a. What remains is to run
  the same loop for the `DSK` span rules and the trailing-span rule, whose
  sources are already written.
- **`TYP`'s exact operand syntax is documented only indirectly.** The Merlin
  Pro manual's OCR is truncated at the entry. Research finding 4 reasons from
  the ProDOS type set and from what Casso already publishes, which is solid for
  the four types that matter, but a primary quote is still missing.
- **`ProDosReader` and `ProDosFileWriter` are declared inside
  `ProDosSkeleton.h`**, so a survey by filename misses them. This cost 020 time
  once already; it is noted here so it does not cost this feature time too.

## Complexity Tracking

> Fill ONLY if Constitution Check has violations that must be justified.

No violations. No entries.
