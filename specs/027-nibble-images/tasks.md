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

- **[P]**: Independent of the tasks around it, so it can be done in any order or
  alongside them. It does NOT promise a distinct file -- several `[P]` tasks add
  separate tests to one file, which is fine because they do not depend on each other
- **[Story]**: Which user story the task belongs to (US1..US4)
- Every task names the exact file or script it touches

## Path Conventions

Production code is in `CassoEmuCore/Devices/Disk/`. Tests are in
`UnitTest/EmuTests/` and `UnitTest/UiTests/`. New files in either must be added to
the owning `.vcxproj` or they do not compile in, and a test file that never compiled
is simply absent from the count rather than failing.

## Concurrency note

A separate session is fixing `BlankDiskBuilder::ValidateSpec`'s missing
`DiskFormat::Do` arm, touching `CassoEmuCore/Devices/Disk/BlankDiskBuilder.cpp`,
`UnitTest/EmuTests/BlankDiskBuilderTests.cpp` and
`UnitTest/EmuTests/DiskCommandRunnerTests.cpp`. Ten Phase 5 tasks land in those same
three files. **Rebase on that work before starting Phase 5**, not after. Task IDs
are deliberately not listed here -- they move whenever this file is edited, and a
stale list is worse than none; the files are the durable identifier.

The container-sweep tests in `BlankDiskBuilderTests.cpp` will fail against
`DiskFormat::Do` until that fix lands, because they sweep the containers the tool
advertises against the arms the builder actually has -- which is exactly the defect
the other session is fixing. A red sweep before the rebase is the test working, not
a task blocked.

The two changes overlap in intent as well as in file: this feature rewrites
`ValidateSpec` to answer from `GetContainers`, which subsumes the missing-arm fix.
Take the other session's work first and let the rewrite absorb it, rather than
resolving a conflict between two versions of the same correction.

---

## Phase 1: Setup

**Purpose**: Somewhere to put the code, and something to test it against.

- [ ] T001 Add `Devices\Disk\NibbleImageCodec.cpp` to `<ClCompile>` and `Devices\Disk\NibbleImageCodec.h` to `<ClInclude>` in `CassoEmuCore/CassoEmuCore.vcxproj`, in the existing alphabetical position beside `NibblizationLayer`
- [ ] T002 Add `EmuTests\NibbleImageCodecTests.cpp` to `<ClCompile>` in `UnitTest/UnitTest.vcxproj`, beside the existing `EmuTests\NibblizationTests.cpp` entry
- [ ] T003 [P] Add a nibble-image builder to `UnitTest/EmuTests/DemoAssets.h` and `DemoAssets.cpp` that GCR-encodes an existing sector fixture into a 232,960-byte buffer and a 223,440-byte buffer, so no test needs a checked-in `.nib` and nothing is downloaded

**Checkpoint**: The project builds unchanged with two empty new files compiled in.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The format enumerator and the load half of the codec. Everything else
depends on these.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

Adding the enumerator alone breaks nothing — every switch on `DiskFormat` has a
`default` arm — so this phase ends with a green suite. The totality test that
deliberately goes red against the new enumerator lives at the head of Phase 3
instead, so no phase gate closes over a knowingly failing suite.

- [ ] T004 Add `Nib` to the `DiskFormat` enum in `CassoEmuCore/Devices/Disk/IDiskImage.h`
- [ ] T005 Declare `NibbleImageCodec` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.h`, every member VerbNoun: the two accepted total lengths and their track sizes as named constants, the sync byte, `Load`, `Serialize`, and a length-to-geometry resolver. Terse one-line comments only; the documentation blocks belong in the `.cpp`
- [ ] T006 Promote `ReadNibbleAt` from a file-scope static in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp` to a public static on `NibblizationLayer`, declared in `NibblizationLayer.h`, so the sector decoder and the byte derivation share one MSB rule instead of two copies that can disagree
- [ ] T007 Run `scripts\RunTests.ps1 -Configuration Debug -Build` and confirm the full suite still passes after T006 with exact counts reported, since it moves a function that `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp`'s sector decoder depends on
- [ ] T008 Implement `NibbleImageCodec::ResolveGeometry` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: total length to track size and track count, refusing any other length. `ERROR_BAD_LENGTH`, never `E_INVALIDARG` — a wrong-sized file the user named is not a coding error
- [ ] T009 Implement `NibbleImageCodec::Load` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: 35 track slots at `trackSize * 8` bits, bytes packed MSB-first in file order, via `ResizeTrack` / `GetTrackBitsForWrite` / `SetTrackBitCount`, then `ClearDirty`
- [ ] T010 [P] Test `ResolveGeometry` in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: both accepted totals map to their track sizes, and lengths just above, just below and far from each are refused with `ERROR_BAD_LENGTH`
- [ ] T011 [P] Test `Load` in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: track count and per-track bit count are exact, bit 0 of the stream is bit 7 of file byte 0, and a track's packed bytes equal the file's bytes for that track. Assert the track count is non-zero before asserting over tracks
- [ ] T012 [P] Test that `Load` accepts bytes with the high bit clear in `UnitTest/EmuTests/NibbleImageCodecTests.cpp` — they are legal in real images and must not be refused

**Checkpoint**: The codec loads, the suite is green, nothing routes to it yet.

---

## Phase 3: User Story 1 — Open a nibble image (Priority: P1) 🎯 MVP

**Goal**: A nibble image mounts and boots, from drag-and-drop, the picker or the
command line, and a file that is not one is refused by name.

**Independent test**: Mount a generated nibble image of `casso-rocks` and confirm it
boots and catalogs identically to the same disk as `.dsk` and as `.woz`.

- [ ] T013 [US1] Write a `DiskFormat` totality test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` that sweeps **the enum**, not the routing table, asserting every enumerator resolves an extension, a diagnosis name and a container word; model it on `UnitTest/DirectiveTokenTests.cpp`, which sweeps both directions and is the exemplar
- [ ] T014 [US1] Run `scripts\RunTests.ps1 -Configuration Debug -Build -Filter DiskFormat` and confirm the T013 test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` FAILS against the `Nib` enumerator before any arm exists — a totality test that passes here is testing nothing
- [ ] T015 [US1] Add the two enumerators to `MountFailure` in `CassoEmuCore/Devices/Disk/MountDiagnosis.h`: `WrongSizeForNibbleImage` and `NotANibbleStream`, with the header comment explaining what each distinguishes, per the rule that every enumerator names something the load path can actually tell apart
- [ ] T016 [US1] Add their clauses to `MountDiagnosis::Describe` in `CassoEmuCore/Devices/Disk/MountDiagnosis.cpp`. The wrong-size clause names the length found and BOTH accepted lengths with the 35-track arithmetic behind them; do not reuse `WrongSizeForFormat`, whose clause names one 143,360-byte size
- [ ] T017 [US1] Rename `MountDiagnosis::ExtensionFor` to `GetPrimaryExtension` in `CassoEmuCore/Devices/Disk/MountDiagnosis.h` and `.cpp` and at its one call site in `Describe`. `ExtensionFor` is noun-first and the project's convention is VerbNoun; `Primary` records that a format may answer to more than one extension, which is exactly what the nibble case introduces. Then add the `Nib` arm, returning `.nib` as the representative name. It CANNOT return both extensions -- it is keyed on `DiskFormat` alone and the two extensions share one enumerator -- so the nibble refusal clause written in T016 must say "a nibble image" rather than naming an extension, or it will describe a `.nb2` file as a `.nib`
- [ ] T018 [US1] Rename `DiskImageStore::DetectFormatByExtension` to `GetSourceFormatByExtension` across all 20 references in `DiskImageStore.h`, `DiskImageStore.cpp`, `VolumeImage.cpp`, `DiskCommandRunner.cpp`, `Casso/Shell/DiskManager.cpp`, `UnitTest/EmuTests/DiskImageStoreTests.cpp` and `UnitTest/UiTests/DriveWidgetStateTests.cpp`. `Source` matches `IDiskImage::GetSourceFormat` and means the format of a file that already EXISTS, which excludes creation -- the one create caller is removed separately in Phase 5
- [ ] T019 [US1] Rewrite the comment block above `GetSourceFormatByExtension` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp` to say the two things the name cannot carry: it answers the container FAMILY only, and for nibble images the track size comes from the file's length and MUST NOT be inferred from the enumerator; and it is the READ list, where what a new image may be written as is `s_kContainers` in `DiskCommandRunner.cpp`
- [ ] T020 [US1] Map both extensions to `DiskFormat::Nib` in `DiskImageStore::GetSourceFormatByExtension` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T021 [US1] Add the `Nib` arm to `DiskImage::LoadFromBytes` in `CassoEmuCore/Devices/Disk/DiskImage.cpp`, routing to `NibbleImageCodec::Load`
- [ ] T022 [US1] Add the nibble arms to `DiskImageStore::ClassifyLoadFailure` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, distinguishing a wrong length from a right length carrying no assemblable nibble
- [ ] T023 [US1] Run `scripts\RunTests.ps1 -Configuration Debug -Build -Filter DiskFormat` and confirm the T013 test in `UnitTest/EmuTests/DiskImageStoreTests.cpp` now PASSES, and that it was the arms above that made it pass
- [ ] T024 [P] [US1] Test extension routing in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: `.nib`, `.nb2`, `.NIB` and `.Nb2` all resolve to `DiskFormat::Nib`, and the four existing extensions still resolve as they did
- [ ] T025 [P] [US1] Extend the EXISTING `IsSupportedDiskImageExtension_AnswersExactlyWhatTheLoaderRoutes` in `UnitTest/UiTests/DriveWidgetStateTests.cpp` — do not write a second one, or the single-answer property ends up split across two tests that can disagree. Its corpus already contains `disk.nib`; point it at the renamed `GetSourceFormatByExtension`, add `.nb2`, and cover the wide overload
- [ ] T026 [US1] Invert the two tests that assert nibble images are REFUSED, now that they are not: `IsMountableImageExtension_RejectsNibbleImages` in `UnitTest/EmuTests/DiskImageStoreTests.cpp` and `IsSupportedDiskImageExtension_RejectsNibbleImages` in `UnitTest/UiTests/DriveWidgetStateTests.cpp`. Rename both to say what they now assert, and rename `IsSupportedDiskImageExtension_AcceptsTheFourMountableTypes` in the same file, which no longer counts correctly. Invert rather than delete: the assertion was right for a build that could not mount these, and the capability belongs asserted where its absence was
- [ ] T027 [P] [US1] Test the mismatched-name cases in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: a 223,440-byte file named `.nib` and a 232,960-byte file named `.nb2` each mount at the track size their length implies
- [ ] T028 [P] [US1] Test the refusals in `UnitTest/EmuTests/DiskFailureModeTests.cpp`: a truncated file, an over-long file, and a right-sized buffer of zeros each produce their own diagnosis clause, the clauses differ from each other, and each names a concrete number
- [ ] T029 [US1] Test with `ExpectedEhmAssert` in `UnitTest/EmuTests/DiskFailureModeTests.cpp` that no malformed nibble image raises an assertion on any mount path — the pattern this codebase uses for edge input
- [ ] T030 [US1] Test mount and boot end to end in `UnitTest/EmuTests/CrossFormatWriteTests.cpp`, which is where `HeadlessHost` and `TextScreenScraper` are already used — NOT `CrossFormatExtractionTests.cpp`, which drives `FakeDiskFileIo` and has neither. Compare the nibble image's boot and catalog against the same disk as `.dsk`, `.woz` and `.po`; `.po` earns its place by carrying a different sector order, and a two-format comparison does not establish the parity the success criterion claims
- [ ] T031 [US1] Test that mounting and ejecting without a guest write leaves the file byte-identical, in `UnitTest/EmuTests/DiskWritePathTests.cpp`, using the store's flush sink so no file is touched
- [ ] T032 [US1] Confirm by hand that the disk picker and drag-and-drop offer `.nib` and `.nb2` with no change to `Casso/Ui/DriveWidgetState.h`, launching `x64\Debug\Casso.exe --machine Apple2e`. Not a unit test: a test may not read the source tree, and T023 covers the part that is mechanically checkable

**Checkpoint**: User Story 1 is deliverable. Nibble images mount, boot, and survive
an eject untouched.

---

## Phase 4: User Story 2 — Keep what the guest wrote (Priority: P1)

**Goal**: Guest writes reach the file, untouched tracks do not change, repeated
cycles do not degrade the disk, and a write that cannot be persisted is reported.

**Independent test**: Boot, `SAVE` a program, eject, remount, `CATALOG`, and confirm
only the written tracks' blocks differ.

- [ ] T033 [US2] Implement byte derivation in `NibbleImageCodec` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: walk a track by the shared MSB rule, bounded by one revolution, returning the derived bytes and reporting a track that yields none rather than returning an empty result that reads as success
- [ ] T034 [US2] Implement the rotation rule in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: rotate the derived sequence so its longest run of `$FF` ends it, falling back to the derivation seam when the track carries no sync run at all
- [ ] T035 [US2] Implement `NibbleImageCodec::Serialize` in `CassoEmuCore/Devices/Disk/NibbleImageCodec.cpp`: copy every clean track verbatim from the loaded file's own bytes, re-derive each dirty track, rotate it, and pad with `$FF` to the block size
- [ ] T036 [US2] Add the `Nib` arm to `DiskImage::Serialize` in `CassoEmuCore/Devices/Disk/DiskImage.cpp`, passing `m_rawSourceBytes` and the per-track dirty bits. It must NOT route through `NibblizationLayer::Denibblize` — no sector decode is involved in writing a nibble image, which is what makes a track that will not decode cost this path nothing
- [ ] T037 [P] [US2] Test the exact-inverse invariant in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: a track whose every byte has the high bit set, loaded and immediately re-derived, yields its original bytes in order with zero padding
- [ ] T038 [P] [US2] Test the arithmetic bound in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: the derived byte count never exceeds `trackBits / 8`, over a synthesized track, a loaded track and a guest-written track
- [ ] T039 [P] [US2] Test padding placement in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: on a track re-derived from `NibblizeDsk` output, all 16 address fields and 16 data fields survive the rotate-and-pad, and the padding sits in a sync run. Assert the field count is 16 rather than merely non-zero
- [ ] T040 [P] [US2] Test the no-sync-run fallback and the all-zero track in `UnitTest/EmuTests/NibbleImageCodecTests.cpp`: both terminate, neither spins, and each produces a defined block
- [ ] T041 [US2] Test the write-back on the live flush path in `UnitTest/EmuTests/DiskWritePathTests.cpp`: with a guest write on one track, the flushed bytes differ in exactly that track's block and are byte-identical everywhere else; with no dirty track, no write occurs at all
- [ ] T042 [US2] Test the unpersisted-write report in `UnitTest/EmuTests/DiskWritePathTests.cpp`: a `FlushSink` that fails on a dirty nibble image produces the loss message naming the image, through the same notifier the sector formats use. This covers FR-015 and SC-006, whose mechanism is pre-existing and format-agnostic but is unverified for this container
- [ ] T043 [US2] Test the no-degrade property in `UnitTest/EmuTests/CrossFormatWriteTests.cpp`: write, flush, reload and write again several times, asserting the volume stays readable and the catalog correct on every pass. Assert the cycle count actually ran, so a loop over nothing cannot pass
- [ ] T044 [US2] Confirm the write-protect attribution in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: a nibble image reports protection from the host file's read-only attribute and from the user setting, and never from an image flag, since the format carries none. Inherited behavior, asserted rather than assumed

**Checkpoint**: User Story 2 is deliverable. The feature is safe to use on real
images.

---

## Phase 5: User Story 3 — Work with a nibble image from the console (Priority: P2)

**Goal**: All nine `disk` commands accept nibble images, and the create surfaces
offer the container consistently.

**Independent test**: Run each command against a nibble image and the same disk as
`.dsk`, and compare.

**Rebase on the concurrent `DiskFormat::Do` fix before starting this phase.**

- [ ] T045 [US3] Move the container catalog out of `DiskCommandRunner.cpp` and into `CassoEmuCore/Devices/Disk/BlankDiskBuilder.h` and `.cpp` as one table of entries -- word, `DiskFormat`, and the track size a new image of it gets. `s_kContainers` becomes a view of that table rather than a second copy of it. **A `DiskFormat` cannot be the unit here**: one enumerator covers two words at two sizes, so a list of formats can offer five choices where the console offers six, and the two surfaces would differ by construction
- [ ] T046 [US3] Rewrite `BlankDiskBuilder::ValidateSpec` in `CassoEmuCore/Devices/Disk/BlankDiskBuilder.cpp` to answer from `GetContainers` rather than its own switch, so the validator and the offered choices cannot disagree
- [ ] T047 [US3] Add the `Nib` arm to `GetContainers` and `BlankDiskBuilder::Build` in `CassoEmuCore/Devices/Disk/BlankDiskBuilder.cpp`

- [ ] T048 [US3] Add `nib` (6,656) and `nb2` (6,384) to the catalog in `CassoEmuCore/Devices/Disk/BlankDiskBuilder.cpp`, and update the two refusal strings in `ResolveContainer` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp` that list the types the tool writes, which read from the catalog rather than restating it
- [ ] T049 [US3] Rework `DiskCommandRunner::ResolveContainer` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp` to resolve the no-`--type` case from `s_kContainers` by the file's extension rather than from `DiskImageStore::GetSourceFormatByExtension`, and to yield the track size alongside the format. The container word IS the extension for all six, so one table answers both branches; and that function is the READ list, which is the wrong list to decide what a new file may be written as. Removing this call is what leaves the renamed function purely about files that already exist
- [ ] T050 [US3] Add the nibble track size to `BlankDiskSpec` in `CassoEmuCore/Devices/Disk/BlankDiskBuilder.h` and honor it in `Build`, so `create` writes the size the name asked for and never a `.nb2` holding 6,656-byte tracks
- [ ] T051 [US3] Make `DiskCommandRunner::RunInit` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp` take the track size from the existing file's length via `IDiskFileIo::Stat`, not from its name. `init` PRESERVES where `create` CHOOSES: an image on disk may carry either track size under either name, and reformatting must not change a file's size
- [ ] T052 [US3] Add the nibble container to `VolumeImage::Load` in `CassoEmuCore/Devices/Disk/VolumeImage.cpp`, decoding through the codec and then through `NibblizationLayer::Denibblize` to sectors, as the WOZ path does
- [ ] T053 [US3] Add the nibble container to `VolumeImage::Save` and `SaveBitStream` in `CassoEmuCore/Devices/Disk/VolumeImage.cpp`, re-encoding only changed tracks. The whole-operation refusal on an unwritable track stays exactly as it is — it is correct here and is the one place in this feature where a track that will not decode legitimately blocks a write
- [ ] T054 [US3] Have `Casso/Ui/Dialogs/CreateDiskDialog.cpp` build `m_imageTypeChoices` from `BlankDiskBuilder::GetContainers`, rendering one entry per catalog word — so `nib` and `nb2` are two choices, matching the console — and take its label and extension text from the entry rather than from its own mappings, which are deleted. No new decision logic enters the executable: the dialog renders what core decides
- [ ] T055 [P] [US3] Test all nine commands against a nibble image in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`, asserting each matches its `.dsk` counterpart, driven through `FakeDiskFileIo` so no file is touched
- [ ] T056 [P] [US3] Test `GetContainers` in `UnitTest/EmuTests/BlankDiskBuilderTests.cpp` by sweeping the `DiskFormat` enum against every `BlankDiskContents` value, so a container added without a pairing rule fails the test instead of asserting at runtime
- [ ] T057 [P] [US3] Test in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp` that every catalog word the console accepts is also offered by `GetContainers` for some contents type, and the reverse. With one table this should be trivially true — which is the point: the test is what proves there is still only one table after somebody adds the next container
- [ ] T058 [P] [US3] Test `create` and `init` for both container words in `UnitTest/EmuTests/BlankDiskBuilderTests.cpp`, sweeping the container list `s_kContainers` advertises rather than restating it
- [ ] T059 [P] [US3] Test that a created nibble image is immediately usable in `UnitTest/EmuTests/BootDiskTests.cpp`: `create --bootable`, then mount and boot it
- [ ] T060 [P] [US3] Test the create-size rule in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`: `--type nib` and a `.nib` name each produce exactly 232,960 bytes, `--type nb2` and a `.nb2` name each produce exactly 223,440, and every produced image mounts back at the track size its name implies. Sweep `s_kContainers` so a container with no size cannot be added silently
- [ ] T061 [P] [US3] Test the init-preserves rule in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`: `init` on a 223,440-byte file named `.nib`, and on a 232,960-byte file named `.nb2`, each leave the file's length exactly as it was
- [ ] T062 [P] [US3] Test the damaged-track refusal in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp`: a nibble image with a deliberately damaged track is refused by a file-level command, the message names the track that would not decode, and nothing is written

**Checkpoint**: User Story 3 is deliverable.

---

## Phase 6: User Story 4 — Know what a nibble image can and cannot do (Priority: P3)

**Goal**: The documentation is true and honest in both directions.

**Independent test**: Read the format documentation and confirm each format states
what can be read, written and modified, with the self-sync loss stated for nibble
images.

- [ ] T063 [P] [US4] Update the format list in `README.md` (the line naming `.woz`, `.dsk`, `.do` and `.po` around line 256, and the capability summary around line 41) to include nibble images, restoring the drag-and-drop claim that was removed for being false
- [ ] T064 [P] [US4] Add a nibble-image section to `docs/disk-write-integrity.md` covering the derivation rule, the fixed-bit-length fact, the padding policy, and why the write-back does not pass through the sector decode
- [ ] T065 [P] [US4] State the self-sync loss plainly wherever formats are compared, and point a user archiving a disk at WOZ, in both `README.md` and `docs/disk-write-integrity.md`
- [ ] T066 [P] [US4] Add the `[Unreleased]` entry to `CHANGELOG.md` under Added, describing the user-visible capability and its limitation in the project's plain register
- [ ] T067 [P] [US4] Mark User Story 2 and FR-003 delivered in `specs/022-disk-image-formats/spec.md`, pointing at this feature rather than leaving the requirement duplicated
- [ ] T068 [P] [US4] Note FR-022 and SC-004 satisfied in `specs/007-ui-overhaul/spec.md`, which have never been satisfiable until now

**Checkpoint**: User Story 4 is deliverable.

---

## Phase 7: Polish and Gates

- [ ] T069 Sweep every file that references `DiskFormat` for a missing arm, driving the sweep from the enum in `CassoEmuCore/Devices/Disk/IDiskImage.h` and from a fresh `grep -rl DiskFormat` over `CassoEmuCore`, `CassoCore`, `Casso` and `CassoCli`. No count is quoted here on purpose: this task's own count has been wrong twice (ten, then fifteen; it is eighteen files today, eight of them headers), and a stale number in the safety net is worse than none
- [ ] T070 Confirm `Casso/Ui/DriveWidgetState.h` is unmodified in the branch diff via `git diff origin/master -- Casso/Ui/DriveWidgetState.h`. Any change means the second extension list has come back and must be reverted
- [ ] T071 Confirm `.specify/feature.json` is not in the branch diff via `git diff origin/master -- .specify/feature.json`. It is a **tracked** file holding per-checkout state, so `git commit -a` sweeps it in without any `git add -A`
- [ ] T072 Run `scripts\Build.ps1 -Configuration Debug` and `scripts\Build.ps1 -Configuration Release`, both with zero warnings
- [ ] T073 Run `scripts\RunTests.ps1 -Configuration Debug -Build` and `scripts\RunTests.ps1 -Configuration Release -Build` in the background, reporting exact counts for each. The two run different test sets, so neither substitutes for the other
- [ ] T074 Run `scripts\CheckStyle.ps1` clean, and `scripts\CheckStyle.ps1 -Mode Staged` before committing any new file — diff mode cannot see a file that has never been committed and will report OK over it
- [ ] T075 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild, with zero warnings. A run over a stale Release build fakes a wall of LNK4020 noise
- [ ] T076 Walk `specs/027-nibble-images/quickstart.md` end to end against a real build, including launching `x64\Debug\Casso.exe --machine Apple2e --disk1 <image>`. Kill only the process ID launched here; other Casso instances are running from other worktrees
- [ ] T077 Remove any generated `.nib` and `.nb2` scratch files from the working tree. Do not add patterns to `.gitignore` — stray files are meant to surface in `git status`

Merge-time steps that are deliberately **not** tasks here, because this branch must
not perform them, are recorded in [plan.md](plan.md) under "Notes on shared state".

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
  which means the serializer must exist. It also depends on the concurrent
  `DiskFormat::Do` fix having been rebased in.
- **US4 depends on** nothing but the decisions, and can be written at any point. It
  is listed late because a document describing behavior that has not shipped is the
  same false claim this story exists to correct.

## Parallel Opportunities

- **Phase 2**: T010, T011 and T012 are three independent tests in one new file; write
  them together once T008 and T009 land.
- **Phase 3**: T024 through T028 touch three different test files with no shared
  state — the widest parallel block in the feature.
- **Phase 4**: T037 through T040 are independent of each other, though they share a
  file.
- **Phase 5**: the test tasks span `DiskCommandRunnerTests.cpp`, `BlankDiskBuilderTests.cpp` and `BootDiskTests.cpp`; the three files are independent, though several tasks share each.
- **Phase 6**: the whole phase is independent work, though `README.md` and
  `docs/disk-write-integrity.md` are each touched by two tasks.

## Implementation Strategy

**MVP is Phase 1 + Phase 2 + Phase 3** — nibble images mount and boot. That is the
capability users are missing, and it is honestly shippable on its own **only
because** an unwritten image is never written back (FR-011). Stopping there is a
real increment rather than a half-feature.

**Phase 4 is not optional and must not be deferred past a release.** The mount path
is the write-back path, so once mounting ships, any guest that writes to a nibble
image is relying on a serializer. Shipping Phase 3 without Phase 4 means guest
writes are discarded on eject — silently, which is the failure mode this codebase
has a standing doctrine against.

**Phases 5 and 6 are genuinely separable** and can follow in a later release.
