# T130 — Variable-declaration column-alignment audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T130
**Constitution**: §I 1.4.0 — column-align sequential declarations: type,
pointer/reference symbol, name, `=`, value. If any line in a block has a
pointer `*` or reference `&`, all lines must include a column for that
symbol; non-pointer lines use a space placeholder so subsequent columns
stay aligned.
**Audit date**: Phase 16

## Method

For every `.cpp` file changed in this feature, scan for runs of ≥ 2
consecutive declaration-like lines (lines matching
`^\s+TYPE [*&]?\s*NAME(\s*=.*)?;`). For each run, compute the column of
the `=` sign on every line that has one. If those columns are not all
equal, the block is a candidate violation. Restrict the audit to blocks
that contain at least one line **added** by this feature.

## Result

The initial scan reported 33 candidate blocks across 16 files. After
filtering to blocks that contain any added line, **23 real violations**
were identified — all in files newly authored by this feature or in
new test scaffolding. Each was fixed by re-padding the shorter-name
lines to align the `=` sign at the column dictated by the longest name
in the block (or, where a `static constexpr` group was abutting a non-
const local-variable group, by inserting a single blank line to split
them into two independently aligned groups, per the constitution's
group-separation allowance).

### Violations found and fixed

| #  | File:Line                                                 | Fix                                                              |
|----|-----------------------------------------------------------|------------------------------------------------------------------|
| 1  | `CassoCore/Cpu6502.cpp:67`                                | Padded `hr` to 10-char name column (matches `dispatched`).       |
| 2  | `Casso/EmulatorShell.cpp:1691`                            | Padded `drive` to 7-char name column (matches `hrMount`).        |
| 3  | `Casso/EmulatorShell.cpp:1966`                            | Inserted blank line between `static constexpr` and local groups. |
| 4  | `CassoEmuCore/Devices/Disk/DiskImageStore.cpp:225`        | Padded `hr` to 6-char name column (matches `fileOk`).            |
| 5  | `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp:196`     | Padded primitive names to 25-char column (matches `encoded[…]`). |
| 6  | `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp:449`     | Padded all 19 primitive lines to 25-char column.                 |
| 7  | `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp:555`     | Padded `high`/`bit0`/`bit1`/`idx` to match `shift` (5 chars).    |
| 8  | `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp:603`     | Padded primitive names to 21-char column (matches `data[…]`).    |
| 9  | `CassoEmuCore/Devices/Disk/WozLoader.cpp:222`             | Padded primitive names to 20-char column (matches `tmap[…]`).    |
| 10 | `CassoEmuCore/Devices/DiskIIController.cpp:24`            | Padded `i` to 6-char column (matches `result`).                  |
| 11 | `CassoEmuCore/Video/AppleDoubleHiResMode.cpp:132`         | Padded primitive names to 27-char column (matches `dots[…]`).    |
| 12 | `UnitTest/EmuTests/DiskIITests.cpp:17`                    | Padded all to 20-char column (matches `kSyntheticTrackBytes`).   |
| 13 | `UnitTest/EmuTests/DiskIITests.cpp:159`                   | Padded `disk` to 9-char column (matches `bitOffset`).            |
| 14 | `UnitTest/EmuTests/DiskIITests.cpp:236`                   | Padded `disk`/`img`/`off`/`i` to 5-char column (matches `value`).|
| 15 | `UnitTest/EmuTests/DiskImageStoreTests.cpp:31`            | Padded `kSlot` to 6-char column (matches `kDrive`).              |
| 16 | `UnitTest/EmuTests/DiskImageStoreTests.cpp:109`           | Padded `raw` and `invoked` `=` columns to align.                 |
| 17 | `UnitTest/EmuTests/DiskImageStoreTests.cpp:156`           | Padded `raw` to 10-char column (matches `flushCount`).           |
| 18 | `UnitTest/EmuTests/HeadlessHost.cpp:123`                  | Padded `hr` to 11-char column (matches `mainRamBase`).           |
| 19 | `UnitTest/EmuTests/NibblizationTests.cpp:179`             | Padded `raw` to 6-char column (matches `differ`).                |
| 20 | `UnitTest/EmuTests/PerformanceTests.cpp:30`               | Padded all to 27-char column (matches `kStabilityToleranceFraction`). |
| 21 | `UnitTest/EmuTests/Phase11IntegrationTests.cpp:129`       | Padded `raw`/`external` to align with `bitsBefore`/`bitsAfter`.  |
| 22 | `UnitTest/EmuTests/ResetSemanticsTests.cpp:284`           | Padded `page`/`offset` to 14-char column (matches `pageHasNonZero`). |
| 23 | `UnitTest/EmuTests/WozLoaderTests.cpp:118`                | Padded `i`/`diff` to align with `byteCount`.                     |

### Pre-existing decl blocks (out of scope)

10 hits remain in pre-existing decl blocks within changed files, but at
lines that were not modified by this feature (`git log -L` confirms no
commit on this branch touched them):

- `Casso/EmulatorShell.cpp:524` (`wstring`/`Word`/`Word` slot menu builder)
- `Casso/EmulatorShell.cpp:2356` (`CopyScreenText` clipboard locals)
- `Casso/EmulatorShell.cpp:2447` (`CopyScreenshot` BITMAPINFOHEADER block)
- `UnitTest/EmuTests/DeviceTests.cpp:61, 70` (RAM/ROM-init test fixtures)
- `UnitTest/EmuTests/KeyboardTests.cpp:95`
- `UnitTest/EmuTests/VideoRenderTests.cpp:151, 154, 245, 413`

These are pre-feature tech debt and are out of Phase 16 scope.

## Verdict

**PASS** — 0 column-alignment violations introduced by this feature
after fixes. All 23 real violations were corrected by surgical
whitespace-only edits that do not affect compilation, semantics, or
test outcomes.
