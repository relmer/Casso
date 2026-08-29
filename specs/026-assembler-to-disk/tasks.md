# Tasks: Assembler-to-Disk Output

**Input**: Design documents from `specs/026-assembler-to-disk/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: Included and NOT optional. Constitution Principle II requires that every public function and significant code path be covered, so test tasks sit inside each story rather than in a trailing phase.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete work)
- **[Story]**: US1–US4, matching [spec.md](spec.md). Setup, Foundational and Polish carry no story label.

## Path Conventions

Existing solution layout. `CassoCore` and `CassoEmuCore` are static libraries; `UnitTest` links both. Nothing goes in an exe (Constitution Principle VI).

**New `.cpp`/`.h` files need explicit `ClCompile`/`ClInclude` entries in the owning `.vcxproj`.** MSBuild does not glob, so a new file that compiles locally can still be missing from a clean build.

---

## Phase 1: Setup

**Purpose**: A trustworthy baseline before anything changes.

- [ ] T001 Establish a green baseline: run `scripts\Build.ps1` then `scripts\RunTests.ps1 -Build` for x64 Debug, and record the passing test count so later runs are compared against a real number rather than an impression
- [ ] T002 Confirm `git config --get core.hooksPath` returns `.githooks` in this worktree so the style gate runs on push, per `scripts\Build.ps1`'s self-enabling behavior

**Checkpoint**: Known-good starting point.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Save points, which every user story below builds on. Nothing here is user-visible.

**⚠️ No user story work can begin until this phase completes.**

- [ ] T003 Add the `SavePoint` struct to `CassoCore/AssemblerTypes.h` with `bytes`, `loadAddress`/`hasLoadAddress`, `name`, `fileType`/`hasFileType`, per [data-model.md](data-model.md). Use the has-flag idiom rather than sentinels — `$00` is a real DOS 3.3 type and `$0000` is a legal address
- [ ] T004 Add `savePoints` to `AssemblyResult` in `CassoCore/AssemblerTypes.h`, documented as REPORTED not acted on, matching the note `outputFileName` already carries. Leave `bytes`, `startAddress` and `endAddress` meaning what they mean today so existing consumers are untouched
- [ ] T005 Add per-symbol output scope to `AssemblyResult` in `CassoCore/AssemblerTypes.h`, keeping `symbols` itself unchanged in shape and meaning (FR-030)
- [ ] T006 Track the current span's start in `CassoCore/AssemblySession.h`/`.cpp` so a span can be cut, and record each symbol's scope as it is defined
- [ ] T007 Close the final span at end of assembly in `CassoCore/AssemblySession.cpp`, so an assembly with no `SAV` yields exactly one save point covering the whole object and there is no separate single-output path to keep in step
- [ ] T008 Derive each save point's load address as the address of its own first byte in `CassoCore/AssemblySession.cpp` (FR-024), reusing the derivation `AssemblyResult::startAddress` already applies to a whole assembly
- [ ] T009 [P] Add `UnitTest/SavePointTests.cpp` covering the invariants in [data-model.md](data-model.md): at least one save point when bytes were emitted, source order, no byte in two save points, every emitted byte in some save point including a trailing span. Assert the COUNT before iterating — a loop over an empty list passes while checking nothing
- [ ] T010 Register `UnitTest/SavePointTests.cpp` in `UnitTest/UnitTest.vcxproj`
- [ ] T011 Verify T009 discriminates: stub the span cut to always return one save point covering everything, confirm the tests go red, restore. A test that stays green under that mutation is measuring nothing

**Checkpoint**: Save points exist and are proven. User stories can proceed.

---

## Phase 3: User Story 1 — Assemble straight onto a disk (P1) 🎯 MVP

**Goal**: An object lands on an existing image, named and typed, with its load address taken from the source's origin and never from a flag.

**Independent test**: Assemble a source with origin `$6000` to an existing image, read the file back, and confirm contents, type, and a recorded load address of `$6000` with no `--load` anywhere on the command line.

- [ ] T012 [US1] Add the image-target fields (`imagePath`, `onDiskName`, `typeName`, `setStartup`) to `CommandLineOptions` in `CassoCore/CommandLineOptions.h`, grouped and commented per [data-model.md](data-model.md). They live with the assembler options, not under `disk`, whose nested group is documented as holding fields that mean nothing to other subcommands
- [ ] T013 [US1] Add `--disk`, `--as` and `--type` rows to `s_kAs65Flags` and `s_kMerlinFlags` in `CassoCore/CommandLineParser.cpp`, category `AssembledCode` (FR-001, FR-002), reusing `disk put`'s words for `--as` and `--type` rather than inventing a fourth name for the same ideas
- [ ] T014 [US1] Add the same flags to the long-option lists in `CassoCore/CommandLineParser.cpp` so `/disk` is not shredded into `-d -i -s -k` by the single-character normalization — the hazard the table already records for `/flat`
- [ ] T015 [P] [US1] Add `CassoEmuCore/Devices/Disk/AssembledFilePlacement.h`/`.cpp` converting a `SavePoint` to a `FilePayload`, defaulting the type to the filesystem's binary type (FR-006) and the encoding always to `Verbatim`. Forward-declare `VolumeKind` rather than including `VolumeImage.h`, per the header hazard `DiskCommandRunner.h` records
- [ ] T016 [US1] Register `AssembledFilePlacement.h`/`.cpp` in `CassoEmuCore/CassoEmuCore.vcxproj`
- [ ] T017 [US1] Add `CassoEmuCore/Cli/ImageArtifactSink.h`/`.cpp` implementing `ArtifactSink`: open through `DiskImageSession`, compose each save point through `IVolume::Write` onto the buffer the previous call returned, commit once at the end. Never commit per save point — that is what makes FR-014 structural
- [ ] T018 [US1] Register `ImageArtifactSink.h`/`.cpp` in `CassoEmuCore/CassoEmuCore.vcxproj`
- [ ] T019 [US1] Select the sink from the options in `CassoEmuCore/Cli/AssemblerMode.cpp`, with an empty `imagePath` as the single switch so the two sinks cannot disagree about whether a disk is targeted
- [ ] T020 [US1] Take the on-volume load address from the save point in `CassoEmuCore/Cli/ImageArtifactSink.cpp` (FR-005). There is no `--load` and must not be one
- [ ] T021 [US1] Keep listing, symbol table and debug info on the host when an image is targeted (FR-004) — `WriteBinary` is redirected, `WriteListing` is not
- [ ] T022 [US1] Refuse a missing image in `CassoEmuCore/Cli/ImageArtifactSink.cpp`, naming the command that creates one, and do NOT set `OpenedImage::isNew`, which exists for `disk create` and skips the freshness check (FR-018)
- [ ] T023 [US1] Surface the remaining open/write refusals — no recognized filesystem, volume full, no free directory entry, illegal name, image held by another program — as edge-layer verdicts naming the condition (FR-015). Never `E_INVALIDARG`, which marks a coding error and asserts
- [ ] T024 [P] [US1] Add `UnitTest/AssemblerToDiskTests.cpp` covering the happy path against a synthetic in-memory volume, load address from origin, host artifacts staying on the host, and each refusal above. Mock the host edge through `IDiskFileIo`; no test touches a real file
- [ ] T025 [US1] Register `UnitTest/AssemblerToDiskTests.cpp` in `UnitTest/UnitTest.vcxproj`
- [ ] T026 [US1] Add a failed-assembly test asserting the image is byte-for-byte identical, comparing the WHOLE buffer rather than a status code
- [ ] T027 [US1] Verify T026 discriminates: mutate `ImageArtifactSink` to commit after each save point instead of once, confirm the test goes red, restore. The guarantee is structural, so this test can pass without the feature being right

**Checkpoint**: US1 is independently shippable and delivers the correctness win on its own.

---

## Phase 4: User Story 2 — Merlin source that names its own output (P2)

**Goal**: `DSK` and `TYP` mean what Merlin meant, with the command line overriding both.

**Independent test**: Assemble Merlin source carrying `DSK PROG` and `TYP $06` against an image with no naming flags, and confirm the volume shows exactly that name and type.

- [ ] T028 [US2] Delete the `TYP` row from `s_kMerlinBoundary` in `CassoCore/MerlinSubsetBoundary.cpp`
- [ ] T029 [US2] Add `HandlePass1FileType` to `CassoCore/AssemblySession.h`/`.cpp` and an entry in the directive dispatch table, so `TYP` sets the type when an image is targeted (FR-009). Removing the boundary row makes `TYP` fall through to that table, where an absent entry turns it into an unknown directive rather than a working one
- [ ] T030 [P] [US2] Add the ProDOS type map to `CassoEmuCore/Devices/Disk/AssembledFilePlacement.cpp` per the table in [contracts/merlin-directives.md](contracts/merlin-directives.md): `$04`/`$06`/`$FC` map to both filesystems, `$FF` maps on ProDOS only
- [ ] T031 [US2] Refuse `$FF` on DOS 3.3 naming both the type and the filesystem (FR-010), and refuse an unrecognized value naming the byte (FR-011). Never approximate — a guessed type surfaces much later as a program that will not load
- [ ] T032 [US2] Point `DSK` at the on-volume name when an image target is given, keeping the host-file meaning when one is not (FR-008), in `CassoCore/AssemblySession.cpp`
- [ ] T033 [US2] Apply command-line-beats-directive for the name and the type (FR-007) in the assembler, which is the layer that sees both — the precedence `ApplyMerlinDefaults` already declines to guess
- [ ] T034 [P] [US2] Add type-map tests to `UnitTest/AssemblerToDiskTests.cpp` sweeping the map in BOTH directions, the way `UnitTest/DirectiveTokenTests.cpp` sweeps its enum, so a missing row cannot hide
- [ ] T035 [P] [US2] Update `UnitTest/MerlinSubsetBoundaryTests.cpp` for a five-row boundary and assert `TYP` is no longer refused
- [ ] T036 [US2] Add a Merlin fixture under `UnitTest/Fixtures/Merlin/` carrying `DSK` and `TYP`, and mark it in-tree as authored rather than vendor source

**Checkpoint**: US2 works on top of US1.

---

## Phase 5: User Story 3 — One source, several output files (P3)

**Goal**: `SAV` produces several complete outputs, to a volume or to host files, with the per-output artifacts that implies.

**Independent test**: Assemble a two-`ORG`/`SAV` source to an image and confirm both files exist with their own load addresses, and that the second holds only the bytes assembled after the first save.

### Multi-output assembly

- [ ] T037 [US3] Delete the `SAV` row from `s_kMerlinBoundary` in `CassoCore/MerlinSubsetBoundary.cpp`
- [ ] T038 [US3] Add `HandlePass2SaveObject` to `CassoCore/AssemblySession.h`/`.cpp` plus its dispatch entry. It runs in **pass 2**, unlike `DSK`'s pass-1 handler, because a save point is a span of emitted bytes
- [ ] T039 [US3] Cut the span and empty the accumulation on `SAV` (FR-012). Bredon: "after a save, the MERLIN object area is 'empty'" — bytes already saved must not appear in a later file
- [ ] T040 [US3] Make a second `DSK` close the current save point and open another in `CassoCore/AssemblySession.cpp` (FR-025), replacing the last-one-wins note in `HandlePass1ObjectFile`
- [ ] T041 [US3] Resolve names in the order [data-model.md](data-model.md) fixes: each save point takes its own directive name first (`SAV` beating `DSK` where both name one span), THEN the command-line override is applied or refused. Overriding first reaches the same refusal through a state where several save points share a name
- [ ] T042 [US3] Refuse a command-line name with several outputs, naming the flag and the count (FR-026). A command-line TYPE has no such limit
- [ ] T043 [US3] Refuse two outputs resolving to one name, naming the file (FR-027), checked before anything is written. This is deliberately not FR-019's cross-run replace
- [ ] T044 [US3] Iterate save points in `FileArtifactSink` in `CassoEmuCore/Cli/ArtifactWriter.cpp` so `SAV` works with no image target (FR-020). The list is the same list the image sink walks — doing both is cheaper than special-casing one
- [ ] T045 [US3] Buffer every host output until the whole assembly succeeds, so a failure after the first save leaves no host file behind (FR-014)

### Per-output host artifacts

- [ ] T046 [US3] Split listing, symbol and debug artifacts into one set per output, named from the output (FR-028, FR-031, FR-032), in `CassoEmuCore/Cli/ArtifactWriter.cpp`
- [ ] T047 [US3] Rework `Assembler::FormatDebugInfo` in `CassoCore/Assembler.cpp` to index per output rather than from one flat map, so "what is at $0310" has one answer where outputs overlap (FR-029)
- [ ] T048 [US3] Repeat the equates above the first output into every per-output artifact (FR-035, FR-036). Each file must stand alone: a debugger holding only one program still needs the hardware addresses it was opened to resolve
- [ ] T049 [US3] Keep single-output artifact names and destinations as they are (FR-033), the Merlin listing flag excepted
- [ ] T050 [US3] Change the Merlin `-l` row in `s_kMerlinFlags` to take no value, writing `<output>.lst` files rather than standard output (FR-034, FR-037), in `CassoCore/CommandLineParser.cpp`
- [ ] T051 [US3] Give a filename supplied to Merlin's `-l` a diagnostic naming the rule, not a generic unknown-flag message (FR-034)
- [ ] T052 [US3] Leave the as65 `-l` row untouched (FR-038) — an as65 compatibility obligation, and as65 has no directive that could produce a second output

### Tests

- [ ] T053 [P] [US3] Add `UnitTest/MerlinSaveObjectTests.cpp` covering span semantics, per-save load addresses, two `DSK`s, both naming refusals, and the host-file path
- [ ] T054 [US3] Register `UnitTest/MerlinSaveObjectTests.cpp` in `UnitTest/UnitTest.vcxproj`
- [ ] T055 [US3] Assert explicitly that the second output does NOT contain the first's bytes. A cumulative implementation passes every other assertion in this phase
- [ ] T056 [US3] Add a two-save fixture under `UnitTest/Fixtures/Merlin/`, labeled in-tree as authored. `CLOCK.S`'s two `SAV`s are mutually exclusive (`DO HOURS-12 / ELSE / FIN`), so the vendor corpus cannot cover this and must not appear to
- [ ] T057 [P] [US3] Update `UnitTest/MerlinSubsetBoundaryTests.cpp` for a four-row boundary and assert `SAV` is no longer refused
- [ ] T058 [US3] Add a fail-after-first-save test asserting the image is byte-for-byte unchanged AND no host file was left behind

**Checkpoint**: Multi-output assembly works to both targets.

---

## Phase 6: User Story 4 — A disk that boots what was assembled (P3)

**Goal**: One command produces a disk that starts the program just assembled.

**Independent test**: Assemble to a bootable image with the startup flag, read the volume back, and confirm it names the assembled file as its startup program.

- [ ] T059 [US4] Extract the startup-runnability rules from `RunBoot` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp`/`.h`, `IsRunnableAsDos33Greeting` included, into something both routes call (FR-022). **The extraction is the point, not the flag** — a second copy is how `run --merlin` once refused `XC` while `merlin` accepted it
- [ ] T060 [US4] Add the `--startup` row to `s_kAs65Flags` and `s_kMerlinFlags` in `CassoCore/CommandLineParser.cpp`, plus the long-option entries
- [ ] T061 [US4] Call `IVolume::SetStartupProgram` from `CassoEmuCore/Cli/ImageArtifactSink.cpp` after the last save point and before the single commit, so it participates in the same transaction (FR-021)
- [ ] T062 [US4] Refuse `--startup` with no image target (FR-023), and refuse a file the volume's operating system would not run, on the shared rules from T059 (FR-022)
- [ ] T063 [P] [US4] Add startup-program tests to `UnitTest/AssemblerToDiskTests.cpp`, including the DOS 3.3 greeting case where a binary named as the greeting leaves the disk booting and the program never running
- [ ] T064 [US4] Assert `RunBoot` and the assembler path accept and refuse the same things, driving both through the shared rules so the two cannot drift

**Checkpoint**: All four stories complete.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T065 [P] Update `docs/merlin-subset.md`: "Six constructs are recognized and refused by name" becomes four, and `TYP`, `SAV` and the corrected `DSK` move to the supported table (FR-013). `MerlinSubsetBoundary::GetHelpText` needs no edit — it composes from the rows, which is the property the table exists to have
- [ ] T066 [P] Update `docs/Assembler.md` and `README.md`: the three-command build loop becomes two, and the bootable case drops from four commands to the same two
- [ ] T067 [P] Update the assembler help output (FR-017), which generates from the flag tables, and confirm the generated text matches the new rows
- [ ] T068 Add `CHANGELOG.md` entries under `[Unreleased]`: the feature, and the Merlin `-l` change stated plainly as a user-visible change
- [ ] T069 Run `scripts\RunDormannTest.ps1` and `scripts\RunHarteTests.ps1 -SkipGenerate`. This changes assembler output paths, so both are required; the checked-in 200-vector Harte depth is correct because no CPU or instruction-set code is touched. Report the depth the runner prints rather than assuming it
- [ ] T070 Run `scripts\Build.ps1 -RunCodeAnalysis` and resolve to zero warnings. Do this on a clean rebuild — analysis over a stale Release build fabricates LNK4020 noise
- [ ] T071 Run `scripts\CheckStyle.ps1 -Mode Staged` before the first commit containing any new file, since diff mode cannot see a file that has never been committed and will report OK while checking nothing
- [ ] T072 Run the full suite in **Debug** (`scripts\RunTests.ps1 -Build`) and compare the count against T001's baseline. Release runs a different set and verifies no assertion behavior, so it is not a substitute for the gate
- [ ] T073 Walk [quickstart.md](quickstart.md) end to end against a real build, including booting the Scenario 5 disk in the emulator — the only step that actually proves the startup program works
- [ ] T074 Add a dialect-parity test to `UnitTest/AssemblerToDiskTests.cpp` driving the SAME image target through both `as65` and `merlin` and asserting identical placement (FR-003). The capability belongs to the assembler and the directives only feed it, so a dialect must not be required to have directives to reach it — nothing else in this list would catch that guarantee decaying
- [ ] T075 Add a no-image-target regression test asserting that assembling with no `--disk` produces byte-for-byte the same host object as before the feature (FR-016, SC-006), using a checked-in expected artifact rather than a self-comparison. Everything else here tests new behavior; this is the only task that watches the old behavior

---

## Dependencies

```
Phase 1 Setup
   └─> Phase 2 Foundational (save points)   ← blocks everything
          ├─> Phase 3 US1 (P1, MVP)         ← independently shippable
          │      └─> Phase 4 US2 (P2)       ← needs the image target to exist
          │             └─> Phase 5 US3 (P3)
          │                    └─> Phase 6 US4 (P3)
          └─────────────────────────────────────> Phase 7 Polish
```

**Story dependencies are real, not bookkeeping.** US2's directives need somewhere to land, so they need US1's target. US3's `SAV` needs US2's type handling to give each output a type. US4 writes a startup entry into the transaction US1 built.

**The exception worth noting**: the per-output artifact work in Phase 5 (T046–T052) depends only on Phase 2's save points, not on any disk work. It can proceed in parallel with Phases 3 and 4 if that suits.

## Parallel Opportunities

Within phases, `[P]` tasks touch different files and can run together:

- **Phase 2**: T009 alongside T003–T008 once the struct exists
- **Phase 3**: T015 (placement) and T024 (tests) alongside the grammar work in T013–T014
- **Phase 4**: T030 (type map), T034, T035 are three separate files
- **Phase 5**: T053 and T057 while the assembler work lands
- **Phase 7**: T065, T066, T067 are three separate documents

## Implementation Strategy

**MVP is Phase 1 + Phase 2 + Phase 3 (US1).** That delivers assembling straight onto a disk and the load-address correctness fix, which is the reason the feature exists. It ships without a single Merlin directive changing, because the capability is the assembler's and the directives only feed it.

**Then increment**: US2 closes `TYP`, US3 closes `SAV`, US4 adds the boot step. Each is independently testable and each leaves the tree shippable.

**Commit per phase**, per the constitution's commit discipline — not one commit at the end.

## Notes on Testing

Three recorded lessons apply directly here and are worth not rediscovering:

- **A degraded write must not read as a healthy one.** Assert the image is byte-for-byte unchanged, not merely that a call failed. `NibblizationLayer::Denibblize` returning `S_OK` over zero-filled sectors (GH #115) is the case this comes from.
- **Assert a non-zero count before iterating.** A save-point loop over an empty list passes while checking nothing and looks identical in the output to a full run.
- **Mutate what the test covers and confirm the test notices.** The all-or-nothing guarantee is structural — `IVolume` computes a whole buffer or none — so its tests can pass without the feature being right. T011, T027 and T055 exist for this and should not be skipped as ceremony.
