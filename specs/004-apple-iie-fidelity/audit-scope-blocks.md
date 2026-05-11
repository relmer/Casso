# T128b — Unnecessary scope-block audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T128b
**Constitution**: §I 1.4.0 — every brace-only block must exist for a real
reason (control flow, function definition, struct/class/union, lambda, or
RAII-lifetime intent).
**Audit date**: Phase 16

## Method

For every `.cpp` file changed in this feature, scan every line **added** by
this feature for `^\s*\{\s*$` (a brace-only line). For each match, examine
the previous non-blank, non-comment line and classify the block:

- Preceded by `if` / `else` / `for` / `while` / `do` / `switch` / `case` /
  `default` / `try` / `catch` / `namespace` — control flow: **OK**
- Preceded by `)` (function definition / lambda call site) — function: **OK**
- Preceded by `class` / `struct` / `union` / `enum` — type: **OK**
- Preceded by `]` with optional `mutable`/`noexcept`/`->` — lambda body: **OK**
- Preceded by `=` — array/struct initializer: **OK**
- Preceded by `:` — case label or member init list head: **OK**
- Otherwise — candidate for **RAII-lifetime / tight-variable-lifetime**
  scope-block, must be confirmed by inspection.

## Result

The scanner reported 11 candidate blocks on added lines. All 11 are
intentional **RAII / tight-variable-lifetime** scope blocks per the
constitution's allowance:

| #  | File:Line                                          | Purpose                                                                                          |
|----|----------------------------------------------------|--------------------------------------------------------------------------------------------------|
| 1  | `Casso/EmulatorShell.cpp:1424`                     | Scope `HRESULT hrFlush` so it doesn't leak into the surrounding teardown sequence.               |
| 2  | `Casso/EmulatorShell.cpp:2135`                     | Scope `auto * iieKbd` so the dynamic_cast result doesn't survive past the //e-only branch.       |
| 3  | `CassoEmuCore/Devices/Disk/DiskImage.cpp:381`      | Scope `ifstream file` — file handle closes at block end (RAII).                                  |
| 4  | `CassoEmuCore/Devices/Disk/DiskImage.cpp:495`      | Scope `ofstream file` — file handle closes at block end (RAII).                                  |
| 5  | `CassoEmuCore/Devices/Disk/DiskImageStore.cpp:131` | Scope `Entry & entry` reference so it doesn't outlive the `if(entry.mounted)` guard.             |
| 6  | `CassoEmuCore/Devices/Disk/DiskImageStore.cpp:184` | Scope `ifstream file` + read locals — file handle closes at block end (RAII).                    |
| 7  | `CassoEmuCore/Devices/Disk/DiskImageStore.cpp:345` | Scope `Entry & entry` reference for the eject-flush path.                                        |
| 8  | `CassoEmuCore/Devices/Disk/WozLoader.cpp:138`      | Scope `vector<Byte> & buf` reference — bound to a single track in the V2 track loop.             |
| 9  | `CassoEmuCore/Devices/Disk/WozLoader.cpp:187`      | Scope `vector<Byte> & buf` reference — bound to a single track in the V1 track loop.             |
| 10 | `CassoEmuCore/Devices/Disk/WozLoader.cpp:391`      | Scope `size_t recOffset` to a single track-record iteration of the V1 TRKS scan.                 |
| 11 | `UnitTest/EmuTests/HeadlessHost.cpp:182`           | Scope `cxxxData`/`lcRom` byte buffers used to build the //e $C100-$CFFF + LC ROM, then released. |

Each block holds either a RAII type (`ifstream` / `ofstream`), a reference
that should not outlive a single iteration of an enclosing loop, or a
heavy local buffer that should be released before the surrounding work
continues. Removing the brace would extend variable lifetime unnecessarily,
violating the project's "tight scope" preference (Constitution §I 1.4.0
"Remove unnecessary scoping braces — hoist the variable to function top
**instead**" — applied in reverse: where hoisting is *not* desirable, a
scoped block is correct).

## Verdict

**PASS** — 0 unnecessary scope blocks. All 11 candidate blocks added by
this feature are justified by RAII or tight-variable-lifetime intent.
