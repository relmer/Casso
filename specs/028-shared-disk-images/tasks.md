# Tasks: Disk Images Shared with a Running Emulator

**Input**: Design documents from `specs/028-shared-disk-images/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: Included and NOT optional. Constitution Principle II requires every public function and significant code path be covered, so test tasks sit inside each story rather than in a trailing phase.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete work)
- **[Story]**: US1–US3, matching [spec.md](spec.md). Setup, Foundational and Polish carry no story label.

## Three conventions this file keeps

**No task refers to another by number.** Three renumberings during planning left references pointing at the wrong task each time, once telling a reader to revert work unrelated to what was being verified. Work is named by what it is.

**Nothing that decides anything goes in an executable.** Constitution Principle VI is explicit that this is about testability, not platform boundaries: "Calling Win32 is not a reason to live in the exe." Both Win32 shims here therefore live in `CassoEmuCore`, beside `Win32DiskFileIo`. The intent channel additionally *cannot* live in `Casso.exe` — it is the sender, and its callers run inside `CassoCli.exe`.

**New `.cpp`/`.h` files need explicit `.vcxproj` entries.** MSBuild does not glob, so a file that compiles locally can still be missing from a clean build. Each phase registers its own.

---

## Phase 1: Setup

- [ ] T001 Establish a green baseline: run `scripts\Build.ps1` then `scripts\RunTests.ps1 -Build` for x64 Debug, and record the passing count so later runs compare against a number rather than an impression
- [ ] T002 Reproduce the defect by hand: mount a disk, write to it with `disk put` from a second process, confirm the guest sees nothing until eject and re-insert. **Use `disk put`, not an assembly** — the assembler cannot target an image on this branch, per the spec-026 gate under Dependencies

**Checkpoint**: Known-good starting point, and the bug seen once with your own eyes.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Identity, the seams, and the re-check every story below rests on.

**⚠️ No user story work can begin until this phase completes.**

- [ ] T003 [P] Add `ImageIdentity` to `CassoEmuCore/Devices/Disk/ImageIdentity.h`/`.cpp` per [data-model.md](data-model.md): size, write time, and a `recorded` flag. Use the has-flag idiom rather than sentinels — a zero size and a zero time are both legal, and `DiskImageSession::OpenedImage` already carries `stampRecorded` for exactly this reason
- [ ] T004 [P] Add the `IImageWatcher` seam to `CassoEmuCore/Devices/Disk/IImageWatcher.h`: watch a directory, report a path that changed, stop watching. Interface only, mirroring `IDiskFileIo`
- [ ] T005 [P] Add the `IIntentChannel` seam to `CassoEmuCore/Cli/IIntentChannel.h` per [contracts/channel.md](contracts/channel.md). `StateIntent` returns void — a failure to deliver degrades to the fallback, and no caller could act on an error
- [ ] T006 Add `PickUpIntent` to `CassoEmuCore/Devices/Disk/ExternalChangePolicy.h` with `Unstated`, `TakeUpInPlace` and `Restart`. `Unstated` is a real value, not a missing one: it is what every writer that is not `CassoCli` produces
- [ ] T007 Add `MountedImageState` and `PendingChange` to `CassoEmuCore/Devices/Disk/MountedImageState.h`/`.cpp` per [data-model.md](data-model.md)
- [ ] T008 Record an `ImageIdentity` at mount on `DiskImageStore::Entry` (FR-001) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and clear it at eject. **Foundational rather than part of the build loop**, because the re-check below is inert without it
- [ ] T009 Re-check the recorded identity immediately before the emulator commits (FR-003, FR-027) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, whatever the watcher has or has not reported. Both later stories cite this as the guarantee that holds when a notification is missed, so it cannot be built after them
- [ ] T010 Register the new core files in `CassoEmuCore/CassoEmuCore.vcxproj`
- [ ] T011 [P] Add `UnitTest/EmuTests/FakeImageWatcher.h` and `UnitTest/EmuTests/FakeIntentChannel.h`, so a test can drive a change and a stated intent with no file and no window
- [ ] T012 Add `UnitTest/EmuTests/ImageIdentityTests.cpp` covering the comparison rules: two recorded identities matching, either unrecorded never comparing equal, and a change in size alone or in time alone being a change. **In `EmuTests/` with its peers**, where every other disk test lives
- [ ] T013 Register the new test files in `UnitTest/UnitTest.vcxproj`
- [ ] T014 Verify the identity tests discriminate: make the comparison ignore the `recorded` flag, confirm they go red, restore. A default-constructed identity comparing equal to a real one is the bug the flag exists to prevent

**Checkpoint**: Identity, the re-check and the seams exist and are proven.

---

## Phase 3: User Story 1 — The build loop shows what was just built (P1) 🎯 MVP

**Goal**: A change made outside reaches the running guest without the developer ejecting and re-inserting by hand.

**Independent test**: Mount a disk, write a changed program onto it with `disk put` from a second process, confirm the guest loads the new version with no manual eject (SC-001).

- [ ] T015 [US1] Refresh the recorded identity after every commit the emulator itself makes (FR-004) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, so its own write is never reported as an external change. This is what makes the feature quiet rather than a source of false alarms
- [ ] T016 [US1] Add `ExternalChangePolicy` in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`: given a stated intent, a fallback answer and whether the image is dirty, decide take-up / restart / ask / conflict. Pure and injected — no clock, no files, no UI
- [ ] T017 [US1] Compose the *ask* prompt in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp` (FR-007), since asking is the default fallback and this phase ships with it. Core decides what is asked and which answers exist; the shell only shows it
- [ ] T018 [US1] Match an image path to a mounted bay in `CassoEmuCore/Devices/Disk/MountedImageState.cpp` (FR-002), normalizing the way mount does. **Comparing two spellings of one path is assertable logic**, so no executable does it
- [ ] T019 [US1] Coalesce changes on a quiet period measured from the LAST change (FR-013) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The clock is injected so a test needs no waiting. **Start at 1 second**, matching the spindown debounce the controller already applies, as a named constant so tuning it later is one edit
- [ ] T020 [US1] Handle a change arriving while an earlier one is still being applied, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`: it updates the pending record rather than being dropped because something was in progress
- [ ] T021 [US1] Add `ApplyPendingPickUp` to `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, taking up the newest contents at a moment with no disk operation in flight (FR-014). **The decision and the work are core's**; the shell only says when
- [ ] T022 [US1] Perform the restart when that is what was decided (FR-005, US1 acceptance scenario 2) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, routed through the existing machine-reset path rather than a second way to reset a machine
- [ ] T023 [US1] Call `ApplyPendingPickUp` from the motor-spindown callback in `Casso/Shell/MachineManager.cpp`, alongside the existing `FlushAll` wiring. **One line and no decisions** — the callback is documented as "a naturally debounced, race-free point" on the thread that owns disk writes
- [ ] T024 [P] [US1] Add `Win32ImageWatcher` in `CassoEmuCore/Devices/Disk/Win32ImageWatcher.h`/`.cpp` over `ReadDirectoryChangesW`. **Watch the DIRECTORY, not the file**: both writers commit by renaming a temporary over the target, so a handle on the image sees its own replacement as a delete
- [ ] T025 [US1] Wire the watcher to the store in `Casso/Shell/DiskManager.cpp`, forwarding raw changed paths to core. It registers and forwards; it does not decide which bay a path belongs to
- [ ] T026 [US1] Treat a directory that cannot be watched as `watching = false` rather than an error (FR-032) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, leaving the write-time re-check as the guarantee
- [ ] T027 [US1] Show the pick-up banner using the existing `DxuiInfoBanner`, carrying a Restart action that remains available afterward (FR-009, FR-010), in `Casso/Shell/DiskManager.cpp`. **Presentation only** — the text and the available actions come from `ExternalChangePolicy`
- [ ] T028 [US1] Make a standing report absorb further changes rather than stack (FR-011) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, and make acting on it read the image fresh so it takes the MOST RECENT contents (FR-012)
- [ ] T029 [US1] Store the fallback answer in `Casso/Config/GlobalUserPrefs.h`/`.cpp` and read it into `ExternalChangePolicy` as an injected value (FR-007). Default to ask, persist across sessions
- [ ] T030 [US1] Surface the fallback answer in Settings so it is changeable without restarting the emulator or re-mounting (FR-008), in `Casso/Ui/`. Without this the preference cannot be changed at all
- [ ] T031 [P] [US1] Add `UnitTest/EmuTests/ExternalChangePolicyTests.cpp` sweeping the decision table in BOTH directions: every combination of stated intent, fallback answer and dirty flag against the outcome each produces. A table swept one way hides a missing row
- [ ] T032 [US1] Add `UnitTest/EmuTests/SharedImageTests.cpp` driving a change through `FakeImageWatcher` and asserting the store picks it up, refreshes its identity, and does not report its own writes as external
- [ ] T033 [US1] Add a coalescing test to `UnitTest/EmuTests/SharedImageTests.cpp`: three changes inside the quiet period produce ONE pick-up, and the third resets the timer rather than the first winning
- [ ] T034 [US1] Add a path-matching test to `UnitTest/EmuTests/SharedImageTests.cpp`: two spellings of one path reach the same bay, and a path no bay holds reaches none
- [ ] T035 [US1] Register this phase's new core and test files in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`
- [ ] T036 [US1] Verify the pick-up tests discriminate: stop refreshing the identity after the emulator's own commit, confirm they go red on a self-inflicted change, restore
- [ ] T037 [US1] Walk [quickstart.md](quickstart.md) Scenario 1 against a real build, including the control: confirm eject-and-re-insert is no longer required

**Checkpoint**: The build loop works, with the ask prompt as its default. Shippable on its own.

---

## Phase 4: User Story 2 — No version is discarded unless the user chose it (P1)

**Goal**: Where both sides have changes, or where the file itself goes away, nothing is lost without the user choosing it.

**Independent test**: Guest writes, image changes outside, both versions survive and the user is asked (SC-002). Separately: delete the image and confirm the emulator offers to save what it holds.

- [ ] T038 [US2] Detect the conflict in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`: an external change AND a dirty image. Either alone is not a conflict, and treating it as one would put a dialog in the build loop
- [ ] T039 [US2] Refuse to write an image back over an unresolved external change (FR-022) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, returning through the existing flush-error path so the loss is surfaced rather than dropped
- [ ] T040 [P] [US2] Name the preserved copy in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`: beside the original, timestamped, **disambiguated where a backup with that timestamp already exists** (FR-021). One-second resolution cannot keep the accumulate-rather-than-overwrite promise alone
- [ ] T041 [US2] Write the preserved copy through the `IDiskFileIo` seam using that name, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, so a test writes no real file
- [ ] T042 [US2] Compose the conflict question in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`, stating what is at stake on BOTH sides — that the guest has written, and that something else changed the file (FR-034) — rather than two unlabeled options
- [ ] T043 [US2] Preserve whichever version the user does not keep and report where it went (FR-020), in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`
- [ ] T044 [US2] Where the preserved copy cannot be written, refuse the discarding action and keep both versions live (FR-024) in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`; the shell offers a location picker. **This is the decision most worth asserting, so it must be reachable from `UnitTest`**
- [ ] T045 [US2] Handle the two ways that offer itself fails — the user cancels rather than choosing, and the location chosen is also unwritable (FR-024) — in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. Both leave the conflict unresolved with both versions live
- [ ] T046 [US2] Resolve an unresolved conflict by ejecting as "keep what is on disk", preserving the guest's copy (FR-023), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. An eject is a plausible answer to being asked a question and must not become the one path that loses work
- [ ] T047 [US2] Make no stated intent and no preference able to resolve a conflict (FR-019), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. The intent says how the guest continues, not whether work may be discarded
- [ ] T048 [US2] Detect that new contents cannot be used — deleted, a different format, a different geometry, undecodable — in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and refuse the pick-up while carrying on with what is held (FR-016)
- [ ] T049 [US2] Offer to save the in-memory disk when the backing file has become unusable (FR-017), reusing that naming, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`. **With the file gone, what the emulator holds may be the only copy of that disk**, so this offer does NOT depend on the dirty flag
- [ ] T050 [US2] Route that offer through the existing `Entry::salvageOffered` and `SalvageDialogContent` machinery in `Casso/Shell/DiskManager.cpp` rather than adding a second rescue path
- [ ] T051 [US2] Leave the machine running and the disk mounted when the offer is declined (FR-018), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp` and the shell's decline handler
- [ ] T052 [US2] Handle an image becoming unusable while a report from an earlier change still stands, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`
- [ ] T053 [P] [US2] Add conflict tests to `UnitTest/EmuTests/SharedImageTests.cpp`: dirty plus external change asks; dirty alone writes directly; external alone picks up; and the backup actually contains the version it claims
- [ ] T054 [US2] Add a backup-name collision test to `UnitTest/EmuTests/SharedImageTests.cpp`: two conflicts on one image within the same second produce two distinct backups, neither overwriting the other
- [ ] T055 [US2] Add a backup-fails test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting the discarding action does NOT proceed and both versions remain reachable
- [ ] T056 [P] [US2] Add unusable-contents tests to `UnitTest/EmuTests/SharedImageTests.cpp`: a deleted image, a wrong-geometry image and an undecodable one each refuse the pick-up and leave the held contents intact; and the rescue offer appears for a CLEAN image too
- [ ] T057 [US2] Verify the conflict tests discriminate: let a stated intent resolve the conflict, confirm they go red, restore. This is the rule most likely to be "simplified" later by someone reading the intent as a policy for everything
- [ ] T058 [US2] Walk [quickstart.md](quickstart.md) Scenarios 2 and 4 against a real build, reading the backup back to confirm it holds the guest's file

**Checkpoint**: Nothing is lost without the user choosing it, including when the file itself goes away.

---

## Phase 5: User Story 1 completion — intent stated by the writer (P1)

**Goal**: The tool that writes the image says what the change should do.

**⚠️ SPLIT BY DEPENDENCY.** The `disk` subcommand half is buildable today. The assembler half needs spec 026's flat image-target options and its `ImageArtifactSink`, neither of which is on this branch.

**Independent test**: `disk put` with `--on-change restart` onto a mounted image restarts the machine; with `--on-change reload` it does not.

### Buildable now — the `disk` grammar

- [ ] T059 [US1] Add `pickUpIntent` to `CommandLineOptions` in `CassoCore/CommandLineOptions.h` (FR-005), beside the nested `disk` group until 026 provides the flat fields
- [ ] T060 [US1] Add the `--on-change` row to the `disk` grammar in `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](contracts/cli.md), plus the long-option entry so `/on-change` is not shredded into single characters. Document that `reload` is the surface spelling of `TakeUpInPlace`
- [ ] T061 [US1] Refuse an unrecognized value naming the value and listing the two accepted, in `CassoCore/CommandLineParser.cpp`
- [ ] T062 [P] [US1] Add `Win32IntentChannel` in `CassoEmuCore/Cli/Win32IntentChannel.h`/`.cpp`: broadcast `WM_COPYDATA` to every top-level `CassoWindow`. **In core beside `Win32DiskFileIo`, and not negotiable** — this is the sender, its callers run inside `CassoCli.exe`, and `CassoCli` cannot link `Casso.exe`. Use `SendMessage` with a timeout, never `PostMessage`: the payload must outlive the call, and a hung emulator must not hang a build
- [ ] T063 [US1] State the intent AFTER a successful commit in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp`. Before the commit would describe contents not yet on disk
- [ ] T064 [US1] Receive the message in `Casso/EmulatorShell.cpp` and hand the raw payload to core, which matches it to a bay and records a pending change (FR-006). The shell does no matching
- [ ] T065 [US1] Discard a stated intent whose image did not actually change, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The channel is a hint about a change, not a substitute for observing one
- [ ] T066 [US1] Make stating an intent with no emulator running a no-op rather than an error (FR-015), with the rule in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp` so `UnitTest` can assert it
- [ ] T067 [P] [US1] Add channel tests to `UnitTest/EmuTests/SharedImageTests.cpp` using `FakeIntentChannel`: the intent reaches the policy, an unstated one falls back, and a stated intent for an unchanged image is discarded
- [ ] T068 [US1] Add a switch-coverage row for `--on-change` in `UnitTest/CliSwitchCoverageTests.cpp` for the `disk` grammar and both flag prefixes, so the flag cannot be documented without working
- [ ] T069 [US1] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`

### Gated on spec 026 — the assembler grammar

- [ ] T070 [US1] Add the `--on-change` row to both assembler grammars in `CassoCore/CommandLineParser.cpp`, and move `pickUpIntent` beside 026's flat image-target fields
- [ ] T071 [US1] Refuse `--on-change` without `--disk` in `CassoCore/CommandLineParser.cpp`, sharing 026's wording for image options given with no image target rather than inventing a second phrasing
- [ ] T072 [US1] State the intent after a successful commit in `CassoEmuCore/Cli/ImageArtifactSink.cpp`
- [ ] T073 [US1] Extend the switch-coverage row in `UnitTest/CliSwitchCoverageTests.cpp` to both assembler grammars
- [ ] T074 [US1] Confirm `--on-change` changes no assembled byte: the same source with and without it produces identical images

**Checkpoint**: Intent comes from the writer, for whichever grammars are available.

---

## Phase 6: User Story 3 — Two writers cannot spoil each other's work (P2)

**Goal**: Two writers produce one whole version, and neither loses the other's change unnoticed.

**Independent test**: Two emulator instances flush the same image; the result is one complete version (SC-004). **This fails on today's build**, which is what makes it worth running.

**The temp-name half needs nothing from any earlier phase.** The stale-detection half is already built in Phase 2 and only needs its tests here.

- [ ] T075 [US3] Give `DiskImageStore::WriteFileAtomically` a temporary name that cannot collide between processes (FR-026) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, **trying `CommitPlan::GetTemporaryPath` first** — it already solves this exact problem for the command line, with an invocation tag and an existence check, and its comment states the failure the emulator still has. Record in a comment why a second scheme was or was not needed
- [ ] T076 [US3] Ensure a temporary left behind by a killed writer is not adopted by another writer as its own (FR-028), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T077 [P] [US3] Add two-writer tests to `UnitTest/EmuTests/SharedImageTests.cpp`: two stores committing the same path derive different temporaries (SC-004), and a commit over an unseen change is detected
- [ ] T078 [US3] Add a failure-injection test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting a FAILED write leaves the image byte-for-byte unchanged (FR-029). The temp-path and cleanup changes above are what this invariant rests on
- [ ] T079 [US3] Add a test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting the atomic-rename guarantee itself (FR-025), so a later refactor that writes in place becomes a failure rather than a silent regression
- [ ] T080 [US3] Verify the collision test discriminates: restore the fixed `.casso-tmp` suffix, confirm it goes red, restore the fix

**Checkpoint**: Two writers are safe from each other.

---

## Phase 7: Polish & Cross-Cutting

- [ ] T081 [P] Update `docs/Assembler.md` and `docs/disk-write-integrity.md` with the `--on-change` flag and the shared-image behavior, including that a pick-up is a disk swap and cannot be verified safe
- [ ] T082 [P] Add `CHANGELOG.md` entries under `[Unreleased]`: the build loop, the conflict handling, and the two defects fixed — the temp-name collision stated as a data-loss fix
- [ ] T083 Confirm every refusal and conflict names the image it concerns (FR-033, SC-005). A user with two disks mounted cannot act on a message that does not say which
- [ ] T084 Measure the idle cost (FR-031, SC-006) by comparing this branch against its merge-base build, since no off-switch exists. Name a threshold — frame-time delta and audio underruns per minute — rather than judging by eye
- [ ] T085 Confirm a session with no external change writes byte-for-byte what today's build writes, and at the same moments (FR-030, SC-003)
- [ ] T086 Run `scripts\CheckStyle.ps1` before the first commit containing a new file, since diff mode cannot see a file that has never been committed
- [ ] T087 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild and resolve to zero warnings. Analysis over a stale Release build fabricates LNK4020 noise
- [ ] T088 Run the full suite in Debug and compare against the Phase 1 baseline
- [ ] T089 Run the full suite in **Release** and compile for **ARM64**. Quality Gates 1 and 4 require both, and this feature adds two Win32 components
- [ ] T090 Walk [quickstart.md](quickstart.md) end to end, including the two-emulator case that fails on today's build

---

## Dependencies

```text
Phase 1 (Setup)
   ↓
Phase 2 (Foundational: identity, re-check, seams) ──┐
   ↓                                                │
Phase 3 (US1: build loop) ── MVP                    │  Phase 6, temp-name half:
   ↓                                                │  free-standing, can go first
Phase 4 (US2: nothing discarded)                    │
   ↓                                                │  Phase 6, stale-detection
Phase 5a (US1: disk grammar)                        │  tests: exercise Phase 2
   ↓                                                │
Phase 5b (US1: assembler)  ⚠ GATED ON SPEC 026      │
   ↓                                                ↓
Phase 7 (Polish) ←──────────────────────────────────┘
```

**Phase 5 is split, not merely annotated.** Its `disk`-grammar half is buildable today and demonstrates the whole mechanism; its assembler half needs 026's flat image-target options and `ImageArtifactSink`, neither of which exists on this branch.

**Phase 6's temp-name half can go first.** It is a data-loss defect in shipped code and needs nothing from any earlier phase. Its stale-detection tests exercise the re-check built in Phase 2.

**Story independence**: US1 and US2 both touch `DiskImageStore`, so they are sequential in practice though independently testable.

## Parallel Opportunities

Within a phase, work on different files with no dependency between them:

- **Phase 2**: the identity type, the two seams, and the fakes.
- **Phase 3**: the Win32 watcher is independent of the core decision path — the policy, the coalescing, the identity bookkeeping — and can proceed alongside it. The policy's test sweep likewise.
- **Phase 4**: the backup-naming rule and the unusable-contents tests are independent of the conflict path.
- **Phase 5**: the Win32 channel is independent of the grammar work.
- **Phase 7**: the two documentation tasks touch nothing else.

## Implementation Strategy

**MVP is Phase 3.** It delivers the loop the whole disk-writing capability exists to serve, and it loses nothing, so it is safe to ship before the conflict handling.

**Ship Phase 4 next, not Phase 5.** Phase 3 makes an existing loss case *more likely* by picking changes up more often, so the guarantee that nothing is discarded should follow immediately. The stated-intent work is a convenience by comparison, and half of it is gated on 026 in any case.

**The temp-name fix can jump the queue** if a quick, self-contained win is wanted: a real defect, no new machinery, and a data-loss bug in shipped code.
