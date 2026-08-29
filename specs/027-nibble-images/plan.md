# Implementation Plan: Nibble Disk Images

**Branch**: `027-nibble-images` | **Date**: 2026-08-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/027-nibble-images/spec.md`

## Summary

Add `.nib` and `.nb2` as a fifth container: mount them, boot from them, write them
back when the guest has changed them, and accept them across all nine `disk`
commands.

The technical approach follows from one fact established in
[research.md](research.md): a track's bit length is fixed when the image is mounted
and a guest write cannot change it, so the derived byte count can only ever be less
than or equal to the block size. Overflow is arithmetically impossible; under-fill
is the normal case. The work is therefore a new byte-level codec sitting where
`WozLoader` sits -- a peer of it, not an extension of `NibblizationLayer`, which
converts sectors rather than nibble bytes -- plus a routing-table entry that the
file filters, the picker and the folder scan pick up without being told, plus two
new refusal reasons, plus a container arm in the blank-disk builder.

Nothing about the sector path changes. That is worth stating as an intent, because
the feature's whole risk profile depends on it: `.dsk`, `.do`, `.po` and `.woz` must
come out of this byte-identical in behavior.

## Technical Context

**Language/Version**: C++, `stdcpplatest`, MSVC v145 (VS 2026)

**Primary Dependencies**: None added. Windows SDK and the STL only; this feature
introduces no third-party code and needs no constitution amendment to the
dependency allowlist.

**Storage**: Host files. Reads and writes go through the existing seams --
`DiskImageStore`'s `ImageReader` / `FlushSink` for the emulator, `IDiskFileIo` for
the console -- so tests never touch the filesystem.

**Testing**: Microsoft C++ Unit Test Framework, in `UnitTest`. New tests join the
existing disk suites (`NibblizationTests.cpp`, `WozLoaderTests.cpp`,
`DiskImageStoreTests.cpp`, `CrossFormatWriteTests.cpp`,
`CrossFormatExtractionTests.cpp`, `DiskCommandRunnerTests.cpp`,
`DiskFailureModeTests.cpp`), plus one new file for the codec itself.

**Target Platform**: Windows 10/11, x64 and ARM64. Nothing here is
architecture-sensitive; ARM64 is build-only.

**Project Type**: Desktop application plus console tool over shared static core
libraries.

**Performance Goals**: A mount must not be perceptibly slower than a `.woz` mount.
The derivation is one linear pass over 53,248 bits per track, 35 tracks -- the same
order of work the existing loaders already do at mount.

**Constraints**: The write-back must leave untouched tracks byte-identical. A
malformed image must produce a verdict, never an assertion: `E_INVALIDARG` marks a
coding error in this tree and always asserts, so wrong-length and
not-a-nibble-stream both need real diagnoses.

**Scale/Scope**: 35 tracks, 6,656 or 6,384 bytes each; 232,960 or 223,440 bytes per
file. One new class pair, one new enumerator on a total enum switched across
roughly a dozen files, two new refusal reasons.

## Constitution Check

*GATE: passed before Phase 0, re-checked after Phase 1 design. No violations.*

| Principle | Assessment |
|---|---|
| **I. Code Quality** | Nothing here resists the house style. The codec is data-in/data-out over byte vectors, so EHM with a single exit is natural. Watch the usual traps: the two accepted track sizes and the sync byte are named constants, not literals; the derivation loop is bounded by one revolution, matching the existing `ReadNibbleAt`. **Every function this feature adds is VerbNoun** -- `GetContainers`, `ResolveGeometry`, `GetPrimaryExtension`, `Load`, `Serialize`. A noun-first name (`ContainersFor`, `ExtensionFor`) reads as a value rather than an action and is not accepted, `OnXxx` handlers aside. |
| **II. Testing Discipline** | Every part is reachable from `UnitTest` without a file on disk. The codec takes and returns byte vectors; the store already has reader and flush seams; the console runner already takes `IDiskFileIo`. The round-trip invariant in research D5 gives the strongest tests a concrete assertion rather than a smoke check. |
| **III. UX Consistency** | The refusals follow `MountDiagnosis::Describe`, which produces a predicate clause the console and the GUI each wrap in their own subject, so one wording serves both. `create`'s type list is extended in the one table that already drives both the acceptance and the error text. |
| **IV. Performance** | One linear pass per track at mount and at flush. No hot path is touched. |
| **V. Simplicity** | One new class pair. The alternative -- bolting nibble byte handling onto `NibblizationLayer` -- would put two unrelated conversions behind one name, which is what the spec's "different seam" observation is about. |
| **VI. Thin Exe, Testable Core** | Every decision lands in `CassoEmuCore`. The drive widget's filter already forwards to the store and the console's command runner already lives in core. The one executable file this feature touches, `CreateDiskDialog.cpp`, comes out holding *less* logic than it does today -- see below. |

**Degraded Operation Must Be Observable** (the doctrine in
`.github/copilot-instructions.md`, which this feature is unusually exposed to):

- The derivation must never return a short or empty track as though it succeeded. A
  track with no high-bit-set byte anywhere has no first nibble, and that is a
  reportable state, not an empty result.
- `DiskFormat` is a **total** enum. Adding an enumerator means sweeping the enum, not
  the tables that switch on it -- a table sweep visits only rows that exist by
  construction and structurally cannot find the arm somebody forgot. This is not
  hypothetical here: `BlankDiskBuilder::ValidateSpec` is missing its
  `DiskFormat::Do` arm today and asserts on a container the tool advertises. That
  defect is filed separately rather than folded in, but the sweep this feature adds
  is what would have caught it.
- Round-trip tests must assert a non-zero track and byte count before asserting over
  the contents, or a codec that produced nothing would pass.

**The create dialog's container list is extracted rather than extended.** Offering
the new container in the interface's Create Disk dialog looked at first like adding
an arm to a switch inside `Casso.exe`, which Principle VI forbids for new code and
which the plan's own Constitution Check row would have contradicted.

Looking at why the arm was needed gives a better answer. `CreateDiskDialog.cpp`
decides which containers can hold which contents -- DOS 3.3 in `.dsk` or `.woz`,
ProDOS in `.po` or `.woz` -- and `BlankDiskBuilder::ValidateSpec` decides the same
thing again in core. **That is two lists of the same rule, which is precisely the
arrangement that let the file filter and the loader disagree over `.nib` in the
first place**, and it is why `ValidateSpec`'s missing `DiskFormat::Do` arm could go
unnoticed while the dialog worked fine.

So the pairing moves into core as `BlankDiskBuilder::GetContainers`, `ValidateSpec`
answers from it, and the dialog renders what it returns. The executable loses a
decision instead of gaining one, the duplication goes away, and the new container
appears in both surfaces because there is only one place left to add it.

## Project Structure

### Documentation (this feature)

```text
specs/027-nibble-images/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Pre-spec notes + Phase 0 decisions
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── nibble-image.md
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks, not created here)
```

### Source Code (repository root)

```text
CassoEmuCore/Devices/Disk/
├── NibbleImageCodec.h        # NEW -- bytes <-> bit streams, both directions
├── NibbleImageCodec.cpp      # NEW
├── IDiskImage.h              # DiskFormat gains Nib
├── DiskImage.cpp             # LoadFromBytes / Serialize gain the Nib arm
├── DiskImageStore.cpp        # extension routing renamed + retargeted, ClassifyLoadFailure
├── MountDiagnosis.h/.cpp     # two new failure reasons + the extension lookup renamed to VerbNoun
├── VolumeImage.cpp           # Load / Save gain the nibble container
├── NibblizationLayer.h/.cpp  # ReadNibbleAt promoted to a shared entry point
├── BlankDiskBuilder.cpp      # ValidateSpec / Build gain the Nib arm
└── DiskCommandRunner.cpp     # s_kContainers gains nib and nb2

UnitTest/EmuTests/
├── NibbleImageCodecTests.cpp     # NEW -- derivation, padding, round trip
├── CrossFormatWriteTests.cpp     # nibble write-back joins the matrix
├── CrossFormatExtractionTests.cpp
├── DiskImageStoreTests.cpp       # routing, filter agreement, refusals
├── DiskFailureModeTests.cpp      # the malformed-image verdicts
├── DiskCommandRunnerTests.cpp    # all nine commands
├── BlankDiskBuilderTests.cpp     # create/init, and the container pairing sweep
├── DiskWritePathTests.cpp        # flush behavior and the unpersisted-write report
└── BootDiskTests.cpp             # a created nibble image boots

UnitTest/UiTests/
└── DriveWidgetStateTests.cpp     # filter agrees with the router

Casso/Ui/Dialogs/CreateDiskDialog.cpp  # renders core's container list, decides nothing
Casso/Ui/DriveWidgetState.h       # NOT CHANGED -- already asks the store
README.md, docs/                  # format table, drag-and-drop claim
CHANGELOG.md                      # user-visible feature entry
```

**Structure Decision**: Everything functional lands in `CassoEmuCore/Devices/Disk`,
beside the loaders it joins. `Casso/Ui/DriveWidgetState.h` is listed above
specifically to record that it must **not** change: it forwards to
`DiskImageStore::IsMountableImageExtension`, so drag-and-drop, the picker and the
folder scan follow the routing table for free. A diff that touches it has
reintroduced the second extension list that was deliberately removed, and should be
rejected on sight.

## Implementation Phases

### Phase A -- The codec, standing alone

`NibbleImageCodec` with no callers yet, and its tests. Two directions:

- **Load**: a byte vector to per-track bit streams. Length picks the track size;
  each track's bytes are packed eight bits per byte, MSB first, and the track bit
  count is set to `trackBytes * 8`.
- **Serialize**: a `DiskImage` plus the original file bytes to a byte vector. Dirty
  tracks are re-derived by the MSB rule, rotated so the longest `$FF` run ends the
  sequence, and padded with `$FF` to the block size. Clean tracks are copied from
  the original bytes verbatim.

This phase is where the round-trip invariant is nailed down: an unmodified track
with every byte's high bit set derives back to exactly its original bytes.

`ReadNibbleAt` is promoted from a file-scope static in `NibblizationLayer.cpp` to a
shared entry point rather than copied, so one MSB rule serves the sector decoder and
the byte derivation. A second copy is how the two would come to disagree about what
a nibble is.

### Phase B -- Routing and refusals

`DiskFormat::Nib`; the extension router is renamed `GetSourceFormatByExtension` and
maps both extensions;
`MountDiagnosis` gains the two reasons a nibble image is refused for, worded as
clauses that name the length found and the lengths accepted;
`ClassifyLoadFailure` distinguishes them. `DiskImage::LoadFromBytes` and
`::Serialize` gain their arms.

At the end of this phase a nibble image mounts, boots, and survives an eject
untouched. User Story 1 is deliverable here.

### Phase C -- Write-back on the live path

Wire `Serialize` to the flush path and prove it under real guest writes: boot,
write, eject, remount, verify. This is where the padding and rotation meet software
that was not written to be accommodating. User Story 2.

The `DiskImage` already keeps `m_rawSourceBytes` and per-track dirty bits, so the
copy-untouched-tracks rule has both halves it needs without new state.

### Phase D -- The console

`VolumeImage::Load` / `Save` gain the container, which is the single seam all nine
`disk` commands reach the format through. `s_kContainers` gains `nib` and `nb2`;
`BlankDiskBuilder` gains its arm so `create` and `init` work. User Story 3.

`Save` for a nibble image is the interesting one: it decodes to sectors, judges
writability and re-encodes only changed tracks, exactly as the WOZ path does --
which means the file-level commands DO meet the sector decode, and its refusal on a
partly-decoded track is correct and stays.

### Phase E -- Documentation and the spec ledger

README format table and the drag-and-drop claim; the format capability
documentation; `CHANGELOG.md`. Mark spec 022's User Story 2 and FR-003 delivered
here; note spec 007's FR-022 and SC-004 satisfied. User Story 4.

## Risks

| Risk | Mitigation |
|---|---|
| Padding lands inside a field and breaks a track that used to work. | The rotation rule (research D3) puts it in the largest sync run. The test is a write-eject-remount cycle on a real disk, not a synthetic buffer. |
| A nibble image with high-bit-clear bytes does not re-derive byte-identically. | Untouched tracks are copied, never re-derived (research D4). A track the guest wrote is expected to change shape; a track it did not must not. |
| A `DiskFormat` arm is missed in one of the ~13 files that switch on it. | Sweep the enum, not the tables. There is a live example of exactly this failure in the tree today. |
| The feature is oversold as preservation. | The spec, the README and the format documentation all state the self-sync loss. WOZ stays the recommendation for archiving. |
| Sector-format behavior drifts. | SC-009: the existing suite passes unchanged, with counts reported. |

## Complexity Tracking

> No constitution violations. Table intentionally empty.

## Notes on shared state

**`CLAUDE.md`'s active-spec block was deliberately not repointed at this plan.**
The project's own guidance says not to flip it from a feature branch while another
session owns it, and there are concurrent worktrees. It still names spec 024. Flip
it when this branch merges, not before.

**`.specify/feature.json` is per-checkout state that must stay out of the merge.**
It points at `specs/027-nibble-images` in this worktree so the speckit workflows
resolve. Note it is a **tracked** file, not an untracked one, so the hazard is wider
than it first appears: no `git add -A` is needed, and a plain `git commit -a` carries
it in. T062 checks the branch diff for it.

**`CLAUDE.md` is stale in two other ways worth correcting at merge time**, neither
of them this feature's doing: it describes spec 020 as implemented-but-unmerged when
it has merged and master is at 1.20.1, and it does not mention that GH #115 is
fixed.
