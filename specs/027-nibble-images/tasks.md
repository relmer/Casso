---

description: "Task list for 027 nibble disk images"
---

# Tasks: Nibble Disk Images

**Input**: Design documents from `/specs/027-nibble-images/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/nibble-image.md](contracts/nibble-image.md)

**Tests**: Included and not optional. Constitution Principle II requires unit tests
for all production code, and Principle VI requires everything to be reachable from
`UnitTest`. Where a test can be written before the code it covers, it is ordered
first.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story the task belongs to (US1..US4)
- Every task names the exact file it touches

## Path Conventions

Production code is in `CassoEmuCore/Devices/Disk/`. Tests are in
`UnitTest/EmuTests/`. New files in either must be added to the owning `.vcxproj`
or they do not compile in, and a test file that never compiled is simply absent
from the count rather than failing.

## Concurrency note

A separate session is fixing `BlankDiskBuilder::ValidateSpec`'s missing
`DiskFormat::Do` arm, touching `BlankDiskBuilder.cpp`,
`UnitTest/EmuTests/BlankDiskBuilderTests.cpp` and
`UnitTest/EmuTests/DiskCommandRunnerTests.cpp`. Tasks T040, T041 and T044 land in
the same files. Rebase on that work before starting Phase 6 rather than resolving a
conflict afterwards.

---

## Phase 1: Setup

**Purpose**: Somewhere to put the code, and something to test it against.

- [ ] T001 Add `Devices\Disk\NibbleImageCodec.cpp` to `<ClCompile>` and `Devices\Disk\NibbleImageCodec.h` to `<ClInclude>` in `CassoEmuCore/CassoEmuCore.vcxproj`, in the existing alphabetical position beside `NibblizationLayer`
- [ ] T002 Add `EmuTests\NibbleImageCodecTests.cpp` to `<ClCompile>` in `UnitTest/UnitTest.vcxproj`, beside the existing `EmuTests\NibblizationTests.cpp` entry
- [ ] T003 [P] Add a nibble-image builder to `UnitTest/EmuTests/DemoAssets.h` / `DemoAssets.cpp` that GCR-encodes an existing sector fixture into a 232,960-byte buffer and a 223,440-byte buffer, so no test needs a checked-in `.nib` and nothing is downloaded

**Checkpoint**: The project builds unchanged with two empty new files compiled in.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The format enumerator and the load half of the codec. Everything else
depends on these.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T004 Add `Nib` to the `DiskFormat` enum in `CassoEmuCore/Devices/Disk/IDiskImage.h`
- [ ] T005 Write a `DiskFormat` totality test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` that sweeps **the enum**, not the routing table, asserting every enumerator resolves an extension, a diagnosis name and a container word; model it on `UnitTest/DirectiveTokenTests.cpp`, which sweeps both directions and is the exemplar
- [ ] T006 Run `scripts\RunTests.ps1 -Configuration Debug -Build -Filter DiskFormat` and confirm the T005 test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` FAILS against the newly added `Nib` enumerator before any arm exists — a totality test that passes here is testing nothing
- [ ] T007 Declare `NibbleImageCodec` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.h`: the two accepted total lengths and their track sizes as named constants, the sync byte, `Load`, `Serialize`, and a length-to-geometry resolver. Terse one-line comments only; the documentation blocks belong in the `.cpp`
- [ ] T008 Promote `ReadNibbleAt` from a file-scope static in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp` to a public static on `NibblizationLayer`, declared in `NibblizationLayer.h`, so the sector decoder and the byte derivation share one MSB rule instead of two copies that can disagree
- [ ] T009 Run `scripts\RunTests.ps1 -Configuration Debug -Build` and confirm the full suite still passes after T008 with counts reported, since it moves a function `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp`'s sector decoder depends on
- [ ] T010 Implement `NibbleImageCodec::ResolveGeometry` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: total length to track size and track count, refusing any other length. `ERROR_BAD_LENGTH`, never `E_INVALIDARG` — a wrong-sized file the user named is not a coding error
- [ ] T011 Implement `NibbleImageCodec::Load` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: 35 track slots at `trackSize * 8` bits, bytes packed MSB-first in file order, via `ResizeTrack` / `GetTrackBitsForWrite` / `SetTrackBitCount`, then `ClearDirty`
- [ ] T012 [P] Test `ResolveGeometry` in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: both accepted totals map to their track sizes, and lengths just above, just below and far from each are refused with `ERROR_BAD_LENGTH`
- [ ] T013 [P] Test `Load` in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: track count and per-track bit count are exact, bit 0 of the stream is bit 7 of file byte 0, and a track's packed bytes equal the file's bytes for that track. Assert the track count is non-zero before asserting over tracks
- [ ] T014 [P] Test that `Load` accepts bytes with the high bit clear in `UnitTest/EmuTests/NibbleImageCodecTests.cpp` — they are legal in real images and must not be refused

**Checkpoint**: The codec loads. Nothing routes to it yet.

---

## Phase 3: User Story 1 — Open a nibble image (Priority: P1) 🎯 MVP

**Goal**: A nibble image mounts and boots, from drag-and-drop, the picker or the
command line, and a file that is not one is refused by name.

**Independent test**: Mount a generated nibble image of `casso-rocks` and confirm it
boots and catalogs identically to the same disk as `.dsk` and `.woz`.

- [ ] T015 [US1] Add the two enumerators to `MountFailure` in `CassoEmuCore/Devices/Disk/MountDiagnosis.h`: `WrongSizeForNibbleImage` and `NotANibbleStream`, with the header comment explaining what each distinguishes, per the rule that every enumerator names something the load path can actually tell apart
- [ ] T016 [US1] Add their clauses to `MountDiagnosis::Describe` in `CassoEmuCore/Devices/Disk/MountDiagnosis.cpp`. The wrong-size clause names the length found and BOTH accepted lengths with the 35-track arithmetic behind them; do not reuse `WrongSizeForFormat`, whose clause names one 143,360-byte size
- [ ] T017 [US1] Add `.nib` and `.nb2` to `MountDiagnosis::ExtensionFor` in `CassoEmuCore/Devices/Disk/MountDiagnosis.cpp`
- [ ] T018 [US1] Map both extensions to `DiskFormat::Nib` in `DiskImageStore::DetectFormatByExtension` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T019 [US1] Add the `Nib` arm to `DiskImage::LoadFromBytes` in `CassoEmuCore/Devices/Disk/DiskImage.cpp`, routing to `NibbleImageCodec::Load`
- [ ] T020 [US1] Add the nibble arms to `DiskImageStore::ClassifyLoadFailure` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, distinguishing a wrong length from a right length carrying no assemblable nibble
- [ ] T021 [US1] Run `scripts\RunTests.ps1 -Configuration Debug -Build -Filter DiskFormat` and confirm the T005 test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` now PASSES, and that it was the arms above that made it pass
- [ ] T022 [P] [US1] Test extension routing in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: `.nib`, `.nb2`, `.NIB` and `.Nb2` all resolve to `DiskFormat::Nib`, and the four existing extensions still resolve as they did
- [ ] T023 [P] [US1] Test the filter-agreement contract in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: the set of extensions `IsMountableImageExtension` accepts equals the set `DetectFormatByExtension` resolves, over both the narrow and wide overloads. This is what keeps a second extension list from coming back
- [ ] T024 [P] [US1] Assert by inspection in the same test file that `Casso/Ui/DriveWidgetState.h` was not modified, then confirm manually that the picker and drag-and-drop offer both new extensions with no filter change
- [ ] T025 [P] [US1] Test the mismatched-name cases in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: a 223,440-byte file named `.nib` and a 232,960-byte file named `.nb2` each mount at the track size their length implies
- [ ] T026 [P] [US1] Test the refusals in `UnitTest/EmuTests/DiskFailureModeTests.cpp`: a truncated file, an over-long file, and a right-sized buffer of zeros each produce their own diagnosis clause, the clauses differ from each other, and each names a concrete number
- [ ] T027 [US1] Test with `ExpectedEhmAssert` in `UnitTest/EmuTests/DiskFailureModeTests.cpp` that no malformed nibble image raises an assertion on any mount path — the pattern this codebase uses for edge input
- [ ] T028 [US1] Test mount and boot end to end in `UnitTest/EmuTests/CrossFormatExtractionTests.cpp` using `HeadlessHost` and `TextScreenScraper`, comparing the nibble image's boot and catalog against the same disk as `.dsk`
- [ ] T029 [US1] Test that mounting and ejecting without a guest write leaves the file byte-identical, in `UnitTest/EmuTests/DiskWritePathTests.cpp`, using the store's flush sink so no file is touched

**Checkpoint**: User Story 1 is deliverable. Nibble images mount, boot, and survive
an eject untouched.

---

## Phase 4: User Story 2 — Keep what the guest wrote (Priority: P1)

**Goal**: Guest writes reach the file, untouched tracks do not change, and repeated
cycles do not degrade the disk.

**Independent test**: Boot, `SAVE` a program, eject, remount, `CATALOG`, and confirm
only the written tracks' blocks differ.

- [ ] T030 [US2] Implement byte derivation in `NibbleImageCodec` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: walk a track by the shared MSB rule, bounded by one revolution, returning the derived bytes and reporting a track that yields none rather than returning an empty result that reads as success
- [ ] T031 [US2] Implement the rotation rule in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: rotate the derived sequence so its longest run of `$FF` ends it, falling back to the derivation seam when the track carries no sync run at all
- [ ] T032 [US2] Implement `NibbleImageCodec::Serialize` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: copy every clean track verbatim from the loaded file's own bytes, re-derive each dirty track, rotate it, and pad with `$FF` to the block size
- [ ] T033 [US2] Add the `Nib` arm to `DiskImage::Serialize` in `CassoEmuCore/Devices/Disk/DiskImage.cpp`, passing `m_rawSourceBytes` and the per-track dirty bits. It must NOT route through `NibblizationLayer::Denibblize` — no sector decode is involved in writing a nibble image, which is what makes a track that will not decode cost this path nothing
- [ ] T034 [P] [US2] Test the exact-inverse invariant in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: a track whose every byte has the high bit set, loaded and immediately re-derived, yields its original bytes in order with zero padding
- [ ] T035 [P] [US2] Test the arithmetic bound in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: the derived byte count never exceeds `trackBits / 8`, over a synthesized track, a loaded track and a guest-written track
- [ ] T036 [P] [US2] Test padding placement in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: on a track re-derived from `NibblizeDsk` output, all 16 address fields and 16 data fields survive the rotate-and-pad, and the padding sits in a sync run. Assert the field count is 16 rather than merely non-zero
- [ ] T037 [P] [US2] Test the no-sync-run fallback and the all-zero track in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: both terminate, neither spins, and each produces a defined block
- [ ] T038 [US2] Test the write-back on the live flush path in `UnitTest/EmuTests/DiskWritePathTests.cpp`: with a guest write on one track, the flushed bytes differ in exactly that track's block and are byte-identical everywhere else; with no dirty track, no write occurs at all
- [ ] T039 [US2] Test the no-degrade property in `UnitTest/EmuTests/CrossFormatWriteTests.cpp`: write, flush, reload and write again several times, asserting the volume stays readable and the catalog correct on every pass. Assert the cycle count actually ran, so a loop over nothing cannot pass

**Checkpoint**: User Story 2 is deliverable. The feature is safe to use on real
images.

---

## Phase 5: User Story 3 — Work with a nibble image from the console (Priority: P2)

**Goal**: All nine `disk` commands accept nibble images.

**Independent test**: Run each command against a nibble image and the same disk as
`.dsk`, and compare.

- [ ] T040 [US3] Add the `Nib` arm to `BlankDiskBuilder::ValidateSpec` and `::Build` in `CassoEmuCore/Devices/Disk/BlankDiskBuilder.cpp`. Rebase on the concurrent `DiskFormat::Do` fix first
- [ ] T041 [US3] Add `nib` and `nb2` to `s_kContainers` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp`, and update the two refusal strings in `ResolveContainer` that list the types the tool writes — both come from the one table today and must continue to
- [ ] T042 [US3] Add the nibble container to `VolumeImage::Load` in `CassoEmuCore/Devices/Disk/VolumeImage.cpp`, decoding through the codec and then through `NibblizationLayer::Denibblize` to sectors, as the WOZ path does
- [ ] T043 [US3] Add the nibble container to `VolumeImage::Save` and `SaveBitStream` in `CassoEmuCore/Devices/Disk/VolumeImage.cpp`, re-encoding only changed tracks. The whole-operation refusal on an unwritable track stays exactly as it is — it is correct here and is the one place in this feature where a track that will not decode legitimately blocks the write
- [ ] T044 [P] [US3] Test all nine commands against a nibble image in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`, asserting each matches its `.dsk` counterpart, driven through `FakeDiskFileIo` so no file is touched
- [ ] T045 [P] [US3] Test `create` and `init` for both container words in `UnitTest/EmuTests/BlankDiskBuilderTests.cpp`, sweeping the container list `s_kContainers` advertises rather than restating it, so a container added without a builder arm fails the test instead of asserting at runtime
- [ ] T046 [P] [US3] Test that a created nibble image is immediately usable in `UnitTest/EmuTests/BootDiskTests.cpp`: `create --bootable` then mount and boot it
- [ ] T047 [P] [US3] Test the sector-surface refusal in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`: a nibble image with a deliberately damaged track is refused by a file-level command with the surface named, and nothing is written
- [ ] T048 [US3] Add `DiskFormat::Nib` to `m_imageTypeChoices` and the extension and label helpers in `Casso/Ui/Dialogs/CreateDiskDialog.cpp`, so the GUI create dialog offers what the console does. NOTE: the spec's FR-014 and FR-015 name only the `disk` subcommand; this task closes a gap the spec left, and the spec should be amended rather than the gap left implicit

**Checkpoint**: User Story 3 is deliverable.

---

## Phase 6: User Story 4 — Know what a nibble image can and cannot do (Priority: P3)

**Goal**: The documentation is true and honest in both directions.

**Independent test**: Read the format documentation and confirm each format states
what can be read, written and modified, with the self-sync loss stated for nibble
images.

- [ ] T049 [P] [US4] Update the format list in `README.md` (the line naming `.woz`, `.dsk`, `.do` and `.po` around line 256, and the capability summary around line 41) to include nibble images, restoring the drag-and-drop claim that was removed for being false
- [ ] T050 [P] [US4] Add a nibble-image section to `docs/disk-write-integrity.md` covering the derivation rule, the fixed-bit-length fact, the padding policy, and why the write-back does not pass through the sector decode
- [ ] T051 [P] [US4] State the self-sync loss plainly wherever formats are compared, and point a user archiving a disk at WOZ, in both `README.md` and `docs/disk-write-integrity.md`
- [ ] T052 [P] [US4] Add the `[Unreleased]` entry to `CHANGELOG.md` under Added, describing the user-visible capability and its limitation in the project's plain register
- [ ] T053 [P] [US4] Mark User Story 2 and FR-003 delivered in `specs/022-disk-image-formats/spec.md`, pointing at this feature rather than leaving the requirement duplicated
- [ ] T054 [P] [US4] Note FR-022 and SC-004 satisfied in `specs/007-ui-overhaul/spec.md`, which have never been satisfiable until now

**Checkpoint**: User Story 4 is deliverable.

---

## Phase 7: Polish and Gates

- [ ] T055 Sweep every file that switches on `DiskFormat` for a missing arm: `BlankDiskBuilder.cpp`, `DiskImage.cpp`, `MountDiagnosis.cpp`, `NibblizationLayer.cpp` and `Casso/Ui/Dialogs/CreateDiskDialog.cpp` carry case labels; `DiskCommandRunner.cpp`, `DiskImageStore.cpp`, `VolumeImage.cpp`, `WozLoader.cpp` and `Casso/Shell/DiskManager.cpp` reference the enum. Drive the sweep from the enum, not from this list
- [ ] T056 Confirm `Casso/Ui/DriveWidgetState.h` is untouched in the branch diff. If it changed, the second extension list has come back and the change must be reverted
- [ ] T057 Update `CLAUDE.md`'s active-spec block to name this feature, and correct its two stale claims — spec 020 has merged and master is at 1.20.1, and GH #115 is fixed. **At merge time only**, never from the feature branch while other sessions are running
- [ ] T058 Run `scripts\Build.ps1 -Configuration Debug` and `-Configuration Release`, both with zero warnings
- [ ] T059 Run `scripts\RunTests.ps1 -Configuration Debug -Build` and `-Configuration Release -Build` in the background, reporting exact counts for each. The two run different test sets, so neither substitutes for the other
- [ ] T060 Run `scripts\CheckStyle.ps1` clean, and `-Mode Staged` before committing any new file — diff mode cannot see a file that has never been committed and will report OK over it
- [ ] T061 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild, with zero warnings. A run over a stale Release build fakes a wall of LNK4020 noise
- [ ] T062 Walk `specs/027-nibble-images/quickstart.md` end to end against a real build, including launching `x64\Debug\Casso.exe --machine Apple2e --disk1 <image>`. Kill only the process ID launched here; other Casso instances are running from other worktrees
- [ ] T063 Remove any generated `.nib` scratch files from the working tree. Do not add patterns to `.gitignore` — stray files are meant to surface in `git status`

---

## Dependencies

```text
Phase 1 (Setup)
      │
Phase 2 (Foundational: enum + codec Load)      ← blocks everything
      │
      ├─────────────┬──────────────┐
      ▼             ▼              │
Phase 3 (US1)   Phase 6 (US4)      │   US4 is documentation and needs
  mount/boot     docs              │   only the decisions, not the code
      │                            │
      ▼                            │
Phase 4 (US2)                      │
  write-back                       │
      │                            │
      ▼                            │
Phase 5 (US3) ◀────────────────────┘
  console       needs US2's Serialize for create/init to produce a
      │         mountable image
      ▼
Phase 7 (Polish and Gates)
```

- **US1 depends on** Phase 2 only.
- **US2 depends on** US1: there is no point writing back an image nothing can mount.
- **US3 depends on** US2, because `create` must produce an image the loader accepts,
  which means the serializer must exist.
- **US4 depends on** nothing but the decisions, and can be written at any point. It
  is listed last because a document describing behavior that has not shipped is the
  same false claim this story exists to correct.

## Parallel Opportunities

- **Phase 2**: T012, T013 and T014 are three independent tests in one new file; write
  them together once T010 and T011 land.
- **Phase 3**: T022 through T027 touch three different test files with no shared
  state — the widest parallel block in the feature.
- **Phase 4**: T034 through T037 are all in `NibbleImageCodecTests.cpp` and are
  independent of each other, though they share a file.
- **Phase 5**: T044 through T047 are four separate test files.
- **Phase 6**: every task is a different document; the whole phase is parallel.

## Implementation Strategy

**MVP is Phase 1 + Phase 2 + Phase 3** — nibble images mount and boot. That is the
capability users are missing, and it is honestly shippable on its own **only
because** an unwritten image is never written back (FR-008). Stopping there is a
real increment rather than a half-feature.

**Phase 4 is not optional and must not be deferred past a release.** The mount path
is the write-back path, so once mounting ships, any guest that writes to a nibble
image is relying on a serializer. Shipping Phase 3 without Phase 4 means guest
writes are discarded on eject — silently, which is the failure mode this codebase
has a standing doctrine against.

**Phases 5 and 6 are genuinely separable** and can follow in a later release.
