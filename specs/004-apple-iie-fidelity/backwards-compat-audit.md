# Backwards-Compatibility Audit — feature `004-apple-iie-fidelity`

**Phase 14 / T121.** Audit log enumerating every test surface and machine
configuration touching original Apple ][ or Apple ][+ behavior, classifying
each as **Pinned** (must never change byte-exactly) or **Growable** (can
add new tests, can't break existing) and listing every assertion modified
during phases 1-13 of this feature with its justification.

Scope: FR-039 (][ and ][+ continue to work; existing tests pass without
assertion changes other than those that encoded bugs since fixed) and
FR-040 (composition over branching — //e wiring is added on top of, not
substituted for, the ][/][+ wiring).

---

## 1. Architectural pin: machine configurations

The two production ][/][+ machine configs MUST remain byte-identical to
their pre-feature shape. They are owned by the Phase 003 platform
emulator feature and the Phase 004 feature has no business modifying
them. If a Phase 004 commit had to change either file, that would be a
direct violation of FR-040 (composition over branching) — the //e
configuration lives in its own file (`apple2e.json`) and the //e build
path is a *separate* composition.

### 1.1 Files

| File                          | Status   | Notes                                              |
| ----------------------------- | -------- | -------------------------------------------------- |
| `Machines/apple2.json`        | **Pinned** | ][ device list, RAM map, video modes, keyboard. |
| `Machines/apple2plus.json`    | **Pinned** | ][+ device list, RAM map, video modes, keyboard.|
| `Machines/apple2e.json`       | Growable (//e) | Owned by feature 004; can change freely.   |

### 1.2 Verification

`git diff master -- Machines/apple2.json Machines/apple2plus.json`
returns the **empty diff** at HEAD = `880d459`. Neither file has been
modified by any commit on the `004-apple-iie-fidelity` branch:

```
$ git log master..HEAD --oneline -- Machines/apple2.json Machines/apple2plus.json
(no commits)
```

The pinned blob hashes (from `git ls-tree master`) are:

```
ad7e61603ebe1f09e6e3fb3403d1c9474741700f  Machines/apple2.json
fe218ad7c443b4dde832849787171d196a08f834  Machines/apple2plus.json
```

These hashes MUST match at every Phase 14+ checkpoint. If they ever
diverge from master without a corresponding spec amendment, that's a
regression — fix the production code, not the configs.

### 1.3 Pinned shape (assert this in `BackwardsCompatTests`)

The configurations encode the following invariants. Each is asserted
directly by `UnitTest/EmuTests/BackwardsCompatTests.cpp` so a future
edit to either JSON file fails the suite:

| Invariant                                                  | Asserted in                                |
| ---------------------------------------------------------- | ------------------------------------------ |
| `name` = "Apple \]\[" / "Apple \]\[ plus"                  | `*_Json_ParsesAsValidMachineConfig`        |
| `cpu` = "6502"                                             | `*_Json_ParsesAsValidMachineConfig`        |
| Single RAM region $0000-$BFFF (no `aux` bank)              | `*_NoAuxRam_NoExtendedVideoModes`          |
| `systemRom.address` = $D000                                | `*_NoAuxRam_NoExtendedVideoModes`          |
| Video modes count = 3 (no text80, no doublehires)          | `*_NoAuxRam_NoExtendedVideoModes`          |
| Keyboard type = "apple2-uppercase"                         | `*_KeyboardAndDevices_Unchanged`           |
| `internalDevices` count = 3                                | `*_KeyboardAndDevices_Unchanged`           |
| Has `apple2-keyboard`, `apple2-speaker`, `apple2-softswitches` | `*_KeyboardAndDevices_Unchanged`       |
| **Does NOT** have `apple2e-mmu`                            | `*_NoMmuPresent`                           |
| **Does NOT** have `apple2e-keyboard`                       | `*_NoMmuPresent`                           |
| **Does NOT** have `apple2e-softswitches`                   | `*_NoMmuPresent`                           |
| **Does NOT** have `language-card`                          | `*_NoMmuPresent`                           |
| `slots` count = 0 (no on-by-default slot devices)          | `*_KeyboardAndDevices_Unchanged`           |

---

## 2. Headless harness composition pin

`HeadlessHost::BuildAppleII` and `HeadlessHost::BuildAppleIIPlus` MUST
remain compositions of *only* the deterministic harness primitives
(`Prng`, `MockHostShell`, `FixtureProvider`, `IAudioSink`). They MUST
NOT pull in the //e wiring (no `MemoryBus`, no `RamDevice`, no
`AppleIIeMmu`, no `AppleIIeKeyboard`, no `AppleIIeSoftSwitchBank`, no
`LanguageCard`, no `LanguageCardBank`, no `EmuCpu`, no
`DiskIIController`).

The //e build path lives in a separate function
(`HeadlessHost::BuildAppleIIe`) which composes the same harness
primitives **plus** the //e devices on top. Branching the ][/][+ build
path to add //e behavior would violate FR-040 and break the test
`AppleII_HeadlessHost_Composes`.

### 2.1 Verification

The composition pin is enforced by:
- `BackwardsCompatTests::AppleII_HeadlessHost_Composes`
- `BackwardsCompatTests::AppleIIPlus_HeadlessHost_Composes`
- `BackwardsCompatTests::MachineKinds_RemainDistinct`

Each asserts every //e-only `unique_ptr` in `EmulatorCore` remains
`nullptr` after `BuildAppleII` / `BuildAppleIIPlus`, and that the three
machine kinds (`AppleII`, `AppleIIPlus`, `AppleIIe`) remain pairwise
distinct enum values.

### 2.2 Determinism pin

`BuildAppleII` and `BuildAppleIIPlus` use the same pinned seed
(`HeadlessHost::kPinnedSeed = 0xCA550001`) as `BuildAppleIIe`, asserted
to produce byte-identical Prng output across two independent builds:

- `BackwardsCompatTests::AppleII_DeterministicAcrossTwoBuilds`
- `BackwardsCompatTests::AppleIIPlus_DeterministicAcrossTwoBuilds`

---

## 3. Test-file audit (][ / ][+ assertion changes per phase)

The following test files were modified during Phases 1-13 of this
feature. Each is audited for ][/][+ assertion changes. **None of the
listed assertion changes weakens an existing ][/][+ guarantee.**

| File                                              | Modified phases | ][/][+ assertions weakened? | Justification |
| ------------------------------------------------- | --------------- | --------------------------- | ------------- |
| `UnitTest/Cpu6502InterfaceTests.cpp`              | 1-3             | No                          | CPU-level; orthogonal to machine kind. ][/][+ continue to share the 6502 with //e. |
| `UnitTest/CpuIrqTests.cpp`                        | 1               | No                          | New IRQ-acknowledge tests; pre-existing IRQ semantics unchanged. |
| `UnitTest/TestHelpers.h`                          | 1-7             | No                          | Header-only helpers; no ][/][+ assertion lives here. |
| `UnitTest/EmuTests/DeviceTests.cpp`               | 2-6             | No                          | Per-device unit tests use `MockBus` only; not machine-kind specific. |
| `UnitTest/EmuTests/DiskIINibbleEngineTests.cpp`   | 11              | No                          | Disk II nibble engine; equally exercised by ][/][+ + Disk II and //e + Disk II. Bit-exact baseline preserved. |
| `UnitTest/EmuTests/DiskIITests.cpp`               | 11              | No                          | Same. ][/][+ booting Disk II remains a supported scenario. |
| `UnitTest/EmuTests/DiskImageStoreTests.cpp`       | 11              | No                          | Disk-image abstraction is below the machine layer. |
| `UnitTest/EmuTests/DiskImageTests.cpp`            | 11              | No                          | Same. |
| `UnitTest/EmuTests/EmulatorShellResetTests.cpp`   | 8, 12           | No                          | Soft-reset semantics; `Apple2`/`Apple2Plus` reset paths preserved verbatim. |
| `UnitTest/EmuTests/FixtureProvider.{cpp,h}`       | F0, 7           | No                          | Test infrastructure; no production assertion. |
| `UnitTest/EmuTests/HeadlessHost.{cpp,h}`          | F0, 7, 11       | No                          | `BuildAppleII` / `BuildAppleIIPlus` remain composition-only branches; no //e wiring leak. |
| `UnitTest/EmuTests/HeadlessHostTests.cpp`         | F0, 7           | No                          | Determinism pin; uses `BuildAppleIIe` exclusively in current tests. |
| `UnitTest/EmuTests/IFixtureProvider.h`            | F0              | No                          | Interface only. |
| `UnitTest/EmuTests/InterruptControllerTests.cpp`  | 1               | No                          | New tests added; no pre-existing ][/][+ assertion modified. |
| `UnitTest/EmuTests/KeyboardTests.cpp`             | 4-5             | No                          | `AppleIIKeyboard` (][/][+) tests untouched; `AppleIIeKeyboard` tests are new and additive. |
| `UnitTest/EmuTests/LanguageCardTests.cpp`         | 6, 10           | No                          | `LanguageCard` itself is shared; ][+ + LC scenarios continue to pass. |
| `UnitTest/EmuTests/MmuTests.cpp`                  | 5-6, 10         | No                          | New `AppleIIeMmu` tests. The MMU is //e-only by FR-040; ][/][+ never see it. |
| `UnitTest/EmuTests/NibblizationTests.cpp`         | 11              | No                          | Disk-layer; orthogonal to machine kind. |
| `UnitTest/EmuTests/Phase7IntegrationTests.cpp`    | 7               | N/A                         | New file; //e-only integration. |
| `UnitTest/EmuTests/Phase8IntegrationTests.cpp`    | 8               | N/A                         | New file; //e-only integration. |
| `UnitTest/EmuTests/Phase9IntegrationTests.cpp`    | 9               | N/A                         | New file; //e-only integration. |
| `UnitTest/EmuTests/Phase11IntegrationTests.cpp`   | 11              | No                          | //e + Disk II; the Disk II rewrite preserved ][/][+ + Disk II byte-for-byte. |
| `UnitTest/EmuTests/EmuValidationSuiteTests.cpp`   | 13              | N/A                         | New file; //e-only validation suite. |
| `UnitTest/EmuTests/PrngTests.cpp`                 | F0              | No                          | Prng is shared; no machine-specific assertion. |
| `UnitTest/EmuTests/ResetSemanticsTests.cpp`       | 7-8             | No                          | New tests for power-cycle vs. soft-reset; ][/][+ reset semantics preserved by composition. |
| `UnitTest/EmuTests/SoftSwitchReadSurfaceTests.cpp`| 5-6             | No                          | New //e read-surface tests; ][/][+ soft-switch reads not touched. |
| `UnitTest/EmuTests/SoftSwitchTests.cpp`           | 5-6             | No                          | `AppleIISoftSwitches` tests untouched; `AppleIIeSoftSwitchBank` tests are new and additive. |
| `UnitTest/EmuTests/SpeakerTests.cpp`              | 4               | No                          | `AppleSpeaker` is shared. |
| `UnitTest/EmuTests/TextScreenScraper.{cpp,h}`     | 7-13            | No                          | Test helper; ][/][+ text scraping unchanged. |
| `UnitTest/EmuTests/KeystrokeInjector.{cpp,h}`     | 7-13            | No                          | Test helper; ][/][+ keyboard-injection paths preserved. |
| `UnitTest/EmuTests/MemoryProbeHelpers.{cpp,h}`    | 7-13            | No                          | Test helper; orthogonal to machine kind. |
| `UnitTest/EmuTests/MockAudioSink.{cpp,h}`         | F0              | No                          | Test infrastructure. |
| `UnitTest/EmuTests/MockHostShell.{cpp,h}`         | F0              | No                          | Test infrastructure. |
| `UnitTest/EmuTests/MockIrqAsserter.{cpp,h}`       | F0              | No                          | Test infrastructure. |
| `UnitTest/EmuTests/VideoModeTests.cpp`            | 9, 12           | No                          | New 80-col / DHGR / mixed-mode tests; pre-existing text40/lores/hires assertions byte-exactly preserved. |
| `UnitTest/EmuTests/VideoRenderTests.cpp`          | 9, 12           | No                          | Same. |
| `UnitTest/EmuTests/VideoTimingTests.cpp`          | 7-9             | No                          | Composition tests; ][/][+ timing pinned. |
| `UnitTest/EmuTests/WozLoaderTests.cpp`            | 11              | No                          | WOZ loader is below the machine layer. |
| `UnitTest/UnitTest.vcxproj`                       | 1-14            | No                          | Project file; only adds new compilation units. |

### 3.1 Net effect

Across phases 1-13, **zero pre-existing ][/][+ assertions were
weakened**. Every change is one of:
1. **Additive** — new test methods asserting //e-specific behavior in
   new test classes, never touching ][/][+ test classes.
2. **Helper-shared** — refactoring a test helper that ][/][+ tests
   happen to call; the ][/][+ test bodies remain byte-identical.
3. **New device** — adding a new `AppleIIe*` device class with its own
   test class; the original `AppleII*` device classes and their tests
   are unchanged.

This is the FR-040 composition pattern in practice: every //e change is
new code added alongside the ][/][+ code, never a substitution.

---

## 4. Pinned ][/][+ tests (must never change byte-exactly)

These test methods encode the canonical ][/][+ regression gate. They
MUST NOT be removed, renamed, or weakened:

- `BackwardsCompatTests::AppleII_Json_ParsesAsValidMachineConfig`
- `BackwardsCompatTests::AppleIIPlus_Json_ParsesAsValidMachineConfig`
- `BackwardsCompatTests::AppleII_NoMmuPresent`
- `BackwardsCompatTests::AppleIIPlus_NoMmuPresent`
- `BackwardsCompatTests::AppleII_NoAuxRam_NoExtendedVideoModes`
- `BackwardsCompatTests::AppleIIPlus_NoAuxRam_NoExtendedVideoModes`
- `BackwardsCompatTests::AppleII_KeyboardAndDevices_Unchanged`
- `BackwardsCompatTests::AppleIIPlus_KeyboardAndDevices_Unchanged`
- `BackwardsCompatTests::AppleII_HeadlessHost_Composes`
- `BackwardsCompatTests::AppleIIPlus_HeadlessHost_Composes`
- `BackwardsCompatTests::AppleII_DeterministicAcrossTwoBuilds`
- `BackwardsCompatTests::AppleIIPlus_DeterministicAcrossTwoBuilds`
- `BackwardsCompatTests::MachineKinds_RemainDistinct`

Plus every pre-existing ][/][+ test in:
- `UnitTest/EmuTests/KeyboardTests.cpp` (the `AppleIIKeyboard*` tests)
- `UnitTest/EmuTests/SoftSwitchTests.cpp` (the `AppleIISoftSwitches*` tests)
- `UnitTest/EmuTests/LanguageCardTests.cpp` (][+ + LC scenarios)
- `UnitTest/EmuTests/DiskIITests.cpp` (][+ + Disk II scenarios)

Any commit that needs to delete or weaken any of these tests is a
regression. The fix belongs in the production code, never in the test.

---

## 5. Growable ][/][+ test surface

It is acceptable — encouraged, even — to **add** new ][/][+ tests in
later phases. What is not acceptable is removing or weakening existing
ones. Specifically, you may:

- Add new test methods to `BackwardsCompatTests`.
- Add new test methods to `KeyboardTests`, `SoftSwitchTests`,
  `LanguageCardTests`, `DiskIITests` that exercise ][/][+ paths.
- Tighten an existing assertion (e.g. promote `IsTrue` to
  `AreEqual` with a specific value), provided every previously-passing
  input still passes.

You may not:

- Remove a pre-existing ][/][+ test method.
- Weaken an existing assertion (e.g. demote `AreEqual` to `IsTrue`).
- Modify `Machines/apple2.json` or `Machines/apple2plus.json` from
  this feature branch.
- Add ][/][+-specific code paths that bypass the ][/][+ test gate.

---

## 6. GATE statement

At Phase 14 GATE (T122), the following must hold:

1. `git diff master -- Machines/apple2.json Machines/apple2plus.json`
   produces empty output.
2. The full UnitTest suite passes on x64 Debug, x64 Release, ARM64
   Debug, ARM64 Release.
3. Every test in `BackwardsCompatTests` passes.
4. Every pre-existing ][/][+ test (Phase 003 baseline) still passes.
5. No assertion in any modified test file has been weakened (this
   document audits each file).

If any of these fail, Phase 14 is BLOCKED — fix the production code,
not the tests.
