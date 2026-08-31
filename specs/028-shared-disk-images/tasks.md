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

- [X] T001 Establish a green baseline: run `scripts\Build.ps1` then `scripts\RunTests.ps1 -Build` for x64 Debug, and record the passing count so later runs compare against a number rather than an impression
- [X] T002 Reproduce the defect by hand: mount a disk, write to it with `disk put` from a second process, confirm the guest sees nothing until eject and re-insert. **Use `disk put`, not an assembly** — the assembler cannot target an image on this branch, per the spec-026 gate under Dependencies

**Checkpoint**: Known-good starting point, and the bug seen once with your own eyes.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Identity, the seams, and the re-check every story below rests on.

**⚠️ No user story work can begin until this phase completes.**

- [X] T003 [P] Add `ImageIdentity` to `CassoEmuCore/Devices/Disk/ImageIdentity.h`/`.cpp` per [data-model.md](data-model.md): size, write time, and a `recorded` flag. **Wrap the existing `FileStamp` from `IDiskFileIo.h` rather than re-declaring its two fields**, or record in a comment why a second shape is needed. Use the has-flag idiom rather than sentinels — a zero size and a zero time are both legal, and `DiskImageSession::OpenedImage` already carries `stampRecorded` for exactly this reason
- [X] T004 [P] Add the `IImageWatcher` seam to `CassoEmuCore/Devices/Disk/IImageWatcher.h`: watch a directory, report a path that changed, stop watching. Interface only, mirroring `IDiskFileIo`
- [X] T005 [P] Add the `IIntentChannel` seam to `CassoEmuCore/Cli/IIntentChannel.h` per [contracts/channel.md](contracts/channel.md). `StateIntent` returns void — a failure to deliver degrades to the fallback, and no caller could act on an error
- [X] T006 Add `PickUpIntent` to `CassoEmuCore/Devices/Disk/ExternalChangePolicy.h` with `Unstated`, `TakeUpInPlace` and `Restart`. `Unstated` is a real value, not a missing one: it is what every writer that is not `CassoCli` produces
- [X] T007 Add `MountedImageState` and `PendingChange` to `CassoEmuCore/Devices/Disk/MountedImageState.h`/`.cpp` per [data-model.md](data-model.md)
- [X] T008 Give `DiskImageStore::Entry` its `sharedState` member and record an `ImageIdentity` into it at mount (FR-001) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and clear it at eject. **Foundational rather than part of the build loop**, because the re-check below is inert without it
- [X] T009 Re-check the recorded identity immediately before the emulator commits (FR-003, FR-027) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, whatever the watcher has or has not reported. **A failing re-check refuses the commit and surfaces through the existing flush-error path**, and the image stays dirty so the writes are still in memory and still flushable once the conflict is resolved -- nothing is dropped; the conflict phase later refines that into a question rather than a refusal. Both later stories cite this as the guarantee that holds when a notification is missed, so it cannot be built after them
- [X] T010 Refresh the recorded identity after every commit the emulator itself makes (FR-004), in the same file. **FOUNDATIONAL, and not separable from the re-check above**: without it the first flush changes the file, nothing updates what was recorded, and the SECOND flush of every ordinary session fails its own re-check and is refused. The two are one mechanism and cannot straddle a checkpoint
- [X] T011 Register the new core files in `CassoEmuCore/CassoEmuCore.vcxproj`
- [X] T012 [P] Add `UnitTest/EmuTests/FakeImageWatcher.h` and `UnitTest/EmuTests/FakeIntentChannel.h`, so a test can drive a change and a stated intent with no file and no window
- [X] T013 Add `UnitTest/EmuTests/ImageIdentityTests.cpp` covering the comparison rules: two recorded identities matching, either unrecorded never comparing equal, and a change in size alone or in time alone being a change. **In `EmuTests/` with its peers**, where every other disk test lives
- [X] T014 Register the new test files in `UnitTest/UnitTest.vcxproj`
- [X] T015 Verify the identity tests discriminate: make the comparison ignore the `recorded` flag, confirm they go red, restore. A default-constructed identity comparing equal to a real one is the bug the flag exists to prevent

**Checkpoint**: Identity, the re-check and the seams exist and are proven. **This is a committable state in which a flush over an externally-changed image now fails visibly, with no question to answer until the conflict phase.** Phase 3's checkpoint records its equivalent caveat; this one should not be quieter.

---

## Phase 3: User Story 1 — The build loop shows what was just built (P1) 🎯 MVP

**Goal**: A change made outside reaches the running guest without the developer ejecting and re-inserting by hand.

**Independent test**: Mount a disk, write a changed program onto it with `disk put` from a second process, confirm the guest loads the new version with no manual eject (SC-001).

- [X] T016 [US1] Add `ExternalChangePolicy` in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`: given a stated intent and whether the image is dirty, decide take-up / restart / ask / conflict. Pure and injected — no clock, no files, no UI. **The fallback answer this originally took was built and then removed by owner decision** (FR-007): nothing stated means the user is asked
- [X] T017 [US1] Compose the *ask* prompt in a new `CassoEmuCore/Devices/Disk/ChangePrompt.h`/`.cpp` (FR-007), which every question this feature asks goes through. **Kept out of `ExternalChangePolicy`** for the reason backup handling was kept out of `MountedImageState`: the policy decides, and composing text is a second job, since asking is the default fallback and this phase ships with it. Core decides what is asked and which answers exist; the shell only shows it
- [X] T018 [US1] Match an image path to a mounted bay in `CassoEmuCore/Devices/Disk/MountedImageState.cpp` (FR-002), normalizing the way mount does. **Comparing two spellings of one path is assertable logic**, so no executable does it
- [X] T019 [US1] Coalesce changes on a quiet period measured from the LAST change (FR-013) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The clock is injected so a test needs no waiting. **Start at 1 second**, matching the spindown debounce the controller already applies, as a named constant so tuning it later is one edit
- [X] T020 [US1] Handle a change arriving while an earlier one is still being applied, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`: it updates the pending record rather than being dropped because something was in progress
- [X] T021 [US1] Defer the pick-up while the image is held by another process, using the existing `IDiskFileIo::IsHeldByAnotherProcess`. The quiet period alone does not cover a text editor or copy tool still writing; both writers here commit atomically, but a third-party writer need not. **The deferral is indefinite and silent by design** -- the pick-up simply happens once the hold is released -- so no timeout is added and none is needed
- [X] T022 [US1] Add `ApplyPendingPickUp` to `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, taking up the newest contents at a moment with no disk operation in flight (FR-014). **The decision and the work are core's**; the shell only says when
- [X] T023 [US1] Decide the restart in core and PERFORM it in the shell, through a machine-reset callback on `DiskImageStore` mirroring `SetMotorOffFlushCallback` (FR-009, FR-010). **A device-layer image store must not reach machine lifecycle directly** -- that is a layering inversion, and the callback is what keeps the decision core-side and the action shell-side
- [X] T024 [US1] Add an idle callback to `CassoEmuCore/Devices/Disk2Controller.h`/`.cpp`, mirroring `SetMotorOffFlushCallback`: fired from `Tick` on the CPU thread when no disk operation is in flight, **rate-limited to at most once per emulated frame** by a named cycle constant. `Tick` is cycle-driven and pumped per instruction, and "no operation in flight" is true nearly always, so an ungated callback would be an indirect dispatch on essentially every instruction. **This seam does not exist** -- `MachineManager` has no tick entry point, only the motor-off callback -- so it is new design work rather than wiring, and the MVP has no delivery path without it. **The spindown callback alone is not enough and this is the headline scenario**: it fires only after a motor-on to motor-off transition with the spindown timer expiring, so a guest sitting at a BASIC prompt -- which is exactly how the build loop is used -- never reaches it and never sees the change. FR-014 asks only that no operation be in flight; spindown is sufficient, not necessary
- [X] T025 [US1] Add a test asserting a pick-up reaches an IDLE machine that has never spun its motor. Without it the MVP fails its own acceptance scenario and looks like a wiring bug
- [X] T026 [US1] Call `ApplyPendingPickUp` from the motor-spindown callback in `Casso/Shell/MachineManager.cpp`, alongside the existing `FlushAll` wiring. **One line and no decisions** — the callback is documented as "a naturally debounced, race-free point" on the thread that owns disk writes
- [X] T027 [P] [US1] Add `Win32ImageWatcher` in `CassoEmuCore/Devices/Disk/Win32ImageWatcher.h`/`.cpp` over `ReadDirectoryChangesW`. **Watch the DIRECTORY, not the file**: both writers commit by renaming a temporary over the target, so a handle on the image sees its own replacement as a delete
- [X] T028 [US1] Install BOTH new callbacks in `Casso/Shell/MachineManager.cpp` -- the idle callback and the machine-reset callback -- alongside the spindown wiring. **Adding a seam is not wiring it**: the idle path is the MVP's only delivery route for a guest sitting at a prompt, and its own test fails until this exists
- [X] T029 [US1] Own the watch lifecycle -- register at mount, drop at eject -- in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, driven through the `IImageWatcher` seam so `FakeImageWatcher` can assert it. **`Casso/Shell/DiskManager.cpp` instantiates the Win32 watcher and hands it over, and does nothing else**: mount-registers-a-watch is orchestration, and orchestration is core's
- [X] T030 [US1] Treat a directory that cannot be watched as `watching = false` rather than an error (FR-032) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, leaving the write-time re-check as the guarantee. **A watchable but untrustworthy location -- a network share, a synchronizing folder -- needs no detection rule of its own**: the re-check is the answer there too, and the only cost is promptness
- [X] T031 [US1] Add the measurement seam SC-006 needs, in `Casso/Shell/DiskManager.cpp`: an undocumented switch that injects a failing `IImageWatcher` and skips the idle-callback registration. **Designed in now rather than retrofitted** -- it has to sit where the watcher is constructed, and bolting it on later is how a developer switch leaks into the UI. Not a user-facing off-switch, and not in the help
- [X] T032 [US1] Show the pick-up banner using the existing `DxuiInfoBanner`, carrying a Restart action that remains available afterward (FR-009, FR-010), as a new composite widget in `Dxui/Widgets/DxuiActionBanner.h`/`.cpp`, beside `DxuiInfoBanner` which it composes, using `DxuiButton` for the action -- **not the `DxuiButtonRow` in `Dxui/Window/`, which is dialog chrome** -- with `Casso/EmulatorShell.cpp` holding only its instantiation. **A host inside the exe could not satisfy the Dxui test this phase also requires**, and Principle VI is the rule that has already been broken twice here. `DxuiInfoBanner` cannot carry the action alone: its own header says "not clickable, no raised surface". **Presentation only** — the text and the available actions come from `ExternalChangePolicy`.
- [X] T033 [US1] Host that banner non-modally over the running machine in `Casso/EmulatorShell.cpp`. The banner appears only inside dialogs and a settings page today, so hosting one over the running machine is genuinely new work rather than wiring. Nothing in the tree hosts a non-modal banner over the running machine yet, so the host is new work rather than a wiring task
- [X] T034 [US1] Make a standing report absorb further changes rather than stack (FR-011) in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`, and make acting on it read the image fresh so it takes the MOST RECENT contents (FR-012)
- [X] T035 [US1] Present the *ask* prompt in `Casso/EmulatorShell.cpp`, taking its text and its available answers from `ExternalChangePolicy`, **which names the image it is about** (FR-033). **Asking is the default fallback, so this phase does not ship without it** -- core composes the question and nothing draws it today
- [X] ~~T036~~ **WITHDRAWN by owner decision.** The stored fallback answer was built, shipped in a working state, and then removed with its Settings row and its preference. A writer that can speak states its intent and never reaches that branch, so the setting would sit at its default forever while a stale stored answer could cost a disk. See FR-007
- [X] ~~T037~~ **WITHDRAWN with T036.** There is no preference to surface
- [X] ~~T038~~ **WITHDRAWN with T036.** The round-trip test was written, passed, and was deleted with the field it covered
- [X] T039 [US1] Add `UnitTest/Dxui/DxuiActionBannerTests.cpp` beside `DxuiInfoBannerTests.cpp` and `DxuiButtonRowTests.cpp`: the banner lays out with its action, and the action reports being invoked
- [X] T040 [P] [US1] Add `UnitTest/EmuTests/ExternalChangePolicyTests.cpp` sweeping the decision table in BOTH directions: every stated intent against the outcome it produces, and every outcome against a situation that reaches it. A table swept one way hides a missing row
- [X] T041 [US1] Add `UnitTest/EmuTests/SharedImageTests.cpp` driving a change through `FakeImageWatcher` and asserting the store picks it up, refreshes its identity, and does not report its own writes as external
- [X] T042 [US1] Add a coalescing test to `UnitTest/EmuTests/SharedImageTests.cpp`: three changes inside the quiet period produce ONE pick-up, and the third resets the timer rather than the first winning
- [X] T043 [US1] Add a path-matching test to `UnitTest/EmuTests/SharedImageTests.cpp`: two spellings of one path reach the same bay, and a path no bay holds reaches none
- [X] T044 [US1] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj`, `Dxui/Dxui.vcxproj` (the new banner widget), `Casso/Casso.vcxproj` (any new Settings file) and `UnitTest/UnitTest.vcxproj`
- [X] T045 [US1] Verify the pick-up tests discriminate: stop refreshing the identity after the emulator's own commit, confirm they go red on a self-inflicted change, restore
- [X] T046 [US1] **DEFER the pick-up entirely while the image is dirty**, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, until the conflict handling exists. Without this guard the policy emits a conflict outcome that nothing in this phase handles, and an MVP-only ship would pick up over guest writes and discard them -- the exact loss the next story exists to prevent. Remove the guard when that story lands
- [X] T047 [US1] Add a test asserting the deferral: a dirty image plus an external change picks up NOTHING and loses NOTHING in this phase
- [ ] T048 [US1] Walk [quickstart.md](quickstart.md) Scenario 1 against a real build **in its flag-free form** -- `disk put` with no `--on-change`, falling back to the declared answer -- since the flag does not exist until Phase 5. Include the control: confirm eject-and-re-insert is no longer required

- [X] T049 [US1] Add tests for the five decisions in this phase that nothing asserts yet, in `UnitTest/EmuTests/`: prompt composition; a change arriving mid-apply; the held-by-another-process deferral; the machine-restart callback; and the watch-degrade path. **The mid-apply and degrade rules are explicit data-model rules**, so their absence is the least defensible

- [X] T050 [US1] Add a guest-visible pick-up test to `ScenarioTests/`, beside `GuestVisibleBootTests`, asserting the guest can load a program written to the disk after mount. **SC-001 says "the guest can run the new program" and that project exists to witness exactly that**; it was overlooked because the plan miscounted the solution's projects

**Checkpoint**: The build loop works, with the ask prompt as its default. Shippable on its own -- **but only just, and on two conditions**. The loop goes quiet the moment the guest first writes to the disk, because the dirty-image guard defers every pick-up from then on. And a guest write meeting an external change gets a refused commit, its writes held in memory rather than preserved to a file -- safe until the session ends and not after. Both are why the next phase ships next, because the dirty-image guard defers every pick-up from then on. That is the strongest argument for shipping the next phase immediately rather than the one after it.

---

## Phase 4: User Story 2 — No version is discarded (P1)

**Goal**: Where both sides have changes, or where the file itself goes away, both versions survive.

**Independent test**: Guest writes, image changes outside, both versions exist afterwards and the user is told where theirs went (SC-002). Separately: delete the image and confirm the emulator offers to save what it holds, then ejects.

**Two owner rulings reshaped this phase after the MVP shipped, and the tasks below are written to them rather than to what came before.** A conflict is now RESOLVED AND REPORTED rather than asked about (FR-019): both versions survive either way, so a modal that stops a running machine to ask which of two preserved files to look at first is ceremony. And a file that has gone leads to an offer to save followed by an EJECT (FR-018), because a drive holding a disk whose file no longer exists is a drive reporting something untrue.

- [X] T051 [US2] **Remove the dirty-image deferral guard** added in the build-loop phase, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, and feed the image's dirty flag into the policy instead. Leaving it in place makes this entire phase unreachable and every change to a dirty disk silently ignored
- [X] T052 [US2] Split `Unusable` into `Deleted` and `Unusable` in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.h`. **The two differ only in what is true and what is said**, and saying "is no longer accessible" about a file the user deleted is the kind of vagueness that makes a message useless
- [X] T053 [US2] Detect the conflict in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`: an external change AND a dirty image. Either alone is not a conflict, and treating it as one would put a dialog in the build loop
- [X] T054 [US2] Refuse to write an image back over an unresolved external change (FR-022) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, returning through the existing flush-error path so the loss is surfaced rather than dropped. **Already built with the identity re-check**; this task confirms it survives the guard's removal rather than adding it again
- [X] T055 [P] [US2] Put preserved-copy naming and writing in their own `CassoEmuCore/Devices/Disk/PreservedCopy.h`/`.cpp` rather than growing `MountedImageState`, which would otherwise carry path matching, coalescing, watch-degrade, the pending record AND all of this. Principle V
- [X] T056 [US2] Name the preserved copy in `CassoEmuCore/Devices/Disk/PreservedCopy.cpp`: beside the original, timestamped, **disambiguated by a zero-padded counter where one with that timestamp already exists** -- `PROG.20260830-014233.dsk`, then `-02`, then `-03` (FR-021). Unpadded, `-10` would sort before `-2` and the promise that the order reads off the directory would not hold. One-second resolution cannot keep the accumulate-rather-than-overwrite promise alone. **Pure: the timestamp is a parameter, not a call**, so a test can name two copies in the same second on purpose
- [X] T057 [US2] Give `DiskImageStore` a timestamp seam beside its clock seam, so the name above is reachable from a test without waiting a second between cases
- [X] T058 [US2] Resolve the conflict in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`: serialize what the guest holds, write it to the preserved name, take up the external contents, and report. **In that order** -- the guest's version must be on disk before the thing that replaces it is mounted, or a failure between the two loses it
- [X] T059 [US2] Compose the conflict REPORT in `CassoEmuCore/Devices/Disk/ChangePrompt.cpp`, naming the image, the drive, and where the preserved copy went (FR-033, FR-034). It carries one action, its own dismissal: it is no longer a question
- [X] T060 [US2] Where the preserved copy CANNOT be written, do not take up the external contents (FR-024) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`. **This is the decision most worth asserting**: the whole promise is that a version is never destroyed, and a preserve that silently did not happen breaks it exactly where it matters. The conflict stays pending and both versions stay live
- [X] T061 [US2] Say so, in `CassoEmuCore/Devices/Disk/ChangePrompt.cpp`: a refusal that names the image, says the guest's writes could not be preserved, and says nothing was mounted as a result
- [X] T062 [US2] Detect that new contents cannot be used -- deleted, a different format, a different geometry, undecodable -- in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, distinguishing gone from unreadable (FR-016)
- [X] T063 [US2] Compose the gone/unreadable prompt in `CassoEmuCore/Devices/Disk/ChangePrompt.cpp`: `x.dsk in Drive 1 has been deleted` or `... is no longer accessible`, a terse accurate reason, and the offer to save the in-memory copy. **The offer does NOT depend on the dirty flag** -- with the file gone, what the emulator holds may be the only copy of that disk whether the guest wrote to it or not (FR-017)
- [X] T064 [US2] Take the save path from the user through the ordinary save dialog in `Casso/EmulatorShell.cpp`, and route it back to the thread that owns disk writes. **Not the disk picker** -- the user is saving a disk, not choosing one to mount
- [X] T065 [US2] Eject the bay afterwards either way, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp` (FR-018), and do NOT open the disk picker. A drive holding a disk whose file no longer exists reports something untrue, and every later write to it would be refused for a reason the user has already been told once
- [X] T066 [US2] Preserve the guest's copy on an eject that happens with writes the file has not seen (FR-023), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`. An eject must not become the one path that loses work
- [X] T067 [US2] Make no stated intent able to resolve a conflict by discarding (FR-019), in `CassoEmuCore/Devices/Disk/ExternalChangePolicy.cpp`. The intent says how the guest continues, not whether work may be destroyed. **Already built and already tested**; this task exists so removing it later is a deliberate act
- [X] T068 [US2] Handle an image becoming unusable while a report from an earlier change still stands, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [X] T069 [US2] Add conflict tests to `UnitTest/EmuTests/SharedImageTests.cpp`: dirty plus external change preserves and mounts; dirty alone flushes normally; external alone picks up; and the preserved copy actually contains the guest's version rather than the external one
- [X] T070 [US2] Add a preserved-name collision test: two conflicts on one image within the same second produce two distinct copies, neither overwriting the other, sorting in the order they happened
- [X] T071 [US2] Add a preserve-fails test asserting the external contents are NOT mounted and both versions remain reachable
- [X] T072 [US2] Add unusable-contents tests: a deleted image, a wrong-geometry image and an undecodable one each refuse the pick-up, leave the held contents intact until answered, and offer the rescue for a CLEAN image too
- [X] T073 [US2] Add an eject test: ejecting with writes the file has not seen preserves them
- [X] T074 [US2] Verify the conflict tests discriminate: let a stated intent resolve the conflict by discarding, confirm they go red, restore. This is the rule most likely to be "simplified" later by someone reading the intent as a policy for everything
- [X] T075 [US2] Verify the preserve-first ordering discriminates: write the preserved copy AFTER taking up the external contents, confirm the test that reads it back goes red, restore
- [X] T076 [US2] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`
- [ ] T077 [US2] Walk [quickstart.md](quickstart.md) Scenarios 2 and 4 against a real build, reading the preserved copy back to confirm it holds the guest's file

**Checkpoint**: Both versions survive every path, including when the file itself goes away.

---

## Phase 5: User Story 1 completion — intent stated by the writer (P1)

**Goal**: The tool that writes the image says what the change should do.

**⚠️ SPLIT BY DEPENDENCY.** The `disk` subcommand half is buildable today. The assembler half needs spec 026's flat image-target options and its `ImageArtifactSink`, neither of which is on this branch.

**Independent test**: `disk put` with `--on-change restart` onto a mounted image restarts the machine; with `--on-change reload` it does not.

### Buildable now — the `disk` grammar

- [ ] T079 [US1] Add `pickUpIntent` to `CommandLineOptions` in `CassoCore/CommandLineOptions.h` (FR-005), beside the nested `disk` group until 026 provides the flat fields
- [ ] T080 [US1] Add the `--on-change` row to the `disk` grammar in `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](contracts/cli.md), plus the long-option entry so `/on-change` is not shredded into single characters. Document that `reload` is the surface spelling of `TakeUpInPlace`
- [ ] T081 [US1] Refuse an unrecognized value naming the value and listing the two accepted, in `CassoCore/CommandLineParser.cpp`
- [ ] T082 [P] [US1] Add `Win32IntentChannel` in `CassoEmuCore/Cli/Win32IntentChannel.h`/`.cpp`: send `WM_COPYDATA` to every top-level `CassoWindow` found by enumeration -- **not `HWND_BROADCAST`**, which `WM_COPYDATA` may not be sent to. **In core beside `Win32DiskFileIo`, and not negotiable** — this is the sender, its callers run inside `CassoCli.exe`, and `CassoCli` cannot link `Casso.exe`. Validate `dwData` against the registered message id so an unrelated `WM_COPYDATA` is ignored. Use `SendMessage` with a timeout, never `PostMessage`: the payload must outlive the call, and a hung emulator must not hang a build
- [ ] T083 [US1] State the intent AFTER a successful commit in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp`. Before the commit would describe contents not yet on disk
- [ ] T084 [US1] Construct `Win32IntentChannel` and inject it into the CLI path in `CassoCli/CassoCli.cpp`, the composition root, mirroring how the shell hands the watcher over. Nothing else builds the sender
- [ ] T085 [US1] Decode the `WM_COPYDATA` payload in `CassoEmuCore/Cli/Win32IntentChannel.cpp` -- intent byte, UTF-8 path, `cbData` bounds and `dwData` validation -- as a function a test can call. **A truncated or malformed payload is assertable logic** and must not end up inline in the shell
- [ ] T086 [US1] Install the UIPI filter for `WM_COPYDATA` on the emulator window in `Casso/EmulatorShell.cpp`, independently of `InstallDragDropTarget`, which is called only when OLE initialization succeeded. **This is the RECEIVER's job**: the filter takes the receiving `HWND`, and the sender runs in `CassoCli.exe` with no window at all
- [ ] T087 [US1] Receive the message in `Casso/EmulatorShell.cpp` and hand the raw payload to core, which matches it to a bay and records a pending change (FR-006). The shell does no matching
- [ ] T088 [US1] Discard a stated intent whose image did not actually change, in `CassoEmuCore/Devices/Disk/MountedImageState.cpp`. The channel is a hint about a change, not a substitute for observing one
- [ ] T089 [US1] Make stating an intent with no emulator running a no-op rather than an error (FR-015) in `CassoEmuCore/Cli/Win32IntentChannel.cpp`. **This is a SENDER property**: no emulator means no receiver, so the receive-side policy is never consulted and could not carry the rule
- [ ] T090 [P] [US1] Add channel tests to `UnitTest/EmuTests/SharedImageTests.cpp` using `FakeIntentChannel`: the intent reaches the policy, an unstated one falls back, and a stated intent for an unchanged image is discarded. Add payload-decode tests too: truncated, oversized, and a wrong `dwData`
- [ ] T091 [US1] Add a switch-coverage row for `--on-change` in `UnitTest/CliSwitchCoverageTests.cpp` for the `disk` grammar and both flag prefixes, so the flag cannot be documented without working, **and assert it renders in the generated help** for that grammar, which Constitution III requires of every feature
- [ ] T092 [US1] Add a test asserting that stating an intent with no emulator running is a no-op rather than an error (FR-015), driven through `FakeIntentChannel` on the sender side, so the guarantee is verified on this branch rather than only in the gated half
- [ ] T093 [US1] Confirm `disk put --on-change` writes byte-for-byte what `disk put` alone writes, so the no-effect-on-output guarantee is verified on this branch and not only in the gated half
- [ ] T094 [US1] Register this phase's new files in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`

### Gated on spec 026 — the assembler grammar

- [ ] T095 [US1] Add the `--on-change` row to both assembler grammars in `CassoCore/CommandLineParser.cpp`, and move `pickUpIntent` beside 026's flat image-target fields
- [ ] T096 [US1] Refuse `--on-change` without `--disk` in `CassoCore/CommandLineParser.cpp`, sharing 026's wording for image options given with no image target rather than inventing a second phrasing
- [ ] T097 [US1] State the intent after a successful commit in `CassoEmuCore/Cli/ImageArtifactSink.cpp`
- [ ] T098 [US1] Extend the switch-coverage row in `UnitTest/CliSwitchCoverageTests.cpp` to both assembler grammars
- [ ] T099 [US1] Confirm `--on-change` changes no assembled byte: the same source with and without it produces identical images

**Checkpoint**: Intent comes from the writer, for whichever grammars are available.

---

## Phase 6: User Story 3 — Two writers cannot spoil each other's work (P2)

**Goal**: Two writers produce one whole version, and neither loses the other's change unnoticed.

**Independent test**: Two emulator instances flush the same image; the result is one complete version (SC-004). **This fails on today's build**, which is what makes it worth running.

**The temp-name half needs nothing from any earlier phase.** The stale-detection half is already built in Phase 2 and only needs its tests here.

- [ ] T100 [US3] Give `DiskImageStore::WriteFileAtomically` a temporary name that cannot collide between processes (FR-026) in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, **trying `CommitPlan::GetTemporaryPath` first** — it already solves this exact problem for the command line, with an invocation tag and an existence check, and its comment states the failure the emulator still has. Record in a comment why a second scheme was or was not needed
- [ ] T101 [US3] Ensure a temporary left behind by a killed writer is not adopted by another writer as its own (FR-028), in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`
- [ ] T102 [US3] Add two-writer tests to `UnitTest/EmuTests/SharedImageTests.cpp`: two stores committing the same path derive different temporaries (SC-004), and a commit over an unseen change is detected
- [ ] T103 [US3] Add a failure-injection test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting a FAILED write leaves the image byte-for-byte unchanged (FR-029). The temp-path and cleanup changes above are what this invariant rests on
- [ ] T104 [US3] Add a test to `UnitTest/EmuTests/SharedImageTests.cpp` asserting the atomic-rename guarantee itself (FR-025), so a later refactor that writes in place becomes a failure rather than a silent regression
- [ ] T105 [US3] Update the three assertions in `UnitTest/EmuTests/DiskImageStoreTests.cpp` that hardcode `target + ".casso-tmp"` to ask the temp-path helper instead. **Without this, Phase 6 goes red for a reason unrelated to the defect** and the discrimination check below is confounded
- [ ] T106 [US3] Verify the collision test discriminates: restore the fixed `.casso-tmp` suffix, confirm it goes red, restore the fix

**Checkpoint**: Two writers are safe from each other.

---

## Phase 7: Polish & Cross-Cutting

- [ ] T107 [P] Update `docs/Assembler.md` and `docs/disk-write-integrity.md` with the `--on-change` flag and the shared-image behavior, including that a pick-up is a disk swap and cannot be verified safe
- [ ] T108 [P] Add `CHANGELOG.md` entries under `[Unreleased]`: the build loop, the conflict handling, and the two defects fixed — the temp-name collision stated as a data-loss fix
- [ ] T109 Confirm every refusal and conflict names the image it concerns (FR-033, SC-005). A user with two disks mounted cannot act on a message that does not say which
- [ ] T110 Measure the idle cost (FR-031, SC-006) by comparing a watched session against one with the same image mounted and watching disabled, **in the SAME build**. Comparing against NO image mounted would differ by drive emulation and the drive widget too, and would not isolate the watcher. A cross-build A/B is untrustworthy here -- clocks move between runs and swamp the signal. Threshold, in **Release** over three runs of five minutes each: p99 frame time within 2% of the not-watching session, and audio underruns per minute within one event of it -- a zero-tolerance bar on a stochastic metric fails on noise. **Force `watching = false` through the `IImageWatcher` seam** -- a test-only failing watcher -- rather than hunting for a directory the API refuses or adding a user-facing off-switch. **Suppress the idle callback in that arm too**: leaving it running in both arms cancels out the very cost this measures, since the idle tick is part of detection. Debug builds underrun on this hardware regardless, so Debug numbers prove nothing
- [ ] T111 Confirm a session with no external change writes byte-for-byte what today's build writes, and at the same moments (FR-030, SC-003)
- [ ] T112 Run `scripts\CheckStyle.ps1` before the first commit containing a new file, since diff mode cannot see a file that has never been committed
- [ ] T113 Run `scripts\Build.ps1 -RunCodeAnalysis` on a clean rebuild and resolve to zero warnings. Analysis over a stale Release build fabricates LNK4020 noise
- [ ] T114 Run the full suite in Debug and compare against the Phase 1 baseline
- [ ] T115 Run the full suite in **Release** and compile for **ARM64**. Quality Gates 1 and 4 require both, and this feature adds two Win32 components
- [ ] T116 Walk [quickstart.md](quickstart.md) end to end, including the two-emulator case that fails on today's build

---

**Checkpoint**: Gates green, documentation current, quickstart walked. Commit.

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

**MVP is Phase 3.** It delivers the loop the whole disk-writing capability exists to serve, and it discards no image file, so it is safe to ship before the conflict handling -- with the caveat its own checkpoint records.

**Ship Phase 4 next, not Phase 5.** Phase 3 makes an existing loss case *more likely* by picking changes up more often, so the guarantee that nothing is discarded should follow immediately. The stated-intent work is a convenience by comparison, and half of it is gated on 026 in any case.

**The temp-name fix can jump the queue** if a quick, self-contained win is wanted: a real defect, no new machinery, and a data-loss bug in shipped code.
