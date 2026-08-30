# Tasks: Disk Images Shared with a Running Emulator

**Input**: Design documents from `specs/028-shared-disk-images/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: Included and NOT optional. Constitution Principle II requires every public function and significant code path be covered, so test tasks sit inside each story rather than in a trailing phase.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete work)
- **[Story]**: US1–US3, matching [spec.md](spec.md). Setup, Foundational and Polish carry no story label.

## Path Conventions

Existing solution layout. `CassoCore` and `CassoEmuCore` are static libraries; `UnitTest` links both. Nothing that decides anything goes in an exe (Constitution Principle VI).

**New `.cpp`/`.h` files need explicit `ClCompile`/`ClInclude` entries in the owning `.vcxproj`.** MSBuild does not glob, so a new file that compiles locally can still be missing from a clean build.

**Phase 6 (US3) is independent of everything before it.** Both halves are defects in today's behavior, need no watcher and no channel, and could be done first if a quick win is wanted.

---

## Phase 1: Setup

**Purpose**: A trustworthy baseline before anything changes.

- [ ] T001 Establish a green baseline: run `scripts\Build.ps1` then `scripts\RunTests.ps1 -Build` for x64 Debug, and record the passing count so later runs compare against a number rather than an impression
- [ ] T002 Reproduce the defect by hand per [quickstart.md](quickstart.md) Scenario 1, so the starting behavior is observed rather than taken from the spec: mount a disk, assemble onto it, confirm the guest sees nothing until eject and re-insert

**Checkpoint**: Known-good starting point, and the bug seen once with your own eyes.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Identity and the seams every story below needs. Nothing user-visible.

**⚠️ No user story work can begin until this phase completes.**

- [ ] T003 [P] Add `ImageIdentity` to `CassoEmuCore/Devices/Disk/ImageIdentity.h`/`.cpp` per [data-model.md](data-model.md): size, write time, and a `recorded` flag. Use the has-flag idiom rather than sentinels — a zero size and a zero time are both legal, and `DiskImageSession::OpenedImage` already carries `stampRecorded` for exactly this reason
- [ ] T004 [P] Add the `IImageWatcher` seam to `CassoEmuCore/Devices/Disk/IImageWatcher.h`: watch a directory, report a path that changed, stop watching. Interface only, mirroring `IDiskFileIo`
- [ ] T005 [P] Add the `IIntentChannel` seam to `CassoEmuCore/Cli/IIntentChannel.h` per [contracts/channel.md](contracts/channel.md). `StateIntent` returns void — a failure to deliver degrades to the fallback and no caller could act on an error
- [ ] T006 Add `PickUpIntent` to `CassoEmuCore/Devices/Disk/ExternalChangePolicy.h` with `Unstated`, `TakeUpInPlace` and `Restart`. `Unstated` is a real value, not a missing one: it is what every writer that is not `CassoCli` produces
- [ ] T007 Add `MountedImageState` and `PendingChange` to `CassoEmuCore/Devices/Disk/MountedImageState.h`/`.cpp` per [data-model.md](data-model.md)
- [ ] T008 Register the new files in `CassoEmuCore/CassoEmuCore.vcxproj`
- [ ] T009 Add `UnitTest/EmuTests/FakeImageWatcher.h` and `UnitTest/EmuTests/FakeIntentChannel.h`, so a test can drive a change and a stated intent with no file and no window
- [ ] T010 [P] Add `UnitTest/ImageIdentityTests.cpp` covering the comparison rules: two recorded identities matching, either unrecorded never comparing equal, and a change in size alone or time alone being a change
- [ ] T011 Register `UnitTest/ImageIdentityTests.cpp` in `UnitTest/UnitTest.vcxproj`
- [ ] T012 Verify T010 discriminates: make the comparison ignore `recorded`, confirm the tests go red, restore. A default-constructed identity comparing equal to a real one is the bug this flag exists to prevent

**Checkpoint**: Identity and seams exist and are proven. User stories can proceed.

---

## Phase 3: User Story 1 — The build loop shows what was just built (P1) 🎯 MVP

**Goal**: A change made outside reaches the running guest, without the developer ejecting and re-inserting by hand.

**Independent test**: Mount a disk, assemble a changed program onto it from a second process, confirm the guest loads the new version with no manual eject.

- [ ] T013 [US1] Record an `ImageIdentity` at mount on `DiskImageStore::Entry` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and clear it at eject
- [ ] T014 [US1] Refresh the recorded identity after every commit the emulator itself makes, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, so its own write is never seen as an external change (FR-004). This is the one that makes the feature quiet rather than a source of false alarms
- [ ] T015 [US1] Add `ExternalChangePolicy` in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`: given a stated intent, a fallback answer and whether the image is dirty, decide take-up / restart / ask / conflict. Pure and injected — no clock, no files, no UI
- [ ] T016 [US1] Coalesce changes on a quiet period measured from the LAST change (FR-013), in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The clock is injected so a test needs no waiting
- [ ] T017 [US1] Apply a pending pick-up from the motor-spindown callback in `Casso/Shell/MachineManager.cpp`, alongside the existing `FlushAll` wiring. That callback is documented as "a naturally debounced, race-free point" on the thread that owns disk writes, which is exactly what FR-014 needs
- [ ] T018 [P] [US1] Add `Win32ImageWatcher` in `Casso/Shell/Win32ImageWatcher.h`/`.cpp` over `ReadDirectoryChangesW`. **Watch the DIRECTORY, not the file**: both writers commit by renaming a temporary over the target, so a handle on the image sees its own replacement as a delete
- [ ] T019 [US1] Wire the watcher to the store in `Casso/Shell/DiskManager.cpp`, watching each mounted image's directory and ignoring paths that are not mounted
- [ ] T020 [US1] Treat a directory that cannot be watched as `watching = false` rather than an error (FR-022) in `Casso/Shell/DiskManager.cpp`, leaving the write-time re-check as the guarantee
- [ ] T021 [US1] Show the pick-up banner using the existing `DxuiInfoBanner`, carrying a Restart action, in `Casso/Shell/DiskManager.cpp`. It stands until acted on rather than clearing itself (FR-010)
- [ ] T022 [US1] Make a standing banner absorb further changes rather than stack (FR-011) in `Casso/Shell/DiskManager.cpp`, and make acting on it read the image fresh at that moment so it takes the MOST RECENT contents (FR-012)
- [ ] T023 [US1] Add the fallback answer to `Casso/Config/GlobalUserPrefs.h`/`.cpp`, defaulting to ask, persisted across sessions and changeable without re-mounting (FR-007, FR-008)
- [ ] T024 [P] [US1] Add `UnitTest/ExternalChangePolicyTests.cpp` sweeping the decision table in BOTH directions: every combination of stated intent, fallback answer and dirty flag, and the outcome each produces. A table swept one way hides a missing row
- [ ] T025 [US1] Add `UnitTest/SharedImageTests.cpp` driving a change through `FakeImageWatcher` and asserting the store picks it up, refreshes its identity, and does not report its own writes as external
- [ ] T026 [US1] Register the new test files in `UnitTest/UnitTest.vcxproj`
- [ ] T027 [US1] Add a coalescing test to `UnitTest/SharedImageTests.cpp` asserting three changes inside the quiet period produce ONE pick-up, and that the third resets the timer rather than the first winning
- [ ] T028 [US1] Verify T025 discriminates: stop refreshing the identity after the emulator's own commit, confirm the tests go red on a self-inflicted change, restore
- [ ] T029 [US1] Walk [quickstart.md](quickstart.md) Scenario 1 against a real build, including the control: confirm eject-and-re-insert is no longer required

**Checkpoint**: The build loop works. Shippable on its own — the fallback answer covers intent until Phase 5.

---

## Phase 4: User Story 2 — A guest's work is never silently overwritten (P1)

**Goal**: Where both sides have changes, neither is discarded without the user choosing it.

**Independent test**: Mount a disk, have the guest write, change the image outside, confirm both versions survive and the user is asked.

- [ ] T030 [US2] Detect the conflict in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`: an external change AND a dirty image. Either alone is not a conflict, and treating it as one would put a dialog in the build loop
- [ ] T031 [US2] Refuse to write an image back over an unresolved external change (FR-025) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, returning through the existing flush-error path so the loss is surfaced rather than dropped
- [ ] T032 [P] [US2] Write the preserved copy beside the original with a timestamp in its name (FR-021), in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. Repeated conflicts in one session must accumulate rather than overwrite each other
- [ ] T033 [US2] Present the conflict question in `Casso/Shell/DiskManager.cpp`, stating what is at stake on BOTH sides — that the guest has written, and that something else changed the file (FR-033) — rather than two unlabeled options
- [ ] T034 [US2] Preserve whichever version the user does not keep, and tell them the path (FR-020), in `Casso/Shell/DiskManager.cpp`
- [ ] T035 [US2] Where the preserved copy cannot be written, refuse the discarding action, keep both versions live, and offer another location (FR-023), in `Casso/Shell/DiskManager.cpp`. **Not** a report followed by the loss anyway
- [ ] T036 [US2] Make no stated intent and no preference able to resolve a conflict (FR-019), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. The intent says how the guest continues, not whether work may be discarded
- [ ] T037 [P] [US2] Add conflict tests to `UnitTest/SharedImageTests.cpp`: dirty plus external change asks; dirty alone writes directly; external alone picks up; and the backup actually contains the version it claims
- [ ] T038 [US2] Add a backup-fails test to `UnitTest/SharedImageTests.cpp` asserting the discarding action does NOT proceed and both versions remain reachable
- [ ] T039 [US2] Verify T037 discriminates: let a stated intent resolve the conflict, confirm the test goes red, restore. This is the rule most likely to be "simplified" later by someone who reads the intent as a policy for everything
- [ ] T040 [US2] Walk [quickstart.md](quickstart.md) Scenario 2 against a real build, reading the backup back to confirm it holds the guest's file

**Checkpoint**: Nothing is lost without the user choosing it.

---

## Phase 5: User Story 1 completion — intent stated by the writer (P1)

**Goal**: The tool that writes the image says what the change should do, instead of the emulator holding a standing answer.

**Independent test**: Assemble with `--on-change restart` onto a mounted image and confirm the machine restarts; with `--on-change reload` and confirm it does not.

- [ ] T041 [US1] Add `pickUpIntent` to `CommandLineOptions` in `CassoCore/CommandLineOptions.h`, beside the image-target fields spec 026 added
- [ ] T042 [US1] Add the `--on-change` row to both assembler grammars and to the `disk` grammar in `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](contracts/cli.md), plus the long-option entries so `/on-change` is not shredded into single characters
- [ ] T043 [US1] Refuse `--on-change` without `--disk` in `CassoCore/CommandLineParser.cpp`, sharing spec 026's wording for image options given with no image target rather than inventing a second phrasing
- [ ] T044 [US1] Refuse an unrecognized value naming the value and listing the two accepted, in `CassoCore/CommandLineParser.cpp`, as the tree does everywhere a name outside a known set appears
- [ ] T045 [P] [US1] Add `Win32IntentChannel` in `Casso/Shell/Win32IntentChannel.h`/`.cpp`: broadcast `WM_COPYDATA` to every top-level `CassoWindow`. Use `SendMessage` with a timeout, never `PostMessage` — the payload must outlive the call, and a hung emulator must not hang a build
- [ ] T046 [US1] State the intent AFTER a successful commit in `CassoEmuCore/Cli/ImageArtifactSink.cpp` and `DiskCommandRunner.cpp`. Before the commit would describe contents not yet on disk
- [ ] T047 [US1] Receive the message in `Casso/EmulatorShell.cpp`, record it as a `PendingChange` on the matching bay, and return. Acting happens on the spindown callback, not here
- [ ] T048 [US1] Discard a stated intent whose image did not actually change, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The channel is a hint about a change, not a substitute for observing one
- [ ] T049 [US1] Make stating an intent with no emulator running a no-op rather than an error (FR-015) in `Casso/Shell/Win32IntentChannel.cpp`, so a build script behaves the same either way
- [ ] T050 [P] [US1] Add channel tests to `UnitTest/SharedImageTests.cpp` using `FakeIntentChannel`: the intent reaches the policy, an unstated one falls back, and a stated intent for an unchanged image is discarded
- [ ] T051 [US1] Add a switch-coverage row for `--on-change` in `UnitTest/CliSwitchCoverageTests.cpp`, in both dialects and both flag prefixes, so the flag cannot be documented without working
- [ ] T052 [US1] Confirm `--on-change` changes no assembled byte: same source with and without it produces identical images

**Checkpoint**: Intent comes from the writer. User Story 1 complete.

---

## Phase 6: User Story 3 — Two writers cannot spoil each other's work (P2)

**Goal**: Two writers produce one whole version, and neither loses the other's change unnoticed.

**Independent test**: Two emulator instances flush the same image; the result is one complete version. A writer committing over a change it never saw is detected.

**This phase needs nothing from Phases 2–5 and can be done first.** Both halves are defects in today's build.

- [ ] T053 [US3] Give `DiskImageStore::WriteFileAtomically` a temporary name that cannot collide between processes, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`. It derives `path + ".casso-tmp"` today, so two instances write into each other and one commits the other's bytes as its own
- [ ] T054 [US3] **Try reusing `CommitPlan::GetTemporaryPath` before writing a second scheme.** It already solves this exact problem for the command line, with an invocation tag and an existence check, and its comment states the failure the emulator still has. A second naming scheme is only justified if its tag source turns out to be CLI-specific
- [ ] T055 [US3] Re-check the recorded identity immediately before the emulator commits (FR-026), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, whatever the watcher has or has not reported. This is what makes a missed notification cost promptness rather than correctness
- [ ] T056 [US3] Ensure a temporary left behind by a killed writer is not adopted by another as its own (FR-027), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T057 [P] [US3] Add two-writer tests to `UnitTest/SharedImageTests.cpp`: two stores committing the same path derive different temporaries, and a commit over an unseen change is detected
- [ ] T058 [US3] Add a test to `UnitTest/SharedImageTests.cpp` asserting the atomic-rename guarantee itself (FR-024), so a later refactor that writes in place becomes a failure rather than a silent regression
- [ ] T059 [US3] Verify T057 discriminates: restore the fixed `.casso-tmp` suffix, confirm the collision test goes red, restore the fix

**Checkpoint**: Two writers are safe from each other.

---

## Phase 7: Polish & Cross-Cutting

- [ ] T060 [P] Update `docs/Assembler.md` and `docs/DiskImages.md` with the `--on-change` flag and the shared-image behavior, including that a pick-up is a disk swap and cannot be verified safe
- [ ] T061 [P] Add `CHANGELOG.md` entries under `[Unreleased]`: the build loop, the conflict handling, and the two defects fixed — the temp-name collision stated as a data-loss fix
- [ ] T062 Confirm every refusal and conflict names the image it concerns (FR-032). A user with two disks mounted cannot act on a message that does not say which
- [ ] T063 Measure the idle cost (SC-006): frame rate and audio with the watcher running against the same session without it. A watcher that wakes on every write in a busy directory is easy to build by accident
- [ ] T064 Confirm a session with no external change writes byte-for-byte what today's build writes (SC-003)
- [ ] T065 Run `scripts\CheckStyle.ps1` before the first commit containing a new file, since diff mode cannot see a file that has never been committed
- [ ] T066 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild and resolve to zero warnings. Analysis over a stale Release build fabricates LNK4020 noise
- [ ] T067 Run the full suite in Debug and compare against T001's baseline
- [ ] T068 Walk [quickstart.md](quickstart.md) end to end, Scenarios 1 through 6, including the two-emulator case that fails on today's build

---

## Dependencies

```text
Phase 1 (Setup)
   ↓
Phase 2 (Foundational) ─────────────┐
   ↓                                │
Phase 3 (US1: build loop) ── MVP    │  Phase 6 (US3: two writers)
   ↓                                │  independent of everything
Phase 4 (US2: nothing lost)         │  above; can go first
   ↓                                │
Phase 5 (US1: stated intent)        │
   ↓                                ↓
Phase 7 (Polish) ←──────────────────┘
```

**Story independence**: US1 and US2 both touch `DiskImageStore`, so they are sequential in practice though independently testable. US3 shares only the file, and its two changes are localized enough to land in either order.

## Parallel Opportunities

- **Phase 2**: T003, T004, T005 and T010 are different files with no dependency on each other.
- **Phase 3**: T018 (the Win32 watcher) is independent of T013–T017 (the core decision path) and can proceed alongside them. T024 likewise.
- **Phase 5**: T045 (the Win32 channel) is independent of T041–T044 (the grammar).
- **Phase 7**: T060 and T061 are documentation and touch nothing else.

## Implementation Strategy

**MVP is Phase 3.** It delivers the loop the whole disk-writing capability exists to serve, and it loses nothing, so it is safe to ship before the conflict handling.

**Ship Phase 4 next, not Phase 5.** Phase 3 makes an existing loss case *more likely* by picking changes up more often, so the guarantee that nothing is discarded should follow immediately. The stated-intent work is a convenience by comparison.

**Phase 6 can jump the queue** if a quick, self-contained win is wanted: two real defects, no new machinery, and the temp-name collision is a data-loss bug in shipped code.
