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

**Commit at every checkpoint.** The constitution requires a commit per completed phase rather than one at the end, so each checkpoint below is a commit point.

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

- [ ] T003 [P] Add `ImageIdentity` to `CassoEmuCore/Devices/Disk/ImageIdentity.h`/`.cpp` per [data-model.md](data-model.md): size, write time, and a `recorded` flag. **Wrap the existing `FileStamp` from `IDiskFileIo.h` rather than re-declaring its two fields**, or record in a comment why a second shape is needed. Use the has-flag idiom rather than sentinels — a zero size and a zero time are both legal, and `DiskImageSession::OpenedImage` already carries `stampRecorded` for exactly this reason
- [ ] T004 [P] Add the `IImageWatcher` seam to `CassoEmuCore/Devices/Disk/IImageWatcher.h`: watch a directory, report a path that changed, stop watching. Interface only, mirroring `IDiskFileIo`
- [ ] T005 [P] Add the `IIntentChannel` seam to `CassoEmuCore/Cli/IIntentChannel.h` per [contracts/channel.md](contracts/channel.md). `StateIntent` returns void — a failure to deliver degrades to the fallback, and no caller could act on an error
- [ ] T006 Add `PickUpIntent` to `CassoEmuCore/Devices/Disk/ExternalChangePolicy.h` with `Unstated`, `TakeUpInPlace` and `Restart`. `Unstated` is a real value, not a missing one: it is what every writer that is not `CassoCli` produces
- [ ] T007 Add `MountedImageState` and `PendingChange` to `CassoEmuCore/Devices/Disk/MountedImageState.h`/`.cpp` per [data-model.md](data-model.md)
- [ ] T008 Record an `ImageIdentity` at mount on `DiskImageStore::Entry` (FR-001) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and clear it at eject. **Foundational rather than part of the build loop**, because the re-check below is inert without it
- [ ] T009 Re-check the recorded identity immediately before the emulator commits (FR-003, FR-027) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, whatever the watcher has or has not reported. **A failing re-check refuses the commit and surfaces through the existing flush-error path**; the conflict phase later refines that into a question rather than a refusal. Both later stories cite this as the guarantee that holds when a notification is missed, so it cannot be built after them
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
- [ ] T021 [US1] Defer the pick-up while the image is held by another process, using the existing `IDiskFileIo::IsHeldByAnotherProcess`. The quiet period alone does not cover a text editor or copy tool still writing; both writers here commit atomically, but a third-party writer need not
- [ ] T022 [US1] Add `ApplyPendingPickUp` to `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, taking up the newest contents at a moment with no disk operation in flight (FR-014). **The decision and the work are core's**; the shell only says when
- [ ] T023 [US1] Perform the restart when that is what was decided (FR-005, US1 acceptance scenario 2) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, routed through the existing machine-reset path rather than a second way to reset a machine
- [ ] T024 [US1] Give `ApplyPendingPickUp` a trigger for the IDLE machine, wired from `Casso/Shell/MachineManager.cpp` on the CPU thread's existing tick path, gated on no disk operation being in flight. **The spindown callback alone is not enough and this is the headline scenario**: it fires only after a motor-on to motor-off transition with the spindown timer expiring, so a guest sitting at a BASIC prompt -- which is exactly how the build loop is used -- never reaches it and never sees the change. FR-014 asks only that no operation be in flight; spindown is sufficient, not necessary
- [ ] T025 [US1] Add a test asserting a pick-up reaches an IDLE machine that has never spun its motor. Without it the MVP fails its own acceptance scenario and looks like a wiring bug
- [ ] T026 [US1] Call `ApplyPendingPickUp` from the motor-spindown callback in `Casso/Shell/MachineManager.cpp`, alongside the existing `FlushAll` wiring. **One line and no decisions** — the callback is documented as "a naturally debounced, race-free point" on the thread that owns disk writes
- [ ] T027 [P] [US1] Add `Win32ImageWatcher` in `CassoEmuCore/Devices/Disk/Win32ImageWatcher.h`/`.cpp` over `ReadDirectoryChangesW`. **Watch the DIRECTORY, not the file**: both writers commit by renaming a temporary over the target, so a handle on the image sees its own replacement as a delete
- [ ] T028 [US1] Wire the watcher to the store in `Casso/Shell/DiskManager.cpp`, forwarding raw changed paths to core. It registers a watch at mount and drops it at eject; it forwards and does not decide which bay a path belongs to
- [ ] T029 [US1] Treat a directory that cannot be watched as `watching = false` rather than an error (FR-032) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, leaving the write-time re-check as the guarantee
- [ ] T030 [US1] Show the pick-up banner using the existing `DxuiInfoBanner`, carrying a Restart action that remains available afterward (FR-009, FR-010), as a new composite widget in `Dxui/Window/DxuiActionBanner.h`/`.cpp`, beside `DxuiButtonRow` which it composes, with `Casso/EmulatorShell.cpp` holding only its instantiation. **A host inside the exe could not satisfy the Dxui test this phase also requires**, and Principle VI is the rule that has already been broken twice here. `DxuiInfoBanner` cannot carry the action alone: its own header says "not clickable, no raised surface". **Presentation only** — the text and the available actions come from `ExternalChangePolicy`.
- [ ] T031 [US1] **`DxuiInfoBanner` is not clickable** -- its own header says "not clickable, no raised surface" -- so the new widget composes it with a `DxuiButtonRow`. Nothing in the tree hosts a non-modal banner over the running machine yet, so the host is new work rather than a wiring task
- [ ] T032 [US1] Make a standing report absorb further changes rather than stack (FR-011) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, and make acting on it read the image fresh so it takes the MOST RECENT contents (FR-012)
- [ ] T033 [US1] Present the *ask* prompt in `Casso/EmulatorShell.cpp`, taking its text and its available answers from `ExternalChangePolicy`, **which names the image it is about** (FR-033). **Asking is the default fallback, so this phase does not ship without it** -- core composes the question and nothing draws it today
- [ ] T034 [US1] Store the fallback answer in `Casso/Config/GlobalUserPrefs.h`/`.cpp` and read it into `ExternalChangePolicy` as an injected value (FR-007). Default to ask, persist across sessions. **The value lives in the exe's prefs struct and the DECISION does not** -- parsing and meaning stay in `ExternalChangePolicy`, which is what keeps this inside Principle VI. Recorded here so it is not re-litigated. Spell the values the way `audioDownloadConsent` does, and parse them in `ExternalChangePolicy` where an unrecognized stored value falls back to ask
- [ ] T035 [US1] Surface the fallback answer in Settings so it is changeable without restarting the emulator or re-mounting (FR-008), in `Casso/Ui/`. Without this the preference cannot be changed at all
- [ ] T036 [US1] Add a round-trip test for the fallback answer to the existing `UnitTest/UiTests/GlobalUserPrefsTests.cpp`, and a binding test for its Settings surface (FR-008). Persisting and being changeable are asserted by nothing today
- [ ] T037 [US1] Add `UnitTest/Dxui/DxuiActionBannerTests.cpp` beside `DxuiInfoBannerTests.cpp` and `DxuiButtonRowTests.cpp`: the banner lays out with its action, and the action reports being invoked
- [ ] T038 [P] [US1] Add `UnitTest/EmuTests/ExternalChangePolicyTests.cpp` sweeping the decision table in BOTH directions: every combination of stated intent, fallback answer and dirty flag against the outcome each produces. A table swept one way hides a missing row
- [ ] T039 [US1] Add `UnitTest/EmuTests/SharedImageTests.cpp` driving a change through `FakeImageWatcher` and asserting the store picks it up, refreshes its identity, and does not report its own writes as external
- [ ] T040 [US1] Add a coalescing test to `UnitTest/EmuTests/SharedImageTests.cpp`: three changes inside the quiet period produce ONE pick-up, and the third resets the timer rather than the first winning
- [ ] T041 [US1] Add a path-matching test to `UnitTest/EmuTests/SharedImageTests.cpp`: two spellings of one path reach the same bay, and a path no bay holds reaches none
- [ ] T042 [US1] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj`, `Dxui/Dxui.vcxproj` (the new banner widget), `Casso/Casso.vcxproj` (any new Settings file) and `UnitTest/UnitTest.vcxproj`
- [ ] T043 [US1] Verify the pick-up tests discriminate: stop refreshing the identity after the emulator's own commit, confirm they go red on a self-inflicted change, restore
- [ ] T044 [US1] **DEFER the pick-up entirely while the image is dirty**, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, until the conflict handling exists. Without this guard the policy emits a conflict outcome that nothing in this phase handles, and an MVP-only ship would pick up over guest writes and discard them -- the exact loss the next story exists to prevent. Remove the guard when that story lands
- [ ] T045 [US1] Add a test asserting the deferral: a dirty image plus an external change picks up NOTHING and loses NOTHING in this phase
- [ ] T046 [US1] Walk [quickstart.md](quickstart.md) Scenario 1 against a real build, including the control: confirm eject-and-re-insert is no longer required

**Checkpoint**: The build loop works, with the ask prompt as its default. Shippable on its own.

---

## Phase 4: User Story 2 — No version is discarded unless the user chose it (P1)

**Goal**: Where both sides have changes, or where the file itself goes away, nothing is lost without the user choosing it.

**Independent test**: Guest writes, image changes outside, both versions survive and the user is asked (SC-002). Separately: delete the image and confirm the emulator offers to save what it holds.

- [ ] T047 [US2] **Remove the dirty-image deferral guard** added in the build-loop phase, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and replace its deferral test with the conflict tests below. Leaving it in place makes this entire phase unreachable and every change to a dirty disk silently ignored
- [ ] T048 [US2] Detect the conflict in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`: an external change AND a dirty image. Either alone is not a conflict, and treating it as one would put a dialog in the build loop
- [ ] T049 [US2] Refuse to write an image back over an unresolved external change (FR-022) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, returning through the existing flush-error path so the loss is surfaced rather than dropped
- [ ] T050 [P] [US2] Put backup naming, writing and preservation in their own `CassoEmuCore/Devices/Disk/PreservedCopy.h`/`.cpp` rather than growing `MountedImageState`, which would otherwise carry path matching, coalescing, watch-degrade, the pending record AND all of this. Principle V
- [ ] T051 [P] [US2] Name the preserved copy in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`: beside the original, timestamped, **disambiguated by a counter suffix where a backup with that timestamp already exists** -- `PROG.20260830-014233.dsk`, then `-2`, then `-3` (FR-021). One-second resolution cannot keep the accumulate-rather-than-overwrite promise alone
- [ ] T052 [US2] Write the preserved copy through the `IDiskFileIo` seam using that name, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, so a test writes no real file
- [ ] T053 [US2] Present the conflict question and the alternate-location picker in `Casso/EmulatorShell.cpp`, both taking their text and answers from core. Phase 4 otherwise composes three questions and draws none of them
- [ ] T054 [US2] Compose the conflict question in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`, **naming the image** (FR-033) and stating what is at stake on BOTH sides — that the guest has written, and that something else changed the file (FR-034) — rather than two unlabeled options
- [ ] T055 [US2] Preserve whichever version the user does not keep and report where it went (FR-020), in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`
- [ ] T056 [US2] Where the preserved copy cannot be written, refuse the discarding action and keep both versions live (FR-024) in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`; the shell offers a location picker. **This is the decision most worth asserting, so it must be reachable from `UnitTest`**
- [ ] T057 [US2] Handle the two ways that offer itself fails — the user cancels rather than choosing, and the location chosen is also unwritable (FR-024) — in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. Both leave the conflict unresolved with both versions live
- [ ] T058 [US2] Resolve an unresolved conflict by ejecting as "keep what is on disk", preserving the guest's copy (FR-023), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. An eject is a plausible answer to being asked a question and must not become the one path that loses work
- [ ] T059 [US2] Make no stated intent and no preference able to resolve a conflict (FR-019), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. The intent says how the guest continues, not whether work may be discarded
- [ ] T060 [US2] Detect that new contents cannot be used — deleted, a different format, a different geometry, undecodable — in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and refuse the pick-up while carrying on with what is held (FR-016)
- [ ] T061 [US2] Offer to save the in-memory disk when the backing file has become unusable (FR-017), reusing that naming, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`. **With the file gone, what the emulator holds may be the only copy of that disk**, so this offer does NOT depend on the dirty flag
- [ ] T062 [US2] Route that offer through the existing `Entry::salvageOffered` and `SalvageDialogContent` machinery in `Casso/EmulatorShell.cpp`, where all 37 salvage references already live, rather than adding a second rescue path
- [ ] T063 [US2] Leave the machine running and the disk mounted when the offer is declined (FR-018), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp` and the shell's decline handler
- [ ] T064 [US2] Handle an image becoming unusable while a report from an earlier change still stands, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`
- [ ] T065 [US2] Handle the image reappearing in a usable state after the user declined to save, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`: re-stat, re-arm the watch, and treat the reappearance as an ordinary change rather than a special case
- [ ] T066 [P] [US2] Add conflict tests to `UnitTest/EmuTests/SharedImageTests.cpp`: dirty plus external change asks; dirty alone writes directly; external alone picks up; and the backup actually contains the version it claims
- [ ] T067 [US2] Add a backup-name collision test to `UnitTest/EmuTests/SharedImageTests.cpp`: two conflicts on one image within the same second produce two distinct backups, neither overwriting the other
- [ ] T068 [US2] Add a backup-fails test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting the discarding action does NOT proceed and both versions remain reachable
- [ ] T069 [P] [US2] Add unusable-contents tests to `UnitTest/EmuTests/SharedImageTests.cpp`: a deleted image, a wrong-geometry image and an undecodable one each refuse the pick-up and leave the held contents intact; and the rescue offer appears for a CLEAN image too
- [ ] T070 [US2] Verify the conflict tests discriminate: let a stated intent resolve the conflict, confirm they go red, restore. This is the rule most likely to be "simplified" later by someone reading the intent as a policy for everything
- [ ] T071 [US2] Walk [quickstart.md](quickstart.md) Scenarios 2 and 4 against a real build, reading the backup back to confirm it holds the guest's file

- [ ] T072 [US2] Add tests to `UnitTest/EmuTests/SharedImageTests.cpp` for four behaviors built in this phase that nothing asserts yet: eject resolves a conflict without discarding either version; declining the rescue leaves the machine running and mounted; an image going unusable while a report stands; and the file reappearing after a declined rescue

**Checkpoint**: Nothing is lost without the user choosing it, including when the file itself goes away.

---

## Phase 5: User Story 1 completion — intent stated by the writer (P1)

**Goal**: The tool that writes the image says what the change should do.

**⚠️ SPLIT BY DEPENDENCY.** The `disk` subcommand half is buildable today. The assembler half needs spec 026's flat image-target options and its `ImageArtifactSink`, neither of which is on this branch.

**Independent test**: `disk put` with `--on-change restart` onto a mounted image restarts the machine; with `--on-change reload` it does not.

### Buildable now — the `disk` grammar

- [ ] T073 [US1] Add `pickUpIntent` to `CommandLineOptions` in `CassoCore/CommandLineOptions.h` (FR-005), beside the nested `disk` group until 026 provides the flat fields
- [ ] T074 [US1] Add the `--on-change` row to the `disk` grammar in `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](contracts/cli.md), plus the long-option entry so `/on-change` is not shredded into single characters. Document that `reload` is the surface spelling of `TakeUpInPlace`
- [ ] T075 [US1] Refuse an unrecognized value naming the value and listing the two accepted, in `CassoCore/CommandLineParser.cpp`
- [ ] T076 [P] [US1] Add `Win32IntentChannel` in `CassoEmuCore/Cli/Win32IntentChannel.h`/`.cpp`: send `WM_COPYDATA` to every top-level `CassoWindow` found by enumeration -- **not `HWND_BROADCAST`**, which `WM_COPYDATA` may not be sent to. **In core beside `Win32DiskFileIo`, and not negotiable** — this is the sender, its callers run inside `CassoCli.exe`, and `CassoCli` cannot link `Casso.exe`. Install the UIPI filter for `WM_COPYDATA` -- the filter takes a WINDOW MESSAGE and cannot see `dwData` -- independently of `InstallDragDropTarget`, which runs only when OLE initialization succeeded -- otherwise an elevated Casso silently drops every intent. Validate `dwData` against the registered message id so an unrelated `WM_COPYDATA` is ignored. Use `SendMessage` with a timeout, never `PostMessage`: the payload must outlive the call, and a hung emulator must not hang a build
- [ ] T077 [US1] State the intent AFTER a successful commit in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp`. Before the commit would describe contents not yet on disk
- [ ] T078 [US1] Receive the message in `Casso/EmulatorShell.cpp` and hand the raw payload to core, which matches it to a bay and records a pending change (FR-006). The shell does no matching
- [ ] T079 [US1] Discard a stated intent whose image did not actually change, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The channel is a hint about a change, not a substitute for observing one
- [ ] T080 [US1] Make stating an intent with no emulator running a no-op rather than an error (FR-015), with the rule in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp` so `UnitTest` can assert it
- [ ] T081 [P] [US1] Add channel tests to `UnitTest/EmuTests/SharedImageTests.cpp` using `FakeIntentChannel`: the intent reaches the policy, an unstated one falls back, and a stated intent for an unchanged image is discarded
- [ ] T082 [US1] Add a switch-coverage row for `--on-change` in `UnitTest/CliSwitchCoverageTests.cpp` for the `disk` grammar and both flag prefixes, so the flag cannot be documented without working
- [ ] T083 [US1] Add a test asserting that stating an intent with no emulator running is a no-op rather than an error (FR-015), which the task building it asks for and nothing does. Then confirm `disk put --on-change` writes byte-for-byte what `disk put` alone writes, so the guarantee is verified on this branch rather than only in the gated half
- [ ] T084 [US1] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`

### Gated on spec 026 — the assembler grammar

- [ ] T085 [US1] Add the `--on-change` row to both assembler grammars in `CassoCore/CommandLineParser.cpp`, and move `pickUpIntent` beside 026's flat image-target fields
- [ ] T086 [US1] Refuse `--on-change` without `--disk` in `CassoCore/CommandLineParser.cpp`, sharing 026's wording for image options given with no image target rather than inventing a second phrasing
- [ ] T087 [US1] State the intent after a successful commit in `CassoEmuCore/Cli/ImageArtifactSink.cpp`
- [ ] T088 [US1] Extend the switch-coverage row in `UnitTest/CliSwitchCoverageTests.cpp` to both assembler grammars
- [ ] T089 [US1] Confirm `--on-change` changes no assembled byte: the same source with and without it produces identical images

**Checkpoint**: Intent comes from the writer, for whichever grammars are available.

---

## Phase 6: User Story 3 — Two writers cannot spoil each other's work (P2)

**Goal**: Two writers produce one whole version, and neither loses the other's change unnoticed.

**Independent test**: Two emulator instances flush the same image; the result is one complete version (SC-004). **This fails on today's build**, which is what makes it worth running.

**The temp-name half needs nothing from any earlier phase.** The stale-detection half is already built in Phase 2 and only needs its tests here.

- [ ] T090 [US3] Give `DiskImageStore::WriteFileAtomically` a temporary name that cannot collide between processes (FR-026) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, **trying `CommitPlan::GetTemporaryPath` first** — it already solves this exact problem for the command line, with an invocation tag and an existence check, and its comment states the failure the emulator still has. Record in a comment why a second scheme was or was not needed
- [ ] T091 [US3] Ensure a temporary left behind by a killed writer is not adopted by another writer as its own (FR-028), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T092 [P] [US3] Add two-writer tests to `UnitTest/EmuTests/SharedImageTests.cpp`: two stores committing the same path derive different temporaries (SC-004), and a commit over an unseen change is detected
- [ ] T093 [US3] Add a failure-injection test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting a FAILED write leaves the image byte-for-byte unchanged (FR-029). The temp-path and cleanup changes above are what this invariant rests on
- [ ] T094 [US3] Add a test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting the atomic-rename guarantee itself (FR-025), so a later refactor that writes in place becomes a failure rather than a silent regression
- [ ] T095 [US3] Update the three assertions in `UnitTest/EmuTests/DiskImageStoreTests.cpp` that hardcode `target + ".casso-tmp"` to ask the temp-path helper instead. **Without this, Phase 6 goes red for a reason unrelated to the defect** and the discrimination check below is confounded
- [ ] T096 [US3] Verify the collision test discriminates: restore the fixed `.casso-tmp` suffix, confirm it goes red, restore the fix

**Checkpoint**: Two writers are safe from each other.

---

## Phase 7: Polish & Cross-Cutting

- [ ] T097 [P] Update `docs/Assembler.md` and `docs/disk-write-integrity.md` with the `--on-change` flag and the shared-image behavior, including that a pick-up is a disk swap and cannot be verified safe
- [ ] T098 [P] Add `CHANGELOG.md` entries under `[Unreleased]`: the build loop, the conflict handling, and the two defects fixed — the temp-name collision stated as a data-loss fix
- [ ] T099 Confirm every refusal and conflict names the image it concerns (FR-033, SC-005). A user with two disks mounted cannot act on a message that does not say which
- [ ] T100 Measure the idle cost (FR-031, SC-006) by comparing a session with a watched image mounted against one with no image mounted, **in the SAME build**. A cross-build A/B is untrustworthy here -- clocks move between runs and swamp the signal. Threshold, in **Release** over three runs of five minutes each: p99 frame time within 2% of the unmounted session, and audio underruns per minute no higher. Debug builds underrun on this hardware regardless, so Debug numbers prove nothing
- [ ] T101 Confirm a session with no external change writes byte-for-byte what today's build writes, and at the same moments (FR-030, SC-003)
- [ ] T102 Run `scripts\CheckStyle.ps1` before the first commit containing a new file, since diff mode cannot see a file that has never been committed
- [ ] T103 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild and resolve to zero warnings. Analysis over a stale Release build fabricates LNK4020 noise
- [ ] T104 Run the full suite in Debug and compare against the Phase 1 baseline
- [ ] T105 Run the full suite in **Release** and compile for **ARM64**. Quality Gates 1 and 4 require both, and this feature adds two Win32 components
- [ ] T106 Walk [quickstart.md](quickstart.md) end to end, including the two-emulator case that fails on today's build

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
Phase 7 (Polish) ←──────────────────────────────────┘
   
Phase 5b (US1: assembler) ⚠ GATED ON SPEC 026 -- hangs off 5a, and the
feature completes without it on this branch
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
